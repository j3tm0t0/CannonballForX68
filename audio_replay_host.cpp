/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
// PC check of the X68000 sound (r031): AUDIO.BIN from an X68000 build with -DCBX68_AUDIO_LOG holds
// the command and engine bytes of every run of the sound code and the FM register writes it made.
// Here the same commands go through the same sound code (engine/audio/osound.cpp) compiled for
// the PC, and the register writes must come out the same, in the same order.
#include "stdint.hpp"
#include "engine/audio/osound.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
OSound osound;
static std::vector<uint32_t> replay_writes;
YM2151::YM2151(float,uint32_t){irq=false;}
YM2151::~YM2151(){}
void YM2151::init(int,int){}
void YM2151::stream_update(){}
int YM2151::read_status(){return 1;}   // as audio_x68k.cpp
void YM2151::write_reg(int r,int v){replay_writes.push_back((uint32_t(r&255)<<8)|uint32_t(v&255));}
SoundChip::SoundChip(){volume=1.0f;initalized=false;}
SoundChip::~SoundChip(){}
void SoundChip::init(uint8_t,int32_t,int32_t){}
void SoundChip::set_volume(uint8_t){}
int16_t* SoundChip::get_buffer(){return 0;}
// returns 0 when every write matches
int audio_replay(const char*file){
 FILE*f=fopen(file,"rb");if(!f){printf("AUDIO REPLAY: cannot open %s\n",file);return 2;}
 std::vector<uint32_t> log;uint8_t b[4];while(fread(b,1,4,f)==4)log.push_back((uint32_t(b[0])<<24)|(uint32_t(b[1])<<16)|(uint32_t(b[2])<<8)|b[3]);fclose(f);
 static uint8_t pcm_ram[0x100];YM2151 ym(0.5f,4000000);
 osound.init(&ym,pcm_ram);replay_writes.clear();   // the X68000 log starts after its init
 unsigned long ticks=0,writes=0;size_t i=0;
 while(i<log.size()){
  if(!(log[i]&0x80000000u)){printf("AUDIO REPLAY: log out of step at %lu\n",(unsigned long)i);return 3;}
  if(i+2>=log.size())break;
  osound.command_input=uint8_t(log[i]);
  for(unsigned k=0;k<4;k++){osound.engine_data[k]=uint8_t(log[i+1]>>(24-8*k));osound.engine_data[4+k]=uint8_t(log[i+2]>>(24-8*k));}
  i+=3;replay_writes.clear();osound.tick();++ticks;
  size_t n=0;while(i+n<log.size()&&!(log[i+n]&0x80000000u))++n;
  if(i+n>=log.size()&&log.size()>=300000)break;   // the log was full: the last run is cut off
  bool same=n==replay_writes.size();for(size_t k=0;same&&k<n;k++)same=log[i+k]==replay_writes[k];
  if(!same){printf("AUDIO REPLAY FAIL at run %lu: X68000 %lu writes, PC %lu\n",ticks,(unsigned long)n,(unsigned long)replay_writes.size());
   for(size_t k=0;k<n||k<replay_writes.size();k++)printf("  %04lx %04lx\n",k<n?(unsigned long)log[i+k]:0xffffUL,k<replay_writes.size()?(unsigned long)replay_writes[k]:0xffffUL);
   return 1;}
  writes+=n;i+=n;}
 printf("AUDIO REPLAY OK runs=%lu writes=%lu\n",ticks,writes);return 0;
}
