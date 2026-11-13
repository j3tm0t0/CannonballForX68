/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
// r031: sound on the X68000's own FM chip. The arcade board and the X68000 both have a YM2151 at
// 4 MHz, so the register writes of Cannonball's sound code (engine/audio/osound.cpp, the ported
// Z80 program) go to the chip as they are. The Sega PCM chip (drums, voices, engine noise) has no
// counterpart and stays silent for now.
// The sound code runs 125 times a second from the vertical blank interrupt (road_irq.S,
// vblank_work), not from the game loop: the tempo then does not depend on how long a picture takes.
#include "stdint.hpp"
#include "engine/audio/osound.hpp"
#include "engine/audio/osoundint.hpp"
#include <cstring>
#include "assets.hpp"
#include "x68k_sysstat.hpp"
#include <x68k/dos.h>

extern OSound osound;
extern "C" {
 volatile uint8_t audio_hold=1;          // 1 = the sound code must not run (start-up, OSoundInt::init)
 volatile uint8_t audio_super=0;         // 1 = supervisor mode, the chip may be written (set by audio_start)
 void audio_start(int with_game);
 volatile uint32_t audio_ticks_done=0,audio_fm_writes=0,audio_fm_timeouts=0;
 uint8_t audio_enabled=1;                // NOSOUND argument: 0
 uint8_t audio_pcm_enabled=1;            // NOPCM argument: 0
 uint8_t audio_fm_enabled=1;             // NOFM argument: 0 (the sound code still runs, for the PCM part)
 volatile uint32_t audio_pcm_bytes=0,audio_pcm_resyncs=0;
 uint16_t audio_ticks_per_blank=2254;    // 125 Hz / 55.458 Hz, in 1/1000 (HZ60: 2083)
 void audio_vblank();
 void audio_silence();
}
#ifdef CBX68_AUDIO_LOG
// Check aid: every run of the sound code (0x80000000 | command, then its 8 engine bytes in two
// longs) and every register write it makes (register << 8 | value), written to AUDIO.BIN at the
// end. The PC build replays the commands through the same sound code and compares the writes
// (CBX68_AUDIO_REPLAY=AUDIO.BIN).
extern "C"{uint32_t audio_log[300000];volatile uint32_t audio_log_n=0;}
static inline void audio_log_put(uint32_t v){if(audio_log_n<300000)audio_log[audio_log_n++]=v;}
#endif
static inline volatile uint8_t* opm_reg(){return (volatile uint8_t*)0xe90001UL;}
static inline volatile uint8_t* opm_data(){return (volatile uint8_t*)0xe90003UL;}   // read: status (bit 7 busy)
static void opm_wait(){
 unsigned n=0;while((*opm_data()&0x80)&&++n<2000){}
 if(n>=2000)++audio_fm_timeouts;
}
// Register select and data are written with interrupts masked: the system's own handlers (the
// timer interrupt drives the FDD lines through register 0x1B) also select a register, and one of
// them coming in between would send our data to its register. For 0x1B the system's copy of the
// value (IOCS work 0x09DA) is read inside the same masked stretch.
static void opm_write(unsigned r,unsigned v){
 // The wait for the busy flag is done with interrupts open (it can take 17 us, and the raster
 // interrupts must stay on time); only the two writes are masked, after one more look at the flag.
 for(unsigned tries=0;;tries++){
  opm_wait();
  uint16_t sr;__asm__ volatile("move.w %%sr,%0\n ori.w #0x0700,%%sr":"=d"(sr)::"memory");
  if(!(*opm_data()&0x80)||tries>=8){
   if(r==0x1b)v=(v&0x03)|(*(volatile uint8_t*)0x09daUL&0xc0);
   *opm_reg()=uint8_t(r);
   *opm_data()=uint8_t(v);
   __asm__ volatile("move.w %0,%%sr"::"d"(sr):"memory");
   break;}
  __asm__ volatile("move.w %0,%%sr"::"d"(sr):"memory");
 }
 ++audio_fm_writes;
}
// ---- YM2151: the real chip ----
YM2151::YM2151(float,uint32_t){irq=false;}
YM2151::~YM2151(){}
void YM2151::init(int,int){}
void YM2151::stream_update(){}
int YM2151::read_status(){return 1;}     // timer A "has run out" (the caller is called 125 times a second), never busy
void YM2151::write_reg(int r,int v){
 if(!audio_enabled||!audio_super)return;
 r&=255;v&=255;
#ifdef CBX68_AUDIO_LOG
 if(!audio_hold)audio_log_put((uint32_t(r)<<8)|uint32_t(v));
#endif
 // timers: the chip must not raise its interrupt on the X68000 (the sound code is paced by the
 // vertical blank), so the timer control register is left alone
 if(r==0x14)return;
 if(!audio_fm_enabled)return;
 // register 0x1B: bits 7 and 6 are the X68000's ADPCM clock select and FDD ready lines (CT1/CT2).
 // opm_write keeps what the system has set and takes only the LFO waveform from the game.
 opm_write(unsigned(r),unsigned(v));
}
// ---- Sega PCM on the X68000's ADPCM (r031c) ----
// The Sega PCM chip plays 16 channels of 8-bit samples from ROM (drums, voices, the engine and
// traffic noise). The X68000 has one ADPCM channel (MSM6258, 4-bit, 15625 Hz). The 16 channels
// are mixed here at 15625 Hz (half the chip's own 31250 Hz: every channel steps two samples at a
// time), in mono, and coded as ADPCM into a ring of memory that the DMA plays without end
// (IOCS _ADPCMLOT with a link array whose last block points back to the first). Each vertical
// blank looks where the DMA is reading and codes enough samples to stay a fixed distance ahead.
// mixed sum -> the ADPCM's 12 bits. 4: the PCM part then comes out as loud as the FM part, as it is
// with Cannonball's own chip emulators for the same sound commands (XEiJ recordings: FM 1897, PCM
// 1074 with a shift of 5; the emulators give 2066 and 2067).
enum {PCM_SHIFT=4};
static uint8_t*pcm_rom=0,*pcm_low=0,*pcm_regs=0;static uint32_t pcm_mask=0;   // the chip's ROM, per-channel address fraction, registers
SegaPCM::SegaPCM(uint32_t,AssetLoader*asset_,uint8_t*ram_,int32_t){ram=ram_;low=new uint8_t[16];memset(low,0,16);pcm_asset=asset_?asset_->asset:0;max_addr=asset_?int32_t(asset_->length):0;bankshift=12;bankmask=0x70;rgnmask=max_addr-1;downsample=0;
 for(int i=0;i<0x100;i++)ram[i]=0xff;
 pcm_rom=pcm_asset;pcm_low=low;pcm_regs=ram;pcm_mask=uint32_t(rgnmask);}
