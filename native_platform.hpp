/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
// BG/raster road display: the CPU publishes scroll/palette entries, not full frames.
#include <x68k/iocs.h>
#include <stdint.h>
#include <cstdio>
#include "report_pager.hpp"
#include "road_bridge.hpp"
#include "visuals.hpp"
#include "x68k_sysstat.hpp"
#include <x68k/dos.h>
#include <cstring>
#include <cstdlib>
extern "C"{extern uint8_t audio_pcm_place;}
#define HW16(a) (*(volatile uint16_t*)(a))
#define HW8(a) (*(volatile uint8_t*)(a))
extern "C" {
void road_vblank_irq();void road_raster_irq();
RoadRow* volatile road_active=0;
RoadRow* volatile road_pending=0;
RoadRow* volatile road_scan=0;
// r031f: where the raster interrupt reads RoadFrame::row_changed (set with road_scan at the vertical blank)
extern "C"{uint16_t* volatile road_mask_scan=0;uint32_t road_mask_offset=uint32_t(offsetof(RoadFrame,row_changed));}
volatile uint32_t road_vblanks=0,road_shown=0,road_irq_count=0;
extern "C"{volatile uint8_t road_vblank_busy=0;volatile uint32_t road_vblank_overruns=0;}
static uint32_t io_probe_ns[5],io_probe_n[5],pcg_long_ns=0;
static int native_light_mode=-1;   // -1 by the measured bus, 0 FULL, 1 LIGHT
extern "C"{extern volatile uint8_t audio_hold;extern volatile uint32_t audio_ticks_done,audio_fm_writes,audio_fm_timeouts,audio_pcm_bytes,audio_pcm_resyncs;extern uint8_t audio_pcm_enabled,audio_fm_enabled;extern uint8_t audio_enabled;extern uint16_t audio_ticks_per_blank;void audio_silence();void audio_start(int with_game);}
#ifdef CBX68_AUDIO_LOG
extern "C"{extern uint32_t audio_log[];extern volatile uint32_t audio_log_n;}
#endif
// MPU caches (main.cpp calls native_cache_on at the start, platform_exit puts the state back).
// Asked through IOCS _SYS_STAT (d1=1 reads the state: bit 0 instruction, bit 1 data; d1=4 sets it
// from d2; d1=3 is not the state but "push and invalidate"), which the X68030 ROM and the 68060 accelerator's driver provide. Without them the call
// fails and nothing is touched: turning the caches on in the CACR by hand would also cache the I/O
// area when nobody has set the MMU or the transparent translation registers up for it.
// r031n: on a 060turbo the program runs faster in the board's local memory than in the machine's
// main memory (a cache miss is filled about 3 times faster there). With 060turbo.sys loaded and
// the program in main memory, it ends itself and is run again in local memory (IOCS _SYS_STAT
// $5001; main memory is used again when local memory is short). The word MAIN is added to the
// arguments of the second run so that it is not tried twice; MAIN typed by hand keeps the program
// in main memory. Must be called before anything is set up: the first run ends inside the call.
static void native_run_in_local_memory(){
 if(!x68k_has_060turbo()||x68k_sys_stat(0x4000)<=0)return;
 // Only in 060turbo.sys's extended mode (its -xm option): there Human68k's memory reaches into
 // local memory. In the compatible mode a program cannot be loaded there.
 if(!x68k_extended_mode())return;
 if(x68k_in_local_memory(reinterpret_cast<const void*>(&native_run_in_local_memory)))return;
 const struct dos_psp*const psp=_dos_getpdb();
 const unsigned char*const line=reinterpret_cast<const unsigned char*>(psp->comline);
 const unsigned len=line[0];if(len>240)return;
 static char path[96];static unsigned char again[8+1+255+1];
 unsigned n=0;for(unsigned k=0;k<68&&psp->exe_path[k];k++)path[n++]=psp->exe_path[k];
 for(unsigned k=0;k<24&&psp->exe_name[k];k++)path[n++]=psp->exe_name[k];
 path[n]=0;
 memcpy(again,line-8,8);                                  // (the 8 bytes before the line say how it is coded)
 again[8]=uint8_t(len+5);memcpy(again+9,line+1,len);memcpy(again+9+len," MAIN",5);again[9+len+5]=0;
 x68k_sys_stat(0x5001,1,path,again+8,0);                  // does not come back when it works
}
static int native_cache_before=-1;static bool native_cache_set=false,native_cache_was_set=false;
static long native_sys_stat(long mode,long value){
 register long d0 __asm__("d0")=0xac;register long d1 __asm__("d1")=mode;register long d2 __asm__("d2")=value;
 __asm__ volatile("trap #15":"+d"(d0),"+d"(d1),"+d"(d2)::"memory");
 return d0;
}
static void native_cache_on(){
 native_cache_before=int(native_sys_stat(1,0));
 if(native_cache_before>=0&&(native_cache_before&3)!=3){native_sys_stat(4,3);native_cache_set=native_cache_was_set=true;}
}
static void native_cache_restore(){
 if(!native_cache_set)return;
 native_sys_stat(4,native_cache_before&3);native_cache_set=false;
}
static int native_audio_game=1;   // main.cpp: 0 for the display test (no ROMs, no sound program)
#ifdef CBX68_PROF
// sampling profiler (road_irq.S): hits per 16 bytes of text, last bucket = outside the program
extern "C"{extern char _start[],_etext[];uint32_t prof_text_size=0;uint32_t prof_hist[(0x140000>>4)+1];}
#endif
}
static RoadFrame road_frames[3];   // r029: one shown, one waiting for the next vertical blank, one being built
// r029: frame pacing (Masa compared the three on XM6 and chose VSYNC, 2026-11-08):
//  VSYNC (default): one picture every 2 screen refreshes (55.46 Hz): even, 27.7 pictures a
//                   second; the game runs about 8% slower than real time.
//  CLOCK (argument): 30 pictures a second by the 1/100 s clock; now and then a picture is shown
//                   for one refresh only (a hitch every few seconds).
//  HZ60 (argument): 60 Hz screen (vertical total 525 lines instead of 568), 2 refreshes a picture.
static bool native_vsync_pace=true,native_hz60=false;
static uint32_t native_idle_waits=0;   // r028: number of STOP waits (end report)
static int native_old_mode,native_ssp;
static uint32_t native_started,native_frames;
static uint32_t native_phase_cs[4],native_mode_cs[2][4];
static uint32_t native_visual_cs[4][4],native_visual_draws[4],native_visual_steps[4];
static unsigned native_measuring_visual=0;
static void native_visual_begin(unsigned mode){native_measuring_visual=mode&3;++native_visual_steps[native_measuring_visual];}
static void native_visual_end(){}

