#include "video.hpp"
#include "hwvideo/hwsprites.hpp"
#include "globals.hpp"
#include "frontend/config.hpp"

/***************************************************************************
    Video Emulation: OutRun Sprite Rendering Hardware.
    Based on MAME source code.

    Copyright Aaron Giles.
    All rights reserved.
***************************************************************************/

/*******************************************************************************************
*  Out Run/X-Board-style sprites
*
*      Offs  Bits               Usage
*       +0   e------- --------  Signify end of sprite list
*       +0   -h-h---- --------  Hide this sprite if either bit is set
*       +0   ----bbb- --------  Sprite bank
*       +0   -------t tttttttt  Top scanline of sprite + 256
*       +2   oooooooo oooooooo  Offset within selected sprite bank
*       +4   ppppppp- --------  Signed 7-bit pitch value between scanlines
*       +4   -------x xxxxxxxx  X position of sprite (position $BE is screen position 0)
*       +6   -s------ --------  Enable shadows
*       +6   --pp---- --------  Sprite priority, relative to tilemaps
*       +6   ------vv vvvvvvvv  Vertical zoom factor (0x200 = full size, 0x100 = half size, 0x300 = 2x size)
*       +8   y------- --------  Render from top-to-bottom (1) or bottom-to-top (0) on screen
*       +8   -f------ --------  Horizontal flip: read the data backwards if set
*       +8   --x----- --------  Render from left-to-right (1) or right-to-left (0) on screen
*       +8   ------hh hhhhhhhh  Horizontal zoom factor (0x200 = full size, 0x100 = half size, 0x300 = 2x size)
*       +E   dddddddd dddddddd  Scratch space for current address
*
*  Out Run only:
*       +A   hhhhhhhh --------  Height in scanlines - 1
*       +A   -------- -ccccccc  Sprite color palette
*
*  X-Board only:
*       +A   ----hhhh hhhhhhhh  Height in scanlines - 1
*       +C   -------- cccccccc  Sprite color palette
*
*  Final bitmap format:
*
*            -s------ --------  Shadow control
*            --pp---- --------  Sprite priority
*            ----cccc cccc----  Sprite color palette
*            -------- ----llll  4-bit pixel data
*
 *******************************************************************************************/

// Enable for hardware pixel accuracy, where sprite shadowing delayed by 1 clock cycle (slower)
#define PIXEL_ACCURACY 0 

hwsprites::hwsprites()
{
}

hwsprites::~hwsprites()
{
}

void hwsprites::init(const uint8_t* src_sprites)
{
    reset();

    if (src_sprites)
    {
        // Convert S16 tiles to a more useable format
        const uint8_t *spr = src_sprites;

        for (uint32_t i = 0; i < SPRITES_LENGTH; i++)
        {
            uint8_t d3 = *spr++;
            uint8_t d2 = *spr++;
            uint8_t d1 = *spr++;
            uint8_t d0 = *spr++;

            sprites[i] = (d0 << 24) | (d1 << 16) | (d2 << 8) | d3;
        }
    }
}

void hwsprites::reset()
{
    // Clear Sprite RAM buffers
    for (uint16_t i = 0; i < SPRITE_RAM_SIZE; i++)
    {
        ram[i] = 0;
        ramBuff[i] = 0;
    }
}

// Clip areas of the screen in wide-screen mode
void hwsprites::set_x_clip(bool on)
{
    // Clip to central 320 width window.
    if (on)
    {
        x1 = config.s16_x_off;
        x2 = x1 + S16_WIDTH;

        if (config.video.hires)
        {
            x1 <<= 1;
            x2 <<= 1;
        }
    }
    // Allow full wide-screen.
    else
    {
        x1 = 0;
        x2 = config.s16_width;
    }
}

uint8_t hwsprites::read(const uint16_t adr)
{
    uint16_t a = adr >> 1;
    if ((adr & 1) == 1)
        return ram[a] & 0xff;
    else
        return ram[a] >> 8;
}

void hwsprites::write(const uint16_t adr, const uint16_t data)
{
    ram[adr >> 1] = data;
}

// Copy back buffer to main ram, ready for blit
void hwsprites::swap()
{
    // swap the halves of the sprite RAM (Cannonball for x68: by their pointers)
    uint16_t* temp = ram;
    ram = ramBuff;
    ramBuff = temp;
}

#include "../../../sprite_fast.hpp"