SegaPCM::~SegaPCM(){}
void SegaPCM::init(int32_t,int32_t){}
void SegaPCM::stream_update(){}
enum {PCM_BLOCKS=4,PCM_BLOCK_BYTES=512,PCM_RING=PCM_BLOCKS*PCM_BLOCK_BYTES,PCM_LEAD=448,PCM_MAX_FILL=448};
struct PcmLink {void*addr;uint16_t len;const PcmLink*next;} __attribute__((__packed__,__aligned__(2)));
static uint8_t pcm_ring_here[PCM_RING];static PcmLink pcm_links_here[PCM_BLOCKS];
// r031n: the DMA controller reads the link array and the ring itself, and it cannot reach the
// 060turbo's local memory. When the program runs there, both are taken from main memory.
static uint8_t*pcm_ring=pcm_ring_here;static PcmLink*pcm_links=pcm_links_here;
extern "C"{uint8_t audio_pcm_place=0;}   // 0 = in the program, 1 = taken from main memory, 2 = no reachable memory (PCM off)
static bool pcm_place(){
 if(x68k_dma_reachable(pcm_ring,PCM_RING)&&x68k_dma_reachable(pcm_links,sizeof pcm_links_here))return true;
 const long before=x68k_sys_stat(0x5003,0);                // DOS _MALLOC from main memory only
 void*const got=_dos_malloc(PCM_RING+sizeof pcm_links_here);
 x68k_sys_stat(0x5003,before);
 if(uint32_t(uintptr_t(got))>=0x81000000UL){audio_pcm_place=2;return false;}
 if(!x68k_dma_reachable(got,PCM_RING+sizeof pcm_links_here)){_dos_mfree(got);audio_pcm_place=2;return false;}
 pcm_ring=static_cast<uint8_t*>(got);pcm_links=reinterpret_cast<PcmLink*>(pcm_ring+PCM_RING);audio_pcm_place=1;
 return true;
}
static unsigned pcm_write=0;static bool pcm_running=false;
static int ad_value=0,ad_index=0;
static const int16_t AD_STEP[49]={16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552};
static const int8_t AD_ADJ[8]={-1,-1,-1,-1,2,4,6,8};
static inline unsigned adpcm_encode(int sample){
 int step=AD_STEP[ad_index],diff=sample-ad_value;unsigned nib=0;
 if(diff<0){nib=8;diff=-diff;}
 int delta=step>>3;
 if(diff>=step){nib|=4;diff-=step;delta+=step;}
 if(diff>=(step>>1)){nib|=2;diff-=step>>1;delta+=step>>1;}
 if(diff>=(step>>2)){nib|=1;delta+=step>>2;}
 ad_value+=(nib&8)?-delta:delta;
 if(ad_value>2047)ad_value=2047;else if(ad_value<-2048)ad_value=-2048;
 ad_index+=AD_ADJ[nib&7];if(ad_index<0)ad_index=0;else if(ad_index>48)ad_index=48;
 return nib;
}
// what the chip's decoder does with one nibble (the coder above keeps the same state)
static inline void adpcm_follow(unsigned nib){
 const int step=AD_STEP[ad_index];int delta=step>>3;
 if(nib&4)delta+=step;if(nib&2)delta+=step>>1;if(nib&1)delta+=step>>2;
 ad_value+=(nib&8)?-delta:delta;
 if(ad_value>2047)ad_value=2047;else if(ad_value<-2048)ad_value=-2048;
 ad_index+=AD_ADJ[nib&7];if(ad_index<0)ad_index=0;else if(ad_index>48)ad_index=48;
}
static uint32_t pcm_written=0,pcm_played=0;static unsigned pcm_last_at=0;   // bytes coded / bytes the DMA has read
static int32_t pcm_mix[PCM_MAX_FILL*2];
// n samples of the 16 channels into pcm_mix (the loop of SegaPCM::stream_update, two chip samples a step)
static void pcm_mix_channels(uint8_t*ram,const uint8_t*rom,uint32_t rgnmask,uint8_t*low,unsigned n){
 memset(pcm_mix,0,n*sizeof pcm_mix[0]);
 for(unsigned ch=0;ch<16;ch++){uint8_t*regs=ram+8*ch;
  if(regs[0x86]&1)continue;
  const uint8_t*bank=rom+(uint32_t(regs[0x86]&0x70)<<12);
  uint32_t addr=(uint32_t(regs[0x85])<<16)|(uint32_t(regs[0x84])<<8)|low[ch];
  const uint32_t loop=(uint32_t(regs[0x05])<<16)|(uint32_t(regs[0x04])<<8);
  const uint8_t end=uint8_t(regs[0x06]+1);
  const int vol=int(regs[2])+int(regs[3]);const uint32_t step=uint32_t(regs[7])*2;
  for(unsigned i=0;i<n;i++){
   if(uint8_t(addr>>16)==end){
    if(!(regs[0x86]&2))addr=loop;else{regs[0x86]|=1;break;}}
   pcm_mix[i]+=(int(bank[(addr>>8)&rgnmask])-0x80)*vol;
   addr=(addr+step)&0xffffff;}
  regs[0x84]=uint8_t(addr>>8);regs[0x85]=uint8_t(addr>>16);low[ch]=(regs[0x86]&1)?0:uint8_t(addr);}
}
static void pcm_start(){
 if(!pcm_place()){audio_pcm_enabled=0;return;}
 // silence, coded (the coder state runs on from here)
 ad_value=0;ad_index=0;
 for(unsigned k=0;k<PCM_RING;k++){unsigned lo=adpcm_encode(0),hi=adpcm_encode(0);pcm_ring[k]=uint8_t(lo|(hi<<4));}
 for(unsigned b=0;b<PCM_BLOCKS;b++){pcm_links[b].addr=pcm_ring+b*PCM_BLOCK_BYTES;pcm_links[b].len=PCM_BLOCK_BYTES;pcm_links[b].next=&pcm_links[(b+1)%PCM_BLOCKS];}
 pcm_write=PCM_LEAD;pcm_written=PCM_LEAD;pcm_played=0;pcm_last_at=0;
 // IOCS _ADPCMLOT: d1 = sampling 15.6 kHz (4) << 8 | output left and right (3), a1 = link array
 {register long d0 __asm__("d0")=0x64;register long d1 __asm__("d1")=0x0403;register void*a1 __asm__("a1")=pcm_links;
  __asm__ volatile("trap #15":"+d"(d0),"+d"(d1),"+a"(a1)::"memory");}
 pcm_running=true;
}
static void pcm_stop(){
 if(!pcm_running)return;
 // IOCS _ADPCMMOD: d1 = 0 ends the output
 {register long d0 __asm__("d0")=0x67;register long d1 __asm__("d1")=0;
  __asm__ volatile("trap #15":"+d"(d0),"+d"(d1)::"memory");}
 pcm_running=false;
}
// each vertical blank, after the sound code: code samples up to PCM_LEAD bytes ahead of the DMA
static void pcm_fill(){
 if(!pcm_running)return;
 const uint32_t mar=*(volatile uint32_t*)0xe840ccUL;                    // DMAC channel 3 memory address
 const uint32_t at=mar-uint32_t(uintptr_t(pcm_ring));
 if(at>=PCM_RING)return;                                                  // read while the DMA moved to the next block
 // bytes the DMA has read since the last look (it moves 141 a blank; a look later than a whole
 // ring, 262 ms, would be miscounted)
 pcm_played+=(unsigned(at)+PCM_RING-pcm_last_at)%PCM_RING;pcm_last_at=unsigned(at);
 int32_t lead=int32_t(pcm_written-pcm_played);
 if(lead<16){
  // The DMA has reached (or passed) the last coded byte and is playing old bytes of the ring.
  // The chip decodes them, so the coder's state is taken through the same bytes, up to where
  // coding starts again 64 bytes ahead of the DMA.
  uint32_t stale=uint32_t(64-lead);if(stale>2*PCM_RING)stale=2*PCM_RING;
  for(uint32_t k=0;k<stale;k++){const unsigned v=pcm_ring[pcm_write];adpcm_follow(v&15);adpcm_follow(v>>4);pcm_write=(pcm_write+1)%PCM_RING;}
  pcm_write=(unsigned(at)+64)%PCM_RING;pcm_written=pcm_played+64;lead=64;++audio_pcm_resyncs;}
 if(lead>=PCM_LEAD)return;
 unsigned bytes=unsigned(PCM_LEAD-lead);if(bytes>PCM_MAX_FILL)bytes=PCM_MAX_FILL;
 pcm_mix_channels(pcm_regs,pcm_rom,pcm_mask,pcm_low,bytes*2);
 const int32_t*m=pcm_mix;
 for(unsigned k=0;k<bytes;k++){
  int a=int(m[0]>>PCM_SHIFT),b=int(m[1]>>PCM_SHIFT);m+=2;
  if(a>2047)a=2047;else if(a<-2048)a=-2048;if(b>2047)b=2047;else if(b<-2048)b=-2048;
  const unsigned lo=adpcm_encode(a),hi=adpcm_encode(b);
  pcm_ring[pcm_write]=uint8_t(lo|(hi<<4));pcm_write=(pcm_write+1)%PCM_RING;}
 audio_pcm_bytes+=bytes;pcm_written+=bytes;
}
// ---- base class (no sample buffer on the X68000) ----
SoundChip::SoundChip(){volume=1.0f;initalized=false;buffer=0;buffer_size=0;frame_size=0;}
SoundChip::~SoundChip(){}
void SoundChip::init(uint8_t,int32_t,int32_t){}
void SoundChip::set_volume(uint8_t){}
void SoundChip::clear_buffer(){}
void SoundChip::write_buffer(const uint8_t,uint32_t,int16_t){}
int16_t SoundChip::read_buffer(const uint8_t,uint32_t){return 0;}
int16_t* SoundChip::get_buffer(){return 0;}

