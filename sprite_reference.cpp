/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
// Offline sprite reference and state comparisons.
// Copyright Aaron Giles. All rights reserved. Based on MAME source code.
// See the original notices in vendor/main/hwvideo/hwsprites.cpp.
#ifndef OR_X68K
#include "video.hpp"
#include "hwvideo/hwsprites.hpp"
#include "frontend/config.hpp"
#include <cstring>
#include <vector>
#include <cstdio>
#include <cstdlib>
#define PIXEL_ACCURACY 0
#if PIXEL_ACCURACY

// Reproduces glowy edge around sprites on top of shadows as seen on Hardware.
// Believed to be caused by shadowing being out by one clock cycle / pixel.
//
// 1/ Sprites Drawn on top of Shadow clears the shadow flags for its opaque pixels.
// 2/ Either the flag clear or the sprite itself is offset by one pixel horizontally.
// 
// Thanks to Alex B. for this implementation.

#define draw_pixel()                                                                                  \
{                                                                                                     \
    if (x >= x1 && x < x2)                                                                            \
    {                                                                                                 \
        if (shadow && pix == 0xa)                                                                     \
        {                                                                                             \
            pPixel[x] &= 0xfff;                                                                       \
            pPixel[x] += S16_PALETTE_ENTRIES;                                                         \
        }                                                                                             \
        else if (pix != 0 && pix != 15)                                                               \
        {                                                                                             \
            if (x > x1) pPixel[x-1] &= 0xfff;                                                         \
            pPixel[x] = (pix | color);                                                                \
        }                                                                                             \
    }                                                                                                 \
}

#else

#define draw_pixel()                                                                                  \
{                                                                                                     \
    if (x >= x1 && x < x2 && pix != 0 && pix != 15)                                                   \
    {                                                                                                 \
        if (shadow && pix == 0xa)                                                                     \
        {                                                                                             \
            pPixel[x] &= 0xfff;                                                                       \
            pPixel[x] += S16_PALETTE_ENTRIES;                                                         \
        }                                                                                             \
        else                                                                                          \
        {                                                                                             \
            pPixel[x] = (pix | color);                                                                \
        }                                                                                             \
    }                                                                                                 \
}

#endif

