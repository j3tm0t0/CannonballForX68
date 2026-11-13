/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
#include "road_bridge.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
RoadAtlas road_atlas;
// Cannonball for x68: the road layout (formerly ROADHW.DAT, made by build_road_atlas.py) is
// built at start-up from the user's road ROM (opr-10185.11, loaded by the asset loader), so
// nothing made from the ROMs is distributed. Same steps and result as build_road_atlas.py.
#include "assets.hpp"
static int road_bit(const uint8_t*road,unsigned y,unsigned x){
 unsigned c=((road[y*64+x/8]>>(7-x%8))&1)|(((road[0x4000+y*64+x/8]>>(7-x%8))&1)<<1);
 if(x>=248&&x<256&&c==3)c=7;return int(c);}
static unsigned road_samples(double e,int*out){unsigned n=0;out[n++]=0;
 while(out[n-1]<255){int old=out[n-1],v=old+1;while(v+1<=255&&double(v-old)<=e*double(old+7))++v;if(n<256)out[n++]=v;else break;}return n;}
static const int ROAD_VARIANTS[5][4]={{-320,-128,0,192},{-128,0,0,384},{0,192,0,512},{192,384,192,512},{384,512,320,512}};
static int road_sample[80];
static unsigned road_tex(const uint8_t*road,unsigned which,unsigned row,unsigned x){
 if(row>=400)return which==2?0xf1:0;
 const int*v=ROAD_VARIANTS[row/80];const int y=road_sample[row%80];const bool in=int(x)>=v[2]&&int(x)<v[3];const int c=road_bit(road,unsigned(y),x);
 if(which==0)return in?(c==0?1:c==7?4:0):0;
 if(which==1)return in?(c==0?1:c==1?2:c==2?3:c==7?4:0):0;
 return 0xf0|(in?(c==1?2:c==2?3:1):1);}
