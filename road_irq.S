/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
.text
.even
.globl road_vblank_irq
.globl road_raster_irq
road_vblank_irq:
 /* Preserve every data/address register across the C callback and its helpers. */
 movem.l %d0-%d7/%a0-%a6,-(%sp)
 addq.l #1,road_vblanks
 /* The raster interrupts of this frame are armed first, and the work of the vertical blank
    (commit, sprite start state) then runs with them enabled: when that work takes longer than
    the blank, the top lines get their raster data late instead of the whole frame losing it. */
 tst.b road_vblank_busy
 beq 3f
 /* the previous blank's work is still running: only restart the raster lines */
 addq.l #1,road_vblank_overruns
 clr.l scenery_scan
 move.l road_active,road_scan
 move.w #72,0xe80012
 bclr #5,0xe8800f
 movem.l (%sp)+,%d0-%d7/%a0-%a6
 rte
3:
 st road_vblank_busy
 move.l road_pending,%d7
 beq 1f
 move.l %d7,road_active
 clr.l road_pending
 addq.l #1,road_shown
1:
 clr.l scenery_scan            /* no line jobs until scenery_vblank has loaded the start state */
 move.l road_active,road_scan
 move.w #72,0xe80012
 bclr #5,0xe8800f
 move.w #0x2500,%sr
 tst.l %d7
 beq 2f
 move.l %d7,-(%sp)
 jsr visuals_native_commit
 addq.l #4,%sp
2:
 /* r026: scenery sprites of the shown frame: load the frame-start state, point at its line jobs */
 move.l road_active,-(%sp)
 jsr scenery_vblank
 addq.l #4,%sp
 move.w #0x2600,%sr
 sf road_vblank_busy
 movem.l (%sp)+,%d0-%d7/%a0-%a6
 rte
road_raster_irq:
 move.l %a0,-(%sp)
#ifdef CBX68_PROF
 /* sampling profiler: count the interrupted PC in 16-byte buckets of the program text */
 move.l %d0,-(%sp)
 move.l 10(%sp),%d0
 sub.l #_start,%d0
 cmp.l prof_text_size,%d0
 bcs 7f
 move.l prof_text_size,%d0
7:
 lsr.l #4,%d0
 lea prof_hist,%a0
 addq.l #1,(%a0,%d0.l*4)
 move.l (%sp)+,%d0
#endif
 addq.l #1,road_irq_count
 move.l road_scan,%a0
 move.l (%a0)+,0xeb0800
 move.l (%a0)+,0xeb0804
 move.l (%a0),0xe80020
 move.l (%a0)+,0xe80024
 move.l (%a0)+,0xe82222
 move.l (%a0)+,0xe82226
 move.l (%a0)+,0xe82242
 move.l (%a0)+,0xe82246
 /* r022: graphic palette 240..243 and 247 = road classes 0,1,2,3 and 7 */
 move.l (%a0)+,0xe821e0
 move.l (%a0)+,0xe821e4
 move.w (%a0)+,0xe821ee
 move.w (%a0)+,0xe8001a
 move.w (%a0)+,0xe8001e
 move.w (%a0)+,0xe82600
 move.w (%a0)+,0xeb0808
 move.w (%a0)+,0xe80012
 move.l %a0,road_scan
 /* r026: scenery sprite jobs of this line: {address, longs-1, data...} ... 0 */
 tst.l scenery_scan
 beq 2f
 move.l scenery_scan,%a0
 addq.l #4,scenery_scan
 tst.l (%a0)
 beq 2f
 movem.l %d0-%d1/%a1-%a2,-(%sp)
 move.l (%a0),%a1
4:
 move.l (%a1)+,%d0
 beq 5f
 move.l %d0,%a2
 move.l (%a1)+,%d1
6:
 move.l (%a1)+,(%a2)+
 dbra %d1,6b
 bra 4b
5:
 movem.l (%sp)+,%d0-%d1/%a1-%a2
2:
 bclr #6,0xe8800f
 move.l (%sp)+,%a0
 rte