void hwsprites::render_reference(const uint8_t priority)
{
    const uint32_t numbanks = SPRITES_LENGTH / 0x10000;

    for (uint16_t data = 0; data < SPRITE_RAM_SIZE; data += 8) 
    {
        // stop when we hit the end of sprite list
        if ((ramBuff[data+0] & 0x8000) != 0) break;

        uint32_t sprpri  = 1 << ((ramBuff[data+3] >> 12) & 3);
        if (sprpri != priority) continue;

        // if hidden, or top greater than/equal to bottom, or invalid bank, punt
        int16_t hide    = (ramBuff[data+0] & 0x5000);
        int32_t height  = (ramBuff[data+5] >> 8) + 1;       
        if (hide != 0 || height == 0) continue;
        
        int16_t bank    = (ramBuff[data+0] >> 9) & 7;
        int32_t top     = (ramBuff[data+0] & 0x1ff) - 0x100;
        uint32_t addr    = ramBuff[data+1];
        int32_t pitch  = ((ramBuff[data+2] >> 1) | ((ramBuff[data+4] & 0x1000) << 3)) >> 8;
        int32_t xpos    =  ramBuff[data+6]; // moved from original structure to accomodate widescreen
        uint8_t shadow  = (ramBuff[data+3] >> 14) & 1;
        int32_t vzoom    = ramBuff[data+3] & 0x7ff;
        int32_t ydelta = ((ramBuff[data+4] & 0x8000) != 0) ? 1 : -1;
        int32_t flip   = (~ramBuff[data+4] >> 14) & 1;
        int32_t xdelta = ((ramBuff[data+4] & 0x2000) != 0) ? 1 : -1;
        int32_t hzoom    = ramBuff[data+4] & 0x7ff;     
        int32_t color   = COLOR_BASE + ((ramBuff[data+5] & 0x7f) << 4);
        int32_t x, y, ytarget, yacc = 0, pix;
            
        // adjust X coordinate
        // note: the threshhold below is a guess. If it is too high, rachero will draw garbage
        // If it is too low, smgp won't draw the bottom part of the road
        if (xpos < 0x80 && xdelta < 0)
            xpos += 0x200;
        xpos -= 0xbe;

        // initialize the end address to the start address
        ramBuff[data+7] = addr;

        // clamp to within the memory region size
        if (numbanks)
            bank %= numbanks;

        const uint32_t* spritedata = sprites + 0x10000 * bank;

        // clamp to a maximum of 8x (not 100% confirmed)
        if (vzoom < 0x40) vzoom = 0x40;
        if (hzoom < 0x40) hzoom = 0x40;

        // loop from top to bottom
        ytarget = top + ydelta * height;

        // Adjust for widescreen mode
        xpos += config.s16_x_off;

        // Adjust for hi-res mode
        if (config.video.hires)
        {
            xpos <<= 1;
            top <<= 1;
            ytarget <<= 1;
            hzoom >>= 1;
            vzoom >>= 1;
        }

        for (y = top; y != ytarget; y += ydelta)
        {
            // skip drawing if not within the cliprect
            if (y >= 0 && y < config.s16_height)
            {
                uint16_t* pPixel = &video.pixels[y * config.s16_width];
                int32_t xacc = 0;

                // non-flipped case
                if (flip == 0)
                {
                    // start at the word before because we preincrement below
                    ramBuff[data+7] = (addr - 1);

                    for (x = xpos; (xdelta > 0 && x < config.s16_width) || (xdelta < 0 && x >= 0); )
                    {
                        uint32_t pixels = spritedata[++ramBuff[data+7]]; // Add to base sprite data the vzoom value

                        // draw four pixels
                        pix = (pixels >> 28) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
                        pix = (pixels >> 24) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
                        pix = (pixels >> 20) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
                        pix = (pixels >> 16) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
                        pix = (pixels >> 12) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
                        pix = (pixels >>  8) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
                        pix = (pixels >>  4) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
                        pix = (pixels >>  0) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;

                        // stop if the second-to-last pixel in the group was 0xf
                        if ((pixels & 0x000000f0) == 0x000000f0)
                            break;
                    }
                }
                // flipped case
                else
                {
                    // start at the word after because we predecrement below
                    ramBuff[data+7] = (addr + 1);

                    for (x = xpos; (xdelta > 0 && x < config.s16_width) || (xdelta < 0 && x >= 0); )
                    {
                        uint32_t pixels = spritedata[--ramBuff[data+7]];

                        // draw four pixels
                        pix = (pixels >>  0) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
                        pix = (pixels >>  4) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
                        pix = (pixels >>  8) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
                        pix = (pixels >> 12) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
                        pix = (pixels >> 16) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
                        pix = (pixels >> 20) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
                        pix = (pixels >> 24) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;
                        pix = (pixels >> 28) & 0xf; while (xacc < 0x200) { draw_pixel(); x += xdelta; xacc += hzoom; } xacc -= 0x200;

                        // stop if the second-to-last pixel in the group was 0xf
                        if ((pixels & 0x0f000000) == 0x0f000000)
                            break;
                    }
                }
            }
            // accumulate zoom factors; if we carry into the high bit, skip an extra row
            yacc += vzoom; 
            addr += pitch * (yacc >> 9);
            yacc &= 0x1ff;
        }
    }
}
void hwsprites::save_render_state(uint16_t* out) const {
    memcpy(out,ram,SPRITE_RAM_SIZE*sizeof(uint16_t));memcpy(out+SPRITE_RAM_SIZE,ramBuff,SPRITE_RAM_SIZE*sizeof(uint16_t));
}
void hwsprites::load_render_state(const uint16_t* in) {
    memcpy(ram,in,SPRITE_RAM_SIZE*sizeof(uint16_t));memcpy(ramBuff,in+SPRITE_RAM_SIZE,SPRITE_RAM_SIZE*sizeof(uint16_t));
}