// Called once in supervisor mode before the interrupts are installed: the game was initialised in
// user mode, where the chip cannot be written, so the sound code is initialised again here.
// with_game = 0 (the display test without ROMs): no sound, the sound program's ROM is not loaded.
void audio_start(int with_game){
 audio_super=1;
 if(!with_game)audio_enabled=0;
 if(!audio_enabled)return;
 osoundint.init();
 if(audio_pcm_enabled&&pcm_rom&&pcm_regs&&pcm_mask>0xffff)pcm_start();
}
// all 8 FM channels: key off, fastest release
void audio_silence(){
 if(!audio_enabled||!audio_super)return;
 pcm_stop();
 for(unsigned c=0;c<8;c++)opm_write(0x08,c);
 for(unsigned r=0xe0;r<0x100;r++)opm_write(r,0x0f);
}
// From the vertical blank interrupt (mask 5). 125 runs of the sound code a second.
void audio_vblank(){
 static uint16_t acc=0;
 if(audio_hold||!audio_enabled)return;
 acc=uint16_t(acc+audio_ticks_per_blank);
 while(acc>=1000){acc=uint16_t(acc-1000);
  osoundint.play_queued_sound();
#ifdef CBX68_AUDIO_LOG
  audio_log_put(0x80000000u|osound.command_input);
  audio_log_put((uint32_t(osound.engine_data[0])<<24)|(uint32_t(osound.engine_data[1])<<16)|(uint32_t(osound.engine_data[2])<<8)|osound.engine_data[3]);
  audio_log_put((uint32_t(osound.engine_data[4])<<24)|(uint32_t(osound.engine_data[5])<<16)|(uint32_t(osound.engine_data[6])<<8)|osound.engine_data[7]);
#endif
  osound.tick();
  ++audio_ticks_done;}
 pcm_fill();
}