static void road_atlas_build(const uint8_t*road){
 const unsigned nr=80;static int smp[256];
 double lo=0.,hi=1.;for(int it=0;it<40;it++){double mid=(lo+hi)/2;if(road_samples(mid,smp)>nr)lo=mid;else hi=mid;}
 unsigned n=road_samples(hi,smp);for(unsigned i=0;i<nr;i++)road_sample[i]=i<n?smp[i]:255;
 road_atlas.nr=uint16_t(nr);
 for(unsigned y=0;y<256;y++){unsigned i=0;while(i<nr&&road_sample[i]<int(y))++i;road_atlas.lrow[y]=uint8_t(i);}
 for(int x=-320;x<512;x++){unsigned i=0;while(i<5&&!(ROAD_VARIANTS[i][0]<=x&&x<ROAD_VARIANTS[i][1]))++i;road_atlas.vtab[x+320]=uint16_t(i*nr);}
 // tiles of textures 0 and 1 (BG0/BG1), first tile empty; identical tiles are shared
 static uint16_t bucket[1024];static uint16_t chain[256];for(unsigned i=0;i<1024;i++)bucket[i]=0xffff;
 unsigned tiles=1;memset(road_atlas.pcg,0,32);
 auto hash=[](const uint8_t*t){uint32_t h=2166136261UL;for(unsigned i=0;i<32;i++){h^=t[i];h*=16777619UL;}return unsigned(h&1023);};
 {unsigned h=hash(road_atlas.pcg);chain[0]=bucket[h];bucket[h]=0;}
 for(unsigned bank=1;bank<=2;bank++){uint16_t*map=bank==1?road_atlas.map0:road_atlas.map1;
  for(unsigned ty=0;ty<64;ty++)for(unsigned tx=0;tx<64;tx++){uint8_t t[32];
   for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x+=2)t[y*4+x/2]=uint8_t((road_tex(road,bank-1,ty*8+y,tx*8+x)<<4)|road_tex(road,bank-1,ty*8+y,tx*8+x+1));
   unsigned h=hash(t),code=0xffff;for(unsigned k=bucket[h];k!=0xffff;k=chain[k])if(!memcmp(road_atlas.pcg+k*32,t,32)){code=k;break;}
   if(code==0xffff){if(tiles>=256){code=0;}else{code=tiles++;memcpy(road_atlas.pcg+code*32,t,32);chain[code]=bucket[h];bucket[h]=uint16_t(code);}}
   map[ty*64+tx]=uint16_t(code?(bank<<8)|code:0);}}
 road_atlas.tiles=uint16_t(tiles);
 for(unsigned r=0;r<512;r++)for(unsigned x=0;x<512;x++)road_atlas.ground[r*512+x]=uint8_t(road_tex(road,2,r,x));
}
bool road_assets_load(){
 if(!assets.road.asset)return false;
 road_atlas_build(assets.road.asset);
 if(road_atlas.tiles>256)return false;
#ifndef OR_X68K
 // PC check: identical to the former ROADHW.DAT when that file is present (CBX68_ROADHW_CHECK=1)
 if(std::getenv("CBX68_ROADHW_CHECK")){FILE*f=fopen("ROADHW.DAT","rb");if(!f){puts("ROADHW CHECK: no ROADHW.DAT");return false;}
  static uint8_t a[400000];size_t n=fread(a,1,sizeof a,f);fclose(f);static uint8_t b[400000];size_t m=0;
  auto w=[&](unsigned v){b[m++]=uint8_t(v>>8);b[m++]=uint8_t(v);};
  memcpy(b,"RHW1",4);m=4;w(road_atlas.tiles);w(road_atlas.nr);memcpy(b+m,road_atlas.lrow,256);m+=256;for(unsigned i=0;i<832;i++)w(road_atlas.vtab[i]);
  memcpy(b+m,road_atlas.pcg,road_atlas.tiles*32);m+=road_atlas.tiles*32;for(unsigned i=0;i<4096;i++)w(road_atlas.map0[i]);for(unsigned i=0;i<4096;i++)w(road_atlas.map1[i]);
  memcpy(b+m,road_atlas.ground,512*512);m+=512*512;
  printf("ROADHW CHECK %s bytes=%lu/%lu tiles=%u\n",n==m&&!memcmp(a,b,m)?"MATCH":"DIFFERENT",(unsigned long)m,(unsigned long)n,road_atlas.tiles);}
#endif
 return true;
}
static uint32_t scroll(int col,int tex,int y){return (uint32_t(col&511)<<16)|((tex-y)&511);}
static int column(const HWRoad&r,unsigned which,unsigned index){int c=(r.ramBuff[(which?0x400:0x200)+index]-0x5f8-r.x_offset)&4095;return c&2048?c-4096:c;}
static void colors(const HWRoad&r,unsigned which,unsigned data,unsigned cw,unsigned* out){
 unsigned off=which?8:0,shift=which?4:0;
 for(unsigned i=0;i<3;i++)out[i]=r.color_offset1^off^(i*2)^((cw>>(shift+i))&1);
 out[3]=(data&512)?out[0]:(r.color_offset2^(which?16:0)^((cw>>8)&15));
 out[4]=r.color_offset1^off^6^((cw>>(shift+3))&1);
}
// r022: graphic page 1 (upper GVRAM byte) holds the original road lines: row L, column x
// = 0xf0|roads[L*512+x] (classes 0,1,2,3,7 -> palette 240..243,247 set per raster line).
// A line that shows one road therefore uses the original pixels. Lines where the 512-wide
// page would wrap visible road onto the screen, and the second road on two-road lines,
// still use the 80-step BG atlas.
static const uint8_t*road_lines=0;
// Rows of graphic page 1 currently holding road line n (-1 = off-road), per row set.
static int16_t road_page_line[2][224];static bool road_page_init=false;
// PC: model of the page-1 classes. X68000 (r031k): a copy in main memory of what the page holds,
// so a row is given only the bytes that differ from the line it held before.
static uint8_t road_page[512*512];
static int16_t line_min[256],line_max[256];
static void road_line_bounds(const HWRoad&r){
 if(road_lines==r.roads)return;
 road_lines=r.roads;
 for(unsigned l=0;l<256;l++){int mn=512,mx=-1;for(int x=0;x<512;x++)if(r.roads[l*512+x]!=3){if(x<mn)mn=x;mx=x;}line_min[l]=mn;line_max[l]=mx;}
}
enum {ROW_HIDDEN=0,ROW_EXACT=1,ROW_EXACT_OTHER=2,ROW_ATLAS=3,ROW_OFFSCREEN=4,ROW_OVERLAP=5};
void road_build(const HWRoad&r,const uint16_t*pal,RoadFrame&f){
 road_line_bounds(r);
 f.visible=f.hidden=f.variant_miss=0;f.exact_rows=f.atlas_rows=0;
 // r023: graphic Y scroll is not taken per raster line (XM6 shows the first line's value),
 // so page 1 keeps a fixed Y: screen line y shows page row gv_base+y, and road_stream()
 // copies the needed road line into that row. Two row sets (0 and 256) alternate with
 // the two RoadFrames, so the rows being written are never the ones on screen.
 static unsigned build_count=0;f.gv_base=(build_count++&1)?256:0;
 for(unsigned y=0;y<224;y++)f.gv_line[y]=-1;
 unsigned control=r.road_control&3;
 for(int y=0;y<224;y++){
  RoadRow& row=f.rows[y];unsigned d[2]={r.ramBuff[y],r.ramBuff[256+y]};
  bool on[2]={(d[0]&2048)==0&&(control!=3),(d[1]&2048)==0&&(control!=0)};
  row.bg_y=row.bg_far_y=(256-y)&511;row.control=0x004c;
  row.next=y==223?1023:72+(y+1)*2;
  unsigned mode;
  if(!on[0]&&!on[1]){
   unsigned pick=control==3?1:control==2?((d[1]&2048)?1:0):((d[0]&2048)?0:1);
   unsigned fill=r.color_offset3|(d[pick]&127);++f.hidden;
   uint16_t c=pal[fill&8191];for(unsigned k=0;k<5;k++)row.ground[k]=c;
   for(unsigned k=0;k<4;k++)row.front[k]=row.back[k]=c;
   row.bg0=row.bg1=scroll(0,480,y);row.gpage=f.gv_base;row.road_enable=0x0210;mode=ROW_HIDDEN;
  }else{
   ++f.visible;
   // (the colours, columns and textures are only needed on lines that show a road)
   unsigned ix[2]={(r.road_control&4)?unsigned(y):(d[0]&511),(r.road_control&4)?unsigned(y+256):(d[1]&511)};
   unsigned cw[2]={r.ramBuff[0x600+ix[0]],r.ramBuff[0x600+ix[1]]};
   unsigned co[2][5];colors(r,0,d[0],cw[0],co[0]);colors(r,1,d[1],cw[1],co[1]);
   unsigned primary=(control==2||control==3)?1:0,secondary=1-primary;
   uint32_t xy[2];int col[2];
   for(unsigned i=0;i<2;i++){
    col[i]=column(r,i,ix[i]);int tex=480;
    if(on[i]&&col[i]>=-320&&col[i]<512)tex=road_atlas.vtab[col[i]+320]+road_atlas.lrow[(d[i]>>1)&255];
    else if(on[i])++f.variant_miss;
    xy[i]=scroll(tex==480?0:col[i],tex,y);
   }
   // Colour of a pixel where both roads are off-road (priority map, both pixels 3).
   unsigned offroad=(control<=1)?co[0][3]:co[1][3];
   // Show exactly the higher-priority road that has pixels on screen; the other one (if on
   // screen) goes to BG1.
   bool vis[2];
   for(unsigned i=0;i<2;i++){unsigned l=(d[i]>>1)&255;int mn=line_min[l],mx=line_max[l];vis[i]=on[i]&&mn<=mx&&col[i]+319>=mn&&col[i]<=mx;}
   unsigned disp=vis[primary]?primary:vis[secondary]?secondary:(on[primary]?primary:secondary),other=1-disp;
   unsigned line=(d[disp]>>1)&255;int c=col[disp],mn=line_min[line],mx=line_max[line];
   bool visible=vis[disp];
   bool wraps=(c+319>=512+mn)||(c<=mx-512);          // the page would repeat road pixels
   on[other]=on[other]&&vis[other];
   if(visible&&on[other]){
    // Two roads with pixels on screen. If their road pixels can meet on this line, keep the
    // higher-priority road in front (BG1 atlas) and show the other one exactly behind it.
    unsigned lo=(d[other]>>1)&255;
    int a0=mn-c,a1=mx-c,b0=line_min[lo]-col[other],b1=line_max[lo]-col[other];
    if(lo==line&&col[other]==c&&!wraps&&(control==1||control==2)){
     // Both roads are the same line at the same place: every pixel has the same class in
     // both, so the priority map picks one road per class and the line stays exact.
     static const uint8_t pmap[2][8]={{0x80,0x81,0x81,0x87,0,0,0,0x00},{0x81,0x81,0x81,0x8f,0,0,0,0x80}};
     static const uint8_t cls_pix[5]={0,1,2,3,7};
     for(unsigned k=0;k<5;k++){unsigned p=cls_pix[k];unsigned win=(pmap[control-1][p]>>p)&1;row.ground[k]=pal[co[win][k==3?3:k==4?4:k]&8191];}
     row.gpage=(uint32_t(c&511)<<16)|f.gv_base;f.gv_line[y]=line;row.bg0=xy[disp];row.bg1=xy[other];row.road_enable=0x0210;
     for(unsigned k=0;k<4;k++){row.front[k]=row.back[k]=row.ground[3];}
     f.mode[y]=ROW_EXACT;++f.exact_rows;continue;
    }
    if(a0<=b1&&b0<=a1&&disp==primary){
     // The road pixels can meet. Same layer order as r018-r021 (it follows the priority
     // map except two documented edge cases): BG0 = priority road asphalt/centre (atlas),
     // BG1 = second road (atlas), graphic page 1 = priority road, now from the exact line
     // (shoulders and off-road) instead of the 80-step ground texture.
     uint16_t oc=pal[offroad&8191];
     for(unsigned k=0;k<4;k++){unsigned cls=k==3?4:k;row.front[k]=pal[co[disp][cls]&8191];row.back[k]=pal[co[other][cls]&8191];}
     row.bg0=xy[disp];row.bg1=xy[other];row.road_enable=0x0210|1|8;
     if(!wraps){
      row.ground[0]=pal[co[disp][0]&8191];row.ground[1]=pal[co[disp][1]&8191];row.ground[2]=pal[co[disp][2]&8191];row.ground[4]=pal[co[disp][4]&8191];row.ground[3]=oc;
      row.gpage=(uint32_t(c&511)<<16)|f.gv_base;f.gv_line[y]=line;
     }else{
      for(unsigned k=0;k<5;k++)row.ground[k]=oc;
      row.gpage=f.gv_base;
     }
     f.mode[y]=ROW_OVERLAP;++f.atlas_rows;continue;
    }
   }
   for(unsigned k=0;k<4;k++){unsigned cls=k==3?4:k;row.front[k]=pal[co[other][cls]&8191];row.back[k]=pal[co[other][cls]&8191];}
   if(!visible||!wraps){
    // Exact line from graphic page 1 (or all off-road when nothing of it is on screen).
    uint16_t oc=pal[offroad&8191];
    if(visible){row.ground[0]=pal[co[disp][0]&8191];row.ground[1]=pal[co[disp][1]&8191];row.ground[2]=pal[co[disp][2]&8191];row.ground[4]=pal[co[disp][4]&8191];}
    else {row.ground[0]=row.ground[1]=row.ground[2]=row.ground[4]=oc;}
    row.ground[3]=oc;
    row.gpage=(uint32_t(c&511)<<16)|f.gv_base;f.gv_line[y]=line;
    row.bg0=xy[disp];row.bg1=xy[other];
    row.road_enable=0x0210|(on[other]?8:0);
    mode=visible?(on[other]?ROW_EXACT_OTHER:ROW_EXACT):ROW_OFFSCREEN;++f.exact_rows;
   }else{
    // Wrapping line: the displayed road from the full BG1 atlas, the other road (if any)
    // from BG0, graphic page 1 as plain off-road colour.
    uint16_t oc=pal[offroad&8191];for(unsigned k=0;k<5;k++)row.ground[k]=oc;
    for(unsigned k=0;k<4;k++){unsigned cls=k==3?4:k;row.back[k]=pal[co[disp][cls]&8191];row.front[k]=pal[co[other][cls]&8191];}
    row.gpage=f.gv_base;row.bg1=xy[disp];row.bg0=xy[other];
    row.road_enable=0x0210|8|(on[other]?1:0);
    mode=ROW_ATLAS;++f.atlas_rows;
   }
  }
  f.mode[y]=mode;
 }
 // A row that has a BG plane off (road_enable bit 0: BG0, bit 3: BG1) shows nothing of that
 // plane, whatever its scroll register holds. Such a row takes the scroll value of the row
 // above, so the raster interrupt does not write the register on that line (row_changed): most
 // rows show the road from the graphic page with both planes off. A later row that turns the
 // plane on carries its own value, which is written on that row as before.
 for(unsigned y=1;y<224;y++){RoadRow&row=f.rows[y];const RoadRow&up=f.rows[y-1];
  if(!(row.road_enable&1))row.bg0=up.bg0;
  if(!(row.road_enable&8))row.bg1=up.bg1;}
}
// After everything that changes the rows (road_build, visuals_build): see RoadFrame::row_changed.
void road_rows_changed(RoadFrame&f){
 f.row_changed[0]=0x3ff;
 for(unsigned y=1;y<224;y++){const RoadRow&a=f.rows[y-1],&b=f.rows[y];unsigned m=0;
  if(a.bg0!=b.bg0)m|=1;if(a.bg1!=b.bg1)m|=2;if(a.gpage!=b.gpage)m|=4;
  if(memcmp(a.front,b.front,sizeof a.front))m|=8;if(memcmp(a.back,b.back,sizeof a.back))m|=16;if(memcmp(a.ground,b.ground,sizeof a.ground))m|=32;
  if(a.bg_y!=b.bg_y)m|=64;if(a.bg_far_y!=b.bg_far_y)m|=128;if(a.control!=b.control)m|=256;if(a.road_enable!=b.road_enable)m|=512;
  f.row_changed[y]=uint16_t(m);}
}
static unsigned bgpixel(const uint16_t*map,uint32_t xy,int x,int y){
 unsigned sx=((xy>>16)+x)&511,sy=((xy&511)+y)&511;
 unsigned code=map[(sy>>3)*64+(sx>>3)]&255;
 unsigned b=road_atlas.pcg[code*32+(sy&7)*4+((sx&7)>>1)];return sx&1?b&15:b>>4;
}
#ifndef OR_X68K
void road_preview(const RoadFrame&f,uint16_t*out){
 for(int y=0;y<224;y++)for(int x=0;x<320;x++){
  const RoadRow&r=f.rows[y];unsigned a=(r.road_enable&1)?bgpixel(road_atlas.map0,r.bg0,x,y):0,b=(r.road_enable&8)?bgpixel(road_atlas.map1,r.bg1,x,y):0;
  unsigned sx=((r.gpage>>16)+x)&511,sy=((r.gpage&511)+y)&511;
  unsigned g=road_page[sy*512+sx];g=g==7?4:g;
  out[y*320+x]=a?r.front[a-1]:b?r.back[b-1]:r.ground[g];
 }
}
#endif

