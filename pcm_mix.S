/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
/* Sega PCM mixing, the samples of one channel between two looks at its end address (68020+).
   void pcm_mix_run(int32_t* m, const uint8_t* bank, uint32_t addr, uint32_t step, uint32_t vol, uint32_t count)
     m      sums, one long a sample; count of them get a product added
     bank   the channel's 64 KB bank of the sample ROM
     addr   16.8 address within the bank (the low 24 bits of the channel's address)
     step   what a sample adds to the address (0..510)
     vol    0..1530 (the two volumes times PCM_GAIN)
     count  1..65536; the caller keeps addr+(count-1)*step below 0x1000000 (inside the bank)
   For k = 0..count-1: m[k] += bank[(addr+k*step)>>8] * vol, the byte taken as 0..255.
   (The chip's samples are the bytes less 0x80: the caller takes 0x80*vol off afterwards.) */
.text
.even
.balignw 16,0x4e71
.globl pcm_mix_run
pcm_mix_run:
 movem.l %d2-%d7,-(%sp)
 move.l 28(%sp),%a0      /* m */
 move.l 32(%sp),%a1      /* bank */
 move.l 36(%sp),%d7      /* addr */
 move.l 40(%sp),%d2      /* step */
 move.l 44(%sp),%d5      /* vol */
 move.l 48(%sp),%d4      /* count */
 move.b %d7,%d1          /* fraction of the address */
 lsr.l #8,%d7            /* byte index */
 move.l %d2,%d6
 lsr.l #8,%d6            /* whole bytes of a step (0 or 1) */
 moveq #0,%d3
 subq.l #1,%d4
1:
 move.b (%a1,%d7.l),%d3
 move.w %d3,%d0
 mulu.w %d5,%d0
 add.l %d0,(%a0)+
 add.b %d2,%d1           /* fraction, the carry goes into the index */
 addx.l %d6,%d7
 dbra %d4,1b
 movem.l (%sp)+,%d2-%d7
 rts
