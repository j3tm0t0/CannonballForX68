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

extern OSound osound;
extern "C" {
 volatile uint8_t audio_hold=1;          // 1 = the sound code must not run (start-up, OSoundInt::init)
 volatile uint8_t audio_super=0;         // 1 = supervisor mode, the chip may be written (set by audio_start)
 void audio_start(int with_game);
 volatile uint32_t audio_ticks_done=0,audio_fm_writes=0,audio_fm_timeouts=0;
 uint8_t audio_enabled=1;                // NOSOUND argument: 0
 uint16_t audio_ticks_per_blank=2254;    // 125 Hz / 55.458 Hz, in 1/1000 (HZ60: 2083)
 void audio_vblank();
 void audio_silence();
}
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
 // timers: the chip must not raise its interrupt on the X68000 (the sound code is paced by the
 // vertical blank), so the timer control register is left alone
 if(r==0x14)return;
 // register 0x1B: bits 7 and 6 are the X68000's ADPCM clock select and FDD ready lines (CT1/CT2).
 // opm_write keeps what the system has set and takes only the LFO waveform from the game.
 opm_write(unsigned(r),unsigned(v));
}
// ---- Sega PCM: not there ----
SegaPCM::SegaPCM(uint32_t,AssetLoader*,uint8_t*ram_,int32_t){ram=ram_;low=0;pcm_asset=0;max_addr=0;bankshift=0;bankmask=0;rgnmask=0;downsample=0;}
SegaPCM::~SegaPCM(){}
void SegaPCM::init(int32_t,int32_t){}
void SegaPCM::stream_update(){}
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
}
// all 8 FM channels: key off, fastest release
void audio_silence(){
 if(!audio_enabled||!audio_super)return;
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
  osound.tick();
  ++audio_ticks_done;}
}