// r023: copy the road lines needed by this frame into its page-1 rows (only changed rows).
void road_stream(const RoadFrame&f){
 if(!road_page_init){
  for(unsigned b=0;b<2;b++)for(unsigned y=0;y<224;y++)road_page_line[b][y]=-1;
  memset(road_page,3,sizeof road_page);   // (X68000: platform_init fills the page with class 3)
  road_page_init=true;
 }
 unsigned set=f.gv_base?1:0;
 for(unsigned y=0;y<224;y++){
  int l=f.gv_line[y];if(l<0||road_page_line[set][y]==l)continue;
  road_page_line[set][y]=l;
  const uint8_t*src=road_lines+l*512;
#ifdef OR_X68K
  // upper byte of each 16-bit pixel = page 1; the lower byte (background) is not touched
  // Neighbouring road lines differ in a few pixels (the edges move by one or two), and a GVRAM
  // byte costs 0.47 us on the real bus: compared 4 pixels at a time with the copy, and only the
  // bytes that differ are written.
  volatile uint8_t*dst=(volatile uint8_t*)(0xc00000UL+(f.gv_base+y)*1024UL);
  uint8_t*have=road_page+(f.gv_base+y)*512;
  for(unsigned x=0;x<512;x+=4){
   if(*(const uint32_t*)(src+x)==*(const uint32_t*)(have+x))continue;
   for(unsigned k=x;k<x+4;k++)if(have[k]!=src[k]){have[k]=src[k];dst[k*2]=0xf0|src[k];}
  }
#else
  memcpy(road_page+(f.gv_base+y)*512,src,512);
#endif
 }
}