void hwsprites::render(const uint8_t priority)
{
    const int width=config.s16_width,screen_height=config.s16_height;
    const int clip_left=x1,clip_right=x2;
    const unsigned clip_span=clip_right>clip_left?unsigned(clip_right-clip_left):0;
    const int x_offset=config.s16_x_off;
    const bool hires=config.video.hires!=0;
    uint16_t* const frame=video.pixels;
    for(unsigned data=0;data<SPRITE_RAM_SIZE;data+=8){
        const uint16_t control=ramBuff[data];
        if(control&0x8000)break;
        const uint16_t zoom_flags=ramBuff[data+3];
        if((1U<<((zoom_flags>>12)&3))!=priority)continue;
        if(control&0x5000)continue;
        const uint16_t flags=ramBuff[data+4];
        SpriteFastParams p;
        p.width=width;p.screen_height=screen_height;p.clip_left=clip_left;p.clip_span=clip_span;p.clip_right=clip_right;
        p.address=ramBuff[data+1];p.pitch=((ramBuff[data+2]>>1)|((flags&0x1000)<<3))>>8;
        p.top=(control&0x1ff)-0x100;p.rows=(ramBuff[data+5]>>8)+1;
        p.y_direction=(flags&0x8000)?1:-1;
        p.hzoom=flags&0x7ff;p.vzoom=zoom_flags&0x7ff;
        if(p.hzoom<0x40)p.hzoom=0x40;if(p.vzoom<0x40)p.vzoom=0x40;
        const bool flip=(flags&0x4000)==0;
        const bool positive=(flags&0x2000)!=0;
        const bool shadow=(zoom_flags&0x4000)!=0;
        p.x=ramBuff[data+6];if(p.x<0x80&&!positive)p.x+=0x200;p.x-=0xbe;p.x+=x_offset;
        if(hires){p.x*=2;p.top*=2;p.rows*=2;p.hzoom>>=1;p.vzoom>>=1;}
        p.color=COLOR_BASE+((ramBuff[data+5]&0x7f)<<4);
        const unsigned bank=((control>>9)&7)%(SPRITES_LENGTH/0x10000);
        ramBuff[data+7]=sprite_fast_dispatch(sprites+bank*0x10000,frame,p,flip,positive,shadow);
    }
}

// Capture one converted entry without mutating either sprite RAM buffer.
void hwsprites::capture_entry(const uint16_t* r,uint16_t* dst){
 if(r[0]&0xd000)return;
 SpriteFastParams p;
 p.width=320;p.screen_height=224;p.clip_left=0;p.clip_right=320;p.clip_span=320;
 const unsigned flags=r[4];p.address=r[1];p.pitch=((r[2]>>1)|((flags&0x1000)<<3))>>8;
 p.top=(r[0]&511)-256;p.rows=(r[5]>>8)+1;p.y_direction=(flags&0x8000)?1:-1;
 p.hzoom=flags&2047;p.vzoom=r[3]&2047;if(p.hzoom<64)p.hzoom=64;if(p.vzoom<64)p.vzoom=64;
 bool positive=(flags&0x2000)!=0;p.x=r[6];if(p.x<128&&!positive)p.x+=512;p.x-=190;
 p.color=COLOR_BASE+((r[5]&127)<<4);
 unsigned bank=((r[0]>>9)&7)%(SPRITES_LENGTH/65536);
 sprite_fast_dispatch(sprites+bank*65536,dst,p,(flags&0x4000)==0,positive,(r[3]&0x4000)!=0);
}
// r026e: the same with the caller's nibble table (values written as given, 0 = transparent)
void hwsprites::capture_entry_table(const uint16_t* r,uint16_t* dst,const uint16_t* table,int* xbounds){
 if(r[0]&0xd000)return;
 SpriteFastParams p;
 p.width=320;p.screen_height=224;p.clip_left=0;p.clip_right=320;p.clip_span=320;
 const unsigned flags=r[4];p.address=r[1];p.pitch=((r[2]>>1)|((flags&0x1000)<<3))>>8;
 p.top=(r[0]&511)-256;p.rows=(r[5]>>8)+1;p.y_direction=(flags&0x8000)?1:-1;
 p.hzoom=flags&2047;p.vzoom=r[3]&2047;if(p.hzoom<64)p.hzoom=64;if(p.vzoom<64)p.vzoom=64;
 bool positive=(flags&0x2000)!=0;p.x=r[6];if(p.x<128&&!positive)p.x+=512;p.x-=190;
 p.color=COLOR_BASE+((r[5]&127)<<4);p.custom=table;p.xbounds=xbounds;
 unsigned bank=((r[0]>>9)&7)%(SPRITES_LENGTH/65536);
  // the caller's table has no shadow entry (0x8000), so the kernels without the shadow test draw the same pixels
 sprite_fast_dispatch(sprites+bank*65536,dst,p,(flags&0x4000)==0,positive,false);
}
// The same with 8-bit pixels (the table's codes are < 256); no shadow kernels.
void hwsprites::capture_entry_table8(const uint16_t* r,uint8_t* dst,const uint16_t* table,int* xbounds,uint32_t row_mask){
 if(r[0]&0xd000)return;
 SpriteFastParams p;
 p.width=320;p.screen_height=224;p.clip_left=0;p.clip_right=320;p.clip_span=320;
 const unsigned flags=r[4];p.address=r[1];p.pitch=((r[2]>>1)|((flags&0x1000)<<3))>>8;
 p.top=(r[0]&511)-256;p.rows=(r[5]>>8)+1;p.y_direction=(flags&0x8000)?1:-1;
 p.hzoom=flags&2047;p.vzoom=r[3]&2047;if(p.hzoom<64)p.hzoom=64;if(p.vzoom<64)p.vzoom=64;
 bool positive=(flags&0x2000)!=0;p.x=r[6];if(p.x<128&&!positive)p.x+=512;p.x-=190;
 p.color=COLOR_BASE+((r[5]&127)<<4);p.custom=table;p.xbounds=xbounds;p.row_mask=row_mask;
 unsigned bank=((r[0]>>9)&7)%(SPRITES_LENGTH/65536);
 const uint32_t* src=sprites+bank*65536;
 if((flags&0x4000)==0){if(positive)sprite_scale<uint8_t,true,1,false>(src,dst,p);else sprite_scale<uint8_t,true,-1,false>(src,dst,p);}
 else{if(positive)sprite_scale<uint8_t,false,1,false>(src,dst,p);else sprite_scale<uint8_t,false,-1,false>(src,dst,p);}
}
