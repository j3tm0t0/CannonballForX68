/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
#pragma once
// Exact sprite kernels (same pixels as the original renderer).
//
// Every row restarts the horizontal accumulator at 0, so output pixel j of a row samples
// source pixel floor(j*hz/512).  With acc kept in [0,hz) at word boundaries, one 8-pixel
// word therefore yields exactly q or q+1 outputs, q=4096/hz, r=4096-q*hz:
//     acc-=r; n=q; if(acc<0){acc+=hz; n++;}
// Fully transparent words and words touching the clip edge advance with that O(1) step;
// only visible, unclipped words run the per-pixel loop.  No per-row tables are built.
// The pixel loop keeps the destination as a pointer and reads colours from a 16-entry
// table indexed by nibble*2, so 0/15 (transparent) and 10 (shadow) need one test each.
#if PIXEL_ACCURACY
#error "sprite_fast.hpp implements PIXEL_ACCURACY 0 only"
#endif
struct SpriteFastParams {
    int width,screen_height,clip_left,clip_right,x,top,rows,y_direction;
    unsigned clip_span;
    uint16_t address,pitch,hzoom,vzoom,color;
    const uint16_t* custom=nullptr;   // r026e: own 16-entry nibble table (roadside objects)
    int* xbounds=nullptr;             // if set: {min,max} screen x that any drawn row may have touched (widened, never narrowed)
};
#if defined(__GNUC__)
#define SPRITE_INLINE static inline __attribute__((always_inline))
#define SPRITE_NOINLINE static __attribute__((noinline))
#else
#define SPRITE_INLINE static inline
#define SPRITE_NOINLINE static
#endif
// Plain C rotates; GCC emits rol.l/ror.l for these on the 68000, and the host runs the same code.
SPRITE_INLINE uint32_t sprite_rol(uint32_t w,unsigned n){return (w<<n)|(w>>(32-n));}
SPRITE_INLINE uint32_t sprite_ror(uint32_t w,unsigned n){return (w>>n)|(w<<(32-n));}
// Empty on purpose: keeps GCC from re-deriving each rotate from the unrotated word with a
// longer shift (68000 shift time grows with the count).  Emits no instruction.
#ifdef OR_X68K
#define SPRITE_KEEP(v) __asm__("" : "+d"(v))
#else
#define SPRITE_KEEP(v) ((void)0)
#endif
// Colour table entry for (nibble*2): 0 = transparent, 0x8000 = shadow, else colour|nibble.
SPRITE_INLINE uint16_t sprite_entry(const uint16_t* table,uint32_t t){
    return *(const uint16_t*)((const char*)table+(uint16_t(t)&0x1e));
}
// One source pixel.  acc enters in [0,hz); "acc-=0x200 first" lets the sign carry the test.
template<int DX,bool Shadow,bool Minify,bool Clip>
SPRITE_INLINE void sprite_pixel(uint16_t*& dst,short& acc,short hz,short unit,uint16_t value,const uint16_t* lo,unsigned span){
    acc-=unit;   // unit is 0x200, passed in so the word loop can hold it in a register
    if(Minify){
        if(acc>=0)return;
        uint16_t* at=DX>0?dst:dst-1;
        if(value&&(!Clip||unsigned(at-lo)<span)){
            if(Shadow&&short(value)<0)*at=(*at&0xfff)+S16_PALETTE_ENTRIES;
            else *at=value;
        }
        dst+=DX;acc+=hz;
    }else if(!value){
        do{dst+=DX;acc+=hz;}while(acc<0);
    }else if(Shadow&&short(value)<0){
        do{uint16_t* at=DX>0?dst:dst-1;if(!Clip||unsigned(at-lo)<span)*at=(*at&0xfff)+S16_PALETTE_ENTRIES;dst+=DX;acc+=hz;}while(acc<0);
    }else if(Clip){
        do{uint16_t* at=DX>0?dst:dst-1;if(unsigned(at-lo)<span)*at=value;dst+=DX;acc+=hz;}while(acc<0);
    }else if(DX>0){
        do{*dst++=value;acc+=hz;}while(acc<0);
    }else{
        do{*--dst=value;acc+=hz;}while(acc<0);
    }
}
// Eight pixels of one word.  Non-flipped words are read from the top nibble down, flipped
// words from the bottom nibble up; t is pre-rotated so the current nibble*2 sits in bits 1-4.
// Returns the rotated word: non-flip ends rotated left by 1, flip ends rotated left by 5.
template<bool Flip,int DX,bool Shadow,bool Minify,bool Clip>
SPRITE_INLINE uint32_t sprite_pixels(uint32_t w,uint16_t*& dst,short& acc,short hz,short unit,const uint16_t* table,const uint16_t* lo,unsigned span){
    uint32_t t=Flip?sprite_rol(w,1):sprite_rol(w,5);
    SPRITE_KEEP(t);
#define SPRITE_NEXT_PIXEL() t=Flip?sprite_ror(t,4):sprite_rol(t,4);SPRITE_KEEP(t);sprite_pixel<DX,Shadow,Minify,Clip>(dst,acc,hz,unit,sprite_entry(table,t),lo,span)
    sprite_pixel<DX,Shadow,Minify,Clip>(dst,acc,hz,unit,sprite_entry(table,t),lo,span);
    SPRITE_NEXT_PIXEL();SPRITE_NEXT_PIXEL();SPRITE_NEXT_PIXEL();SPRITE_NEXT_PIXEL();
    SPRITE_NEXT_PIXEL();SPRITE_NEXT_PIXEL();SPRITE_NEXT_PIXEL();
#undef SPRITE_NEXT_PIXEL
    return t;
}
// Rare path: word overlapping the clip edge.  Only writes; the caller advances dst/acc.
template<bool Flip,int DX,bool Shadow,bool Minify>
SPRITE_NOINLINE void sprite_word_clipped(uint32_t w,uint16_t* dst,short acc,short hz,const uint16_t* table,const uint16_t* lo,unsigned span){
    sprite_pixels<Flip,DX,Shadow,Minify,true>(w,dst,acc,hz,0x200,table,lo,span);
}
struct SpriteRowSetup {
    short hz,r;
    int q;                     // whole outputs per word
    int stop,fast_lo,fast_hi;  // offsets from the row start
    const uint16_t* table;
    unsigned span;
    int left;
};
// Every nibble 0 or 15 <=> the bits of each nibble are equal.
SPRITE_INLINE bool sprite_word_clear(uint32_t w){return ((w^(w<<1))&0xeeeeeeeeu)==0;}
// Rows whose 16-bit word index could wrap: index arithmetic as in the reference loop.
template<bool Flip,int DX,bool Shadow,bool Minify>
SPRITE_NOINLINE uint16_t sprite_row_wrapped(const uint32_t* bank,uint16_t* row,uint16_t address,int start_x,const SpriteRowSetup& s){
    uint16_t scratch=uint16_t(address+(Flip?1:-1));
    uint16_t* dst=row+start_x+(DX>0?0:1);
    uint16_t* const stop=row+s.stop;
    const uint16_t* const lo=row+s.left;
    short acc=0;
    while(DX>0?dst<stop:dst>=stop){
        scratch=uint16_t(scratch+(Flip?-1:1));
        const uint32_t w=bank[scratch];
        if(!sprite_word_clear(w))sprite_word_clipped<Flip,DX,Shadow,Minify>(w,dst,acc,s.hz,s.table,lo,s.span);
        acc-=s.r;dst+=DX*s.q;if(acc<0){acc+=s.hz;dst+=DX;}
        if((w&(Flip?0x0f000000u:0x000000f0u))==(Flip?0x0f000000u:0x000000f0u))break;
    }
    return scratch;
}
// End marker: the second-to-last nibble of the word, as in the reference loop.
// Tested on 16-bit halves (word-size 68000 compares; same result).
template<bool Flip>
SPRITE_INLINE bool sprite_word_end(uint32_t w){
    if(Flip)return (uint16_t(w>>16)&0x0f00)==0x0f00;
    return (uint16_t(w)&0x00f0)==0x00f0;
}
// A word that is transparent or touches the clip edge: draw clipped, then take the O(1) step.
template<bool Flip,int DX,bool Shadow,bool Minify>
SPRITE_INLINE bool sprite_edge_word(const uint32_t*& src,uint16_t*& dst,short& acc,short hz,short r,int q,const uint16_t* table,uint16_t* row,const SpriteRowSetup& s){
    const uint32_t w=Flip?*--src:*src++;
    if(!sprite_word_clear(w))sprite_word_clipped<Flip,DX,Shadow,Minify>(w,dst,acc,hz,table,row+s.left,s.span);
    acc-=r;dst+=DX*q;if(acc<0){acc+=hz;dst+=DX;}
    return sprite_word_end<Flip>(w);
}
// One row without index wrap, in three phases along the drawing direction:
// words before the clip window, words whose every output is inside it, words after it.
// Only the middle phase draws without per-pixel clipping, and it needs one bound test per word
// (its bound also keeps it on screen).  Kept out of line so the word loop gets the registers.
template<bool Flip,int DX,bool Shadow,bool Minify>
SPRITE_NOINLINE const uint32_t* sprite_row_words(const uint32_t* src,uint16_t* row,uint16_t* dst,const SpriteRowSetup& s){
    const uint16_t* const table=s.table;
    const short hz=s.hz,r=s.r;
    const int q=s.q;
    short acc=0;
    short unit=0x200;SPRITE_KEEP(unit);
    uint16_t* const stop=row+s.stop;
    if(DX>0){
        uint16_t* const fast_lo=row+s.fast_lo;
        while(dst<fast_lo){
            if(dst>=stop)return src;
            if(sprite_edge_word<Flip,DX,Shadow,Minify>(src,dst,acc,hz,r,q,table,row,s))return src;
        }
        uint16_t* const fast_hi=row+s.fast_hi;
        while(dst<=fast_hi){
            const uint32_t w=Flip?*--src:*src++;
            if(sprite_word_clear(w)){
                acc-=r;dst+=q;if(acc<0){acc+=hz;dst++;}
                if(sprite_word_end<Flip>(w))return src;
                continue;
            }
            // t is w rotated left by 1 (non-flip) or 5 (flip); test the end marker there.
            const uint32_t t=sprite_pixels<Flip,DX,Shadow,Minify,false>(w,dst,acc,hz,unit,table,0,0);
            if(Flip?(t&0xe0000001u)==0xe0000001u:(uint16_t(t)&0x01e0)==0x01e0)return src;
        }
        while(dst<stop){
            if(sprite_edge_word<Flip,DX,Shadow,Minify>(src,dst,acc,hz,r,q,table,row,s))return src;
        }
    }else{
        uint16_t* const fast_hi=row+s.fast_hi;
        while(dst>fast_hi){
            if(dst<stop)return src;
            if(sprite_edge_word<Flip,DX,Shadow,Minify>(src,dst,acc,hz,r,q,table,row,s))return src;
        }
        uint16_t* const fast_lo=row+s.fast_lo;
        while(dst>=fast_lo){
            const uint32_t w=Flip?*--src:*src++;
            if(sprite_word_clear(w)){
                acc-=r;dst-=q;if(acc<0){acc+=hz;dst--;}
                if(sprite_word_end<Flip>(w))return src;
                continue;
            }
            const uint32_t t=sprite_pixels<Flip,DX,Shadow,Minify,false>(w,dst,acc,hz,unit,table,0,0);
            if(Flip?(t&0xe0000001u)==0xe0000001u:(uint16_t(t)&0x01e0)==0x01e0)return src;
        }
        while(dst>=stop){
            if(sprite_edge_word<Flip,DX,Shadow,Minify>(src,dst,acc,hz,r,q,table,row,s))return src;
        }
    }
    return src;
}
template<bool Flip,int DX,bool Shadow,bool Minify>
static uint16_t sprite_visible(const uint32_t* bank,uint16_t* frame,const SpriteFastParams& p){
    const int width=p.width,screen_height=p.screen_height;
    const int top=p.top,count=p.rows,dy=p.y_direction,start_x=p.x;
    int first=dy>0?-top:top-screen_height+1;
    int last=dy>0?screen_height-top:top+1;
    if(first<0)first=0;if(last>count)last=count;
    uint32_t address=p.address;
    uint16_t scratch=p.address;
    if(first>=last)return scratch;
    const uint16_t pitch=p.pitch,vz=p.vzoom;
    unsigned yacc=0;
    if(first){uint32_t skip=uint16_t(first)*uint16_t(vz);address+=(skip>>9)*pitch;yacc=skip&0x1ff;}
    const uint16_t advance[5]={0,pitch,uint16_t(pitch*2),uint16_t(pitch*3),uint16_t(pitch*4)};
    SpriteRowSetup s;
    uint16_t table[16];
    if(p.custom)for(unsigned k=0;k<16;k++)table[k]=p.custom[k];
    else for(unsigned k=0;k<16;k++)table[k]=(k==0||k==15)?0:(Shadow&&k==10)?0x8000:uint16_t(p.color|k);
    s.table=table;
    s.hz=short(p.hzoom);
    s.q=4096/p.hzoom;
    s.r=short(4096-s.q*p.hzoom);
    const int most=s.r?s.q+1:s.q;     // outputs a single word can produce
    s.left=p.clip_left;s.span=p.clip_span;
    // DX>0: dst=&row[x], window [fast_lo,fast_hi] also stays below the screen width.
    // DX<0: dst=&row[x+1], fast_lo=left+most>=1 also keeps x>=0.
    if(DX>0){s.stop=width;s.fast_lo=p.clip_left;s.fast_hi=(p.clip_right<width?p.clip_right:width)-most;}
    else{s.stop=1;s.fast_lo=p.clip_left+most;s.fast_hi=p.clip_right;}
    // Upper bound on words a row can read before leaving the screen (each word moves >= q).
    const unsigned reach=DX>0?(start_x<width?unsigned(width-start_x):0u):(start_x>=0?unsigned(start_x+1):0u);
    const unsigned words_max=reach/unsigned(s.q)+1;
    int row_index=(top+(dy>0?first:-first))*width;
    const int row_step=dy>0?width:-width;
    unsigned words_seen=0;   // most words one row read (each word moves dst by q or q+1 pixels)
    for(int y=first;y<last;y++){
        uint16_t* const row=frame+row_index;
        const uint16_t index=uint16_t(address);
        const bool wraps=Flip?unsigned(index)+1<words_max:unsigned(index)+words_max>0x10000u;
        if(wraps){
            scratch=sprite_row_wrapped<Flip,DX,Shadow,Minify>(bank,row,index,start_x,s);
        }else{
            const uint32_t* src=sprite_row_words<Flip,DX,Shadow,Minify>(bank+index+(Flip?1:0),row,row+start_x+(DX>0?0:1),s);
            scratch=uint16_t(src-bank-(Flip?0:1));
        }
        const unsigned n=uint16_t(Flip?index-scratch:scratch-index)+1u;
        if(n>words_seen)words_seen=n;
        yacc+=vz;address+=advance[yacc>>9];yacc&=0x1ff;row_index+=row_step;
    }
    if(p.xbounds){
        const int reach_px=int(words_seen)*most+1;
        const int lo=DX>0?start_x-1:start_x-reach_px,hi=DX>0?start_x+reach_px:start_x+1;
        if(lo<p.xbounds[0])p.xbounds[0]=lo;
        if(hi>p.xbounds[1])p.xbounds[1]=hi;
    }
    return scratch;
}
template<bool Flip,int DX,bool Shadow>
static uint16_t sprite_scale(const uint32_t* src,uint16_t* dst,const SpriteFastParams& p){
    return p.hzoom>=0x200?sprite_visible<Flip,DX,Shadow,true>(src,dst,p):sprite_visible<Flip,DX,Shadow,false>(src,dst,p);
}
template<bool Flip,int DX>
static uint16_t sprite_shadow(const uint32_t* src,uint16_t* dst,const SpriteFastParams& p,bool shadow){
    return shadow?sprite_scale<Flip,DX,true>(src,dst,p):sprite_scale<Flip,DX,false>(src,dst,p);
}
static uint16_t sprite_fast_dispatch(const uint32_t* src,uint16_t* dst,const SpriteFastParams& p,bool flip,bool positive,bool shadow){
    if(flip)return positive?sprite_shadow<true,1>(src,dst,p,shadow):sprite_shadow<true,-1>(src,dst,p,shadow);
    return positive?sprite_shadow<false,1>(src,dst,p,shadow):sprite_shadow<false,-1>(src,dst,p,shadow);
}
#undef SPRITE_INLINE
#undef SPRITE_NOINLINE
#undef SPRITE_KEEP
