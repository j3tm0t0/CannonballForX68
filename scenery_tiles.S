/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
/* Scenery tile pass, first pass over one 16x16 tile (68020+).
   uint32_t scenery_tile_rows(const uint16_t* src, unsigned rows, uint32_t* rowLR, const uint16_t* block_bit)
     src       top-left pixel of the tile in the 320-wide capture buffer
     rows      1..16
     rowLR     [0..15] left 8 nibbles of each row, [16..31] right 8 nibbles
     block_bit 256 entries, bit of the palette block of a pixel code
   Returns (rowbits<<16)|mask, the same values the C loop builds.
   Returns 0xffffffff when a pixel code is >= 256 (a colour that still needs a lookup):
   nothing but rowLR has been written, and the caller runs the C loop for this tile. */
.text
.even
.globl scenery_tile_rows
scenery_tile_rows:
 movem.l %d2-%d7/%a2,-(%sp)
 move.l 32(%sp),%a0
 move.l 36(%sp),%d7
 move.l 40(%sp),%a1
 move.l 44(%sp),%a2
 moveq #0,%d3            /* mask */
 moveq #0,%d5            /* rowbits */
 moveq #1,%d6            /* bit of this row */
 moveq #0,%d1
 moveq #15,%d2
 subq.l #1,%d7
1:
 move.l (%a0),%d0
 or.l 4(%a0),%d0
 or.l 8(%a0),%d0
 or.l 12(%a0),%d0
 or.l 16(%a0),%d0
 or.l 20(%a0),%d0
 or.l 24(%a0),%d0
 or.l 28(%a0),%d0
 bne 2f
 clr.l (%a1)
 clr.l 64(%a1)
 lea 640(%a0),%a0
 bra 3f
2:
 and.l #0xff00ff00,%d0
 bne 9f
 or.l %d6,%d5
 moveq #0,%d4
 .rept 8
 move.w (%a0)+,%d1
 or.w (%a2,%d1.l*2),%d3
 lsl.l #4,%d4
 and.w %d2,%d1
 or.b %d1,%d4
 .endr
 move.l %d4,(%a1)
 moveq #0,%d4
 .rept 8
 move.w (%a0)+,%d1
 or.w (%a2,%d1.l*2),%d3
 lsl.l #4,%d4
 and.w %d2,%d1
 or.b %d1,%d4
 .endr
 move.l %d4,64(%a1)
 lea 608(%a0),%a0
3:
 addq.l #4,%a1
 add.l %d6,%d6
 dbra %d7,1b
 swap %d5
 move.w %d3,%d5
 move.l %d5,%d0
 movem.l (%sp)+,%d2-%d7/%a2
 rts
9:
 moveq #-1,%d0
 movem.l (%sp)+,%d2-%d7/%a2
 rts

/* Finish a tile that has one palette block (68020+).
   uint32_t scenery_tile_finish(uint16_t* src, uint32_t rowbits, const uint32_t* rowLR, uint32_t* enc)
   Copies the 32 longs of rowLR to enc, clears the capture rows named in rowbits, and returns
   the FNV-1a hash taken over left,right of row 0..15 (the order of the C loop). */
.globl scenery_tile_finish
scenery_tile_finish:
 movem.l %d2-%d4,-(%sp)
 move.l 24(%sp),%a0      /* rowLR */
 move.l 28(%sp),%a1      /* enc */
 move.l #2166136261,%d0
 move.l #16777619,%d2
 moveq #15,%d4
1:
 move.l (%a0)+,%d1
 move.l %d1,(%a1)+
 eor.l %d1,%d0
 mulu.l %d2,%d0
 move.l 60(%a0),%d1
 move.l %d1,60(%a1)
 eor.l %d1,%d0
 mulu.l %d2,%d0
 dbra %d4,1b
 move.l 16(%sp),%a0      /* src */
 move.l 20(%sp),%d3      /* rowbits */
 beq 4f
 moveq #0,%d1
2:
 lsr.l #1,%d3
 bcc 3f
 move.l %d1,(%a0)
 move.l %d1,4(%a0)
 move.l %d1,8(%a0)
 move.l %d1,12(%a0)
 move.l %d1,16(%a0)
 move.l %d1,20(%a0)
 move.l %d1,24(%a0)
 move.l %d1,28(%a0)
 tst.l %d3
3:
 lea 640(%a0),%a0
 bne 2b
4:
 movem.l (%sp)+,%d2-%d4
 rts

/* Second pass of a tile with several palette blocks (68020+).
   void scenery_tile_split(uint16_t* src, uint32_t rowbits, uint32_t** encp)
     encp[code>>4] = pattern (32 longs, zeroed by the caller) of the block of that pixel code
   Every pixel code must be < 256 (scenery_tile_rows returned a result for this tile).
   ORs each pixel's nibble into the pattern of its block and clears the rows named in rowbits. */
.globl scenery_tile_split
scenery_tile_split:
 movem.l %d2-%d6/%a2,-(%sp)
 move.l 28(%sp),%a0
 move.l 32(%sp),%d3
 move.l 36(%sp),%a2
 moveq #15,%d2
 moveq #0,%d5            /* row*4 */
 moveq #64,%d6           /* row*4+64 */
 tst.l %d3
 beq 9f
1:
 lsr.l #1,%d3
 bcc 5f
 moveq #28,%d4
 .rept 8
 move.w (%a0)+,%d0
 beq 2f
 move.w %d0,%d1
 lsr.w #4,%d1
 move.l (%a2,%d1.w*4),%a1
 and.l %d2,%d0
 lsl.l %d4,%d0
 or.l %d0,(%a1,%d5.l)
2:
 subq.l #4,%d4
 .endr
 moveq #28,%d4
 .rept 8
 move.w (%a0)+,%d0
 beq 2f
 move.w %d0,%d1
 lsr.w #4,%d1
 move.l (%a2,%d1.w*4),%a1
 and.l %d2,%d0
 lsl.l %d4,%d0
 or.l %d0,(%a1,%d6.l)
2:
 subq.l #4,%d4
 .endr
 moveq #0,%d0
 move.l %d0,-(%a0)
 move.l %d0,-(%a0)
 move.l %d0,-(%a0)
 move.l %d0,-(%a0)
 move.l %d0,-(%a0)
 move.l %d0,-(%a0)
 move.l %d0,-(%a0)
 move.l %d0,-(%a0)
5:
 lea 640(%a0),%a0
 addq.l #4,%d5
 addq.l #4,%d6
 tst.l %d3
 bne 1b
9:
 movem.l (%sp)+,%d2-%d6/%a2
 rts

/* uint32_t scenery_pcg_hash(const uint32_t* enc): FNV-1a over left,right of row 0..15 */
.globl scenery_pcg_hash
scenery_pcg_hash:
 movem.l %d2-%d3,-(%sp)
 move.l 12(%sp),%a0
 move.l #2166136261,%d0
 move.l #16777619,%d2
 moveq #15,%d3
1:
 move.l (%a0)+,%d1
 eor.l %d1,%d0
 mulu.l %d2,%d0
 move.l 60(%a0),%d1
 eor.l %d1,%d0
 mulu.l %d2,%d0
 dbra %d3,1b
 movem.l (%sp)+,%d2-%d3
 rts
