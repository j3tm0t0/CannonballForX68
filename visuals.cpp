/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
#include "visuals.hpp"
#include "video.hpp"
#include "engine/osprites.hpp"
#include "engine/oferrari.hpp"
#include "engine/oroad.hpp"
#include "engine/otiles.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "report_pager.hpp"
#include <algorithm>
BackgroundAssets background_assets;
unsigned visual_mode=0;
bool hud_enabled=true;   // r027: H key, score/time/speed on the text plane
uint32_t background_columns=0,car_overflows=0,car_tiles_total=0,car_frames=0,car_text_frames=0,visual_frames[4]={0,0,0,0};
unsigned car_peak_tiles=0,car_peak_patterns=0,car_peak_line=0;
typedef uint32_t __attribute__((may_alias)) alias_u32;
alignas(4) static uint16_t capture_pixels[320*224];
static const uint16_t* capture_palette=0;
static const uint16_t* background_palette=0;
static volatile uint16_t text_active_bank=0;
static uint8_t text_color_lookup[8192];
static int old_low=224,old_high=0;
static int near_target=0,far_target=0,near_loaded=0,far_loaded=0;
static bool near_rows[512];
static int composed_dx=0,composed_dy=0;
static uint32_t background_recomposed_rows=0;
static uint32_t background_far_checks=0,background_data_errors=0,background_palette_errors=0;
static bool background_ready=false,background_attempted=false,background_refresh=false,background_palette_refresh=false;
static uint32_t background_rebuilds=0,background_capacity_waits=0,background_source_frames=0;
#ifndef OR_X68K
static uint8_t background_ring[512*512];
#endif
static int mod(int v,int n){v%=n;return v<0?v+n:v;}
static uint16_t tile_word(unsigned a){const uint8_t*p=video.tile_layer->text_ram;return (p[a]<<8)|p[a+1];}
// A visible black pixel must not use the hardware transparent colour word.
static uint16_t opaque_colour(uint16_t c){return c?c:1;}
bool visuals_load(){return true;}
// ---------------------------------------------------------------------------
// r024 background: per-page caches instead of a full re-decode on every page switch.
// Each of the 16 tile-RAM pages is decoded once into page_cache (code = local colour
// base<<4 | pixel, 0 = transparent). A layer is four quadrant pages; a 256-entry table per
// quadrant turns a code into the GVRAM value (far 1..14, near 17..31). Page switches only
// rebuild these tables; a page is decoded again only after tile RAM in that page was
// written. Slots keep their previous colour where possible, so unchanged quadrants keep
// their GVRAM values and only changed areas are rewritten.
// ---------------------------------------------------------------------------
extern uint32_t video_tile_page_writes[16];
struct PageMeta{bool overflow;uint8_t nbase;uint8_t base[16];uint16_t used[16];uint8_t rowany[256];uint16_t rmin[256],rmax[256];};
struct PageCache{bool valid;uint32_t writes;PageMeta m;uint8_t*px;};
static uint8_t page_store[17][256*512];        // 16 pages + 1 buffer for the page being decoded
static PageCache page_cache[16];
static bool page_updated[16];
static unsigned long bg_pending_frames=0;
struct BgLayer{bool valid,ok;uint16_t sel;uint8_t page[4];uint32_t writes[4];uint8_t lut[4][256];uint8_t slot_of[1040];uint16_t colour[16];uint16_t used_src[256];uint8_t used_slot[256];unsigned used_count;};
static BgLayer bg_layer[2];
static uint32_t page_decodes=0,page_decode_fine=0,layer_updates=0,layer_remaps=0,dirty_refreshes=0;
uint32_t background_full_compares=0,background_compare_skips=0;   // kept for the report line
#ifdef OR_X68K
uint32_t platform_fine_clock();
#endif
static uint32_t rebuild_fine_total=0,rebuild_fine_max=0,full_stream_fine_total=0,full_stream_fine_max=0,full_streams=0;
static uint32_t commit_counts_total=0,commit_counts_max=0,commit_calls=0;
// r024d: pages are decoded by one job, a few tile rows per frame, into a spare buffer.
// The finished page replaces the old one in a single step, so a page that the game
// rewrites keeps showing its previous picture until the new one is complete (no stall).
struct PageJob{bool active;unsigned p,ty;uint32_t writes;PageMeta m;uint8_t*buf;};
static PageJob page_job;static uint8_t*page_spare=0;static bool page_store_init=false;
static void page_store_setup(){if(page_store_init)return;for(unsigned p=0;p<16;p++){page_cache[p].px=page_store[p];page_cache[p].valid=false;}page_spare=page_store[16];page_store_init=true;}
static bool page_stale(unsigned p){return !page_cache[p].valid||page_cache[p].writes!=video_tile_page_writes[p];}
static bool page_in_use(unsigned p){for(unsigned l=0;l<2;l++)for(unsigned q=0;q<4;q++)if(bg_layer[l].page[q]==p&&(bg_layer[l].valid))return true;return false;}
static void page_job_step(unsigned rows){
 page_store_setup();
 if(page_job.active&&page_job.writes!=video_tile_page_writes[page_job.p])page_job.active=false;   // rewritten meanwhile: restart
 if(!page_job.active){
  int pick=-1;
  for(unsigned p=0;p<16&&pick<0;p++)if(page_stale(p)&&page_in_use(p))pick=p;
  for(unsigned p=0;p<16&&pick<0;p++)if(page_stale(p))pick=p;
  if(pick<0)return;
  page_job.active=true;page_job.p=pick;page_job.ty=0;page_job.writes=video_tile_page_writes[pick];page_job.buf=page_spare;
  memset(&page_job.m,0,sizeof page_job.m);for(unsigned r=0;r<256;r++){page_job.m.rmin[r]=511;page_job.m.rmax[r]=0;}
 }
#ifdef OR_X68K
 uint32_t t=platform_fine_clock();
#endif
 PageMeta&c=page_job.m;const unsigned p=page_job.p;const uint16_t sel=p*0x1111;
 for(unsigned end=page_job.ty+rows>32?32:page_job.ty+rows;page_job.ty<end;page_job.ty++){unsigned ty=page_job.ty;
  for(unsigned tx=0;tx<64;tx++)for(unsigned yy=0;yy<8;yy++){
   uint16_t colour=0;uint32_t word=video.tile_layer->background_row(sel,tx,ty,yy,colour);
   uint8_t*out=page_job.buf+(ty*8+yy)*512+tx*8;
   if(!word){out[0]=out[1]=out[2]=out[3]=out[4]=out[5]=out[6]=out[7]=0;continue;}
   unsigned idx=colour>>3,local=0;
   for(;local<c.nbase;local++)if(c.base[local]==idx)break;
   if(local==c.nbase){if(c.nbase==16){c.overflow=true;local=0;}else c.base[c.nbase++]=idx;}
   for(unsigned xx=0;xx<8;xx++){
    unsigned pixel=(word>>(28-xx*4))&15;
    out[xx]=pixel?((local<<4)|pixel):0;
    if(pixel){c.used[local]|=1<<pixel;c.rowany[ty*8+yy]=1;unsigned px_=tx*8+xx;if(px_<c.rmin[ty*8+yy])c.rmin[ty*8+yy]=px_;if(px_>c.rmax[ty*8+yy])c.rmax[ty*8+yy]=px_;}
   }
  }
 }
 if(page_job.ty>=32){
  PageCache&pc=page_cache[p];page_spare=pc.px;pc.px=page_job.buf;pc.m=page_job.m;pc.valid=true;pc.writes=page_job.writes;
  page_job.active=false;page_updated[p]=true;++page_decodes;
 }
#ifdef OR_X68K
 page_decode_fine+=platform_fine_clock()-t;
#endif
}
static void page_idle_step(){
 bool first=!bg_layer[0].ok||!bg_layer[1].ok;     // nothing shown yet: decode faster
 page_job_step(first?8:2);
}
// (Re)assign colour slots for a layer. Sources keep their previous slot when the colour still
// fits; returns false when the layer needs more colours than its slots.
static bool layer_remap(unsigned l,const uint16_t*pal){
 BgLayer&L=bg_layer[l];const unsigned limit=l?14:15;
 static uint8_t new_slot[1040];memset(new_slot,255,sizeof new_slot);
 uint16_t col[16]={0};bool claimed[16]={false};bool ok=true;
 uint16_t src_list[256];unsigned n=0;
 for(unsigned q=0;q<4;q++){const PageCache&pc=page_cache[L.page[q]];const PageMeta&c=pc.m;if(c.overflow||!pc.valid)ok=false;
  for(unsigned b=0;b<c.nbase;b++)for(unsigned px=1;px<16;px++)if(c.used[b]&(1<<px)){
   unsigned s=c.base[b]*8+px;if(s>=1040)continue;
   bool dup=false;for(unsigned k=0;k<n;k++)if(src_list[k]==s){dup=true;break;}
   if(!dup){if(n<256)src_list[n++]=s;else ok=false;}
  }}
 // pass 1: keep previous slots
 for(unsigned k=0;k<n;k++){unsigned s=src_list[k],o=L.valid?L.slot_of[s]:255;
  if(o!=255&&o>=1&&o<=limit&&(!claimed[o]||col[o]==pal[s])){claimed[o]=true;col[o]=pal[s];new_slot[s]=o;}}
 // pass 2: same colour or a free slot
 for(unsigned k=0;k<n;k++){unsigned s=src_list[k];if(new_slot[s]!=255)continue;
  unsigned o=0;for(unsigned t=1;t<=limit;t++)if(claimed[t]&&col[t]==pal[s]){o=t;break;}
  if(!o)for(unsigned t=1;t<=limit;t++)if(!claimed[t]){o=t;claimed[t]=true;col[t]=pal[s];break;}
  if(!o){ok=false;o=0;}
  new_slot[s]=o;}
 memset(L.slot_of,255,sizeof L.slot_of);L.used_count=0;
 for(unsigned k=0;k<n;k++){unsigned s=src_list[k];L.slot_of[s]=new_slot[s];L.used_src[L.used_count]=s;L.used_slot[L.used_count++]=new_slot[s];}
 for(unsigned t=0;t<16;t++)L.colour[t]=claimed[t]?col[t]:0;
 for(unsigned q=0;q<4;q++){const PageMeta&c=page_cache[L.page[q]].m;uint8_t*lut=L.lut[q];memset(lut,0,256);
  for(unsigned b=0;b<c.nbase;b++)for(unsigned px=1;px<16;px++){unsigned s=c.base[b]*8+px;unsigned v=s<1040&&L.slot_of[s]!=255?L.slot_of[s]:0;lut[(b<<4)|px]=v?(l?v:16+v):0;}}
 ++layer_remaps;
 L.ok=ok;
 return ok;
}
static void background_palette_from_layers(){
 memset(background_assets.palette,0,sizeof background_assets.palette);
 for(unsigned t=1;t<16;t++)background_assets.palette[16+t]=bg_layer[0].colour[t];
 for(unsigned t=1;t<15;t++)background_assets.palette[t]=bg_layer[1].colour[t];
}
// r025: column refresh bookkeeping per layer ring (ring column = layer world & 511).
struct BgRing{int loaded;unsigned dirty_count;bool pass_active,dirty_all,dirty_half[2];
 bool column_dirty[512],pass_cols[512],row_valid[512],row_shown[512],visible_done[512];};
static BgRing bg_ring[2];
static bool background_prepare(const uint16_t*pal){
 bool any_change=false,ok=true;uint8_t old_lut[2][4][256];bool affected[2][4]={{false}};
 for(unsigned l=0;l<2;l++){
  BgLayer&L=bg_layer[l];unsigned sel=tile_word(0xe80+l*2);
  page_store_setup();
  bool changed=!L.valid||sel!=L.sel;
  for(unsigned q=0;q<4&&!changed;q++)if(page_updated[L.page[q]])changed=true;
  if(changed){
   ++layer_updates;
#ifdef OR_X68K
   uint32_t t=platform_fine_clock();
#endif
   memcpy(old_lut[l],L.lut,sizeof L.lut);
   uint8_t old_page[4];memcpy(old_page,L.page,4);bool was=L.valid;
   for(unsigned q=0;q<4;q++){unsigned p=(sel>>(q*4))&15;L.page[q]=p;if(!was||old_page[q]!=p||page_updated[p])affected[l][q]=true;}
   L.sel=sel;
   ok=layer_remap(l,pal)&&ok;L.valid=true;
   for(unsigned q=0;q<4;q++)if(memcmp(old_lut[l][q],L.lut[q],256))affected[l][q]=true;
   any_change=true;
#ifdef OR_X68K
   uint32_t d=platform_fine_clock()-t;rebuild_fine_total+=d;if(d>rebuild_fine_max)rebuild_fine_max=d;
#endif
  }else ++background_compare_skips;
 }
 if(!any_change){
  // palette changes: keep slots if every slot still has one colour, else remap
  for(unsigned l=0;l<2&&ok;l++){BgLayer&L=bg_layer[l];uint16_t col[16];bool seen[16]={false};bool conflict=false,diff=false;
   for(unsigned k=0;k<L.used_count;k++){unsigned t=L.used_slot[k],s=L.used_src[k];if(!t)continue;
    if(seen[t]&&col[t]!=pal[s]){conflict=true;break;}seen[t]=true;col[t]=pal[s];if(col[t]!=L.colour[t])diff=true;}
   if(conflict){memcpy(old_lut[l],L.lut,sizeof L.lut);ok=layer_remap(l,pal)&&ok;for(unsigned q=0;q<4;q++)if(memcmp(old_lut[l][q],L.lut[q],256))affected[l][q]=true;any_change=true;}
   else if(diff){for(unsigned t=1;t<16;t++)if(seen[t])L.colour[t]=col[t];background_palette_from_layers();background_palette_refresh=true;}
  }
 }
 if(any_change){
  background_palette_from_layers();background_palette_refresh=true;
  if(!background_attempted){background_refresh=true;}
  // mark ring columns whose far or near half changed
  bool far_half[2]={affected[1][0]||affected[1][2],affected[1][1]||affected[1][3]};
  bool near_half[2]={affected[0][0]||affected[0][2],affected[0][1]||affected[0][3]};
  // r025: each layer ring marks the half of its own world that changed
  if(far_half[0]||far_half[1]){bg_ring[1].dirty_all=true;bg_ring[1].dirty_half[0]|=far_half[0];bg_ring[1].dirty_half[1]|=far_half[1];}
  if(near_half[0]||near_half[1]){bg_ring[0].dirty_all=true;bg_ring[0].dirty_half[0]|=near_half[0];bg_ring[0].dirty_half[1]|=near_half[1];}
  ++background_rebuilds;
 }
 for(unsigned p=0;p<16;p++)page_updated[p]=false;
 background_attempted=true;
 bool was_ready=background_ready;background_ready=bg_layer[0].ok&&bg_layer[1].ok;ok=background_ready;
 if(!ok&&was_ready!=ok)++background_capacity_waits;
 return background_ready;
}
// r030: full-screen tile pictures with more colours than the two 15-colour layers can hold
// (the music select screen: about 60 colours). Both tile layers are composed into one 8-bit
// picture: the far and near nibble planes at the same scroll show a byte as one 256-colour
// index (1..239; 240.. are the road's). Each tile colour index gets its own slot, so a palette
// change only rewrites colours. Used only for a still picture (pages decoded, the same tile RAM,
// pages and scroll as the frame before), composed once; when the two layers fit again, the ring
// background is rebuilt in full (its video memory was overwritten).
extern uint32_t video_tile_ram_writes;
static bool static_on=false,static_dirty=false,static_pal_dirty=false;
static uint8_t static_img[320*224];
static uint16_t static_colour[240],static_index[240];static unsigned static_ncol=0;
static uint32_t static_key[7],static_seen[7];
static uint8_t static_slot_of[8192];
static uint32_t static_composes=0,static_frames=0;
static bool static_prepare(const uint16_t*pal){
 for(unsigned l=0;l<2;l++)for(unsigned q=0;q<4;q++){unsigned p=bg_layer[l].page[q];if(!page_cache[p].valid||page_stale(p))return false;}
 const uint32_t key[7]={tile_word(0xe80),tile_word(0xe82),tile_word(0xe90),tile_word(0xe92),tile_word(0xe98),tile_word(0xe9a),video_tile_ram_writes};
 const bool still=!memcmp(key,static_seen,sizeof key);memcpy(static_seen,key,sizeof key);
 if(static_on&&!memcmp(key,static_key,sizeof key)){
  for(unsigned s=1;s<=static_ncol;s++){const uint16_t c=opaque_colour(pal[static_index[s]]);if(c!=static_colour[s]){static_colour[s]=c;static_pal_dirty=true;}}
  return true;
 }
 if(!still)return false;   // moving: wait until it stands still (one frame)
 ++static_composes;
 static uint16_t buf[320*224];memset(buf,0,sizeof buf);
 video.tile_layer->update_tile_values();
 video.tile_layer->render_tile_layer(buf,1,0);video.tile_layer->render_tile_layer(buf,0,0);   // as Video::prepare_frame
 for(unsigned s=1;s<=static_ncol;s++)static_slot_of[static_index[s]]=0;
 static_ncol=0;bool ok=true;
 for(unsigned i=0;i<320*224;i++){
  const unsigned v=buf[i]&8191;if(!v){static_img[i]=0;continue;}
  unsigned s=static_slot_of[v];
  if(!s){if(static_ncol>=239){ok=false;break;}s=++static_ncol;static_index[s]=uint16_t(v);static_colour[s]=opaque_colour(pal[v]);static_slot_of[v]=uint8_t(s);}
  static_img[i]=uint8_t(s);
 }
 if(!ok){for(unsigned s=1;s<=static_ncol;s++)static_slot_of[static_index[s]]=0;static_ncol=0;static_on=false;return false;}
 memcpy(static_key,key,sizeof key);static_on=true;static_dirty=true;
 return true;
}
static bool near_has(unsigned nx,unsigned ny){unsigned q=(ny>=256?2:0)+(nx>=512?1:0);const PageMeta&m=page_cache[bg_layer[0].page[q]].m;unsigned r=ny&255,x=nx&511;return m.rowany[r]&&x>=m.rmin[r]&&x<=m.rmax[r];}
static bool near_row_any(unsigned ny){unsigned h=ny>=256?2:0;const BgLayer&L=bg_layer[0];return page_cache[L.page[h]].m.rowany[ny&255]||page_cache[L.page[h+1]].m.rowany[ny&255];}
static inline uint8_t layer_value(unsigned l,unsigned x,unsigned y){
 const BgLayer&L=bg_layer[l];unsigned q=(y>=256?2:0)+(x>=512?1:0);
 return L.lut[q][page_cache[L.page[q]].px[(y&255)*512+(x&511)]];
}
// Both layers use one 8-bit surface; each keeps its original world coordinates.
static uint8_t composed_pixel(unsigned world,unsigned y,int dx,int dy){
 unsigned n=layer_value(0,(world+dx)&1023,(y+dy)&511);
 return n?n:layer_value(1,world&1023,y&511);
}

