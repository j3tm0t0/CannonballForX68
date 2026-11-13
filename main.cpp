/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
#include "platform_prefix.cpp"
#include "engine/otraffic.hpp"
#include "road_bridge.hpp"
#include "visuals.hpp"
#include <cstdlib>
#include "cbx68_test.hpp"
#include "report_pager.hpp"   // r028: the end figures stop at each full screen (X68000)
static void snapshot(const char* name) {
    if (!g_pixels) return;
    FILE* f=fopen(name,"wb"); if(!f)return;
    fprintf(f,"P6\n320 224\n255\n");
    for(int i=0;i<320*224;i++){unsigned w=g_pixels[i];unsigned c=(((w>>6)&31)*8<<16)|(((w>>11)&31)*8<<8)|(((w>>1)&31)*8); fputc(c>>16,f);fputc(c>>8,f);fputc(c,f);}
    fclose(f);
}
#ifndef OR_X68K

#include "verify_timeline.hpp"
#include "verify_road_bridge.hpp"
#endif
#ifdef OR_X68K
// r020: per-part timing of the draw preparation (1/20000 s units) and the D-key road dump.
static uint32_t part_road[4],part_build[4],part_stream[4],part_max[4][3],part_road_stream[4];
static unsigned road_dump_count=0;static bool road_dump_requested=false;
static void road_dump(const RoadFrame&f,int frame){
 if(road_dump_count>=10)return;
 char name[16];snprintf(name,sizeof name,"RDUMP%u.BIN",road_dump_count);
 FILE*o=fopen(name,"wb");if(!o)return;
 uint32_t hdr[8]={0x52444d50UL,2,(uint32_t)frame,visual_mode,hwroad.road_control,hwroad.color_offset1,hwroad.color_offset2,hwroad.color_offset3};
 int32_t xo=hwroad.x_offset;
 fwrite(hdr,4,8,o);fwrite(&xo,4,1,o);
 fwrite(hwroad.ramBuff,2,HWRoad::ROAD_RAM_SIZE/2,o);
 fwrite(display_palette,2,8192,o);
 fwrite(f.rows,sizeof(RoadRow),224,o);
 fclose(o);++road_dump_count;
}
#endif
int main(int argc,char**argv) {
    int max_frames=argc>1?atoi(argv[1]):1000;
#ifdef OR_X68K
 max_frames=argc>1?atoi(argv[1]):0;
#endif
    puts("Cannonball for x68 v0.2 - unofficial X68000 port by Mattsuan-Z@ColtMaz");
    puts("Based on Cannonball (OutRun engine) by Chris White and the Cannonball team");
    puts("OutRun is a trademark of SEGA. Not affiliated with SEGA or the Cannonball team. ROMs not included.");fflush(stdout);
#ifdef OR_X68K
    // The MPU caches: on a 68030 and above the program is many times slower with them off, and they
    // are off when no driver has turned them on (a 68060 accelerator without its driver). They are
    // turned on here (IOCS _SYS_STAT), and what was found is put back at the end (platform_exit).
    native_cache_on();
    if(native_cache_before<0)puts("MPU cache: this system has no cache control (IOCS _SYS_STAT); if this is a 68030 or above, the game is much slower with the caches off.");
    else printf("MPU cache: was %d%s\n",native_cache_before,native_cache_was_set?", turned on":"");
    fflush(stdout);
#endif
    // Cannonball for x68: "CBX68 TEST" = display test without ROMs (PC: 2nd argument = frames)
#ifdef OR_X68K
    // r029: VSYNC (default) / CLOCK / HZ60 anywhere after the first argument choose the frame pacing (native_platform.hpp)
    for(int a=2;a<argc;a++){char w[8]={0};for(int k=0;k<7&&argv[a][k];k++)w[k]=char(argv[a][k]|32);
     if(!strcmp(w,"vsync"))native_vsync_pace=true;
     if(!strcmp(w,"clock"))native_vsync_pace=false;
     if(!strcmp(w,"hz60")){native_hz60=true;native_vsync_pace=true;}
     // SKIP / NOSKIP: leave pictures out when the machine cannot draw one every 2 refreshes, so the
     // game keeps its speed (default: on in a normal game, off in a measured run with a frame count)
     if(!strcmp(w,"skip"))native_skip_mode=1;
     if(!strcmp(w,"noskip"))native_skip_mode=0;
     if(!strcmp(w,"nosound"))audio_enabled=0;   // r031: sound off
     if(!strcmp(w,"nopcm"))audio_pcm_enabled=0;   // r031c: FM only
     if(!strcmp(w,"nofm"))audio_fm_enabled=0;}    // PCM only
    if(native_hz60)audio_ticks_per_blank=2083;   // 125 Hz / 60 Hz
    if(native_skip_mode<0)native_skip_mode=max_frames==0?1:0;
#endif
    cbx68_test=argc>1&&(argv[1][0]|32)=='t'&&(argv[1][1]|32)=='e'&&(argv[1][2]|32)=='s'&&(argv[1][3]|32)=='t'&&!argv[1][4];
    if(cbx68_test){
     // r028: CBX68 TEST HEAVY (PC: stage1 test FRAMES heavy) = more and bigger shapes, more colours
     for(int a=2;a<argc;a++)if((argv[a][0]|32)=='h'&&(argv[a][1]|32)=='e'&&(argv[a][2]|32)=='a'&&(argv[a][3]|32)=='v'&&(argv[a][4]|32)=='y'&&!argv[a][5])cbx68_test_heavy=true;
#ifdef OR_X68K
     max_frames=0;
#else
     max_frames=argc>2&&argv[2][0]>='0'&&argv[2][0]<='9'?atoi(argv[2]):600;
#endif
     argc=1;}
    bool road_demo=argc>3&&strcmp(argv[3],"manual");bool road_right=road_demo&&!strcmp(argv[3],"right");
    bool visual_cycle=argc>4&&!strcmp(argv[4],"cycle");
    // Cannonball for x68: 5th argument = ROM folder (default ROMS\, as Cannonball's roms/ folder)
    {extern const char* cbx68_rom_path;
     auto option_word=[](const char*a){char w[8]={0};for(int k=0;k<7&&a[k];k++)w[k]=char(a[k]|32);
      return !strcmp(w,"vsync")||!strcmp(w,"clock")||!strcmp(w,"hz60")||!strcmp(w,"skip")||!strcmp(w,"noskip")||!strcmp(w,"nosound")||!strcmp(w,"nopcm")||!strcmp(w,"nofm");};
     if(argc>5&&!option_word(argv[5]))cbx68_rom_path=argv[5];}
    if(argc>4){if(!strcmp(argv[4],"road")||visual_cycle)visual_mode=0;else if(!strcmp(argv[4],"bg"))visual_mode=1;else if(!strcmp(argv[4],"car"))visual_mode=2;else if(!strcmp(argv[4],"both"))visual_mode=3;}
    memset(&config.engine,0,sizeof config.engine);
    config.data.asset_path="assets/"; config.data.res_path="./";config.data.save_path="./";config.data.crc32=0;
    config.menu.enabled=0;
    config.video.mode=0;config.video.scale=1;config.video.fps=2;config.video.widescreen=0;
    config.video.hires=0;config.video.shadow=0;config.video.fps_count=0;
    config.sound.enabled=0;config.sound.rate=15625;config.sound.fix_samples=0;config.sound.music_timer=0x30;config.sound.preview=0;
    config.controls.gear=0;config.controls.haptic=0;config.controls.rumble=0;config.controls.analog=0;
    config.controls.steer_speed=3;config.controls.pedal_speed=4;config.smartypi.enabled=0;
    config.engine.dip_time=0;config.engine.dip_traffic=1;
    config.engine.freeze_timer=false;config.engine.disable_traffic=false;config.engine.level_objects=1;
    config.engine.randomgen=1;config.engine.fix_bugs=false;config.engine.fix_timer=false;
    config.engine.layout_debug=false;config.engine.new_attract=0;config.engine.hiscore_timer=0x30;
    config.s16_width=S16_WIDTH;config.s16_height=S16_HEIGHT;config.s16_x_off=0;
    config.fps=60;config.tick_fps=30;config.cont_traffic=0;
    render_schedule.draw30=!(argc>2&&!strcmp(argv[2],"60"));
    puts(render_schedule.draw30?"DRAW30 / GAME30 (V switches)":"DRAW60 / GAME30 (V switches)");
    puts("Loading assets...");fflush(stdout);
    if(cbx68_test){puts("DISPLAY TEST: no ROM is used; the pictures are simple shapes made by this program.");cbx68_test_assets();visual_mode=3;}
    else if(!assets.load_revb_assets(false)){
        // Cannonball for x68: as in Cannonball, the game needs the user's own ROM files
        extern const char* cbx68_rom_path;extern unsigned cbx68_rom_errors;
        printf("\nSome ROM files are missing or incorrect (see above).\n");(void)cbx68_rom_errors;
        printf("Place the OutRun (Revision B) ROM files in %s and start again.\n",cbx68_rom_path);
        printf("To check the display without ROMs, run: CBX68 TEST\n");
        return 1;}
#ifndef OR_X68K
    // r031: replay a sound log of the X68000 build through the PC's sound code and compare (audio_replay_host.cpp)
    if(const char*al=getenv("CBX68_AUDIO_REPLAY")){extern int audio_replay(const char*);return audio_replay(al);}
#endif
    if(!road_assets_load()){puts("ROAD DATA BUILD FAILED");return 3;}   // from the road ROM, before the video set-up frees it
    puts("Assets loaded. Initializing display...");fflush(stdout);
    omusic.load_widescreen_map("./");
    if(!video.init(&assets,&config.video)){puts("Display initialization failed.");return 1;}
    if(cbx68_test){cbx68_test_setup();puts("Display test ready. 1-4: display  ESC: quit");fflush(stdout);}
    else{puts("Initializing game...");fflush(stdout);outrun.init();}
    if(!cbx68_test){
#ifdef OR_X68K
    puts("Game ready. C: credit  ENTER: start  UP/Z: accel  DOWN/X: brake  LEFT/RIGHT  SPACE: gear");
    puts("1-4: display  G: roadside objects  H: score  R: restart  ESC: quit");fflush(stdout);
#else
    puts("Game ready. 1: road  2: car  3: background  4: both  ESC: quit.");fflush(stdout);
#endif
    }
    if(!visuals_load()){puts("BACKGROUND INIT FAILED");return 4;}
#ifndef OR_X68K
    if(getenv("STAGE1_CHECK_ROAD"))verify_road_bridge();
#endif
#ifndef OR_X68K
    // r020: replay an RDUMPn.BIN written by the X68000 build (D key) and compare
    // the BG/raster model with the original road renderer for that exact frame.
    if(getenv("STAGE1_ROAD_DUMP")){
     FILE*f=fopen(getenv("STAGE1_ROAD_DUMP"),"rb");if(!f){puts("DUMP OPEN FAILED");return 5;}
     auto be32=[&](){uint32_t v=0;for(int i=0;i<4;i++)v=(v<<8)|fgetc(f);return v;};
     auto be16=[&](){unsigned a=fgetc(f),b=fgetc(f);return uint16_t((a<<8)|b);};
     uint32_t hdr[8];for(int i=0;i<8;i++)hdr[i]=be32();int32_t xo=(int32_t)be32();
     if(hdr[0]!=0x52444d50UL||(hdr[1]!=1&&hdr[1]!=2)){puts("DUMP MAGIC FAILED");return 5;}
     const unsigned grounds=hdr[1]==1?3:5;
     hwroad.road_control=hdr[4];hwroad.color_offset1=hdr[5];hwroad.color_offset2=hdr[6];hwroad.color_offset3=hdr[7];hwroad.x_offset=xo;
     for(unsigned i=0;i<HWRoad::ROAD_RAM_SIZE/2;i++)hwroad.ramBuff[i]=be16();
     for(unsigned i=0;i<8192;i++)display_palette[i]=be16();
     static RoadRow dumped[224];
     for(unsigned y=0;y<224;y++){RoadRow&r=dumped[y];r.bg0=be32();r.bg1=be32();r.gpage=be32();for(int k=0;k<4;k++)r.front[k]=be16();for(int k=0;k<4;k++)r.back[k]=be16();for(unsigned k=0;k<grounds;k++)r.ground[k]=be16();r.bg_y=be16();r.bg_far_y=be16();r.control=be16();r.road_enable=be16();r.next=be16();}
     fclose(f);
     static RoadFrame rf;road_build(hwroad,display_palette,rf);road_stream(rf);
     unsigned row_diff=0;for(unsigned y=0;y<224;y++){const RoadRow&a=rf.rows[y],&b=dumped[y];
      bool same=a.bg0==b.bg0&&a.bg1==b.bg1&&a.gpage==b.gpage&&!memcmp(a.front,b.front,8)&&!memcmp(a.back,b.back,8)&&(hdr[1]==1||!memcmp(a.ground,b.ground,10))&&a.road_enable==b.road_enable&&a.next==b.next;
      if(!same){if(row_diff<8)printf("ROW DIFF y=%u\n",y);++row_diff;}}
     static uint16_t ref[320*224],br[320*224];
     for(int i=0;i<320*224;i++)ref[i]=0xffff;
     hwroad.render_foreground_lores_reference(ref);
     for(int i=0;i<320*224;i++)if(ref[i]!=0xffff)ref[i]=display_palette[ref[i]&8191];
     road_preview(rf,br);
     unsigned diff=0;for(int i=0;i<320*224;i++)if(ref[i]!=0xffff&&ref[i]!=br[i])++diff;
     printf("DUMP frame=%lu visual=%lu road_control=%lu rows_differ=%u pixels_differ=%u visible=%u hidden=%u miss=%u\n",(unsigned long)hdr[2],(unsigned long)hdr[3],(unsigned long)hdr[4],row_diff,diff,rf.visible,rf.hidden,rf.variant_miss);
     const char*out=getenv("STAGE1_ROAD_DUMP_PPM");
     if(out){FILE*o=fopen(out,"wb");fprintf(o,"P6\n640 224\n255\n");
      for(int y=0;y<224;y++)for(int x=0;x<640;x++){unsigned w=x<320?ref[y*320+x]:br[y*320+x-320];if(w==0xffff){fputc(255,o);fputc(0,o);fputc(255,o);continue;}unsigned c=(((w>>6)&31)*8<<16)|(((w>>11)&31)*8<<8)|(((w>>1)&31)*8);fputc(c>>16,o);fputc(c>>8,o);fputc(c,o);}
      fclose(o);}
     return 0;
    }
#endif
#ifndef OR_X68K

#endif
    cannonball::state=cannonball::STATE_GAME;
    int ingame=-1;
#ifdef OR_X68K
    bool time_hold=false,autopilot=false,auto_right=true;   // r027c check aids (T, A; LEFT/RIGHT pick the side at forks)
#endif
    int actual_frames=0;
    int cycle_start=0;
    bool reset_held=false;
    bool display_held=false;
#ifndef OR_X68K
    bool automatic_switch=argc>2&&!strcmp(argv[2],"toggle");
    timeline_open();
#endif
#ifdef OR_X68K
 native_audio_game=cbx68_test?0:1;
 platform_init();
#else
 visuals_native_init();
 if(getenv("STAGE1_CHECK_VISUALS"))visuals_selftest();
#endif
    for(int frame=0;(!max_frames||frame<max_frames);frame++) {
        if(visual_cycle)visual_mode=(frame/200)&3;
        bool logic=(frame&1)==0;cannonball::tick_frame=logic;
#ifndef OR_X68K
        if(automatic_switch&&frame>0&&frame%100==0)render_schedule.draw30=!render_schedule.draw30;
#endif
#ifdef OR_X68K
        if(cbx68_test&&logic){
            bool quit=false;
            for(int n=0;n<64&&_iocs_b_keysns();n++){
                int c=_iocs_b_keyinp()&255;cbx68_test_key=unsigned(c);
                if(c==27)quit=true;
                if(c=='1')visual_mode=0;
                if(c=='2')visual_mode=2;
                if(c=='3')visual_mode=1;
                if(c=='4')visual_mode=3;
                if(c=='v'||c=='V')render_schedule.draw30=!render_schedule.draw30;
                if(c=='l'||c=='L')cbx68_test_heavy=!cbx68_test_heavy;   // r028: light / heavy load
            }
            if(quit)break;
        }
#endif
        if(logic&&!cbx68_test){
#ifdef OR_X68K
            bool queued_reset=false,queued_exit=false,queued_display=false;
            for(int n=0;n<64&&_iocs_b_keysns();n++){
                int c=_iocs_b_keyinp()&255;
                if(c=='r'||c=='R')queued_reset=true;
                if(c=='v'||c=='V')queued_display=true;
                if(c==27)queued_exit=true;
                if(c=='1')visual_mode=0;
                if(c=='2')visual_mode=2;
                if(c=='3')visual_mode=1;
                if(c=='4')visual_mode=3;
                if(c=='b'||c=='B')visual_mode^=1;
                if(c=='k'||c=='K')visual_mode^=2;
                if(c=='d'||c=='D')road_dump_requested=true;
                if(c=='g'||c=='G'){extern bool scenery_enabled;scenery_enabled=!scenery_enabled;}   // r026
                if(c=='h'||c=='H'){extern bool hud_enabled;hud_enabled=!hud_enabled;}   // r027
                if(c=='t'||c=='T')time_hold=!time_hold;      // r027c: check aid, time stays at 99
                if(c=='a'||c=='A')autopilot=!autopilot;      // r027c: check aid, drives by itself
            }
            if(queued_exit)break;
            bool display_key=native_key(0x2d);
            if((display_key||queued_display)&&!display_held)render_schedule.draw30=!render_schedule.draw30;
            display_held=display_key;
            bool reset_now=native_key(0x13);
            if((reset_now||queued_reset)&&!reset_held){outrun.init();ingame=-1;cycle_start=frame;}
            reset_held=reset_now;
#endif
            memcpy(input.keys_old,input.keys,sizeof input.keys);memset(input.keys,0,sizeof input.keys);
            // r027c: on the X68000 credit and start come from the keys (C, Enter) only; the automatic
            // credit/start stays for measured runs (frame count given) and the PC checks.
#ifdef OR_X68K
            const bool auto_start=max_frames!=0||road_demo;
#else
            const bool auto_start=true;
#endif
            input.keys[Input::COIN]=auto_start&&frame-cycle_start>=20&&frame-cycle_start<28;
            input.keys[Input::START]=auto_start&&frame-cycle_start>=60&&frame-cycle_start<68;
            if(outrun.game_state==GS_INGAME&&ingame<0)ingame=frame;
            // r025: time limit 99 for every new game (set while the start sequence runs, before the
            // countdown; checkpoint extensions still add and stop at 99). Host: only with STAGE1_TIME99=1
            // so the recorded game states stay comparable with r018.
#ifdef OR_X68K
            const bool time99=true;
#else
            static const bool time99=std::getenv("STAGE1_TIME99")!=0;
#endif
            if(time99&&(outrun.game_state==GS_START1||outrun.game_state==GS_START2||outrun.game_state==GS_START3))ostats.time_counter=0x99;
            if(ingame>=0){input.keys[Input::ACCEL]=true;input.keys[Input::GEAR1]=frame-ingame>=80;}
#ifdef OR_X68K
            if(!platform_keys(frame, ingame, max_frames!=0||road_demo))break;
#endif
#ifdef OR_X68K
            if(time_hold&&outrun.game_state==GS_INGAME)ostats.time_counter=0x99;
            if(autopilot&&ingame>=0){
                if(input.keys[Input::LEFT])auto_right=false;
                if(input.keys[Input::RIGHT])auto_right=true;
                road_right=auto_right;
                input.keys[Input::ACCEL]=true;input.keys[Input::BRAKE]=false;input.keys[Input::GEAR1]=true;
            }
#endif
#ifndef OR_X68K
            // r027c PC check of all courses: STAGE1_ROUTE=LRRL.. picks the side at the fork of each
            // stage (default: the right/left argument); STAGE1_TIME_HOLD=1 keeps the time at 99.
            static const char*route=std::getenv("STAGE1_ROUTE");
            static const bool time_hold=std::getenv("STAGE1_TIME_HOLD")!=0;
            if(time_hold&&outrun.game_state==GS_INGAME)ostats.time_counter=0x99;
            {static int seen_stage=-1;if(ostats.cur_stage!=seen_stage){seen_stage=ostats.cur_stage;printf("COURSE frame=%d stage=%d\n",frame,seen_stage);}}
            if(route&&ostats.cur_stage>=0&&ostats.cur_stage<(int)strlen(route))road_right=route[ostats.cur_stage]=='R';
#endif
#ifdef OR_X68K
            if((road_demo||autopilot)&&ingame>=0){
#else
            if(road_demo&&ingame>=0){
#endif
                int width=oroad.road_width>>16;
                int target=width>255?(road_right?-width:width):0;
                input.keys[Input::LEFT]=oinitengine.car_x_pos<target-20;
                input.keys[Input::RIGHT]=oinitengine.car_x_pos>target+20;
            }
            oinputs.tick();oinputs.do_gear();
        }
#ifdef OR_X68K
        uint32_t phase_start=native_clock();native_visual_begin(visual_mode);
#endif
        if(cbx68_test)cbx68_test_frame(frame);else outrun.tick(logic);
#ifdef OR_X68K
        native_phase_record(0,phase_start);phase_start=native_clock();
#endif
#ifndef OR_X68K
        timeline_step(frame,logic);
#endif
        bool draw=render_schedule.should_draw(frame);
#ifdef OR_X68K
        if(draw&&native_skip_mode>0&&!cbx68_test)draw=platform_draw_due(frame);
#endif
        if(draw){
#ifdef OR_X68K
            RoadFrame& road_frame=platform_road_back();
#else
            static RoadFrame road_frame;
#endif
#ifdef OR_X68K
            // r029: build first (into the free third frame), then wait until the frame before has
            // been taken by the vertical blank, then write video memory (road page rows, text).
            {unsigned vm=visual_mode&3;uint32_t t0=platform_fine_clock();
            road_build(hwroad,display_palette,road_frame);uint32_t ta=platform_fine_clock();
            visuals_build(road_frame,display_palette);
            uint32_t t2=platform_fine_clock();
            platform_road_handoff();visuals_handoff(road_frame);
            uint32_t tb=platform_fine_clock();
            road_stream(road_frame);uint32_t tc=platform_fine_clock();part_road_stream[vm]+=tc-tb;
            visuals_stream(road_frame);
            uint32_t t3=platform_fine_clock();
            const uint32_t road_t=(ta-t0)+(tc-tb);
            part_road[vm]+=road_t;part_build[vm]+=t2-ta;part_stream[vm]+=t3-tc;
            if(road_t>part_max[vm][0])part_max[vm][0]=road_t;if(t2-ta>part_max[vm][1])part_max[vm][1]=t2-ta;if(t3-tc>part_max[vm][2])part_max[vm][2]=t3-tc;}
            if(road_dump_requested){road_dump_requested=false;road_dump(road_frame,frame);}
#else
            road_build(hwroad,display_palette,road_frame);   // r029: same order as the X68000
            visuals_build(road_frame,display_palette);visuals_handoff(road_frame);
            road_stream(road_frame);
            visuals_stream(road_frame);
#endif
#ifdef OR_X68K
            native_phase_record(1,phase_start);phase_start=native_clock();
            road_rows_changed(road_frame);   // r031f: the raster interrupt writes only what differs from the line before
            platform_road_submit(road_frame);
            native_phase_record(2,phase_start);
#else
            road_preview(road_frame,video.pixels);visuals_preview(road_frame,video.pixels);g_pixels=video.pixels;
            {// r028 PC check: digest of every drawn picture (compared with the v0.1 code)
             static uint64_t dg=1469598103934665603ULL;static unsigned dn=0;for(int i=0;i<320*224;i++){dg^=video.pixels[i];dg*=1099511628211ULL;}++dn;
             if(dn%50==0||getenv("R028_DIGEST_ALL"))printf("R028 DIGEST draw=%u frame=%d %016llx\n",dn,frame,(unsigned long long)dg);}
            timeline_render(frame);
            if(frame==299||frame==599||frame==999||frame%1000==999){char n[48];snprintf(n,sizeof n,"road_%05d.ppm",frame);snapshot(n);}
#endif
        }
#ifdef OR_X68K
        phase_start=native_clock();
#endif
#ifdef OR_X68K
        platform_sync(frame);
        native_phase_record(3,phase_start);
        native_visual_end();
        if(native_irq_failed)break;
#endif
        render_schedule.record(logic,draw);
        actual_frames=frame+1;
    }
    #ifdef OR_X68K
    platform_exit();
#endif
    report_printf("base_steps=%d ingame=%d state=%d stage=%d pos=%x speed=%x car_x=%d\n",actual_frames,ingame,outrun.game_state,ostats.cur_stage,oroad.road_pos>>16,oinitengine.car_increment>>16,oinitengine.car_x_pos);
#ifndef OR_X68K
    timeline_close();

    if(getenv("STAGE1_SPRITE_STATS")){FILE* f=fopen(getenv("STAGE1_SPRITE_STATS"),"w");video.sprite_layer->print_sprite_work(f);if(f)fclose(f);}
    const char* trace_root=getenv("STAGE1_TRACE");
    if(trace_root){std::string profile_path=std::string(trace_root)+".profile.txt";FILE* f=fopen(profile_path.c_str(),"w");draw_profile_print(f,true);if(f)fclose(f);}
    report_printf("SCHEDULE steps=%u logic=%u draws=%u mode30_steps=%u mode60_steps=%u verified_renders=%u\n",render_schedule.steps[0]+render_schedule.steps[1],render_schedule.logic[0]+render_schedule.logic[1],render_schedule.draws[0]+render_schedule.draws[1],render_schedule.steps[0],render_schedule.steps[1],0U);
#endif
    report_printf("road_ctrl=%u split_state=%u horizon=%d curve=%d route=%d\n",unsigned(oroad.road_ctrl),unsigned(oinitengine.rd_split_state),int(oroad.horizon_y_bak),int(oinitengine.road_curve),int(oinitengine.route_selected));
    report_printf("VISUAL road=%lu bg=%lu car=%lu both=%lu bg_columns=%lu car_frames=%lu max_tiles=%u max_patterns=%u max_line=%u overflow=%lu\n",(unsigned long)visual_frames[0],(unsigned long)visual_frames[1],(unsigned long)visual_frames[2],(unsigned long)visual_frames[3],(unsigned long)background_columns,(unsigned long)car_frames,car_peak_tiles,car_peak_patterns,car_peak_line,(unsigned long)car_overflows);
    report_printf("CAR TEXT frames=%lu\n",(unsigned long)car_text_frames);
#ifdef OR_X68K
    {const char*names[4]={"ROAD","BG","CAR","BOTH"};
     for(unsigned m=0;m<4;m++){uint32_t d=native_visual_draws[m];if(!d)continue;
      report_printf("R020 TABLE %s draws=%lu us/draw road=%lu visuals_build=%lu stream=%lu max_us road=%lu build=%lu stream=%lu\n",names[m],(unsigned long)d,(unsigned long)(part_road[m]*50UL/d),(unsigned long)(part_build[m]*50UL/d),(unsigned long)(part_stream[m]*50UL/d),(unsigned long)(part_max[m][0]*50UL),(unsigned long)(part_max[m][1]*50UL),(unsigned long)(part_max[m][2]*50UL));}
     for(unsigned m=0;m<4;m++)if(native_visual_draws[m])report_printf("R024 ROAD %s road_stream us/draw=%lu\n",names[m],(unsigned long)(part_road_stream[m]*50UL/native_visual_draws[m]));
     report_printf("R020 DUMPS files=%u\n",road_dump_count);}
#endif
    visuals_print();
#ifdef OR_X68K
    native_road_memory_report();
#endif
    if(cbx68_test){report_printf("DISPLAY TEST END load=%s\n",cbx68_test_heavy?"HEAVY":"LIGHT");return 0;}
    return ingame<0?2:0;
}
