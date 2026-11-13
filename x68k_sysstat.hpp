/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). */
// IOCS _SYS_STAT ($AC) and what 060turbo.sys adds to it (r031n).
//  d1=1      cache state            d1=4      set the caches from d2
//  d1=$8000  version, a1='060T'     d1=$4000  bytes of local memory
//  d1=$4002  a1 in local memory? (0 = yes)
//  d1=$4004  a1, d2 bytes reachable by DMA? (0 = yes)
//  d1=$5001  end this process and run a program again in local memory
//  d1=$5003  where DOS _MALLOC takes memory from (0 = main memory only), returns the old value
//  d1=$C002  0 = compatible mode, 1 = extended mode    d1=$C006  cache / store buffer / branch cache / superscalar bits
//  d1=$F000  physical address of a1
#pragma once
#include <stdint.h>
static inline long x68k_sys_stat(long mode,long value=0,const void*p1=0,const void*p2=0,const void*p3=0,uint32_t*a1_out=0,long*d1_out=0){
 register long d0 __asm__("d0")=0xac;register long d1 __asm__("d1")=mode;register long d2 __asm__("d2")=value;
 register const void*a1 __asm__("a1")=p1;register const void*a2 __asm__("a2")=p2;register const void*a3 __asm__("a3")=p3;
 __asm__ volatile("trap #15":"+d"(d0),"+d"(d1),"+d"(d2),"+a"(a1),"+a"(a2),"+a"(a3)::"memory");
 if(a1_out)*a1_out=uint32_t(uintptr_t(a1));
 if(d1_out)*d1_out=d1;
 return d0;
}
static inline bool x68k_has_060turbo(){
 static int known=-1;
 if(known<0){uint32_t name=0;const long v=x68k_sys_stat(0x8000,0,0,0,0,&name);known=(v!=-1&&name==0x30363054UL)?1:0;}   // '060T'
 return known>0;
}
// 060turbo.sys's extended mode (its -xm option). In version 0.61, $C002 answers 0 in d0 in the
// compatible mode, but in the extended mode it leaves d0 as it was inside the call (not 0; 518 was
// seen) instead of setting 1. So: anything but 0 and -1 is taken as extended. (A wrong yes only
// costs a try: the program is then run again in main memory.)
static inline bool x68k_extended_mode(){
 if(!x68k_has_060turbo())return false;
 const long d0=x68k_sys_stat(0xc002);
 return d0!=0&&d0!=-1;
}
static inline bool x68k_in_local_memory(const void*p){return x68k_has_060turbo()&&x68k_sys_stat(0x4002,0,p)==0;}
// reachable by the DMA controller, and at the same physical address as the program sees it
static inline bool x68k_dma_reachable(const void*p,uint32_t bytes){
 if(!x68k_has_060turbo())return true;
 return x68k_sys_stat(0x4004,long(bytes),p)==0&&uint32_t(x68k_sys_stat(0xf000,0,p))==uint32_t(uintptr_t(p));
}