#ifdef OR_X68K
#define BG_STORE_BASE ((volatile uint8_t*)0xc00001UL)
#define BG_STORE_X(x) ((x)*2)
#define BG_STORE_ROW 1024
#else
#define BG_STORE_BASE background_ring
#define BG_STORE_X(x) (x)
#define BG_STORE_ROW 512
#endif
__attribute__((unused)) static void store_background(unsigned world,unsigned y,uint8_t value){
 unsigned x=world&511;
#ifdef OR_X68K
 // Write only the background byte; never read or rewrite the road byte.
 volatile uint8_t*dst=(volatile uint8_t*)(0xc00001UL+(y*512+x)*2);
 *dst=value;
#else
 background_ring[y*512+x]=value;
#endif
}
// values of one layer column (layer x) for rows ystart.. (wrapping at 512)
static void fill_col(unsigned l,unsigned x,unsigned ystart,unsigned count,uint8_t*out){
 const BgLayer&L=bg_layer[l];unsigned i=0;
 while(i<count){
  unsigned y=(ystart+i)&511,end=y<256?256:512,n=end-y;if(n>count-i)n=count-i;
  unsigned q=(y>=256?2:0)+(x>=512?1:0);const uint8_t*lut=L.lut[q];const uint8_t*src=page_cache[L.page[q]].px+(y&255)*512+(x&511);
  for(unsigned k=0;k<n;k++){out[i+k]=lut[*src];src+=512;}
  i+=n;
 }
}
// values of one layer row (layer y) for columns xstart.. (wrapping at 1024)
static void fill_row(unsigned l,unsigned xstart,unsigned y,unsigned count,uint8_t*out){
 const BgLayer&L=bg_layer[l];unsigned i=0,h=y>=256?2:0;
 while(i<count){
  unsigned x=(xstart+i)&1023,end=x<512?512:1024,n=end-x;if(n>count-i)n=count-i;
  unsigned q=h+(x>=512?1:0);const uint8_t*lut=L.lut[q];const uint8_t*src=page_cache[L.page[q]].px+(y&255)*512+(x&511);
  for(unsigned k=0;k<n;k++)out[i+k]=lut[src[k]];
  i+=n;
 }
}
static int unwrap_target(int previous,int requested,int width){int d=mod(requested,width)-mod(previous,width);if(d>width/2)d-=width;else if(d<-width/2)d+=width;return previous+d;}
// ---------------------------------------------------------------------------
// r025: the two background layers on separate 4-bit planes of graphic page 0 (the old
// ORGAME layout, checked on XM6 with r025t). GVRAM low byte = far slot<<4 | near slot.
// R12/R13 scroll the low nibble (near), R14/R15 the high nibble (far), so each layer keeps
// its own X and Y: nothing is recomposed when the layers move against each other.
// Palette index f*16+n = near colour n if n, else far colour f (240..255 stay for the road).
// Each layer has its own ring: ring column = layer world & 511, ring row = layer row.
// A write changes only the layer's own nibble (read-modify-write of the background byte).
// Layer values: far 1..14, near 17..31 -> nibble (v&15)<<shift.
// ---------------------------------------------------------------------------
static void ring_column_rows(unsigned l,int world,unsigned y0,unsigned count){
 uint8_t val[512];fill_col(l,unsigned(world)&1023,y0&511,count,val);
 const unsigned s=l?4:0;const uint8_t keep=l?0x0f:0xf0;
 volatile uint8_t*base=BG_STORE_BASE+BG_STORE_X(unsigned(world)&511);
 for(unsigned i=0;i<count;i++){volatile uint8_t*p=base+((y0+i)&511)*BG_STORE_ROW;*p=uint8_t((*p&keep)|((val[i]&15)<<s));}
}
// ring row y, ring columns of world first..first+count-1
static void ring_row_span(unsigned l,int first,unsigned count,unsigned y){
 uint8_t val[512];fill_row(l,unsigned(first)&1023,y&511,count,val);
 const unsigned s=l?4:0;const uint8_t keep=l?0x0f:0xf0;
 volatile uint8_t*row=BG_STORE_BASE+(y&511)*BG_STORE_ROW;
 unsigned i=0;
 while(i<count){
  unsigned c=unsigned(first+int(i))&511,n=512-c;if(n>count-i)n=count-i;
  volatile uint8_t*out=row+BG_STORE_X(c);
  for(unsigned k=0;k<n;k++){volatile uint8_t*p=out+BG_STORE_X(k);*p=uint8_t((*p&keep)|((val[i+k]&15)<<s));}
  i+=n;
 }
}
// r024c..g bookkeeping, now per layer: row_valid[y] = ring row y is correct in all 512 ring
// columns; column writes write only the shown rows; a shown row that is not valid is
// rewritten across the whole ring. Page changes mark the ring columns of the changed half;
// the columns on screen (set P) are rewritten over several frames (budget per frame), the
// others 16 a frame.
static uint32_t ring_column_count[2]={0,0},ring_row_count[2]={0,0};
static void ring_column_shown(unsigned l,int world){
 BgRing&R=bg_ring[l];
 for(unsigned y=0;y<512;){if(!R.row_shown[y]){++y;continue;}unsigned a=y;while(y<512&&R.row_shown[y])++y;ring_column_rows(l,world,a,y-a);}
 unsigned c=unsigned(world)&511;if(R.column_dirty[c]){R.column_dirty[c]=false;--R.dirty_count;}
 ++background_columns;++ring_column_count[l];
}
static void ring_drop_hidden(BgRing&R){for(unsigned y=0;y<512;y++)if(!R.row_shown[y])R.row_valid[y]=false;}
static void ring_stream(unsigned l,int target,unsigned y0,unsigned shown,bool refresh){
 BgRing&R=bg_ring[l];
 memset(R.row_shown,0,sizeof R.row_shown);for(unsigned r=0;r<shown;r++)R.row_shown[(y0+r)&511]=true;
 int d=target-R.loaded;
 bool full=refresh||d>=512||d<=-512;
 // r024f: a large jump of the view: mark the ring like a page change instead of 500 columns at once
 if(!full&&(d>64||d<-64)){R.dirty_all=true;R.dirty_half[0]=R.dirty_half[1]=true;d=0;R.loaded=target;}
 if(full){
  memset(R.row_valid,0,sizeof R.row_valid);memset(R.column_dirty,0,sizeof R.column_dirty);R.dirty_count=0;
  R.dirty_all=false;R.dirty_half[0]=R.dirty_half[1]=false;R.pass_active=false;memset(R.pass_cols,0,sizeof R.pass_cols);++full_streams;
 }else{
  bool wrote=false;
  if(d>0)for(int i=0;i<d;i++){ring_column_shown(l,R.loaded+416+i);wrote=true;}
  else for(int i=d;i<0;i++){ring_column_shown(l,R.loaded-96+i);wrote=true;}
  if(wrote)ring_drop_hidden(R);
 }
 if(R.dirty_all){
  for(int i=-96;i<416;i++){unsigned w=unsigned(target+i);bool h=(w&1023)>=512;
   if(R.dirty_half[h]&&!R.column_dirty[w&511]){R.column_dirty[w&511]=true;++R.dirty_count;}}
  R.dirty_all=false;R.dirty_half[0]=R.dirty_half[1]=false;++dirty_refreshes;memset(R.visible_done,0,sizeof R.visible_done);
  memset(R.pass_cols,0,sizeof R.pass_cols);R.pass_active=false;
  for(int i=0;i<320;i++){unsigned c=unsigned(target+i)&511;if(R.column_dirty[c]){R.pass_cols[c]=true;R.pass_active=true;}}
 }
 if(R.dirty_count){
#ifdef OR_X68K
  uint32_t t=platform_fine_clock();
#endif
  bool first_pass=R.pass_active;
  if(R.pass_active){
   unsigned budget=6000;
   const unsigned seam=unsigned(target-96)&511;
   for(int r=int(shown)-1;r>=0&&budget;r--){unsigned y=(y0+unsigned(r))&511;if(R.visible_done[y])continue;
    for(unsigned c=0;c<512;){if(!R.pass_cols[c]){++c;continue;}unsigned a=c;++c;while(c<512&&R.pass_cols[c]&&c!=seam)++c;
     // ring columns a..c-1 -> world columns with the same ring index near the current view
     int w=target-96+int((a-seam)&511);
     ring_row_span(l,w,c-a,y);budget=budget>(c-a)?budget-(c-a):0;}
    R.visible_done[y]=true;
   }
   bool all=true;for(unsigned r=0;r<shown;r++)if(!R.visible_done[(y0+r)&511]){all=false;break;}
   if(all){for(unsigned c=0;c<512;c++)if(R.pass_cols[c]){R.pass_cols[c]=false;if(R.column_dirty[c]){R.column_dirty[c]=false;--R.dirty_count;}}R.pass_active=false;memset(R.visible_done,0,sizeof R.visible_done);}
  }
  // other dirty columns on screen: their shown rows at once (only a few scroll in per frame)
  for(int i=0;i<320;){unsigned c=unsigned(target+i)&511;if(!R.column_dirty[c]||R.pass_cols[c]){++i;continue;}
   int a=i;while(i<320&&R.column_dirty[unsigned(target+i)&511]&&!R.pass_cols[unsigned(target+i)&511])++i;
   for(unsigned y=0;y<512;y++)if(R.row_shown[y])ring_row_span(l,target+a,unsigned(i-a),y);
   for(int k=a;k<i;k++){unsigned cc=unsigned(target+k)&511;R.column_dirty[cc]=false;--R.dirty_count;}}
  // dirty columns of the ring margin: 16 a frame
  {unsigned budget=16;
   for(int i=-96;i<416&&budget;){
    unsigned c=unsigned(target+i)&511;
    if(!R.column_dirty[c]||R.pass_cols[c]||(i>=0&&i<320)){++i;continue;}
    int a=i;while(i<416&&budget>unsigned(i-a)&&R.column_dirty[unsigned(target+i)&511]&&!R.pass_cols[unsigned(target+i)&511]&&!(i>=0&&i<320))++i;
    unsigned n=unsigned(i-a);
    for(unsigned y=0;y<512;y++)if(R.row_shown[y])ring_row_span(l,target+a,n,y);
    for(int k=a;k<i;k++){unsigned cc=unsigned(target+k)&511;R.column_dirty[cc]=false;--R.dirty_count;}
    budget-=n;
   }}
  ring_drop_hidden(R);
#ifdef OR_X68K
  uint32_t e=platform_fine_clock()-t;
  if(first_pass){full_stream_fine_total+=e;if(e>full_stream_fine_max)full_stream_fine_max=e;}
#else
  (void)first_pass;
#endif
 }
 // shown rows that are not current: whole ring row
 for(unsigned y=0;y<512;y++)if(R.row_shown[y]&&!R.row_valid[y]){ring_row_span(l,target-96,512,y);R.row_valid[y]=true;++ring_row_count[l];}
 R.loaded=target;
#ifdef OR_X68K
 // Audit a small number of pixels of this layer's nibble plane once its rows are current.
 if(!R.pass_active&&!R.dirty_count)for(unsigned r=0;r<shown&&r<24&&background_far_checks<64;r+=7)for(int i=0;i<320&&background_far_checks<64;i+=13){
  unsigned world=unsigned(target+i),y=(y0+r)&511;uint8_t v=layer_value(l,world&1023,y);
  if(!v)continue;
  unsigned actual=(*(volatile uint16_t*)(0xc00000UL+(y*512+(world&511))*2)>>(l?4:0))&15;
  if(actual!=(v&15u))++background_data_errors;
  unsigned idx=l?(v&15u)*16:(v&15u);
  uint16_t colour=*(volatile uint16_t*)(0xe82000UL+idx*2);
  if(colour!=opaque_colour(background_assets.palette[v]))++background_palette_errors;
  ++background_far_checks;
 }
#endif
}
static void stream_hud(const HudPacket&H);
// r029: called after the frame before has been taken by the vertical blank (the frame is built
// earlier, while that one was still waiting): the text bank for the car on the text plane is
// chosen now, from the bank that is actually shown.
void visuals_handoff(RoadFrame&f){
 if(f.visual.car.text_mode&&!hud_enabled)f.visual.car.text_bank=uint16_t(1-text_active_bank);
}
void visuals_stream(const RoadFrame&f){
 stream_hud(f.visual.hud);   // r027
#ifdef OR_X68K
 if(f.visual.car.text_mode){
  for(unsigned plane=0;plane<4;plane++)for(unsigned y=0;y<224;y++){
   const uint32_t*src=(const uint32_t*)(f.visual.car.text_bits[plane]+y*40);
   volatile uint32_t*dst=(volatile uint32_t*)(0xe00000UL+plane*0x20000UL+(f.visual.car.text_bank*256+y)*128);
   for(unsigned i=0;i<10;i++)dst[i]=src[i];
  }
 }
#endif
 if(f.visual.flags&4){   // r030: the 8-bit picture (rows and columns 0.., both nibble planes)
#ifdef OR_X68K
  if(static_dirty||static_pal_dirty)for(unsigned s=1;s<=static_ncol;s++)*(volatile uint16_t*)(0xe82000UL+s*2)=static_colour[s];
#endif
  static_pal_dirty=false;
  if(static_dirty){
#ifdef OR_X68K
   for(unsigned y=0;y<224;y++){volatile uint8_t*d=(volatile uint8_t*)(0xc00001UL+y*1024UL);const uint8_t*src=static_img+y*320;
    for(unsigned x=0;x<320;x++)d[x*2]=src[x];}
#else
   for(unsigned y=0;y<224;y++)memcpy(background_ring+y*512,static_img+y*320,320);
#endif
   static_dirty=false;
  }
  return;
 }
 if(!(f.visual.flags&1)||!background_ready)return;
#ifdef OR_X68K
 // r025: palette index f*16+n (f = far slot 0..14, n = near slot 0..15)
 if(background_palette_refresh)for(unsigned fv=0;fv<15;fv++)for(unsigned n=0;n<16;n++){unsigned i=fv*16+n;if(!i)continue;
  *(volatile uint16_t*)(0xe82000UL+i*2)=opaque_colour(n?background_assets.palette[16+n]:background_assets.palette[fv]);}
#endif
 // rows shown this frame: lines without road (the sky part) plus 16 lines of margin
 int ymax=-1;for(int y=0;y<224;y++)if(f.mode[y]==0)ymax=y;
 unsigned shown=ymax<0?0:unsigned(ymax+1+16);if(shown>512)shown=512;
 bool refresh=background_refresh;
 ring_stream(0,f.visual.near_world,f.visual.pad&511,shown,refresh);
 ring_stream(1,f.visual.far_world,f.rows[0].bg_far_y&511,shown,refresh);
 near_loaded=f.visual.near_world;far_loaded=f.visual.far_world;
 background_refresh=background_palette_refresh=false;
}
void visuals_native_init(){near_loaded=far_loaded=0;bg_ring[0].loaded=bg_ring[1].loaded=0;}
// Keep BG character rows 0..49 and 60 intact; other map rows are never displayed.
static unsigned car_pattern_slot(unsigned index){
 unsigned base=(road_atlas.tiles+3)/4,first=128-base;
 if(index<first)return base+index;
 unsigned k=index-first,row=k%13;row=row<10?50+row:61+row-10;
 return 128+(k/13)*64+row;
}
// Reuse the original 16-colour banks; split a tile only where banks overlap.
// r020: banks met by the last build_car_direct() call, in first-seen order (for the car cache key).
static uint16_t direct_banks[16];static unsigned direct_bank_count=0;static bool direct_banks_overflow=false;
static bool build_car_direct(CarPacket&out,const uint16_t*pal,int low,int high){
 uint8_t banks[512];memset(banks,255,sizeof banks);
 direct_bank_count=0;direct_banks_overflow=false;
 uint16_t masks[14*20]={0};unsigned row_counts[14]={0};
 // r019: same raster order as r018; two transparent pixels are skipped with one long test.
 for(int y=low;y<high;y++){
  const uint16_t*row=capture_pixels+y*320;uint16_t*mrow=masks+(y>>4)*20;
  const alias_u32*pairs=(const alias_u32*)row;
  for(int x=0;x<320;x+=2){
   if(!pairs[x>>1])continue;
   for(int d=0;d<2;d++){
    unsigned v=row[x+d];if(!v)continue;
    if(!(v&15))return false;
    unsigned bank=(v&8191)>>4,slot=banks[bank];
    if(slot==255){
     for(slot=0;slot<out.palettes;slot++)if(!memcmp(out.palette[slot]+1,pal+bank*16+1,30))break;
     if(slot==out.palettes){if(slot==13)return false;++out.palettes;memcpy(out.palette[slot],pal+bank*16,32);out.groups[slot]=15;}
     banks[bank]=slot;
     if(direct_bank_count<16)direct_banks[direct_bank_count++]=bank;else direct_banks_overflow=true;
    }
    mrow[(x+d)>>4]|=1<<slot;
   }
  }
 }
 const unsigned limit=128;   // r027: the pattern pool is shared and reused down the screen
 for(int ty=low/16;ty<(high+15)/16;ty++)for(int tx=0;tx<20;tx++){
  unsigned mask=masks[ty*20+tx];
  // With one bank in the tile every visible pixel belongs to it, so the bank test is redundant.
  const bool single=mask&&!(mask&(mask-1));
  for(unsigned slot=0;mask;slot++,mask>>=1){
   if(!(mask&1))continue;
   if(out.count>=limit||++row_counts[ty]>32)return false;
   unsigned tile=out.count++;out.patterns=out.count;out.pattern_index[tile]=tile;
   out.registers[tile][0]=tx*16+16;out.registers[tile][1]=ty*16+16;
   out.registers[tile][2]=uint16_t((slot+3)<<8);out.registers[tile][3]=3;   // r027: pattern set by the scheduler
   uint8_t*encoded=out.pcg[tile];
   for(int yy=0;yy<16;yy++){
    uint8_t*left=encoded+(yy>>3)*32+(yy&7)*4,*right=left+64;
    const uint16_t*src=capture_pixels+(ty*16+yy)*320+tx*16;
    const alias_u32*src32=(const alias_u32*)src;
    if(ty*16+yy>=224||!(src32[0]|src32[1]|src32[2]|src32[3]|src32[4]|src32[5]|src32[6]|src32[7])){
     left[0]=left[1]=left[2]=left[3]=right[0]=right[1]=right[2]=right[3]=0;continue;
    }
    // Rows outside the scanned range keep the full bank test, exactly as before.
    const bool fast=single&&ty*16+yy>=low&&ty*16+yy<high;
    for(int xx=0;xx<16;xx+=2){
     unsigned a=src[xx],b=src[xx+1],n0,n1;
     if(fast){n0=a&15;n1=b&15;}
     else {n0=a&&banks[(a&8191)>>4]==slot?a&15:0;n1=b&&banks[(b&8191)>>4]==slot?b&15:0;}
     (xx<8?left:right)[(xx&7)>>1]=(n0<<4)|n1;
    }
   }
  }
 }
 for(unsigned n:row_counts)out.line_max=std::max(out.line_max,uint16_t(n));
 return true;
}
// r020 car cache.
// The direct conversion depends only on the selected sprite entries (7 words each, in
// sorted order) and on the 16 colours of every bank it met. A hit copies the stored
// packet; a miss runs the r019 path and stores the result. Overflow/TEXT results are
// never stored. The PC build always runs the full path and compares it with the cache.
struct CarCacheEntry{
 CarCacheEntry*next;uint32_t hash;
 uint16_t num,low,high,bank_count;
 uint16_t key[6*7];uint16_t banks[16];uint16_t colours[16][16];
 uint16_t count,patterns,palettes,line_max;
 uint16_t palette[13][16],groups[13];
 uint16_t*registers;uint16_t*pattern_index;uint8_t*pcg;
};
static CarCacheEntry*car_cache_buckets[256];
static unsigned car_cache_entries=0;static uint32_t car_cache_bytes=0;
uint32_t car_cache_hits=0,car_cache_misses=0,car_cache_stores=0,car_cache_rejects=0;
static const uint32_t CAR_CACHE_LIMIT_BYTES=1536UL*1024UL;
static uint32_t car_key_hash(const uint16_t*key,unsigned words){uint32_t h=2166136261UL;for(unsigned i=0;i<words;i++){h^=key[i];h*=16777619UL;}return h;}
static CarCacheEntry*car_cache_find(uint32_t h,unsigned num,int low,int high,const uint16_t*key,const uint16_t*pal){
 for(CarCacheEntry*e=car_cache_buckets[h&255];e;e=e->next){
  if(e->hash!=h||e->num!=num||e->low!=low||e->high!=high)continue;
  if(memcmp(e->key,key,num*14))continue;
  bool same=true;for(unsigned b=0;b<e->bank_count&&same;b++)same=!memcmp(e->colours[b],pal+e->banks[b]*16,32);
  if(same)return e;
 }
 return 0;
}
static void car_cache_apply(CarPacket&out,const CarCacheEntry&e){
 out.count=e.count;out.patterns=e.patterns;out.palettes=e.palettes;out.line_max=e.line_max;out.overflow=0;out.text_mode=0;
 memcpy(out.palette,e.palette,e.palettes*32);memcpy(out.groups,e.groups,sizeof e.groups);
 memcpy(out.registers,e.registers,e.count*8);memcpy(out.pattern_index,e.pattern_index,e.count*2);
 memcpy(out.pcg,e.pcg,e.patterns*128);
}
static bool car_cache_same(const CarPacket&out,const CarCacheEntry&e){
 return out.count==e.count&&out.patterns==e.patterns&&out.palettes==e.palettes&&out.line_max==e.line_max&&!out.overflow&&!out.text_mode&&
  !memcmp(out.palette,e.palette,e.palettes*32)&&!memcmp(out.groups,e.groups,e.palettes*2)&&   /* r027c: groups of unused palettes do not matter */!memcmp(out.registers,e.registers,e.count*8)&&
  !memcmp(out.pattern_index,e.pattern_index,e.count*2)&&!memcmp(out.pcg,e.pcg,e.patterns*128);
}
static void car_cache_store(uint32_t h,unsigned num,int low,int high,const uint16_t*key,const uint16_t*pal,const CarPacket&out){
 if(direct_banks_overflow||out.overflow||out.text_mode){++car_cache_rejects;return;}
 uint32_t extra=out.count*8+out.count*2+out.patterns*128;
 if(car_cache_bytes+sizeof(CarCacheEntry)+extra>CAR_CACHE_LIMIT_BYTES){++car_cache_rejects;return;}
 CarCacheEntry*e=(CarCacheEntry*)malloc(sizeof(CarCacheEntry));uint8_t*blob=(uint8_t*)malloc(extra?extra:1);
 if(!e||!blob){free(e);free(blob);++car_cache_rejects;return;}
 e->hash=h;e->num=num;e->low=low;e->high=high;memcpy(e->key,key,num*14);
 e->bank_count=direct_bank_count;for(unsigned b=0;b<direct_bank_count;b++){e->banks[b]=direct_banks[b];memcpy(e->colours[b],pal+direct_banks[b]*16,32);}
 e->count=out.count;e->patterns=out.patterns;e->palettes=out.palettes;e->line_max=out.line_max;
 memcpy(e->palette,out.palette,sizeof e->palette);memcpy(e->groups,out.groups,sizeof e->groups);
 e->registers=(uint16_t*)blob;e->pattern_index=(uint16_t*)(blob+out.count*8);e->pcg=blob+out.count*10;
 memcpy(e->registers,out.registers,out.count*8);memcpy(e->pattern_index,out.pattern_index,out.count*2);memcpy(e->pcg,out.pcg,out.patterns*128);
 e->next=car_cache_buckets[h&255];car_cache_buckets[h&255]=e;++car_cache_entries;++car_cache_stores;car_cache_bytes+=sizeof(CarCacheEntry)+extra;
}
// Rows of capture_pixels that may be non-zero (the last real capture). The r018/r019 code
// cleared the previous frame's range; with cache hits the last capture can be older.
static int capture_low=224,capture_high=0;
static bool car_last_hit=false;
#ifdef OR_X68K
uint32_t platform_fine_clock();
#endif
uint32_t car_fine_total=0,car_fine_capture=0,car_fine_direct=0,car_fine_max=0;
static void build_car_body(CarPacket&out,const uint16_t*pal);
static void build_car(CarPacket&out,const uint16_t*pal){
#ifdef OR_X68K
 uint32_t t=platform_fine_clock();
#endif
 build_car_body(out,pal);
#ifdef OR_X68K
 uint32_t d=platform_fine_clock()-t;car_fine_total+=d;if(d>car_fine_max)car_fine_max=d;
#endif
}
static void build_car_body(CarPacket&out,const uint16_t*pal){
 out.count=out.patterns=out.palettes=out.overflow=out.line_max=out.text_mode=0;capture_palette=pal;
 const unsigned ids[]={OSprites::SPRITE_FERRARI,OSprites::SPRITE_PASS1,OSprites::SPRITE_PASS2,OSprites::SPRITE_CRASH,OSprites::SPRITE_CRASH_PASS1,OSprites::SPRITE_CRASH_PASS2};
 unsigned indices[6],num=0;int low=224,high=0;
 for(unsigned id:ids){
  const oentry&e=osprites.jump_table[id];unsigned index=e.dst_index;
  if(!(e.control&OSprites::ENABLE)||index>=osprites.sprite_count||osprites.sprite_entries[index].scratch!=id)continue;
  const uint16_t*r=osprites.sprite_entries[index].data;if(r[0]&0xd000)continue;
  int top=(r[0]&511)-256,rows=(r[5]>>8)+1,bottom=top+((r[4]&0x8000)?rows-1:1-rows);
  low=std::min(low,std::min(top,bottom));high=std::max(high,std::max(top,bottom)+1);indices[num++]=index;
 }
 low=std::max(0,low);high=std::min(224,high);
 std::sort(indices,indices+num);
 uint16_t key[6*7];for(unsigned i=0;i<num;i++)memcpy(key+i*7,osprites.sprite_entries[indices[i]].data,14);
 uint32_t key_hash=car_key_hash(key,num*7)^(uint32_t(low)<<8)^uint32_t(high);
 CarCacheEntry*cached=low<high?car_cache_find(key_hash,num,low,high,key,pal):0;
 if(cached)++car_cache_hits;else if(low<high)++car_cache_misses;
#ifdef OR_X68K
 const bool use_hit=true;
#else
 // PC: normally rebuild and compare; STAGE1_CAR_CACHE_SKIP=1 runs the native hit path.
 static const bool use_hit=std::getenv("STAGE1_CAR_CACHE_SKIP")!=0;
#endif
 car_last_hit=false;
 if(cached&&use_hit){
  car_cache_apply(out,*cached);old_low=low;old_high=high;car_last_hit=true;
  car_peak_tiles=std::max(car_peak_tiles,unsigned(out.count));car_peak_patterns=std::max(car_peak_patterns,unsigned(out.patterns));car_peak_line=std::max(car_peak_line,unsigned(out.line_max));
  car_tiles_total+=out.count;++car_frames;return;
 }
#ifdef OR_X68K
 uint32_t tc=platform_fine_clock();
#endif
 int clear_low=std::min(low,std::min(old_low,capture_low)),clear_high=std::max(high,std::max(old_high,capture_high));
 if(clear_low<clear_high)memset(capture_pixels+clear_low*320,0,(clear_high-clear_low)*320*2);
 old_low=low;old_high=high;capture_low=low;capture_high=high;
 for(unsigned i=0;i<num;i++)video.sprite_layer->capture_entry(osprites.sprite_entries[indices[i]].data,capture_pixels);
#ifdef OR_X68K
 car_fine_capture+=platform_fine_clock()-tc;
#endif

#ifndef OR_X68K
 if(std::getenv("STAGE1_CHECK_CAR")){
  uint16_t saved[2048],test[2048]={0};static uint16_t reference[320*224];
  video.sprite_layer->save_render_state(saved);
  for(unsigned i=0;i<num;i++)memcpy(test+1024+i*8,osprites.sprite_entries[indices[i]].data,14);
  test[1024+num*8]=0x8000;video.sprite_layer->load_render_state(test);
  memset(reference,0,sizeof reference);uint16_t*previous=video.pixels;video.pixels=reference;
  video.sprite_layer->render_reference(8);video.pixels=previous;video.sprite_layer->load_render_state(saved);
  if(memcmp(reference,capture_pixels,sizeof reference)){fprintf(stderr,"CAR REFERENCE FAIL\n");std::exit(10);}
 }
#endif
 if(low>=high)return;
#ifdef OR_X68K
 uint32_t td=platform_fine_clock();
#endif
 bool direct_ok=build_car_direct(out,pal,low,high);
#ifdef OR_X68K
 car_fine_direct+=platform_fine_clock()-td;
#endif
 if(direct_ok){
#ifndef OR_X68K
  if(cached&&!car_cache_same(out,*cached)){fprintf(stderr,"CAR CACHE FAIL\n");std::exit(15);}
#endif
  if(!cached&&!out.overflow)car_cache_store(key_hash,num,low,high,key,pal,out);
 }
 if(!direct_ok){
 out.count=out.patterns=out.palettes=out.overflow=out.line_max=0;
 unsigned row_counts[14]={0};
 for(int ty=low/16;ty<(high+15)/16;ty++)for(int tx=0;tx<20;tx++){
  uint16_t colors[256];unsigned color_count=0;
  for(int yy=0;yy<16&&ty*16+yy<224;yy++)for(int xx=0;xx<16;xx++){
   unsigned value=capture_pixels[(ty*16+yy)*320+tx*16+xx];if(!value)continue;
   uint16_t color=pal[value&8191];bool found=false;
   for(unsigned k=0;k<color_count;k++)if(colors[k]==color)found=true;
   if(!found)colors[color_count++]=color;
  }
  for(unsigned first=0;first<color_count;first+=15){
   unsigned last=std::min(first+15,color_count),slot=13,best_missing=16;
   for(unsigned k=0;k<out.palettes;k++){
    unsigned missing=0;
    for(unsigned c=first;c<last;c++){bool found=false;for(unsigned n=1;n<=out.groups[k];n++)if(out.palette[k][n]==colors[c])found=true;if(!found)++missing;}
    if(out.groups[k]+missing<=15&&missing<best_missing){slot=k;best_missing=missing;}
   }
   if(slot==13){
    if(out.palettes>=13){out.overflow|=1;continue;}
    slot=out.palettes++;out.groups[slot]=0;out.palette[slot][0]=0;
   }
   for(unsigned c=first;c<last;c++){
    bool found=false;for(unsigned n=1;n<=out.groups[slot];n++)if(out.palette[slot][n]==colors[c])found=true;
    if(!found)out.palette[slot][++out.groups[slot]]=colors[c];
   }
   if(out.count>=128){out.overflow|=8;continue;}
   unsigned tile=out.count;uint8_t encoded[128];
   out.registers[tile][0]=tx*16+16;out.registers[tile][1]=ty*16+16;
   out.registers[tile][2]=((slot+3)<<8);out.registers[tile][3]=3;
   for(int yy=0;yy<16;yy++)for(int xx=0;xx<16;xx+=2){
    unsigned nibble[2]={0,0};
    for(unsigned d=0;d<2;d++){
     unsigned value=ty*16+yy<224?capture_pixels[(ty*16+yy)*320+tx*16+xx+d]:0;if(!value)continue;
     uint16_t color=pal[value&8191];bool in_layer=false;for(unsigned c=first;c<last;c++)if(colors[c]==color)in_layer=true;
     if(in_layer)for(unsigned n=1;n<=out.groups[slot];n++)if(out.palette[slot][n]==color){nibble[d]=n;break;}
    }
    encoded[((xx>>3)*2+(yy>>3))*32+(yy&7)*4+((xx&7)>>1)]=(nibble[0]<<4)|nibble[1];
   }
   unsigned pattern=0;
   for(;pattern<out.patterns;pattern++)if(!memcmp(out.pcg[pattern],encoded,128))break;
   if(pattern==out.patterns){
    if(out.patterns>=128){out.overflow|=2;continue;}
    memcpy(out.pcg[out.patterns++],encoded,128);
   }
   out.pattern_index[tile]=pattern;   // r027: pattern number set by the scheduler
   ++out.count;++row_counts[ty];
  }
 }
 for(unsigned n:row_counts)out.line_max=std::max(out.line_max,uint16_t(n));

 if(out.line_max>32)out.overflow|=4;
 // Large single-palette images use an inactive text surface, preserving all pixels.
 if(out.overflow&&out.palettes==1){
  memset(out.text_bits,0,sizeof out.text_bits);memset(text_color_lookup,0,sizeof text_color_lookup);
  bool valid=true;
  for(int y=old_low;y<old_high;y++)for(int x=0;x<320;x++){
   unsigned value=capture_pixels[y*320+x];if(!value)continue;
   unsigned index=value&8191,n=text_color_lookup[index];
   if(!n){for(n=1;n<=out.groups[0];n++)if(out.palette[0][n]==pal[index])break;if(n>out.groups[0]){valid=false;continue;}text_color_lookup[index]=n;}
   for(unsigned plane=0;plane<4;plane++)if(n&(1<<plane))out.text_bits[plane][y*40+x/8]|=0x80>>(x&7);
  }
  // r027: with the score etc. on text rows 0..223, the car uses rows 256.. (that frame shows no score)
  if(valid){out.count=out.patterns=out.overflow=out.line_max=0;out.text_mode=1;out.text_bank=hud_enabled?1:1-text_active_bank;++car_text_frames;}
 }


 }
#ifndef OR_X68K
 for(unsigned i=0;i<out.count;i++){
  unsigned pattern=out.registers[i][2]&255;
 // r027: the car's patterns are placed by the scenery scheduler (checked by its emulation)
 }
#endif
 if(out.overflow){++car_overflows;
#ifndef OR_X68K
 fprintf(stderr,"CAR CAPACITY flags=%u count=%u palettes=%u line=%u\n",out.overflow,out.count,out.palettes,out.line_max);
#endif
 }
 car_peak_tiles=std::max(car_peak_tiles,unsigned(out.count));car_peak_patterns=std::max(car_peak_patterns,unsigned(out.patterns));car_peak_line=std::max(car_peak_line,unsigned(out.line_max));
 car_tiles_total+=out.count;++car_frames;
}
#ifndef OR_X68K
// r026 measurement (PC only, STAGE1_SCENERY_STATS=1): what the roadside objects would need as
// hardware sprites. Each enabled sprite entry that is not the player car is drawn alone into a
// 320x224 buffer (same capture as the car), then counted in 16x16 tiles.
struct SceneryStat{unsigned fit_total92,fit_pair92,fit_pair64,pair_max_max;uint64_t frames,tiles_sum,pixels_sum;unsigned tiles_max,row_max,banks_max,colours_max,entries_max,combined_max,combined_row_max;unsigned hist[8];};
static SceneryStat scenery_stat[2];   // 0: roadside objects only, 1: every sprite except the car
static void scenery_measure(const uint16_t*pal,const CarPacket&car){
 static const bool on=std::getenv("STAGE1_SCENERY_STATS")!=0;if(!on)return;
 static uint16_t buf[2][320*224];memset(buf,0,sizeof buf);
 unsigned entries[2]={0,0};
 for(unsigned i=0;i<osprites.sprite_count;i++){
  const osprite&s=osprites.sprite_entries[i];unsigned id=s.scratch;const uint16_t*r=s.data;
  if(r[0]&0xd000)continue;
  if(id==OSprites::SPRITE_FERRARI||id==OSprites::SPRITE_PASS1||id==OSprites::SPRITE_PASS2||id==OSprites::SPRITE_CRASH||id==OSprites::SPRITE_CRASH_PASS1||id==OSprites::SPRITE_CRASH_PASS2)continue;
  bool roadside=id<OSprites::SPRITE_ENTRIES;
  if(roadside){video.sprite_layer->capture_entry(r,buf[0]);++entries[0];}
  video.sprite_layer->capture_entry(r,buf[1]);++entries[1];
 }
 for(unsigned k=0;k<2;k++){
  SceneryStat&S=scenery_stat[k];const uint16_t*b=buf[k];
  unsigned tiles=0,row_max=0,pixels=0;static bool bank_seen[512];static uint16_t colours[4096];unsigned banks=0,ncol=0;memset(bank_seen,0,sizeof bank_seen);
  unsigned rowt[14]={0};for(unsigned i=0;i<car.count;i++){int ty=(int(car.registers[i][1])-16)/16;if(ty>=0&&ty<14)rowt[ty]++;}
  for(unsigned ty=0;ty<14;ty++){unsigned row=0;
   for(unsigned tx=0;tx<20;tx++){bool any=false;
    for(unsigned yy=0;yy<16;yy++)for(unsigned xx=0;xx<16;xx++){unsigned v=b[(ty*16+yy)*320+tx*16+xx];if(!v)continue;any=true;++pixels;
     unsigned bk=(v&8191)>>4;if(!bank_seen[bk]){bank_seen[bk]=true;++banks;}
     uint16_t c=pal[v&8191];bool f=false;for(unsigned q=0;q<ncol&&!f;q++)f=colours[q]==c;if(!f&&ncol<4096)colours[ncol++]=c;}
    if(any){++tiles;++row;++rowt[ty];}}
   row_max=std::max(row_max,row);}
  ++S.frames;S.tiles_sum+=tiles;S.pixels_sum+=pixels;S.tiles_max=std::max(S.tiles_max,tiles);S.row_max=std::max(S.row_max,row_max);
  S.banks_max=std::max(S.banks_max,banks);S.colours_max=std::max(S.colours_max,ncol);S.entries_max=std::max(S.entries_max,entries[k]);
  {unsigned pm=0;for(unsigned r=0;r+1<14;r++)pm=std::max(pm,rowt[r]+rowt[r+1]);S.pair_max_max=std::max(S.pair_max_max,pm);if(pm<=92)++S.fit_pair92;if(pm<=64)++S.fit_pair64;if(tiles+car.count<=92)++S.fit_total92;}
  unsigned comb=tiles+car.count;S.combined_max=std::max(S.combined_max,comb);S.combined_row_max=std::max(S.combined_row_max,row_max+unsigned(car.line_max));
  static const unsigned edge[7]={1,33,65,75,129,257,513};unsigned h=0;while(h<7&&comb>=edge[h])++h;++S.hist[h];
 }
}
static void scenery_print(){
 if(!std::getenv("STAGE1_SCENERY_STATS"))return;
 for(unsigned k=0;k<2;k++){const SceneryStat&S=scenery_stat[k];
  printf("SCENERY %s frames=%llu tiles avg=%llu max=%u row_max=%u banks_max=%u colours_max=%u entries_max=%u pixels_avg=%llu with_car tiles_max=%u row_max=%u hist(0,1-32,33-64,65-74,75-128,129-256,257-512,513+)=",k?"all_but_car":"roadside",
   (unsigned long long)S.frames,(unsigned long long)(S.frames?S.tiles_sum/S.frames:0),S.tiles_max,S.row_max,S.banks_max,S.colours_max,S.entries_max,(unsigned long long)(S.frames?S.pixels_sum/S.frames:0),S.combined_max,S.combined_row_max);
  for(unsigned h=0;h<8;h++)printf("%u%s",S.hist[h],h<7?",":"\n");
  printf("SCENERY %s fit(no reuse, <=92 patterns)=%u fit(2-row reuse, <=92)=%u fit(2-row reuse, <=64)=%u pair_max=%u\n",k?"all_but_car":"roadside",S.fit_total92,S.fit_pair92,S.fit_pair64,S.pair_max_max);}
}
#endif
// ---------------------------------------------------------------------------
// r026 roadside objects (and every other sprite except the player car and shadows) as
// hardware sprites, in the colours of the original.
// 1. All those sprite entries are drawn in sprite-list order into one 320x224 buffer (the same
//    capture as the car), so overlaps are already resolved.
// 2. The original 16-colour banks met in this frame are packed by colour into the sprite
//    palette blocks after the car's (a block holds up to 15 colours of one or more banks).
// 3. Each 16x16 tile gets one sprite per palette block present in it.
// 4. Sprite numbers after the car's and the PCG patterns after the car's are given out from the
//    top tile row down. A number/pattern used by tile row r may be used again by row r+2: it is
//    rewritten by the raster interrupt during row r+1 (lines 16r+19..16r+29), at most
//    SCENERY_LINE_BUDGET longs per line. Fresh ones are loaded in the vertical blank.
//    What does not fit is left out (counted as dropped).
// ---------------------------------------------------------------------------
alignas(4) static uint16_t scenery_pixels[320*224];
static int scenery_low=224,scenery_high=0;
static uint8_t scenery_map[8192];static const uint16_t*scenery_palette=0;
static uint32_t scenery_diag[8];bool scenery_enabled=true;   // G key: roadside objects on/off (the car patterns are scheduled either way)
static uint16_t scenery_used[8192];static bool scenery_bank_init=false;static const unsigned COLOR_BASE_SCENERY=0x800;   // hwsprites COLOR_BASE
#ifndef OR_X68K
static const uint16_t*scenery_check_pixels=0;
alignas(4) static uint16_t scenery_std[320*224];   // PC: the original capture, for the checks
#endif
static uint32_t scenery_car_dropped=0,scenery_shared_total=0,scenery_frames=0,scenery_count_total=0,scenery_dropped_total=0,scenery_reused_total=0,scenery_bank_drops=0,scenery_overflow=0;
static unsigned scenery_peak=0,scenery_line_peak=0,scenery_block_peak=0;
static uint32_t scenery_reuse_frames=0;   // r030
uint32_t scenery_fine_total=0,scenery_fine_capture=0,scenery_fine_max=0,scenery_fine_colour=0,scenery_fine_tiles=0;
static const unsigned SCENERY_LINE_BUDGET=96;   // longs per raster interrupt (two patterns + registers)
#ifndef OR_X68K
static unsigned scenery_entry_list[256],scenery_entry_count=0;
static uint64_t scenery_dropped_pixels=0,scenery_pixels_total=0,scenery_shadow_pixels=0;
#endif
static bool scenery_is_car(unsigned id){
 return id==OSprites::SPRITE_FERRARI||id==OSprites::SPRITE_PASS1||id==OSprites::SPRITE_PASS2||id==OSprites::SPRITE_CRASH||id==OSprites::SPRITE_CRASH_PASS1||id==OSprites::SPRITE_CRASH_PASS2;
}
static uint32_t scenery_ref(SceneryPacket&S,unsigned off){
#ifdef OR_X68K
 return (uint32_t)(uintptr_t)&S.jobs[off];
#else
 (void)S;return off+1;
#endif
}
struct SceneryJob{int16_t line;uint8_t pcg;uint8_t pad;uint16_t res,sprite;};
#ifdef OR_X68K
extern "C" uint32_t scenery_tile_finish(uint16_t*src,uint32_t rowbits,const uint32_t*rowLR,uint32_t*enc);
extern "C" void scenery_tile_split(uint16_t*src,uint32_t rowbits,uint32_t**encp);
extern "C" uint32_t scenery_pcg_hash(const uint32_t*enc);
extern "C" uint32_t scenery_tile_rows_learn(const uint16_t*src,unsigned rows,uint32_t*rowLR,const uint16_t*block_bit,uint8_t*met_q);
extern "C" uint32_t scenery_tile_rows(const uint16_t*src,unsigned rows,uint32_t*rowLR,const uint16_t*block_bit);
#ifdef CBX68_ASM_CHECK
static uint32_t asm_check[8];static uint8_t chk_met[256];   // rows: calls, colour lookups left to C, mismatches; finish: calls, mismatches
#endif
#endif
static void build_scenery_body(SceneryPacket&S,const uint16_t*pal,CarPacket&car,const HudPacket&hud,bool sprites_on){
 S.count=S.kept=S.dropped=S.blocks=S.dropped_banks=S.reused=S.line_longs_max=S.shared=S.car_dropped=0;
 S.start_job=0;memset(S.line_job,0,sizeof S.line_job);
 const bool car_hw=!car.overflow&&!car.text_mode;
 const unsigned car_sprites=car_hw?car.count:0,car_patterns=car_hw?car.patterns:0,car_blocks=car_hw?car.palettes:0;
 S.first_block=3+car_blocks;scenery_palette=pal;
 const unsigned block_limit=car_blocks<13?13-car_blocks:0;
 // Plan: the banks of this frame's entries, with the colours each was seen to use in earlier
 // frames (all 14 if never seen), packed by colour like r026 (largest first). Colours met that
 // were not planned are placed when met.
 unsigned nused=0,bcount[13]={0};
 static int8_t bank_block[512];static uint16_t bank_list[512];static uint8_t scenery_met[8192];unsigned nbank=0;
 if(!scenery_bank_init){memset(bank_block,-1,sizeof bank_block);scenery_bank_init=true;}
 static uint16_t bank_seen_mask[512];
 {static uint16_t plan_bank[128];unsigned np=0;
  for(unsigned i=0;i<osprites.sprite_count;i++){const osprite&e=osprites.sprite_entries[i];const uint16_t*r=e.data;
   if(!sprites_on||!scenery_enabled||(r[0]&0xd000)||((r[3]>>12)&3)!=3||scenery_is_car(e.scratch))continue;
   unsigned bank=(COLOR_BASE_SCENERY>>4)+(r[5]&127);if(bank_block[bank]!=-1)continue;
   bank_block[bank]=-2;bank_list[nbank++]=uint16_t(bank);if(np<128)plan_bank[np++]=uint16_t(bank);}
  static uint16_t pcol[128][15],pidx[128][15],pn[128],order[128];
  for(unsigned j=0;j<np;j++){unsigned bank=plan_bank[j],mask=bank_seen_mask[bank]?bank_seen_mask[bank]:0x7ffe;pn[j]=0;
   for(unsigned n=1;n<15;n++)if(mask&(1u<<n)){unsigned idx=bank*16+n;pidx[j][pn[j]]=uint16_t(idx);pcol[j][pn[j]]=pal[idx];pn[j]++;}
   order[j]=uint16_t(j);}
  std::sort(order,order+np,[](uint16_t a,uint16_t b){return pn[a]>pn[b]||(pn[a]==pn[b]&&a<b);});
  // r028: which blocks already hold a colour, as a small hash table colour -> block bits (same
  // choices as the r026e search over every block and colour, without the nested compares)
  static uint16_t ch_key[512],ch_mask[512],ch_list[512];static uint8_t ch_used[512];unsigned ch_n=0;
  auto ch_slot=[&](uint16_t c)->unsigned{unsigned h=((c*40503u)>>7)&511;while(ch_used[h]&&ch_key[h]!=c)h=(h+1)&511;return h;};
  for(unsigned o=0;o<np;o++){unsigned j=order[o];int best=-1;unsigned best_new=99;
   uint16_t um[15];unsigned nu=0;   // block bits of the bank's distinct colours (first occurrence order)
   for(unsigned c=0;c<pn[j];c++){bool dup=false;for(unsigned c2=0;c2<c;c2++)if(pcol[j][c2]==pcol[j][c]){dup=true;break;}
    if(!dup){const unsigned h=ch_slot(pcol[j][c]);um[nu++]=ch_used[h]?ch_mask[h]:0;}}
   for(unsigned b=0;b<S.blocks;b++){unsigned add=0;const unsigned bit=1u<<b;
    for(unsigned u=0;u<nu;u++)if(!(um[u]&bit))++add;
    if(bcount[b]+add<=15&&add<best_new){best=int(b);best_new=add;}}
   if(best<0&&S.blocks<block_limit){best=S.blocks++;bcount[best]=0;}
   if(best<0)continue;   // placed when met
   bank_block[plan_bank[j]]=int8_t(best);
   for(unsigned c=0;c<pn[j];c++){unsigned n=1;for(;n<=bcount[best];n++)if(S.palette[best][n]==pcol[j][c])break;
    if(n>bcount[best]){n=++bcount[best];S.palette[best][n]=pcol[j][c];
     const unsigned h=ch_slot(pcol[j][c]);if(!ch_used[h]){ch_used[h]=1;ch_key[h]=pcol[j][c];ch_mask[h]=0;ch_list[ch_n++]=uint16_t(h);}ch_mask[h]|=uint16_t(1u<<best);}
    scenery_map[pidx[j][c]]=uint8_t(((best+1)<<4)|n);scenery_used[nused++]=pidx[j][c];}
  }
  for(unsigned k=0;k<ch_n;k++)ch_used[ch_list[k]]=0;
 }
 // r030: a frame whose roadside sprites are the same as in the last computed frame (same
 // entries, palette plan and colours; e.g. standing at the start) takes that frame's tiles
 // instead of capturing and converting again (the same result). Not on colour-learning frames.
 const bool learn=(scenery_frames&7)==0;
 static uint16_t reuse_key[256*23+256],reuse_new[256*23+256];static unsigned reuse_len=0;static bool reuse_valid=false;
 unsigned key_len=0;bool key_ok=sprites_on&&scenery_enabled;
 if(key_ok){
  reuse_new[key_len++]=uint16_t(car_blocks);reuse_new[key_len++]=S.blocks;
  for(unsigned b=0;b<S.blocks;b++){reuse_new[key_len++]=uint16_t(bcount[b]);for(unsigned n=1;n<=bcount[b];n++)reuse_new[key_len++]=S.palette[b][n];}
  for(unsigned i=0;i<osprites.sprite_count&&key_ok;i++){const osprite&e=osprites.sprite_entries[i];const uint16_t*r=e.data;
   if((r[0]&0xd000)||((r[3]>>12)&3)!=3||scenery_is_car(e.scratch))continue;
   if(key_len+23>sizeof reuse_new/2){key_ok=false;break;}
   for(unsigned k=0;k<7;k++)reuse_new[key_len++]=r[k];
   const unsigned base=COLOR_BASE_SCENERY+((r[5]&127)<<4);for(unsigned n=0;n<16;n++)reuse_new[key_len++]=pal[base|n];}
 }
 const bool reuse=key_ok&&!learn&&reuse_valid&&key_len==reuse_len&&!memcmp(reuse_new,reuse_key,key_len*2);
 // 1. capture
#ifdef OR_X68K
 uint32_t tc=platform_fine_clock();
#endif
 // r026c: the buffer is cleared by the tile pass (each non-empty 16-pixel run is zeroed after use)
 int low=224,high=0;
 // tiles that a captured sprite may have drawn into (bit tx of row ty); the tile pass
 // visits only these (a tile no sprite reached has no pixels: same result, no reads)
 uint32_t tile_live[14]={0};
#ifndef OR_X68K
 scenery_entry_count=0;
#endif
 for(unsigned i=0;i<osprites.sprite_count;i++){
  const osprite&e=osprites.sprite_entries[i];const uint16_t*r=e.data;
  // the game draws only priority-8 sprites (video.cpp render(8))
  if(!sprites_on)break;
  ++scenery_diag[0];if(!scenery_enabled){++scenery_diag[1];continue;}if(r[0]&0xd000){++scenery_diag[2];continue;}
  if(((r[3]>>12)&3)!=3){++scenery_diag[3];continue;}if(scenery_is_car(e.scratch)){++scenery_diag[4];continue;}++scenery_diag[5];
  int top=(r[0]&511)-256,rows=(r[5]>>8)+1,bottom=top+((r[4]&0x8000)?rows-1:1-rows);
  low=std::min(low,std::min(top,bottom));high=std::max(high,std::max(top,bottom)+1);
  // r026e: drawn straight into palette-block codes ((block+1)<<4|index); colours not planned
  // keep their palette index (>= 0x800) and are placed in the tile pass. Shadow pixels are left
  // out (no darkening on the X68000).
  if(!reuse){uint16_t table[16];const unsigned base=COLOR_BASE_SCENERY+((r[5]&127)<<4);const bool shadow=(r[3]&0x4000)!=0;
   for(unsigned k=0;k<16;k++){if(k==0||k==15||(shadow&&k==10)){table[k]=0;continue;}unsigned q=scenery_map[base|k];table[k]=uint16_t(q==255?0:q?q:(base|k));}
   int xb[2]={320,-1};video.sprite_layer->capture_entry_table(r,scenery_pixels,table,xb);
   const int x0=std::max(0,xb[0]),x1=std::min(319,xb[1]),y0=std::max(0,std::min(top,bottom)),y1=std::min(223,std::max(top,bottom));
   if(x0<=x1&&y0<=y1){const uint32_t cols=((2u<<(x1>>4))-1)&~((1u<<(x0>>4))-1);for(int ty=y0>>4;ty<=y1>>4;ty++)tile_live[ty]|=cols;}}
#ifndef OR_X68K
  video.sprite_layer->capture_entry(r,scenery_std);
#endif
#ifndef OR_X68K
  if(scenery_entry_count<256)scenery_entry_list[scenery_entry_count++]=i;
#endif
 }
 low=std::max(0,low);high=std::min(224,high);scenery_low=low;scenery_high=high;
 if(low<high)++scenery_diag[6];
#ifndef OR_X68K
 if(std::getenv("STAGE1_CHECK_CAR")&&scenery_entry_count){
  uint16_t saved[2048],test[2048]={0};static uint16_t reference[320*224];unsigned n=std::min(scenery_entry_count,127u);
  video.sprite_layer->save_render_state(saved);
  for(unsigned k=0;k<n;k++)memcpy(test+1024+k*8,osprites.sprite_entries[scenery_entry_list[k]].data,14);
  test[1024+n*8]=0x8000;video.sprite_layer->load_render_state(test);
  memset(reference,0,sizeof reference);uint16_t*previous=video.pixels;video.pixels=reference;
  video.sprite_layer->render_reference(8);video.pixels=previous;video.sprite_layer->load_render_state(saved);
  if(n==scenery_entry_count&&memcmp(reference,scenery_std,sizeof reference)){fprintf(stderr,"SCENERY REFERENCE FAIL\n");std::exit(16);}
 }
#endif
#ifdef OR_X68K
 scenery_fine_capture+=platform_fine_clock()-tc;
#endif
 // 2+3. r026c: one pass over the 16-pixel runs that have pixels. Colours get their palette block
 // when first met: a bank keeps one block while it has room (a new bank opens a new block while
 // there are free ones, else goes to the block with most room); a full block sends the rest of
 // the bank on to another block. Each tile gets one sprite per block present in it.
#ifdef OR_X68K
 uint32_t tp=platform_fine_clock();
#endif
#ifndef OR_X68K
 static uint16_t check_copy[320*224];memcpy(check_copy,scenery_std,sizeof check_copy);scenery_check_pixels=check_copy;memset(scenery_std,0,sizeof scenery_std);
#endif
 auto lookup=[&](unsigned v)->unsigned{
  const unsigned idx=v&8191;unsigned q=scenery_map[idx];if(q)return q==255?0:q;
  const unsigned bank=idx>>4;const uint16_t col=pal[idx];
  if(bank_block[bank]==-1){bank_block[bank]=-2;bank_list[nbank++]=uint16_t(bank);}
  int b=-1;int cur=bank_block[bank];
  if(cur>=0){for(unsigned n=1;n<=bcount[cur];n++)if(S.palette[cur][n]==col){q=((cur+1)<<4)|n;break;}
   if(!q&&bcount[cur]<15)b=cur;}
  if(!q&&b<0){for(unsigned k=0;k<S.blocks&&!q;k++)for(unsigned n=1;n<=bcount[k];n++)if(S.palette[k][n]==col){q=((k+1)<<4)|n;break;}}
  if(!q&&b<0){if(S.blocks<block_limit){b=S.blocks++;bcount[b]=0;}
   else{unsigned room=0;for(unsigned k=0;k<S.blocks;k++)if(15-bcount[k]>room){room=15-bcount[k];b=int(k);}}}
  if(!q&&b>=0){unsigned n=++bcount[b];S.palette[b][n]=col;q=((b+1)<<4)|n;bank_block[bank]=int8_t(b);}
  if(!q){scenery_map[idx]=255;++S.dropped_banks;}else scenery_map[idx]=uint8_t(q);
  scenery_used[nused++]=uint16_t(idx);
  return q;};
 static uint32_t sprite_hash[SCENERY_MAX];
 // r026d: fast path for colours already placed; colours are learned on every 8th frame only;
 // pixel pairs that are both empty are skipped; one-block tiles are encoded without tests.
 static uint8_t met_q[256];
 // r028: one pass per tile builds the 16 rows of nibbles and the block mask together. Most tiles
 // have one palette block: their pattern is finished by that pass (no second pass over the tile).
 // Tiles with more blocks split all blocks in one more pass. Same output as r026e: colours are
 // looked up in the same pixel order, and the patterns, hashes and sprite order are unchanged.
 static uint16_t block_bit[256];static bool block_bit_init=false;
 if(!block_bit_init){for(unsigned q=0;q<256;q++)block_bit[q]=uint16_t(q>=16?1u<<((q>>4)-1):0);block_bit_init=true;}
#if defined(OR_X68K)&&defined(CBX68_ASM_CHECK)
 const uint32_t chk_bails=asm_check[1];
#endif
 if(!reuse)for(int ty=low/16;ty<(high+15)/16;ty++){
  const unsigned rows=unsigned(std::min(16,224-ty*16));
  for(uint32_t live=tile_live[ty];live;live&=live-1){const unsigned tx=unsigned(__builtin_ctz(live));
  uint32_t rowLR[32];uint32_t*const rowL=rowLR,*const rowR=rowLR+16;unsigned mask=0,rowbits=0;
  uint16_t*const tile_src=scenery_pixels+ty*16*320+tx*16;
#ifdef OR_X68K
  // asm first pass (scenery_tiles.S): whole tile, unless a colour still needs a lookup
  bool rows_done=false;
#ifdef CBX68_ASM_CHECK
  uint32_t chkLR[32];uint32_t chk=0xffffffffu;
  // (learn frames: the asm marks met_q on a copy, compared with the C marks at the end of the frame)
  {chk=learn?scenery_tile_rows_learn(tile_src,rows,chkLR,block_bit,chk_met):scenery_tile_rows(tile_src,rows,chkLR,block_bit);++asm_check[0];if(chk==0xffffffffu)++asm_check[1];}
#else
  {const uint32_t rv=learn?scenery_tile_rows_learn(tile_src,rows,rowLR,block_bit,met_q):scenery_tile_rows(tile_src,rows,rowLR,block_bit);
   if(rv!=0xffffffffu){mask=rv&0xffff;rowbits=rv>>16;rows_done=true;}}
#endif
  if(!rows_done)
#endif
  for(unsigned yy=0;yy<rows;yy++){
   uint16_t*src=tile_src+yy*320;alias_u32*s32=(alias_u32*)src;
   if(!(s32[0]|s32[1]|s32[2]|s32[3]|s32[4]|s32[5]|s32[6]|s32[7])){rowL[yy]=rowR[yy]=0;continue;}
   uint32_t a=0,b=0;
   for(unsigned xx=0;xx<8;xx+=2){unsigned q0=0,q1=0;
    if(s32[xx>>1]){q0=src[xx];q1=src[xx+1];
     if(q0>=256)q0=lookup(q0);   // a colour that was not planned (same order as before)
     if(q1>=256)q1=lookup(q1);
     mask|=block_bit[q0]|block_bit[q1];
     if(learn){met_q[q0]=1;met_q[q1]=1;}}
    a=(a<<8)|((q0&15)<<4)|(q1&15);}
   for(unsigned xx=8;xx<16;xx+=2){unsigned q0=0,q1=0;
    if(s32[xx>>1]){q0=src[xx];q1=src[xx+1];
     if(q0>=256)q0=lookup(q0);
     if(q1>=256)q1=lookup(q1);
     mask|=block_bit[q0]|block_bit[q1];
     if(learn){met_q[q0]=1;met_q[q1]=1;}}
    b=(b<<8)|((q0&15)<<4)|(q1&15);}
   rowL[yy]=a;rowR[yy]=b;rowbits|=1u<<yy;}
#if defined(OR_X68K)&&defined(CBX68_ASM_CHECK)
  if(chk!=0xffffffffu){bool bad=chk!=((uint32_t(rowbits)<<16)|mask);
   for(unsigned yy=0;yy<rows&&!bad;yy++)bad=chkLR[yy]!=rowL[yy]||chkLR[yy+16]!=rowR[yy];
   if(bad)++asm_check[2];}
#endif
  if(!mask){   // r028 fix: pixels whose colour was dropped (no palette room) are cleared too
   for(unsigned yy=0;yy<rows;yy++)if(rowbits&(1u<<yy)){alias_u32*s32=(alias_u32*)(tile_src+yy*320);s32[0]=s32[1]=s32[2]=s32[3]=s32[4]=s32[5]=s32[6]=s32[7]=0;}
   continue;}
  for(unsigned yy=rows;yy<16;yy++)rowL[yy]=rowR[yy]=0;
  if(!(mask&(mask-1))){   // one block: the nibbles are the pattern
   const unsigned bnum=unsigned(__builtin_ctz(mask));
   if(S.count>=SCENERY_MAX){++S.dropped;++scenery_overflow;
    for(unsigned yy=0;yy<rows;yy++)if(rowbits&(1u<<yy)){alias_u32*s32=(alias_u32*)(tile_src+yy*320);s32[0]=s32[1]=s32[2]=s32[3]=s32[4]=s32[5]=s32[6]=s32[7]=0;}
    continue;}
   ScenerySprite&sp=S.sprite[S.count];uint32_t*enc=S.pcg[S.count];
   sp.x=int16_t(tx*16+16);sp.y=int16_t(ty*16+16);sp.block=uint8_t(bnum);sp.row=uint8_t(ty);sp.kept=0;
#if defined(OR_X68K)&&!defined(CBX68_ASM_CHECK)
   sprite_hash[S.count]=scenery_tile_finish(tile_src,rowbits,rowLR,enc);++S.count;
   continue;
#endif
   uint32_t h=2166136261UL;
   for(unsigned yy=0;yy<16;yy++){const uint32_t l=rowL[yy],r=rowR[yy];enc[yy]=l;enc[yy+16]=r;h^=l;h*=16777619UL;h^=r;h*=16777619UL;}
#if defined(OR_X68K)&&defined(CBX68_ASM_CHECK)
   {static uint16_t chk_px[16*16];uint32_t chk_enc[32];for(unsigned yy=0;yy<rows;yy++)memcpy(chk_px+yy*16,tile_src+yy*320,32);
    // run the asm on the real rows, then compare hash, pattern and that exactly the rows in rowbits were cleared
    const uint32_t ah=scenery_tile_finish(tile_src,rowbits,rowLR,chk_enc);++asm_check[3];
    bool bad=ah!=h||memcmp(chk_enc,enc,sizeof chk_enc);
    for(unsigned yy=0;yy<rows&&!bad;yy++)for(unsigned xx=0;xx<16;xx++){const uint16_t want=(rowbits&(1u<<yy))?0:chk_px[yy*16+xx];if(tile_src[yy*320+xx]!=want){bad=true;break;}}
    if(bad)++asm_check[4];}
#endif
   sprite_hash[S.count]=h;++S.count;
   for(unsigned yy=0;yy<rows;yy++)if(rowbits&(1u<<yy)){alias_u32*s32=(alias_u32*)(tile_src+yy*320);s32[0]=s32[1]=s32[2]=s32[3]=s32[4]=s32[5]=s32[6]=s32[7]=0;}
   continue;}
  // several blocks: one sprite per block, in block order (dropped when the list is full)
  unsigned ord[16],blocks_here[16],nb=0;
  for(unsigned b=0,mm=mask;mm;b++,mm>>=1)if(mm&1){ord[b+1]=nb;blocks_here[nb++]=b;}
  unsigned first=S.count,take=0;
  for(unsigned j=0;j<nb;j++){if(S.count>=SCENERY_MAX){++S.dropped;++scenery_overflow;continue;}++S.count;++take;}
  uint32_t hj[16];for(unsigned j=0;j<take;j++){hj[j]=2166136261UL;
   ScenerySprite&sp=S.sprite[first+j];sp.x=int16_t(tx*16+16);sp.y=int16_t(ty*16+16);sp.block=uint8_t(blocks_here[j]);sp.row=uint8_t(ty);sp.kept=0;}
#ifdef OR_X68K
#ifdef CBX68_ASM_CHECK
  // asm second pass on a copy of the tile; compared with the C result below
  static uint16_t split_px[16*320];static uint32_t split_enc[16][32];uint32_t split_hash[16];
  const bool split_chk=chk!=0xffffffffu&&take==nb;
  if(split_chk){uint32_t*encp[16];for(unsigned yy=0;yy<rows;yy++)memcpy(split_px+yy*320,tile_src+yy*320,32);
   for(unsigned j=0;j<nb;j++){memset(split_enc[j],0,128);encp[blocks_here[j]+1]=split_enc[j];}
   scenery_tile_split(split_px,rowbits,encp);for(unsigned j=0;j<nb;j++)split_hash[j]=scenery_pcg_hash(split_enc[j]);++asm_check[5];}
#else
  if(rows_done&&take==nb){uint32_t*encp[16];
   for(unsigned j=0;j<nb;j++){memset(S.pcg[first+j],0,128);encp[blocks_here[j]+1]=S.pcg[first+j];}
   scenery_tile_split(tile_src,rowbits,encp);
   for(unsigned j=0;j<nb;j++)sprite_hash[first+j]=scenery_pcg_hash(S.pcg[first+j]);
   continue;}
#endif
#endif
  for(unsigned yy=0;yy<16;yy++){uint32_t lj[16]={0},rj[16]={0};
   if(rowbits&(1u<<yy)){uint16_t*src=tile_src+yy*320;   // read again: every colour is placed by now
    for(unsigned xx=0;xx<8;xx++){unsigned q=src[xx];if(q>=256)q=lookup(q);if(q){unsigned j=ord[q>>4];lj[j]|=uint32_t(q&15)<<(28-4*xx);}
     q=src[xx+8];if(q>=256)q=lookup(q);if(q){unsigned j=ord[q>>4];rj[j]|=uint32_t(q&15)<<(28-4*xx);}}
    alias_u32*s32=(alias_u32*)src;s32[0]=s32[1]=s32[2]=s32[3]=s32[4]=s32[5]=s32[6]=s32[7]=0;}
   for(unsigned j=0;j<take;j++){uint32_t*enc=S.pcg[first+j];enc[yy]=lj[j];enc[yy+16]=rj[j];hj[j]^=lj[j];hj[j]*=16777619UL;hj[j]^=rj[j];hj[j]*=16777619UL;}}
  for(unsigned j=0;j<take;j++)sprite_hash[first+j]=hj[j];
#if defined(OR_X68K)&&defined(CBX68_ASM_CHECK)
  if(split_chk){bool bad=false;for(unsigned j=0;j<nb;j++)if(split_hash[j]!=hj[j]||memcmp(split_enc[j],S.pcg[first+j],128))bad=true;
   for(unsigned yy=0;yy<rows&&!bad;yy++)if(rowbits&(1u<<yy))for(unsigned xx=0;xx<16;xx++)if(split_px[yy*320+xx])bad=true;
   if(bad)++asm_check[6];}
#endif
  }
 }
 // learn which colours each bank uses (planned and met), then clear the tables
 for(unsigned k=0;k<nused;k++){unsigned idx=scenery_used[k],q=scenery_map[idx];if(learn&&q&&q!=255&&met_q[q])bank_seen_mask[idx>>4]|=uint16_t(1u<<(idx&15));scenery_map[idx]=0;}
#if defined(OR_X68K)&&defined(CBX68_ASM_CHECK)
 // asm marks must be C marks; with no tile handed back to C this frame they must be all of them
 if(learn){bool bad=false;for(unsigned q=1;q<256;q++)if(chk_met[q]?!met_q[q]:(met_q[q]&&chk_bails==asm_check[1]))bad=true;
  ++asm_check[7];if(bad)++asm_check[2];memset(chk_met,0,sizeof chk_met);}
#endif
 if(learn)memset(met_q,0,sizeof met_q);
 for(unsigned k=0;k<nbank;k++)bank_block[bank_list[k]]=-1;
 for(unsigned b=0;b<S.blocks;b++)for(unsigned n=bcount[b]+1;n<16;n++)S.palette[b][n]=0;
 // r030: keep this frame's tiles for a frame with the same sprites, or take the kept ones
 {static uint16_t c_count,c_blocks,c_dropped,c_dropped_banks;static uint16_t c_palette[13][16];
  static ScenerySprite c_sprite[SCENERY_MAX];static uint32_t c_pcg[SCENERY_MAX][32],c_hash[SCENERY_MAX];
  if(reuse){S.count=c_count;S.blocks=c_blocks;S.dropped=c_dropped;S.dropped_banks=c_dropped_banks;memcpy(S.palette,c_palette,sizeof c_palette);
   memcpy(S.sprite,c_sprite,S.count*sizeof c_sprite[0]);memcpy(S.pcg,c_pcg,S.count*sizeof c_pcg[0]);memcpy(sprite_hash,c_hash,S.count*4);++scenery_reuse_frames;}
  else if(key_ok){c_count=S.count;c_blocks=S.blocks;c_dropped=S.dropped;c_dropped_banks=S.dropped_banks;memcpy(c_palette,S.palette,sizeof c_palette);
   memcpy(c_sprite,S.sprite,S.count*sizeof c_sprite[0]);memcpy(c_pcg,S.pcg,S.count*sizeof c_pcg[0]);memcpy(c_hash,sprite_hash,S.count*4);
   memcpy(reuse_key,reuse_new,key_len*2);reuse_len=key_len;reuse_valid=true;}
  else reuse_valid=false;}
#ifdef OR_X68K
 uint32_t ts=platform_fine_clock();scenery_fine_tiles+=ts-tp;
#endif
 // 4. sprite numbers and PCG patterns, reused down the screen. r026: the car's patterns are in
 // the same pool (the car is in the lower rows, the roadside objects mostly higher up), and
 // tiles with the same pixels share one pattern.
 const unsigned cap_p=std::min(256u,unsigned(128-(road_atlas.tiles+3)/4+26));
 const unsigned nS=128>car_sprites?128-car_sprites:0,nP=cap_p;
 static int8_t s_last[128],p_last[256];static int16_t p_content[256];static uint32_t p_hash[256];
 for(unsigned k=0;k<nS;k++)s_last[k]=-100;for(unsigned k=0;k<nP;k++){p_last[k]=-100;p_content[k]=-1;}
 // resident patterns by hash (64 chains), so a tile finds a pattern with the same pixels
 // without testing every resident one. The lowest matching number is taken, as before.
 static int16_t p_chain_head[64],p_chain_next[256];for(unsigned b=0;b<64;b++)p_chain_head[b]=-1;
 auto chain_of=[](uint32_t h)->unsigned{return (h^(h>>6)^(h>>12)^(h>>18)^(h>>24))&63;};
 unsigned fresh_s=0,fresh_p=0;
 static uint16_t sq[256],pq[512];static int8_t pq_last[512];unsigned sq_h=0,sq_t=0,pq_h=0,pq_t=0;
 int room4=0,room34=0;
 static uint16_t line_load[SCENERY_LINES];memset(line_load,0,sizeof line_load);
 // r027: text palette of each score/time band after the first, in the empty text row above it
 static int16_t hud_line[28];
 for(unsigned b=1;b<hud.bands;b++){hud_line[b]=int16_t(hud.band_first[b]*8-4);line_load[hud_line[b]]+=10;}
 auto find_line=[&](int from,int z,unsigned need,int&ptr)->int{int l=std::max(from,ptr);const bool at=l==ptr;
  for(;l<=z;l++){if(line_load[l]+need<=SCENERY_LINE_BUDGET)return l;if(at)ptr=l+1;}return -1;};
 static SceneryJob jobs[2*(SCENERY_MAX+128)];unsigned njobs=0;
 static uint16_t p_of[SCENERY_MAX+128];
 // users: car tiles (index SCENERY_MAX+i) and roadside sprites, row by row, car first
 static uint16_t users[SCENERY_MAX+128];unsigned nu=0;
 // one counting pass instead of one pass per row (same order: car tiles, then the rest)
 {unsigned start[15]={0};uint8_t car_row[128];
  for(unsigned i=0;i<car_sprites;i++){const unsigned r=unsigned(car.registers[i][1]-16)/16;car_row[i]=uint8_t(r<14?r:14);if(r<14)++start[r+1];}
  for(unsigned i=0;i<S.count;i++)if(S.sprite[i].row<14)++start[S.sprite[i].row+1];
  for(unsigned r=1;r<15;r++)start[r]+=start[r-1];
  nu=start[14];
  unsigned car_end[14];for(unsigned r=0;r<14;r++)car_end[r]=start[r];
  for(unsigned i=0;i<car_sprites;i++)if(car_row[i]<14)users[car_end[car_row[i]]++]=uint16_t(SCENERY_MAX+i);
  for(unsigned i=0;i<S.count;i++){const unsigned r=S.sprite[i].row;if(r<14)users[car_end[r]++]=uint16_t(i);}
 }
 static uint32_t car_pcg32[128][32];
 for(unsigned i=0;i<car_sprites;i++){const uint8_t*b=car.pcg[car.pattern_index[i]];
#if defined(__BYTE_ORDER__)&&__BYTE_ORDER__==__ORDER_BIG_ENDIAN__
  memcpy(car_pcg32[i],b,128);   // the bytes already are the big-endian longs
#else
  for(unsigned q=0;q<32;q++)car_pcg32[i][q]=(uint32_t(b[q*4])<<24)|(uint32_t(b[q*4+1])<<16)|(uint32_t(b[q*4+2])<<8)|b[q*4+3];
#endif
 }
 auto pcg_of=[&](unsigned u)->const uint32_t*{return u>=SCENERY_MAX?car_pcg32[u-SCENERY_MAX]:S.pcg[u];};
 auto content_id=[&](unsigned u)->int{return u>=SCENERY_MAX?int(SCENERY_MAX+car.pattern_index[u-SCENERY_MAX]):int(u);};
 S.car_dropped=0;
 for(unsigned q=0;q<nu;q++){const unsigned u=users[q];const bool is_car=u>=SCENERY_MAX;
  const int r=is_car?int(car.registers[u-SCENERY_MAX][1]-16)/16:S.sprite[u].row;
  const uint32_t*mine=pcg_of(u);
  uint32_t hash;if(is_car){hash=2166136261UL;for(unsigned yy=0;yy<16;yy++){const unsigned w=(yy>>3)*8+(yy&7);hash^=mine[w];hash*=16777619UL;hash^=mine[w+16];hash*=16777619UL;}}else hash=sprite_hash[u];
  int sn=-1,pn=-1;bool sre=false,pre=false,shared=false;
  if(!is_car){
   if(fresh_s<nS)sn=int(fresh_s);
   else if(sq_h!=sq_t&&s_last[sq[sq_h&255]]<=r-2){sn=sq[sq_h&255];sre=true;}
  }
  // a resident pattern with the same pixels
  for(int k=p_chain_head[chain_of(hash)];k>=0;k=p_chain_next[k])
   if(p_hash[k]==hash&&(pn<0||k<pn)&&!memcmp(pcg_of(unsigned(p_content[k])),mine,128)){pn=k;shared=true;}
  if(pn<0&&fresh_p<nP)pn=int(fresh_p);
  if(pn<0){
   // oldest first; one whose last row grew by sharing goes to the back again
   for(unsigned guard=0;pq_h!=pq_t&&guard<512&&pn<0;guard++){unsigned k=pq[pq_h&511];
    if(p_last[k]!=pq_last[pq_h&511]){pq[pq_t&511]=uint16_t(k);pq_last[pq_t&511]=p_last[k];++pq_t;++pq_h;continue;}
    if(p_last[k]<=r-2){pn=int(k);pre=true;}else break;}
  }
  if((!is_car&&sn<0)||pn<0){if(is_car){++S.car_dropped;car.registers[u-SCENERY_MAX][3]=0;}else ++S.dropped;continue;}
  int ls=-1,lp=-1;
  if(sre||pre){
   const int z=16*r-3;
   if(sre)ls=find_line(16*(s_last[sn]+1)+3,z,4,room4);
   if(sre&&ls<0){if(is_car){++S.car_dropped;car.registers[u-SCENERY_MAX][3]=0;}else ++S.dropped;continue;}
   if(sre)line_load[ls]+=4;
   if(pre)lp=find_line(16*(p_last[pn]+1)+3,z,34,room34);
   if(pre&&lp<0){if(sre)line_load[ls]-=4;if(is_car){++S.car_dropped;car.registers[u-SCENERY_MAX][3]=0;}else ++S.dropped;continue;}
   if(pre)line_load[lp]+=34;
   ++S.reused;
  }
  if(!shared&&!pre)fresh_p=std::max(fresh_p,unsigned(pn)+1);
  if(!is_car&&!sre)fresh_s=std::max(fresh_s,unsigned(sn)+1);
  if(pre)++pq_h;                                   // taken from the head
  if(sre)++sq_h;
  if(!is_car){sq[sq_t&255]=uint16_t(sn);++sq_t;}
  if(!shared){pq[pq_t&511]=uint16_t(pn);pq_last[pq_t&511]=int8_t(std::max<int>(p_last[pn]==-100?r:p_last[pn],r));++pq_t;}
  p_last[pn]=int8_t(std::max<int>(p_last[pn],r));p_of[u]=uint16_t(pn);
  if(!shared){
   if(p_content[pn]>=0){int16_t*at=&p_chain_head[chain_of(p_hash[pn])];while(*at!=int16_t(pn))at=&p_chain_next[*at];*at=p_chain_next[pn];}
   p_content[pn]=int16_t(u);p_hash[pn]=hash;
   {const unsigned c=chain_of(hash);p_chain_next[pn]=p_chain_head[c];p_chain_head[c]=int16_t(pn);}
   jobs[njobs++]={int16_t(lp),1,0,uint16_t(pn),uint16_t(u)};}
  else ++S.shared;
  if(is_car){unsigned i=u-SCENERY_MAX;car.registers[i][2]=uint16_t((car.registers[i][2]&0xff00)|(car_pattern_slot(pn)&255));}
  else{s_last[sn]=int8_t(r);S.sprite[u].kept=1;++S.kept;jobs[njobs++]={int16_t(ls),0,0,uint16_t(sn),uint16_t(u)};}
 }
 // 5. job lists: vertical blank first, then one list per raster interrupt
 unsigned off=0;
 auto emit=[&](const SceneryJob&j){
  if(!j.pcg){const ScenerySprite&sp=S.sprite[j.sprite];unsigned n=car_sprites+j.res,pattern=car_pattern_slot(p_of[j.sprite]);
   S.jobs[off++]=0xeb0000UL+n*8;S.jobs[off++]=1;
   S.jobs[off++]=(uint32_t(uint16_t(sp.x))<<16)|uint16_t(sp.y);
   S.jobs[off++]=(uint32_t(((S.first_block+sp.block)<<8)|(pattern&255))<<16)|3u;}
  else{unsigned pattern=car_pattern_slot(j.res);const uint32_t*p=pcg_of(j.sprite);
   S.jobs[off++]=0xeb8000UL+pattern*128;S.jobs[off++]=31;
   memcpy(S.jobs+off,p,128);off+=32;}
 };
 static int16_t head[SCENERY_LINES+1],next_job[2*(SCENERY_MAX+128)];
 for(unsigned l=0;l<=SCENERY_LINES;l++)head[l]=-1;
 for(int k=int(njobs)-1;k>=0;k--){unsigned l=jobs[k].line<0?SCENERY_LINES:unsigned(jobs[k].line);next_job[k]=head[l];head[l]=int16_t(k);}
 auto emit_pal=[&](unsigned b){S.jobs[off++]=0xe82200UL;S.jobs[off++]=7;for(unsigned q=0;q<8;q++)S.jobs[off++]=(uint32_t(hud.band_pal[b][q*2])<<16)|hud.band_pal[b][q*2+1];};
 S.start_job=scenery_ref(S,off);
 if(hud.active&&hud.bands)emit_pal(0);
 for(int k=head[SCENERY_LINES];k>=0;k=next_job[k])emit(jobs[k]);
 S.jobs[off++]=0;
 for(unsigned l=0;l<224;l++){if(!line_load[l])continue;
  S.line_job[l]=scenery_ref(S,off);
  for(unsigned b=1;b<hud.bands;b++)if(hud_line[b]==int(l))emit_pal(b);
  for(int k=head[l];k>=0;k=next_job[k])emit(jobs[k]);
  S.jobs[off++]=0;
  S.line_longs_max=std::max<unsigned>(S.line_longs_max,line_load[l]);}
 (void)content_id;
}
#if defined(OR_X68K)&&defined(CBX68_TRACE)
// per frame: scenery, capture, tiles (50us units), sprites, game sprite entries, dropped -> FRAMES.BIN
static uint16_t frame_trace[4096][6];static unsigned frame_trace_n=0;
#endif
static void build_scenery(SceneryPacket&S,const uint16_t*pal,CarPacket&car,const HudPacket&hud,bool sprites_on){
#ifdef OR_X68K
 uint32_t t=platform_fine_clock();
#ifdef CBX68_TRACE
 const uint32_t trace_c=scenery_fine_capture,trace_t=scenery_fine_tiles;
#endif
#endif
 build_scenery_body(S,pal,car,hud,sprites_on);
#ifdef OR_X68K
 uint32_t d=platform_fine_clock()-t;scenery_fine_total+=d;if(d>scenery_fine_max)scenery_fine_max=d;
#ifdef CBX68_TRACE
 if(frame_trace_n<4096){uint16_t*r=frame_trace[frame_trace_n++];r[0]=uint16_t(d);r[1]=uint16_t(scenery_fine_capture-trace_c);r[2]=uint16_t(scenery_fine_tiles-trace_t);
  r[3]=S.count;r[4]=uint16_t(osprites.sprite_count);r[5]=uint16_t(S.dropped);}
#endif
#endif
 ++scenery_frames;scenery_car_dropped+=S.car_dropped;scenery_shared_total+=S.shared;scenery_count_total+=S.count;scenery_dropped_total+=S.dropped;scenery_reused_total+=S.reused;scenery_bank_drops+=S.dropped_banks;
 scenery_peak=std::max<unsigned>(scenery_peak,S.count);scenery_line_peak=std::max<unsigned>(scenery_line_peak,S.line_longs_max);scenery_block_peak=std::max<unsigned>(scenery_block_peak,S.blocks);
}
// Raster/vblank side. scenery_scan points at the job pointer of the next raster interrupt.
static uint32_t scenery_zero[SCENERY_LINES];
extern "C" {volatile uint32_t scenery_scan=0;void scenery_vblank(const RoadRow*rows);}
static void scenery_run(const uint32_t*j){
 while(uint32_t dst=*j++){uint32_t n=*j++ +1;
#ifdef OR_X68K
  volatile uint32_t*d=(volatile uint32_t*)dst;while(n--)*d++=*j++;
#else
  (void)dst;j+=n;
#endif
 }
}
extern "C" void scenery_vblank(const RoadRow*rows){
 scenery_scan=(uint32_t)(uintptr_t)scenery_zero;
 if(!rows)return;
 const RoadFrame*f=(const RoadFrame*)rows;
 if(!(f->visual.flags&2)&&!f->visual.hud.active)return;
 const SceneryPacket&S=f->visual.scenery;
#ifdef OR_X68K
 if(S.start_job)scenery_run((const uint32_t*)S.start_job);
#endif
 scenery_scan=(uint32_t)(uintptr_t)S.line_job;
}
#ifndef OR_X68K
// PC: run the job lists on a model of the sprite registers and PCG, draw every line with the
// state at that line, and compare with the kept sprites drawn directly. Also the kept pixels
// against the captured colours, and the capture against the reference sprite renderer.
static void scenery_preview(const RoadFrame&f,uint16_t*out){
 const SceneryPacket&S=f.visual.scenery;const CarPacket&car=f.visual.car;
 const bool car_hw=!car.overflow&&!car.text_mode;const unsigned car_sprites=car_hw?car.count:0;
 static uint16_t reg[128][4];static uint8_t pcg[32768];
 for(unsigned n=0;n<128;n++)reg[n][3]=0;
 for(unsigned n=0;n<car_sprites;n++)for(unsigned w=0;w<4;w++)reg[n][w]=car.registers[n][w];   // commit
 auto run=[&](uint32_t ref){if(!ref)return;const uint32_t*j=S.jobs+(ref-1);
  while(uint32_t dst=*j++){uint32_t n=*j++ +1;
   for(uint32_t k=0;k<n;k++,dst+=4){uint32_t v=*j++;
    if(dst>=0xeb0000&&dst<0xeb0400){unsigned w=(dst-0xeb0000)/2;if(w/4<car_sprites){fprintf(stderr,"SCENERY FAIL writes car sprite\n");std::exit(16);}reg[w/4][w%4]=v>>16;reg[(w+1)/4][(w+1)%4]=v&65535;}
    else if(dst>=0xeb8000&&dst<0xec0000){unsigned a=dst-0xeb8000;pcg[a]=v>>24;pcg[a+1]=v>>16;pcg[a+2]=v>>8;pcg[a+3]=v;}
    else if(dst>=0xe82200&&dst<0xe82220){}   // r027 text palette (checked by the HUD preview)
    else{fprintf(stderr,"SCENERY FAIL address %x\n",dst);std::exit(16);}}}};
 run(S.start_job);
 static uint32_t expected[320*224],shown[320*224];memset(expected,0,sizeof expected);memset(shown,0,sizeof shown);
 for(unsigned i=0;i<S.count;i++){const ScenerySprite&sp=S.sprite[i];if(!sp.kept)continue;
  for(unsigned yy=0;yy<16;yy++)for(unsigned xx=0;xx<16;xx++){int x=sp.x-16+int(xx),y=sp.y-16+int(yy);if(x<0||x>=320||y<0||y>=224)continue;
   unsigned n=(S.pcg[i][(xx>>3)*16+yy]>>(28-4*(xx&7)))&15;
   if(n){if(expected[y*320+x]){fprintf(stderr,"SCENERY FAIL overlap\n");std::exit(16);}expected[y*320+x]=S.palette[sp.block][n]|0x10000u;}}}
 static uint32_t scenery_only[320*224];memcpy(scenery_only,expected,sizeof expected);
 for(unsigned i=0;i<car_sprites;i++){if(!car.registers[i][3])continue;unsigned slot=((car.registers[i][2]>>8)&15)-3;int tx=int(car.registers[i][0])-16,ty=int(car.registers[i][1])-16;
  for(int yy=0;yy<16;yy++)for(int xx=0;xx<16;xx++){int x=tx+xx,y=ty+yy;if(x<0||x>=320||y<0||y>=224)continue;
   unsigned b=car.pcg[car.pattern_index[i]][((xx>>3)*2+(yy>>3))*32+(yy&7)*4+((xx&7)>>1)],n=xx&1?b&15:b>>4;
   if(n)expected[y*320+x]=car.palette[slot][n]|0x10000u;}}
 for(unsigned y=0;y<224;y++){
  run(S.line_job[y]);
  for(int n=127;n>=0;n--){if(!reg[n][3])continue;int top=int(reg[n][1])-16;if(int(y)<top||int(y)>=top+16)continue;
   unsigned yy=y-top,pattern=reg[n][2]&255,block=(reg[n][2]>>8)&15;
   const bool is_car=n<int(car_sprites);
   if(is_car?(block<3||block>=3+car.palettes):(block<S.first_block||block>=S.first_block+S.blocks)){fprintf(stderr,"SCENERY FAIL block\n");std::exit(16);}
   for(unsigned xx=0;xx<16;xx++){int x=int(reg[n][0])-16+int(xx);if(x<0||x>=320)continue;
    unsigned b=pcg[pattern*128+((xx>>3)*2+(yy>>3))*32+(yy&7)*4+((xx&7)>>1)],v=xx&1?b&15:b>>4;
    if(v)shown[y*320+x]=(is_car?car.palette[block-3][v]:S.palette[block-S.first_block][v])|0x10000u;}}
 }
 for(unsigned i=0;i<320*224;i++){
  if(shown[i]!=expected[i]){fprintf(stderr,"SCENERY FAIL raster x=%u y=%u\n",i%320,i/320);std::exit(16);}
  unsigned v=scenery_check_pixels[i];if(v==S16_PALETTE_ENTRIES)v=0;
  if(v>=S16_PALETTE_ENTRIES){++scenery_shadow_pixels;if(scenery_only[i])out[i]=uint16_t(shown[i]);continue;}   // darkened by a shadow: shown undarkened
  if(v){++scenery_pixels_total;if(!scenery_only[i])++scenery_dropped_pixels;else if((scenery_only[i]&0xffff)!=scenery_palette[v&8191]){fprintf(stderr,"SCENERY FAIL colour x=%u y=%u\n",i%320,i/320);std::exit(16);}}
  else if(scenery_only[i]){fprintf(stderr,"SCENERY FAIL extra pixel\n");std::exit(16);}
  if(scenery_only[i]&&shown[i])out[i]=uint16_t(shown[i]);
 }
}
#endif
// ---------------------------------------------------------------------------
// r027 score / time / speed etc. (the game's text layer, priority 1, as the original draws it)
// on the X68000 text plane (4 bit planes, in front of sprites and graphics).
// The text layer is drawn into a 320x224 buffer by the game's own renderer; only cells whose
// text RAM word changed are converted. The text plane has 15 colours, the score line alone
// uses up to 17, so the text palette is changed down the screen: text rows with something in
// them form bands (separated by empty rows); each band has its own 15 colours, written by the
// raster interrupt in the middle of the empty row above it. A band that needs more than 15
// colours shows the extra ones in the nearest of its colours.
// ---------------------------------------------------------------------------
// r027b: a text cell's 64 pixel values straight from the tile rows (as render_text_layer draws them:
// value = colour group*8 + pixel, 0 = transparent), so the whole layer is no longer drawn every frame.
static bool hud_cell_pixels(unsigned my,unsigned cx,unsigned bank,uint16_t*px){
 const uint8_t*tr=video.tile_layer->text_ram;unsigned i=my*128+(cx+24)*2;unsigned code=(tr[i]<<8)|tr[i+1];
 if(!((code>>15)&1))return false;
 const unsigned colour=(code>>9)&7;unsigned t=((code&0x1ff)+bank*0x1000)&0x1fff;if(!t)return false;
 bool any=false;
 for(unsigned r=0;r<8;r++){uint32_t w=video.tile_layer->text_row(t,r);
  for(unsigned x=0;x<8;x++){unsigned c=(w>>(28-4*x))&15;px[r*8+x]=uint16_t(c?colour*8+c:0);if(c)any=true;}}
 return any;
}
static uint16_t hud_code[28][40];static bool hud_valid=false;static unsigned hud_age=0;
static uint32_t hud_rows_mask=0;static unsigned hud_bank=999;
static uint8_t hud_row_band[28];static uint8_t hud_band_first[28];static unsigned hud_bands=0;
static uint16_t hud_band_idx[28][16];static uint8_t hud_band_n[28];
static uint16_t hud_band_aidx[28][48];static uint8_t hud_band_aslot[28][48],hud_band_an[28];
static uint32_t hud_frames=0,hud_cells_total=0,hud_full_total=0,hud_approx_total=0;static unsigned hud_cells_peak=0,hud_bands_peak=0;
uint32_t hud_fine_total=0;
static unsigned hud_colour_distance(uint16_t a,uint16_t b){
 int d=0;for(unsigned sh=1;sh<16;sh+=5){int x=int((a>>sh)&31)-int((b>>sh)&31);d+=x*x;}return unsigned(d);}