static uint32_t saved_vblank,saved_raster;
static uint16_t saved_text_palette[16];
static bool native_irq_failed=false;
static uint8_t saved_iera,saved_imra,saved_aer,saved_tacr,saved_tadr;
static unsigned short mask_irqs(){unsigned short s;__asm__ volatile("move.w %%sr,%0\n\tmove.w #0x2700,%%sr":"=d"(s)::"memory","cc");return s;}
static void restore_irqs(unsigned short s){__asm__ volatile("move.w %0,%%sr"::"d"(s):"memory","cc");}
static uint32_t native_clock(){return (uint32_t)_iocs_ontime().sec;}
// r019 measurement: the 1/100 s clock plus the MFP Timer C counter between ticks.
// Units are 1/20000 s (50 us), assuming Timer C reloads every 200 counts (checked below).
static uint32_t fine_last=0,fine_backsteps=0,fine_retries=0;static unsigned fine_tmin=255,fine_tmax=0;
uint32_t platform_fine_clock(){
 for(unsigned tries=0;;tries++){
  uint32_t a=native_clock();unsigned t=HW8(0xe88023);uint32_t b=native_clock();
  if(a!=b&&tries<4){++fine_retries;continue;}
  if(t<fine_tmin)fine_tmin=t;
  if(t>fine_tmax)fine_tmax=t;
  unsigned within=t>=1&&t<=200?200-t:0;
  uint32_t v=b*200UL+within;
  if(v<fine_last){++fine_backsteps;v=fine_last;}
  fine_last=v;return v;
 }
}
static uint32_t fine_phase[4][4],fine_phase_max[4][4],fine_work_max[4],fine_overruns[4],fine_late[4];
static uint32_t fine_draw_work=0;
static uint32_t fine_start=0,fine_phase_start=0;
static void native_fine_record(unsigned phase){
 uint32_t n=platform_fine_clock(),d=n-fine_phase_start;fine_phase_start=n;unsigned m=native_measuring_visual;
 fine_phase[m][phase]+=d;if(d>fine_phase_max[m][phase])fine_phase_max[m][phase]=d;
 if(phase<3)fine_draw_work+=d;
}
static void native_phase_record(unsigned phase,uint32_t start){native_fine_record(phase);uint32_t n=native_clock();uint32_t elapsed=n>=start?n-start:8640000UL-start+n;native_phase_cs[phase]+=elapsed;native_visual_cs[native_measuring_visual][phase]+=elapsed;native_mode_cs[render_schedule.mode()][phase]+=elapsed;}
static uint32_t native_elapsed(){uint32_t n=native_clock();return n>=native_started?n-native_started:8640000UL-native_started+n;}
static void platform_present(uint16_t*,uint32_t*){}
static bool native_key(int sc){return (_iocs_bitsns(sc>>3)&(1<<(sc&7)))!=0;}
// r029: the frame to build into is the one that is neither shown nor waiting to be shown, so
// building starts at once. Only the video memory writes (road page rows, text plane) wait, in
// platform_road_handoff(), until the waiting frame has been taken (its row set is then free).
static RoadFrame& platform_road_back(){
 if(native_elapsed()>200&&!road_irq_count)native_irq_failed=true;
 unsigned short sr=mask_irqs();RoadRow*a=road_active,*w=road_pending;restore_irqs(sr);
 for(unsigned k=0;k<3;k++)if(road_frames[k].rows!=a&&road_frames[k].rows!=w)return road_frames[k];
 return road_frames[0];   // not reached: three frames, at most two in use
}
static uint32_t native_handoff_fine=0,native_handoff_waits=0,native_handoff_calls=0;
static void platform_road_handoff(){
 uint32_t start=native_clock(),t0=platform_fine_clock();++native_handoff_calls;
 while(road_pending){if(native_clock()-start>200){native_irq_failed=true;road_pending=0;break;}native_idle_waits++;native_handoff_waits++;__asm__ volatile("stop #0x2000":::"memory");}
 native_handoff_fine+=platform_fine_clock()-t0;
}
static void platform_road_submit(RoadFrame&f){__asm__ volatile("":::"memory");road_pending=f.rows;++native_frames;++native_visual_draws[native_measuring_visual];}
static void platform_init(){
 native_old_mode=_iocs_crtmod(-1);native_ssp=_iocs_b_super(0);for(unsigned i=0;i<16;i++)saved_text_palette[i]=HW16(0xe82200UL+i*2);_iocs_crtmod(10);_iocs_b_curoff();_iocs_sp_init();_iocs_sp_off();
 HW16(0xe82600)=0;HW16(0xe80028)=(HW16(0xe80028)&0x00ff)|0x0300;HW16(0xeb0808)=0;HW16(0xe8002a)=0;
 for(unsigned i=0;i<512;i++)HW16(0xeb0000UL+i*2)=0;
 for(unsigned i=0;i<unsigned(road_atlas.tiles)*16;i++)HW16(0xeb8000UL+i*2)=(unsigned(road_atlas.pcg[i*2])<<8)|road_atlas.pcg[i*2+1];
 for(unsigned i=0;i<4096;i++){HW16(0xebc000UL+i*2)=road_atlas.map0[i];HW16(0xebe000UL+i*2)=road_atlas.map1[i];}
 // r023: graphic page 1 starts as off-road; road_stream() fills the rows each frame uses.
 for(unsigned i=0;i<512UL*512;i++)HW16(0xc00000UL+i*2)=0xf3<<8;
 visuals_native_init();
 HW16(0xe80002)=2;HW16(0xe80004)=4;HW16(0xe80006)=44;
 HW16(0xeb080c)=8;HW16(0xe8000c)=72;HW16(0xe8000e)=520;HW16(0xeb080e)=72;
 if(native_hz60)HW16(0xe80008)=524;   // r029 HZ60: R04 vertical total 525 lines: 31.5 kHz / 525 = 60 Hz (display lines 72..520 unchanged)
 HW16(0xe80012)=1023;HW16(0xe82400)=1;HW16(0xe82500)=0x06e4;HW16(0xe82600)=0x004f;
 for(unsigned a=0xe80014;a<=0xe80026;a+=2)HW16(a)=0;
 road_build(hwroad,display_palette,road_frames[0]);road_stream(road_frames[0]);road_rows_changed(road_frames[0]);road_active=road_frames[0].rows;
 audio_start(native_audio_game);   // r031: FM chip (needs supervisor mode); not in the display test
 unsigned short sr=mask_irqs();
 saved_vblank=*(volatile uint32_t*)0x134;saved_raster=*(volatile uint32_t*)0x138;
 saved_iera=HW8(0xe88007);saved_imra=HW8(0xe88013);saved_aer=HW8(0xe88003);saved_tacr=HW8(0xe88019);saved_tadr=HW8(0xe8801f);

#ifdef CBX68_PROF
 prof_text_size=uint32_t(_etext-_start);if(prof_text_size>0x140000)prof_text_size=0x140000;
#endif
 *(volatile uint32_t*)0x134=(uint32_t)road_vblank_irq;*(volatile uint32_t*)0x138=(uint32_t)road_raster_irq;
 HW8(0xe88003)=saved_aer&0xef;HW8(0xe88007)=saved_iera|0x60;HW8(0xe88013)=saved_imra|0x60;
 HW8(0xe8800b)&=0x9f;HW8(0xe8800f)&=0x9f;
 HW8(0xe88019)=0;HW8(0xe8801f)=1;HW8(0xe88019)=8;
 HW16(0xeb0808)=0x0219;
 // Line budget of the scenery jobs from the time a long written to the PCG takes with the sprite
 // display on (MFP timer C, 50 us a step; pattern 127 is in the scenery pool and is loaded
 // before it is shown). 60 us of longs a line period (63.5 us): the rest is for the line's own
 // registers and the other interrupts. XEiJ 060turbo: 50 without wait cycles, 31 with them.
 {volatile uint32_t*q=(volatile uint32_t*)(0xeb8000UL+127*128);
  unsigned t0=HW8(0xe88023),t=t0;uint32_t guard=0;while(t==t0&&++guard<200000UL)t=HW8(0xe88023);t0=t;
  uint32_t n=0;unsigned steps=0;
  while(steps<40&&n<400000UL){for(unsigned j=0;j<32;j++)q[j]=0;n+=32;t=HW8(0xe88023);steps=(t0+200-t)%200;}
  pcg_long_ns=steps?steps*50000UL/n:0;
  if(pcg_long_ns)visuals_set_line_budget(unsigned(60000UL/pcg_long_ns));
  // r031l: light mode on a slow bus (XEiJ 060turbo: 1.0 us a long without wait cycles, 1.9 us with
  // them, as on the real board). Arguments LIGHT / FULL choose it by hand.
  visuals_set_light(native_light_mode<0?pcg_long_ns>=1400:native_light_mode>0);}
 restore_irqs(sr);
 native_started=native_clock();native_frames=0;
 fine_start=fine_phase_start=platform_fine_clock();
}
// Picture skipping: the game does one step a screen refresh (VSYNC pace). When drawing has put
// it 2 or more refreshes behind, the next picture is left out (the step itself still runs), at
// most 27 in a row so the screen still changes once a second. Far behind (a long pause), the count starts again.
#ifndef CBX68_SKIP_RENDER_PREP
#define CBX68_SKIP_RENDER_PREP 1
#endif
static const bool SKIP_RENDER_PREP=CBX68_SKIP_RENDER_PREP;   // r031m: a step whose picture is skipped leaves out what only the picture needs
static int native_skip_mode=-1,native_lag=0;static uint32_t native_skipped_draws=0;
// (the schedule: frame f0 belongs at screen refresh v0, and every frame after it one refresh later)
static uint32_t native_skip_v0=0;static int native_skip_f0=0;static bool native_skip_started=false;
static bool platform_draw_due(int frame){
 uint32_t&v0=native_skip_v0;int&f0=native_skip_f0;bool&started=native_skip_started;static unsigned row=0;
 const uint32_t v=road_vblanks;
 if(!started){v0=v;f0=frame;started=true;}
 const int lag=int(v-v0)-(frame-f0);native_lag=lag;
 if(lag>60||lag<-60){v0=v;f0=frame;row=0;native_lag=0;return true;}
 if(lag<0){v0=v;f0=frame;native_lag=0;}                    // ahead (platform_sync waits): no credit is saved
 if(lag>=2&&row<27){++row;++native_skipped_draws;return false;}   // (27: at least one picture a second)
 row=0;return true;
}
static void platform_sync(int frame){
 // Use one display period as the deadline; never repay old missed periods.
 static uint32_t deadline=0;static bool previous30=true;
 bool mode30=render_schedule.draw30;
 if(mode30&&!(frame&1))return;
 if(native_vsync_pace){   // r029: count screen refreshes instead of the clock
  static uint32_t vdeadline=0;static bool vprevious30=true;static bool vstarted=false;
  const uint32_t v=road_vblanks;
  // r031o: with picture skipping, wait only until the refresh this frame ends at on the schedule
  // of platform_draw_due. (Before: after a step that was behind, the next wait was 2 whole
  // refreshes counted from the end of the step, so the time the step itself took was lost again.)
  if(native_skip_mode>0&&native_skip_started){
   // (platform_draw_due is asked about frame+2 next, the picture of the next game step)
   const uint32_t until=native_skip_v0+uint32_t(frame+2-native_skip_f0);
   while(int32_t(road_vblanks-until)<0){native_idle_waits++;__asm__ volatile("stop #0x2000":::"memory");}
   vstarted=false;return;
  }
  if(!vstarted||vprevious30!=mode30){vdeadline=v;vprevious30=mode30;vstarted=true;}
  vdeadline+=mode30?2:1;
  {unsigned m=native_measuring_visual;if(fine_draw_work>fine_work_max[m])fine_work_max[m]=fine_draw_work;fine_draw_work=0;
   if(v>=vdeadline){++fine_overruns[m];fine_late[m]+=(v-vdeadline+1)*(native_hz60?100u:108u);}}
  if(v>=vdeadline){vdeadline=v;return;}
  while(road_vblanks<vdeadline){native_idle_waits++;__asm__ volatile("stop #0x2000":::"memory");}
  return;
 }
 uint32_t now=native_elapsed()*60UL;
 if(previous30!=mode30){deadline=now;previous30=mode30;}
 deadline+=mode30?200:100;
 {unsigned m=native_measuring_visual;if(fine_draw_work>fine_work_max[m])fine_work_max[m]=fine_draw_work;fine_draw_work=0;
  if(now>deadline){++fine_overruns[m];fine_late[m]+=now-deadline;}}
 if(now>deadline){deadline=now;return;}
 // r028: wait with STOP (the CPU sleeps until the next interrupt: raster line, vertical blank or the
 // 1/100 s clock that native_elapsed() reads), instead of a busy loop. Saves host CPU in emulators.
 while(native_elapsed()*60UL<deadline){native_idle_waits++;__asm__ volatile("stop #0x2000":::"memory");}
}
static unsigned road_map_errors=0,road_pattern_errors=0,road_ground_errors=0;
static void native_road_memory_check(){
 unsigned maps=0,patterns=0,ground=0;
 for(unsigned row=0;row<64;row++)if(row<50||row==60)for(unsigned x=0;x<64;x++){
  unsigned i=row*64+x;
  if(HW16(0xebc000UL+i*2)!=road_atlas.map0[i])++maps;
  if(HW16(0xebe000UL+i*2)!=road_atlas.map1[i])++maps;
 }
 for(unsigned i=0;i<unsigned(road_atlas.tiles)*16;i++)if(HW16(0xeb8000UL+i*2)!=((unsigned(road_atlas.pcg[i*2])<<8)|road_atlas.pcg[i*2+1]))++patterns;
 for(unsigned y=0;y<512;y+=17)for(unsigned x=0;x<512;x+=19){
  unsigned i=y*512+x;
  {unsigned v=HW16(0xc00000UL+i*2)>>8;if(v<0xf0||v>0xf7||(v>0xf3&&v<0xf7))++ground;}
 }
 road_map_errors=maps;road_pattern_errors=patterns;road_ground_errors=ground;
}
static void native_road_memory_report(){ report_printf("ROAD MEMORY map_errors=%u pattern_errors=%u ground_errors=%u\n",road_map_errors,road_pattern_errors,road_ground_errors);}
static void platform_exit(){
 uint32_t t=native_elapsed();unsigned short sr=mask_irqs();
 audio_hold=1;audio_silence();   // r031
 // Bus speed probe: ns per 32-bit write to RAM, PCG, BG scroll, text palette and CRTC R09.
 // Run before the sprite display is turned off: XEiJ (and the real sprite controller) make the
 // CPU wait longer for the PCG while the display is on. Interrupts are off here, so time is read straight from the MFP timer C counter (50 us a step).
 {static uint32_t probe_ram;static const uint32_t probe_at[5]={0,0xeb8000UL,0xeb0800UL,0xe82200UL,0xe80012UL};
  for(unsigned k=0;k<5;k++){volatile uint32_t*q=k?(volatile uint32_t*)probe_at[k]:(volatile uint32_t*)&probe_ram;const uint32_t v=*q;
   unsigned t0=HW8(0xe88023),t=t0;while(t==t0)t=HW8(0xe88023);t0=t;   // start on a step edge
   uint32_t n=0;unsigned steps=0;
   while(steps<40&&n<400000UL){for(unsigned j=0;j<8;j++){*q=v;*q=v;*q=v;*q=v;}n+=32;t=HW8(0xe88023);steps=(t0+200-t)%200;}
   io_probe_ns[k]=steps?steps*50000UL/n:0;io_probe_n[k]=n;}}
 HW8(0xe88019)=0;HW16(0xe80012)=1023;HW16(0xeb0808)=0;
 *(volatile uint32_t*)0x134=saved_vblank;*(volatile uint32_t*)0x138=saved_raster;
 HW8(0xe88003)=saved_aer;HW8(0xe88007)=saved_iera;HW8(0xe88013)=saved_imra;
 HW8(0xe8800b)&=0x9f;HW8(0xe8800f)&=0x9f;HW8(0xe8801f)=saved_tadr;HW8(0xe88019)=saved_tacr;
 native_road_memory_check();
 // r025: clear the sprite registers (the car stayed on the exit screen) and turn sprites off
 for(unsigned i=0;i<512;i++)HW16(0xeb0000UL+i*2)=0;
 // r026e: also clear the PCG/BG area (car and roadside pictures stayed on the exit screen)
 for(unsigned i=0;i<16384;i++)HW16(0xeb8000UL+i*2)=0;
 // r027b: the score etc. use all 4 text planes; the console clears only its own
 for(unsigned p=0;p<4;p++)for(unsigned i=0;i<512*64;i++)HW16(0xe00000UL+p*0x20000UL+i*2)=0;
 native_cache_restore();
 restore_irqs(sr);_iocs_crtmod(native_old_mode);_iocs_sp_off();_iocs_b_curon();for(unsigned i=0;i<16;i++)HW16(0xe82200UL+i*2)=saved_text_palette[i];_iocs_b_clr_al();_iocs_b_locate(0,0);if(native_ssp>0)_iocs_b_super(native_ssp);
 // (written before the pages of figures: the measuring scripts wait for this file, then press keys)
 FILE*f=fopen("ROADPERF.TXT","w");if(f){fprintf(f,"visual_mode=%u\nbackground_columns=%lu\ncar_frames=%lu\ncar_text_frames=%lu\nmax_sprites=%u\nmax_patterns=%u\nmax_line=%u\noverflow=%lu\n",visual_mode,(unsigned long)background_columns,(unsigned long)car_frames,(unsigned long)car_text_frames,car_peak_tiles,car_peak_patterns,car_peak_line,(unsigned long)car_overflows);fprintf(f,"submitted=%lu shown=%lu cs=%lu\nlogic_cs=%lu table_cs=%lu publish_cs=%lu wait_cs=%lu\nvblanks=%lu raster_irqs=%lu\n",(unsigned long)native_frames,(unsigned long)road_shown,(unsigned long)t,(unsigned long)native_phase_cs[0],(unsigned long)native_phase_cs[1],(unsigned long)native_phase_cs[2],(unsigned long)native_phase_cs[3],(unsigned long)road_vblanks,(unsigned long)road_irq_count);fclose(f);}
 report_pager_start();   // r028: stop at each full screen

 const char* labels[]={"ROAD","BG","CAR","BOTH"};
 for(unsigned m=0;m<4;m++){
  uint32_t total=0;for(unsigned k=0;k<4;k++)total+=native_visual_cs[m][k];
  report_printf("MODE %s steps=%lu draws=%lu logic_cs=%lu table_cs=%lu publish_cs=%lu wait_cs=%lu fps_x1000=%lu\n",labels[m],(unsigned long)native_visual_steps[m],(unsigned long)native_visual_draws[m],(unsigned long)native_visual_cs[m][0],(unsigned long)native_visual_cs[m][1],(unsigned long)native_visual_cs[m][2],(unsigned long)native_visual_cs[m][3],total?native_visual_draws[m]*100000UL/total:0);
 }
 if(native_irq_failed)report_puts("ROAD INTERRUPT TIMEOUT - report the counters below.");
 report_printf("R028 IDLE stop_waits=%lu\n",(unsigned long)native_idle_waits);
 report_printf("R031 SOUND %s ticks=%lu fm_writes=%lu fm_timeouts=%lu\n",audio_enabled&&audio_fm_enabled?"FM":"off",(unsigned long)audio_ticks_done,(unsigned long)audio_fm_writes,(unsigned long)audio_fm_timeouts);
 report_printf("R031 PCM %s adpcm_bytes=%lu resyncs=%lu\n",audio_enabled&&audio_pcm_enabled?"ADPCM":"off",(unsigned long)audio_pcm_bytes,(unsigned long)audio_pcm_resyncs);
 report_printf("SKIP %s skipped_draws=%lu\n",native_skip_mode>0?"on":"off",(unsigned long)native_skipped_draws);
 report_printf("R029 PACE %s screen=%s\n",native_vsync_pace?"VSYNC (2 refreshes a picture)":"CLOCK (30 a second)",native_hz60?"60 Hz":"55.46 Hz");
 report_printf("R029 HANDOFF calls=%lu stop_waits=%lu us/draw=%lu\n",(unsigned long)native_handoff_calls,(unsigned long)native_handoff_waits,(unsigned long)(native_handoff_calls?native_handoff_fine*50UL/native_handoff_calls:0));
 report_printf("ROAD BRIDGE r030 320x224 BG/raster\nframes=%lu shown=%lu cs=%lu fps_x1000=%lu\n",(unsigned long)native_frames,(unsigned long)road_shown,(unsigned long)t,t?native_frames*100000UL/t:0);
 report_printf("logic_cs=%lu table_cs=%lu publish_cs=%lu wait_cs=%lu\n",(unsigned long)native_phase_cs[0],(unsigned long)native_phase_cs[1],(unsigned long)native_phase_cs[2],(unsigned long)native_phase_cs[3]);
 report_printf("visual_mode=%u road=%lu bg=%lu car=%lu both=%lu\n",visual_mode,(unsigned long)visual_frames[0],(unsigned long)visual_frames[1],(unsigned long)visual_frames[2],(unsigned long)visual_frames[3]);
 report_printf("bg_columns=%lu car_frames=%lu car_tiles=%lu max_tiles=%u max_patterns=%u max_line=%u overflow=%lu\n",(unsigned long)background_columns,(unsigned long)car_frames,(unsigned long)car_tiles_total,car_peak_tiles,car_peak_patterns,car_peak_line,(unsigned long)car_overflows);
 report_printf("car_text_frames=%lu\n",(unsigned long)car_text_frames);
 report_printf("shown_x1000=%lu\n",t?road_shown*100000UL/t:0);
 {int on_stack=0;static int in_data=1;void*const heap=malloc(16);
  const auto where=[](const void*p){return x68k_in_local_memory(p)?"LOCAL":"MAIN";};
  report_printf("MEMORY 060turbo.sys=%d mode=%s code=%s data=%s heap=%s stack=%s pcm=%s cpu_bits=%ld\n",int(x68k_has_060turbo()),!x68k_has_060turbo()?"-":x68k_extended_mode()?"extended":"compatible",
   where(reinterpret_cast<const void*>(&native_run_in_local_memory)),where(&in_data),where(heap),where(&on_stack),audio_pcm_place==1?"MAIN(taken)":audio_pcm_place==2?"none":"in program",x68k_has_060turbo()?x68k_sys_stat(0xc006):-1L);
  free(heap);}
 report_printf("MPU cache at start=%d%s (-1: no cache control in this system, 3: both on)\n",native_cache_before,native_cache_was_set?", turned on by this program":"");
 report_printf("IOSPEED ns/long ram=%lu pcg=%lu bg=%lu pal=%lu crtc=%lu\n",(unsigned long)io_probe_ns[0],(unsigned long)io_probe_ns[1],(unsigned long)io_probe_ns[2],(unsigned long)io_probe_ns[3],(unsigned long)io_probe_ns[4]);
 report_printf("LINE BUDGET longs=%u (pcg ns/long at start=%lu) scenery=%s\n",visuals_line_budget(),(unsigned long)pcg_long_ns,visuals_light()?"LIGHT":"FULL");
 report_printf("vblanks=%lu raster_irqs=%lu vblank_overruns=%lu\n",(unsigned long)road_vblanks,(unsigned long)road_irq_count,(unsigned long)road_vblank_overruns);
 {
  uint32_t fine_total=fine_last-fine_start;
  report_printf("FINE r024 unit=50us total=%lu cs_equiv=%lu tcdr=%u..%u backsteps=%lu retries=%lu\n",(unsigned long)fine_total,(unsigned long)(fine_total/200),fine_tmin,fine_tmax,(unsigned long)fine_backsteps,(unsigned long)fine_retries);
  for(unsigned m=0;m<4;m++){
   uint32_t d=native_visual_draws[m];if(!d)continue;
   report_printf("FINE %s draws=%lu us/draw logic=%lu table=%lu publish=%lu wait=%lu max_us table=%lu work=%lu overruns=%lu late_cs=%lu\n",labels[m],(unsigned long)d,
    (unsigned long)(fine_phase[m][0]*50UL/d),(unsigned long)(fine_phase[m][1]*50UL/d),(unsigned long)(fine_phase[m][2]*50UL/d),(unsigned long)(fine_phase[m][3]*50UL/d),
    (unsigned long)(fine_phase_max[m][1]*50UL),(unsigned long)(fine_work_max[m]*50UL),(unsigned long)fine_overruns[m],(unsigned long)(fine_late[m]/60));
  }
 }
#ifdef CBX68_AUDIO_LOG
 {FILE*af=fopen("AUDIO.BIN","wb");if(af){fwrite(audio_log,4,audio_log_n,af);fclose(af);}}
#endif
#ifdef CBX68_PROF
 {FILE*pf=fopen("PROF.BIN","wb");if(pf){fwrite(&prof_text_size,4,1,pf);fwrite(prof_hist,4,(prof_text_size>>4)+1,pf);fclose(pf);}}
#endif
}