void hwsprites::verify_sprite_cases(){
    std::vector<uint32_t> saved(sprites,sprites+SPRITES_LENGTH);
    uint16_t original[2048],initial[2048],a_state[2048],b_state[2048];save_render_state(original);
    uint16_t oldx1=x1,oldx2=x2;int ow=config.s16_width,oh=config.s16_height,oo=config.s16_x_off,hi=config.video.hires;
    uint16_t* oldpixels=video.pixels;
    uint32_t seed=0x1251937;auto next=[&](){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
    for(unsigned i=0;i<SPRITES_LENGTH;i++)sprites[i]=next()|((i%8==7)?0x0f0000f0:0);
    const unsigned hz[]={0,63,64,65,127,128,255,256,511,512,513,1023,2047};
    const unsigned vz[]={0,63,64,65,511,512,513,2047};
    const int tops[]={-256,-65,-1,0,1,7,112,216,223,224,255};
    const unsigned heights[]={1,2,8,32,255,256},pitches[]={0,1,2,127,128,254,255};
    const unsigned xs[]={0,127,128,182,189,190,191,501,511,512,65534,65535};
    const unsigned ads[]={0,1,7,8,0x7ff8,0xfff8,0xfffe,0xffff};
    unsigned cases=0;
    for(int mode=0;mode<4;mode++){
        config.s16_width=(mode&1)?398:320;config.s16_height=224;config.s16_x_off=(mode&1)?39:0;config.video.hires=mode>=2;
        if(config.video.hires){config.s16_width*=2;config.s16_height*=2;}
        int n=config.s16_width*config.s16_height;
        std::vector<uint16_t>a(n+128),b(n+128);
        for(unsigned flags=0;flags<16;flags++)for(unsigned h:hz)for(unsigned v:vz){
            unsigned c=cases++;memset(initial,0,sizeof initial);
            for(int k=0;k<2;k++){
                unsigned j=c+k*7;uint16_t*r=initial+SPRITE_RAM_SIZE+k*8;
                unsigned pitch=pitches[j%7];
                r[0]=((j%8)<<9)|((tops[j%11]+256)&511);
                r[1]=ads[j%8];r[2]=(pitch&127)<<9;
                r[3]=0x3000|((flags&4)?0x4000:0)|v;
                r[4]=((flags&1)?0:0x4000)|((flags&2)?0x2000:0)|((flags&8)?0x8000:0)|((pitch&128)?0x1000:0)|h;
                r[5]=((heights[j%6]-1)<<8)|(j%128);r[6]=xs[j%12];r[7]=next();
            }
            initial[SPRITE_RAM_SIZE+16]=0x8000;
            if(c%17==0)initial[SPRITE_RAM_SIZE+8]|=0x1000;
            if(c%19==0)initial[SPRITE_RAM_SIZE+11]=0x1000;
            switch(c%4){case 0:x1=0;x2=config.s16_width;break;case 1:x1=16;x2=config.s16_width-16;break;case 2:x1=x2=config.s16_width/2;break;default:x1=config.s16_width/2+10;x2=config.s16_width/2;}
            std::fill(a.begin(),a.end(),0xa55a);std::fill(b.begin(),b.end(),0xa55a);
            std::fill(a.begin()+64,a.end()-64,uint16_t(c&4095));std::fill(b.begin()+64,b.end()-64,uint16_t(c&4095));
            load_render_state(initial);video.pixels=a.data()+64;render(8);save_render_state(a_state);
            load_render_state(initial);video.pixels=b.data()+64;render_reference(8);save_render_state(b_state);
            if(a!=b||memcmp(a_state,b_state,sizeof a_state)){
                fprintf(stderr,"SPRITE EDGE FAIL case=%u mode=%d flags=%u hz=%u vz=%u pixels=%d state=%d\n",c,mode,flags,h,v,a!=b,memcmp(a_state,b_state,sizeof a_state));exit(8);
            }
        }
    }
    memcpy(sprites,saved.data(),sizeof sprites);load_render_state(original);x1=oldx1;x2=oldx2;
    config.s16_width=ow;config.s16_height=oh;config.s16_x_off=oo;config.video.hires=hi;video.pixels=oldpixels;
    printf("SPRITE EDGE PASS cases=%u pixels_and_state=exact\n",cases);
}
// r010 additions: structured sprite data that the random set above rarely produces.
// Long rows without end markers, fully transparent words (0/15 mixes), fully opaque words,
// shadow-heavy words, 16-bit index wrap in the middle of a row, up to six overlapping sprites
// (shadow over shadow), and x positions swept across both clip edges at every offset.
void hwsprites::verify_sprite_extra_cases(){
    std::vector<uint32_t> saved(sprites,sprites+SPRITES_LENGTH);
    uint16_t original[2048],initial[2048],a_state[2048],b_state[2048];save_render_state(original);
    uint16_t oldx1=x1,oldx2=x2;int ow=config.s16_width,oh=config.s16_height,oo=config.s16_x_off,hi=config.video.hires;
    uint16_t* oldpixels=video.pixels;
    uint32_t seed=0x7a3c5e91;auto next=[&](){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;};
    // Nibble classes: transparent (0/15), opaque (1-14), shadow (10).
    auto clear_nib=[&](){return (next()&1)?15u:0u;};
    auto opaque_nib=[&](){return 1u+next()%14u;};
    auto word_of=[&](unsigned kind){
        uint32_t w=0;
        for(int k=0;k<8;k++){
            unsigned n;
            switch(kind){
                case 0:n=clear_nib();break;                                   // all transparent
                case 1:n=opaque_nib();break;                                  // all opaque
                case 2:n=(next()&1)?clear_nib():opaque_nib();break;           // mixed
                case 3:n=(next()%3==0)?10u:((next()&1)?clear_nib():opaque_nib());break; // shadow-heavy
                default:n=(next()&3)?0u:opaque_nib();break;                   // sparse
            }
            w=(w<<4)|n;
        }
        return w;
    };
    // Bank layout: runs of a single word kind so rows see long transparent and opaque stretches.
    // End markers (nibble 15 at either marker position) only where placed below.
    for(unsigned i=0;i<SPRITES_LENGTH;){
        const unsigned kind=next()%5,run=1+next()%24;
        for(unsigned j=0;j<run&&i<SPRITES_LENGTH;j++,i++){
            uint32_t w=word_of(kind);
            w&=~0x0f0000f0u;                                      // clear both marker nibbles...
            if(kind==1||kind==3)w|=0x01000010u;                   // ...keeping opaque words opaque
            sprites[i]=w;
        }
    }
    for(unsigned i=0;i<SPRITES_LENGTH;i+=61+(i%7)){sprites[i]|=(i&1)?0x0f000000u:0x000000f0u;}
    // Last bank near the 16-bit index ends: long rows that wrap 0xffff<->0x0000.
    for(unsigned i=0;i<64;i++){sprites[3*0x10000+0xffff-i]=word_of(i%5)&~0x0f0000f0u;sprites[3*0x10000+i]=word_of((i+2)%5)&~0x0f0000f0u;}
    const unsigned hz[]={0x40,0x41,0x7f,0x80,0x81,0xaa,0xff,0x100,0x101,0x155,0x1ff,0x200,0x201,0x2aa,0x300,0x3ff,0x400,0x401,0x555,0x7fe,0x7ff};
    const unsigned vz[]={0x40,0x80,0x1ff,0x200,0x201,0x3ff,0x7ff};
    const unsigned ads[]={0x0000,0x0001,0x1234,0x7ff0,0xffc0,0xfff0,0xfffe,0xffff};
    unsigned cases=0;
    unsigned long long before[6],after[6];sprite_work_paths(before);
    for(int mode=0;mode<4;mode++){
        config.s16_width=(mode&1)?398:320;config.s16_height=224;config.s16_x_off=(mode&1)?39:0;config.video.hires=mode>=2;
        if(config.video.hires){config.s16_width*=2;config.s16_height*=2;}
        const int width=config.s16_width,n=width*config.s16_height;
        std::vector<uint16_t>a(n+128),b(n+128);
        for(unsigned c=0;c<1536;c++,cases++){
            switch(c%5){
                case 0:x1=0;x2=width;break;
                case 1:x1=16;x2=width-16;break;
                case 2:x1=config.s16_x_off;x2=config.s16_x_off+320;break;
                case 3:x1=width/2-3;x2=width/2+5;break;
                default:x1=width/2+10;x2=width/2;break;
            }
            memset(initial,0,sizeof initial);
            const unsigned sprites_in_case=1+c%6;
            const int edge=(c&8)?x2:x1;                 // sweep x across one clip edge
            for(unsigned k=0;k<sprites_in_case;k++){
                uint16_t* r=initial+SPRITE_RAM_SIZE+k*8;
                const unsigned flags=(c+k*5)&15,h=hz[(c+k*3)%21],v=vz[(c/3+k)%7];
                const bool positive=(flags&2)!=0;
                const bool wrap=(c%23)==k;
                const unsigned bank=wrap?3:((c+k)%7);
                const unsigned addr=wrap?ads[(c/23+k)%8]:(next()&0xffff);
                const unsigned pitch=(c%11==0)?0:1+next()%60;
                // Screen x to sprite x: undo hires doubling and the 0xbe / widescreen offsets.
                int sx=edge+int((c*7+k*13)%41)-20;
                if(config.video.hires)sx/=2;
                const int raw=sx + 0xbe - config.s16_x_off;   // >=0x80 here, so no left-wrap adjust
                const int top=int(c%9)*20-40+int(k)*3;
                const unsigned height=(c%4==0)?256:16+next()%96;
                r[0]=(bank<<9)|((top+256)&511);
                r[1]=addr;r[2]=(pitch&127)<<9;
                r[3]=0x3000|((flags&4)?0x4000:0)|v;
                r[4]=((flags&1)?0:0x4000)|(positive?0x2000:0)|((flags&8)?0x8000:0)|((pitch&128)?0x1000:0)|h;
                r[5]=((height-1)<<8)|(next()%128);r[6]=uint16_t(raw);r[7]=next();
            }
            initial[SPRITE_RAM_SIZE+sprites_in_case*8]=0x8000;
            std::fill(a.begin(),a.end(),0xa55a);std::fill(b.begin(),b.end(),0xa55a);
            // Background with some shadowed pixels already present, so shadow-on-shadow is visible.
            for(int i=0;i<n;i++){uint16_t v=uint16_t((i*37+c)&0xfff);if((i+c)%5==0)v+=S16_PALETTE_ENTRIES;a[64+i]=b[64+i]=v;}
            load_render_state(initial);
            if(!config.video.hires)count_sprite_work();   // path coverage (the counter has no hires model)
            video.pixels=a.data()+64;render(8);save_render_state(a_state);
            load_render_state(initial);video.pixels=b.data()+64;render_reference(8);save_render_state(b_state);
            if(a!=b||memcmp(a_state,b_state,sizeof a_state)){
                fprintf(stderr,"SPRITE EXTRA FAIL case=%u mode=%d pixels=%d state=%d\n",c,mode,a!=b,memcmp(a_state,b_state,sizeof a_state));exit(9);
            }
        }
    }
    memcpy(sprites,saved.data(),sizeof sprites);load_render_state(original);x1=oldx1;x2=oldx2;
    config.s16_width=ow;config.s16_height=oh;config.s16_x_off=oo;config.video.hires=hi;video.pixels=oldpixels;
    sprite_work_paths(after);
    for(int i=0;i<6;i++)if(after[i]==before[i]){fprintf(stderr,"SPRITE EXTRA COVERAGE FAIL path=%d\n",i);exit(9);}
    printf("SPRITE EXTRA PASS cases=%u pixels_and_state=exact words_clear=%llu words_inside=%llu words_edge=%llu words_wrapped=%llu rows_16plus_words=%llu shadow_outputs=%llu\n",
        cases,after[0]-before[0],after[1]-before[1],after[2]-before[2],after[3]-before[3],after[4]-before[4],after[5]-before[5]);
}
#endif