static unsigned hud_slot(unsigned b,unsigned v,const uint16_t*pal){
 for(unsigned s=1;s<=hud_band_n[b];s++)if(hud_band_idx[b][s]==v)return s;
 for(unsigned k=0;k<hud_band_an[b];k++)if(hud_band_aidx[b][k]==v)return hud_band_aslot[b][k];
 const uint16_t c=pal[v];
 unsigned s=0;
 for(unsigned t=1;t<=hud_band_n[b]&&!s;t++)if(pal[hud_band_idx[b][t]]==c)s=t;          // same colour
 if(!s&&hud_band_n[b]<15){s=++hud_band_n[b];hud_band_idx[b][s]=uint16_t(v);return s;}
 if(!s){unsigned best=~0u;for(unsigned t=1;t<=hud_band_n[b];t++){unsigned d=hud_colour_distance(pal[hud_band_idx[b][t]],c);if(d<best){best=d;s=t;}}++hud_approx_total;}
 if(hud_band_an[b]<48){hud_band_aidx[b][hud_band_an[b]]=uint16_t(v);hud_band_aslot[b][hud_band_an[b]++]=uint8_t(s);}
 return s;
}
static void build_hud_body(HudPacket&H,const uint16_t*pal,bool car_on_text){
 H.count=0;H.full=0;H.overflow=0;H.active=hud_enabled?1:0;H.bands=0;
 if(!hud_enabled){hud_valid=false;return;}
 if(car_on_text){H.active=0;hud_valid=false;return;}   // the car is on the text plane this frame
 const uint8_t*tr=video.tile_layer->text_ram;
 const unsigned bank=video.tile_layer->text_bank();
 static uint8_t changed[28][40];uint32_t rows=0;
 for(unsigned my=0;my<28;my++)for(unsigned cx=0;cx<40;cx++){unsigned i=my*128+(cx+24)*2;uint16_t code=uint16_t((tr[i]<<8)|tr[i+1]);
  changed[my][cx]=!hud_valid||code!=hud_code[my][cx];hud_code[my][cx]=code;
  if((code>>15)&1){unsigned t=((code&0x1ff)+bank*0x1000)&0x1fff;if(t)rows|=1u<<my;}}
 const bool full=!hud_valid||++hud_age>=300||bank!=hud_bank||rows!=hud_rows_mask;
 hud_bank=bank;hud_rows_mask=rows;
 if(full){hud_age=0;hud_valid=true;}
 uint16_t px[64];
 if(full){
  // bands: runs of rows with something in them; colours by use, most used first
  hud_bands=0;
  for(unsigned my=0;my<28;my++){if(!(rows&(1u<<my))){hud_row_band[my]=255;continue;}
   if(my==0||!(rows&(1u<<(my-1)))){hud_band_first[hud_bands]=uint8_t(my);hud_band_n[hud_bands]=0;hud_band_an[hud_bands]=0;++hud_bands;}
   hud_row_band[my]=uint8_t(hud_bands-1);}
  static uint16_t cnt_idx[256],cnt_n[256];
  for(unsigned b=0;b<hud_bands;b++){unsigned nk=0;
   for(unsigned my=hud_band_first[b];my<28&&hud_row_band[my]==b;my++)for(unsigned cx=0;cx<40;cx++){if(!hud_cell_pixels(my,cx,bank,px))continue;
    for(unsigned q=0;q<64;q++){unsigned v=px[q];if(!v)continue;
    unsigned k=0;for(;k<nk;k++)if(cnt_idx[k]==v)break;if(k==nk){if(nk==256)continue;cnt_idx[nk]=uint16_t(v);cnt_n[nk]=0;++nk;}++cnt_n[k];}}
   // most used first
   for(unsigned a=0;a<nk;a++)for(unsigned c=a+1;c<nk;c++)if(cnt_n[c]>cnt_n[a]){std::swap(cnt_n[a],cnt_n[c]);std::swap(cnt_idx[a],cnt_idx[c]);}
   for(unsigned k=0;k<nk;k++)hud_slot(b,cnt_idx[k],pal);
  }
 }
 for(unsigned my=0;my<28;my++)for(unsigned cx=0;cx<40;cx++){if(!full&&!changed[my][cx])continue;
  uint8_t*pl=H.planes[H.count];memset(pl,0,32);const unsigned b=hud_row_band[my];
  if(b!=255&&hud_cell_pixels(my,cx,bank,px))for(unsigned r=0;r<8;r++){const uint16_t*src=px+r*8;
   for(unsigned x=0;x<8;x++){unsigned v=src[x];if(!v)continue;unsigned s=hud_slot(b,v,pal);
    for(unsigned p=0;p<4;p++)if(s&(1u<<p))pl[p*8+r]|=uint8_t(0x80>>x);}}
  H.pos[H.count++]=uint16_t(my*40+cx);}
 H.full=full;H.bands=uint16_t(hud_bands);
 for(unsigned my=0;my<28;my++)H.row_band[my]=hud_row_band[my];
 for(unsigned b=0;b<hud_bands;b++){H.band_first[b]=hud_band_first[b];
  for(unsigned s=0;s<16;s++)H.band_pal[b][s]=s&&s<=hud_band_n[b]?opaque_colour(pal[hud_band_idx[b][s]]):0;}
 ++hud_frames;hud_cells_total+=H.count;if(full)++hud_full_total;
 hud_bands_peak=std::max(hud_bands_peak,hud_bands);hud_cells_peak=std::max<unsigned>(hud_cells_peak,H.count);
}
static void build_hud(HudPacket&H,const uint16_t*pal,bool car_on_text){
#ifdef OR_X68K
 uint32_t t=platform_fine_clock();
#endif
 build_hud_body(H,pal,car_on_text);
#ifdef OR_X68K
 hud_fine_total+=platform_fine_clock()-t;
#endif
}
static void stream_hud(const HudPacket&H){
#ifdef OR_X68K
 for(unsigned k=0;k<H.count;k++){unsigned cy=H.pos[k]/40,cx=H.pos[k]%40;const uint8_t*pl=H.planes[k];
  for(unsigned p=0;p<4;p++){volatile uint8_t*d=(volatile uint8_t*)(0xe00000UL+p*0x20000UL+cy*8*128+cx);for(unsigned r=0;r<8;r++)d[r*128]=pl[p*8+r];}}
#else
 (void)H;
#endif
}
#ifndef OR_X68K
// PC: a model of the text plane, checked against the game's reference text renderer.
static uint8_t hud_model[224*320];
static uint64_t hud_pixels_total=0,hud_pixels_approx=0;
static void hud_preview(const RoadFrame&f,uint16_t*out,const uint16_t*pal){
 const HudPacket&H=f.visual.hud;if(!H.active)return;
 if(H.full)memset(hud_model,0,sizeof hud_model);
 for(unsigned k=0;k<H.count;k++){unsigned cy=H.pos[k]/40,cx=H.pos[k]%40;
  for(unsigned r=0;r<8;r++)for(unsigned x=0;x<8;x++){unsigned s=0;for(unsigned p=0;p<4;p++)if(H.planes[k][p*8+r]&(0x80>>x))s|=1u<<p;hud_model[(cy*8+r)*320+cx*8+x]=uint8_t(s);}}
 // the palette change of each band falls in an empty text row
 for(unsigned b=1;b<H.bands;b++){unsigned r=H.band_first[b];if(!r||H.row_band[r-1]!=255){fprintf(stderr,"HUD FAIL band row\n");std::exit(17);}}
 static uint16_t ref[320*224];memset(ref,0,sizeof ref);video.tile_layer->render_text_layer_reference(ref,1);
 for(unsigned i=0;i<320*224;i++){unsigned s=hud_model[i],v=ref[i];const unsigned row=i/320/8,b=H.row_band[row];
  if(bool(s)!=bool(v)){fprintf(stderr,"HUD FAIL pixel x=%u y=%u\n",i%320,i/320);std::exit(17);}
  if(!v)continue;++hud_pixels_total;
  if(b==255){fprintf(stderr,"HUD FAIL band\n");std::exit(17);}
  const uint16_t shown=H.band_pal[b][s];
  if(shown!=opaque_colour(pal[v])){bool approx=false;for(unsigned k=0;k<hud_band_an[b];k++)if(hud_band_aidx[b][k]==v&&hud_band_aslot[b][k]==s)approx=true;
   if(!approx){fprintf(stderr,"HUD FAIL colour x=%u y=%u\n",i%320,i/320);std::exit(17);}++hud_pixels_approx;}
  if(f.visual.flags)out[i]=shown;}
}
#endif
void visuals_build(RoadFrame&f,const uint16_t*pal){
 VisualFrame&v=f.visual;v.flags=visual_mode;v.pad=tile_word(0xe90)&511;++visual_frames[visual_mode&3];
 background_palette=pal;
 if(visual_mode&1)page_idle_step();   // r024d: a finished page is taken over by background_prepare in the same frame
 bool show_background=(visual_mode&1)&&background_prepare(pal);
 // r030: a full-screen picture that the two layers cannot hold is shown as one 8-bit picture
 const bool show_static=!show_background&&(visual_mode&1)&&static_prepare(pal);
 if(!show_static&&static_on){static_on=false;background_refresh=true;background_palette_refresh=true;}
 if(show_static){v.flags|=4;v.pad=0;++static_frames;}
 near_target=unwrap_target(near_target,192-tile_word(0xe98),1024);
 far_target=unwrap_target(far_target,192-tile_word(0xe9a),1024);
 if(show_background)++background_source_frames;
 v.near_world=near_target;v.far_world=far_target;
 if(show_static)v.near_world=v.far_world=0;   // r030: the picture is at columns 0..319
 for(int y=0;y<224;y++){
  f.rows[y].bg_y=f.rows[y].bg_far_y=tile_word(0xe92)&511;
  f.rows[y].bg_y=v.pad;   // r025: near layer rows on the low-nibble plane (R13), far rows on R15
  if(show_static)f.rows[y].bg_far_y=0;   // r030: both planes at row 0
  // r021: the original draws the road over the tile layers on every line where either
  // road is enabled, and shows the tiles (over the line colour) only where both are off.
  // So the background graphic page is shown only on lines without road BG.
  f.rows[y].control=((show_background||show_static)&&f.mode[y]==0)?0x004f:0x004c; // r022: mode 0 = line without road
 }
 v.car.count=v.car.patterns=v.car.palettes=v.car.overflow=v.car.line_max=v.car.text_mode=0;
 if(visual_mode&2)build_car(v.car,pal);
 build_hud(v.hud,pal,v.car.text_mode!=0);   // r027: before the scenery jobs (its palette changes go into them)
 build_scenery(v.scenery,pal,v.car,v.hud,(visual_mode&2)!=0);   // r026
#ifndef OR_X68K
 if(show_background||(visual_mode&2))scenery_measure(pal,v.car);
#endif
 for(int y=0;y<224;y++)if(v.car.text_mode||v.hud.active)f.rows[y].control|=0x20;
}
#ifdef OR_X68K
static void commit_body(const RoadFrame*f);
extern "C" void visuals_native_commit(const RoadFrame*f){
 unsigned a=*(volatile uint8_t*)0xe88023;
 commit_body(f);
 unsigned b=*(volatile uint8_t*)0xe88023;
 unsigned d=a>=b?a-b:a+200-b; // down-counter, at most one reload while masked
 commit_counts_total+=d;if(d>commit_counts_max)commit_counts_max=d;++commit_calls;
}
static void commit_body(const RoadFrame*f){
#else
extern "C" void visuals_native_commit(const RoadFrame*f){
#endif
#ifdef OR_X68K
 *(volatile uint16_t*)0xe80018=f->visual.far_world&511;*(volatile uint16_t*)0xe8001c=f->visual.far_world&511;
 // r025: the low nibble (near layer) scrolls on its own; R14 above keeps the far layer
 *(volatile uint16_t*)0xe80018=f->visual.near_world&511;
 const CarPacket&c=f->visual.car;
 const HudPacket&hud=f->visual.hud;
 // r027: text plane in front of sprites when it shows the score etc.
 *(volatile uint16_t*)0xe82500=(c.text_mode||hud.active)?0x12e4:0x06e4;
 if(hud.active){*(volatile uint16_t*)0xe80014=0;*(volatile uint16_t*)0xe80016=0;}   // text palette: scenery job lists
 for(unsigned i=0;i<128;i++)*(volatile uint16_t*)(0xeb0006UL+i*8)=0;
 *(volatile uint16_t*)0xe82600=f->rows[0].control;
 // r026: palette blocks of the roadside objects (after the car's blocks)
 if(f->visual.flags&2){const SceneryPacket&S=f->visual.scenery;for(unsigned b=0;b<S.blocks;b++)for(unsigned n=1;n<16;n++)*(volatile uint16_t*)(0xe82200UL+(S.first_block+b)*32+n*2)=opaque_colour(S.palette[b][n]);}
 if(!(f->visual.flags&2)||c.overflow)return;
 if(c.text_mode){
  for(unsigned i=1;i<16;i++)*(volatile uint16_t*)(0xe82200UL+i*2)=opaque_colour(c.palette[0][i]);
  *(volatile uint16_t*)0xe80014=0;*(volatile uint16_t*)0xe80016=c.text_bank*256;text_active_bank=c.text_bank;return;
 }
 for(unsigned i=0;i<c.palettes;i++)for(unsigned n=1;n<=c.groups[i];n++)*(volatile uint16_t*)(0xe82200UL+(i+3)*32+n*2)=opaque_colour(c.palette[i][n]);
 // r026: the car's PCG patterns are written by the scenery job lists (shared pattern pool)
 for(unsigned i=0;i<c.count;i++)for(unsigned n=0;n<4;n++)*(volatile uint16_t*)(0xeb0000UL+i*8+n*2)=c.registers[i][n];
#endif
}
static void visuals_preview_body(const RoadFrame&f,uint16_t*out);
void visuals_preview(const RoadFrame&f,uint16_t*out){
 visuals_preview_body(f,out);
#ifndef OR_X68K
 hud_preview(f,out,background_palette);   // r027: on top
#endif
}
static void visuals_preview_body(const RoadFrame&f,uint16_t*out){
 if(f.visual.flags&4){   // r030: the 8-bit picture
#ifndef OR_X68K
  static uint16_t sref[320*224];const bool check=std::getenv("STAGE1_CHECK_VISUALS")!=0;
  if(check){memset(sref,0,sizeof sref);video.tile_layer->update_tile_values();video.tile_layer->render_tile_layer_reference(sref,1,0);video.tile_layer->render_tile_layer_reference(sref,0,0);}
  for(int y=0;y<224;y++)for(int x=0;x<320;x++){
   const unsigned s=background_ring[y*512+x];
   if(check){const unsigned e=sref[y*320+x];if(bool(e)!=bool(s)||(e&&opaque_colour(background_palette[e&8191])!=static_colour[s])){fprintf(stderr,"STATIC PICTURE FAIL x=%d y=%d\n",x,y);std::exit(8);}}
   if(s&&(f.rows[y].control&3)==3)out[y*320+x]=static_colour[s];
  }
#endif
 }else
 if((f.visual.flags&1)&&background_ready){
#ifndef OR_X68K
  static uint16_t reference[320*224];
  bool check=std::getenv("STAGE1_CHECK_VISUALS")!=0;
  // r024d: while a page in use is still being decoded the old picture is shown on purpose
  bool bg_pages_pending=false;for(unsigned l=0;l<2;l++)for(unsigned q=0;q<4;q++)if(page_stale(bg_layer[l].page[q]))bg_pages_pending=true;
  if(bg_pages_pending)++bg_pending_frames;
  // r024e: the visible part may still be part-way through its rewrite
  bool bg_visible_pending=bg_ring[0].pass_active||bg_ring[1].pass_active;
  if(check){memset(reference,0,sizeof reference);video.tile_layer->update_tile_values();video.tile_layer->render_tile_layer_reference(reference,1,0);video.tile_layer->render_tile_layer_reference(reference,0,0);}
#endif
  for(int y=0;y<224;y++)for(int x=0;x<320;x++){
   unsigned ny=(f.visual.pad+y)&511,fy=(f.rows[y].bg_far_y+y)&511;
   unsigned n=layer_value(0,(f.visual.near_world+x)&1023,ny),a=layer_value(1,(f.visual.far_world+x)&1023,fy);
   unsigned index=n?n:a;
#ifndef OR_X68K
   // r025: each nibble plane against its own layer (near: low nibble at the near ring
   // position, far: high nibble at the far ring position); hardware index f*16+n shows
   // near colour n if n, else far colour f, which is the composed index above.
   if((f.rows[y].control&3)==3){
    unsigned an=background_ring[ny*512+((f.visual.near_world+x)&511)]&15,af=background_ring[fy*512+((f.visual.far_world+x)&511)]>>4;
    if(!bg_ring[0].pass_active&&an!=(n&15)){fprintf(stderr,"BACKGROUND RING FAIL near x=%d y=%d expected=%u actual=%u\n",x,y,n&15,an);std::exit(8);}
    if(!bg_ring[1].pass_active&&af!=a){fprintf(stderr,"BACKGROUND RING FAIL far x=%d y=%d expected=%u actual=%u\n",x,y,a,af);std::exit(8);}
    unsigned shown_index=af*16+an,shown_colour=an?background_assets.palette[16+an]:background_assets.palette[af];
    if(!bg_visible_pending&&shown_index&&shown_colour!=background_assets.palette[index]){fprintf(stderr,"BACKGROUND RING FAIL colour x=%d y=%d\n",x,y);std::exit(8);}
   }
   if(check&&!bg_pages_pending){unsigned expected=reference[y*320+x];if(bool(expected)!=bool(index)||(expected&&background_palette[expected&8191]!=background_assets.palette[index])){fprintf(stderr,"BACKGROUND SOURCE FAIL x=%d y=%d source=%u actual=%u\n",x,y,expected,index);std::exit(14);}}
#endif
   if(index&&(f.rows[y].control&3)==3)out[y*320+x]=background_assets.palette[index];
  }
 }
#ifndef OR_X68K
 if(f.visual.flags&2)scenery_preview(f,out);   // r026: drawn under the car
#endif
 const CarPacket&c=f.visual.car;if(c.overflow)return;
 if(c.text_mode){
  for(unsigned y=0;y<224;y++)for(unsigned x=0;x<320;x++){
   unsigned n=0;for(unsigned plane=0;plane<4;plane++)if(c.text_bits[plane][y*40+x/8]&(0x80>>(x&7)))n|=1<<plane;
   if(n)out[y*320+x]=c.palette[0][n];
#ifndef OR_X68K
   unsigned value=capture_pixels[y*320+x];
   if(bool(value)!=bool(n)||(value&&capture_palette[value&8191]!=c.palette[0][n])){fprintf(stderr,"CAR TEXT FAIL\n");std::exit(13);}
#endif
  }
  return;
 }
 for(unsigned i=0;i<c.count;i++){
  unsigned slot=((c.registers[i][2]>>8)&15)-3;int tx=int(c.registers[i][0])-16,ty=int(c.registers[i][1])-16;
  for(int yy=0;yy<16&&ty+yy<224;yy++)for(int xx=0;xx<16&&tx+xx<320;xx++){
   unsigned b=c.pcg[c.pattern_index[i]][((xx>>3)*2+(yy>>3))*32+(yy&7)*4+((xx&7)>>1)],n=xx&1?b&15:b>>4;
   if(n)out[(ty+yy)*320+tx+xx]=c.palette[slot][n];
  }
 }
#ifndef OR_X68K
 if((f.visual.flags&2)&&!car_last_hit)for(int y=old_low;y<old_high;y++)for(int x=0;x<320;x++){
  unsigned value=capture_pixels[y*320+x];if(!value)continue;
  bool match=false;
  for(unsigned i=0;i<c.count;i++){
   int xx=x-int(c.registers[i][0])+16,yy=y-int(c.registers[i][1])+16;if(xx<0||xx>=16||yy<0||yy>=16)continue;
   unsigned b=c.pcg[c.pattern_index[i]][((xx>>3)*2+(yy>>3))*32+(yy&7)*4+((xx&7)>>1)],n=xx&1?b&15:b>>4;
   if(n){unsigned slot=((c.registers[i][2]>>8)&15)-3;match=c.palette[slot][n]==capture_palette[value&8191];}
  }
  if(!match){fprintf(stderr,"CAR TILE FAIL x=%d y=%d\n",x,y);std::exit(9);}
 }
#endif
}

#ifndef OR_X68K
void visuals_selftest(){puts("BACKGROUND SOURCE CHECK ready");}
#endif

void visuals_print(){
 report_printf("R027 HUD frames=%lu cells avg=%lu max=%u full=%lu bands_max=%u approx=%lu us total=%lu\n",(unsigned long)hud_frames,(unsigned long)(hud_frames?hud_cells_total/hud_frames:0),hud_cells_peak,(unsigned long)hud_full_total,hud_bands_peak,(unsigned long)hud_approx_total,(unsigned long)(hud_fine_total*50UL));
#ifndef OR_X68K
 report_printf("R027 HUD pixels total=%llu approx=%llu\n",(unsigned long long)hud_pixels_total,(unsigned long long)hud_pixels_approx);
#endif
#ifdef CBX68_ASM_CHECK
 report_printf("ASM CHECK tile_rows calls=%lu to_c=%lu mismatches=%lu finish calls=%lu mismatches=%lu split calls=%lu mismatches=%lu\n",(unsigned long)asm_check[0],(unsigned long)asm_check[1],(unsigned long)asm_check[2],(unsigned long)asm_check[3],(unsigned long)asm_check[4],(unsigned long)asm_check[5],(unsigned long)asm_check[6]);
#endif
#ifdef CBX68_TRACE
 {FILE*tf=fopen("FRAMES.BIN","wb");if(tf){fwrite(frame_trace,12,frame_trace_n,tf);fclose(tf);}}
#endif
 report_printf("R026 PHASES us capture=%lu colours=%lu tiles=%lu rest=%lu\n",(unsigned long)(scenery_fine_capture*50UL),(unsigned long)(scenery_fine_colour*50UL),(unsigned long)(scenery_fine_tiles*50UL),(unsigned long)((scenery_fine_total-scenery_fine_capture-scenery_fine_colour-scenery_fine_tiles)*50UL));
 report_printf("R026 DIAG entries=%lu off=%lu hidden=%lu priority=%lu car=%lu passed=%lu frames_rows=%lu count=%u\n",(unsigned long)scenery_diag[0],(unsigned long)scenery_diag[1],(unsigned long)scenery_diag[2],(unsigned long)scenery_diag[3],(unsigned long)scenery_diag[4],(unsigned long)scenery_diag[5],(unsigned long)scenery_diag[6],(unsigned)osprites.sprite_count);
 report_printf("R026 SCENERY frames=%lu sprites avg=%lu max=%u dropped=%lu car_dropped=%lu shared=%lu reused=%lu bank_drops=%lu blocks_max=%u line_longs_max=%u overflow=%lu us total=%lu capture=%lu max=%lu\n",(unsigned long)scenery_frames,(unsigned long)(scenery_frames?scenery_count_total/scenery_frames:0),scenery_peak,(unsigned long)scenery_dropped_total,(unsigned long)scenery_car_dropped,(unsigned long)scenery_shared_total,(unsigned long)scenery_reused_total,(unsigned long)scenery_bank_drops,scenery_block_peak,scenery_line_peak,(unsigned long)scenery_overflow,(unsigned long)(scenery_fine_total*50UL),(unsigned long)(scenery_fine_capture*50UL),(unsigned long)(scenery_fine_max*50UL));
#ifndef OR_X68K
 report_printf("R026 SCENERY pixels total=%llu dropped=%llu shadow_undarkened=%llu\n",(unsigned long long)scenery_pixels_total,(unsigned long long)scenery_dropped_pixels,(unsigned long long)scenery_shadow_pixels);
#endif
#ifndef OR_X68K
 scenery_print();
#endif
#ifdef OR_X68K
 report_printf("R025 BG nibble planes columns near=%lu far=%lu rows near=%lu far=%lu\n",(unsigned long)ring_column_count[0],(unsigned long)ring_column_count[1],(unsigned long)ring_row_count[0],(unsigned long)ring_row_count[1]);
#endif
#ifdef OR_X68K
 report_printf("R024 BG page_decodes=%lu decode_us=%lu layer_updates=%lu remaps=%lu refreshes=%lu update_us total=%lu max=%lu visible_refresh_us total=%lu max=%lu dirty_left=%u\n",(unsigned long)page_decodes,(unsigned long)(page_decode_fine*50UL),(unsigned long)layer_updates,(unsigned long)layer_remaps,(unsigned long)dirty_refreshes,(unsigned long)(rebuild_fine_total*50UL),(unsigned long)(rebuild_fine_max*50UL),(unsigned long)(full_stream_fine_total*50UL),(unsigned long)(full_stream_fine_max*50UL),bg_ring[0].dirty_count+bg_ring[1].dirty_count);
 report_printf("R020 CAR us total=%lu capture=%lu direct=%lu max=%lu cache hits=%lu misses=%lu stores=%lu rejects=%lu entries=%u bytes=%lu\n",(unsigned long)(car_fine_total*50UL),(unsigned long)(car_fine_capture*50UL),(unsigned long)(car_fine_direct*50UL),(unsigned long)(car_fine_max*50UL),(unsigned long)car_cache_hits,(unsigned long)car_cache_misses,(unsigned long)car_cache_stores,(unsigned long)car_cache_rejects,car_cache_entries,(unsigned long)car_cache_bytes);
 report_printf("R019 COMMIT calls=%lu us avg=%lu max=%lu\n",(unsigned long)commit_calls,(unsigned long)(commit_calls?commit_counts_total*50UL/commit_calls:0),(unsigned long)(commit_counts_max*50UL));
#endif
 report_printf("R030 SCENERY REUSE frames=%lu of %lu\n",(unsigned long)scenery_reuse_frames,(unsigned long)scenery_frames);
 report_printf("R030 STATIC PICTURE frames=%lu composes=%lu colours=%u\n",(unsigned long)static_frames,(unsigned long)static_composes,static_ncol);
 report_printf("BACKGROUND source_frames=%lu rebuilds=%lu capacity_waits=%lu recomposed_rows=%lu\n",(unsigned long)background_source_frames,(unsigned long)background_rebuilds,(unsigned long)background_capacity_waits,(unsigned long)background_recomposed_rows);report_printf("BG TRANSFER far_checks=%lu data_errors=%lu palette_errors=%lu\n",(unsigned long)background_far_checks,(unsigned long)background_data_errors,(unsigned long)background_palette_errors);}
