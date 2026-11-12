/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
.text
.even
.globl road_vblank_irq
.globl road_raster_irq
.lcomm vblank_frame,4
.lcomm vblank_deferred,2
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
 clr.w scenery_owed
 addq.b #1,scenery_gen
 move.l road_active,road_scan
 move.w #72,0xe80012
 bclr #5,0xe8800f
 movem.l (%sp)+,%d0-%d7/%a0-%a6
 rte
3:
 st road_vblank_busy
 /* how the pass of the last frame ended (scenery_vblank: may the start state be loaded in part) */
 move.l road_scan,road_scan_end
 tst.b scenery_busy
 sne scenery_unfinished
 tst.w scenery_owed
 beq 4f
 st scenery_unfinished
4:
 move.l road_pending,%d7
 beq 1f
 move.l %d7,road_active
 clr.l road_pending
 addq.l #1,road_shown
1:
 move.l %d7,vblank_frame
 clr.l scenery_scan            /* no line jobs until scenery_vblank has loaded the start state */
 clr.w scenery_owed
 addq.b #1,scenery_gen           /* a job list of the old frame that is still being written stops */
 move.l road_active,road_scan
 move.w #72,0xe80012
 bclr #5,0xe8800f
 /* A raster interrupt below us is still writing a job list of the old frame: the run it is in
    would land after the new start state. Its loop does the work of this blank when it stops. */
 tst.b scenery_busy
 beq 2f
 st vblank_deferred
 movem.l (%sp)+,%d0-%d7/%a0-%a6
 rte
2:
 move.w #0x2500,%sr
 bsr vblank_work
 move.w #0x2600,%sr
 sf road_vblank_busy
 movem.l (%sp)+,%d0-%d7/%a0-%a6
 rte
/* commit of a new frame and the scenery start state; called at mask 5, d0-d1/a0-a1 are lost */
vblank_work:
 move.l vblank_frame,%d0
 beq 1f
 move.l %d0,-(%sp)
 jsr visuals_native_commit
 addq.l #4,%sp
1:
 /* r026: scenery sprites of the shown frame: load the frame-start state, point at its line jobs */
 move.l road_active,-(%sp)
 jsr scenery_vblank
 addq.l #4,%sp
 rts
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
 /* End of service here: the scenery jobs below run with the interrupt mask at 5, so the next
    raster interrupt (and the other MFP interrupts) can come in while they are written. The
    road registers and R09 of every line are then written on time whatever the jobs take. */
 bclr #6,0xe8800f
 /* r026: scenery sprite jobs of this line: {address, longs-1, data...} ... 0.
    A line whose interrupt arrives while earlier jobs are still being written is only counted
    (scenery_owed); the loop that is running takes it afterwards. */
 tst.l scenery_scan
 beq 2f
 addq.w #1,scenery_owed
#ifdef CBX68_TRACE
 /* how far the jobs are behind: the largest count, and the lines where it is over 3 */
 move.w scenery_owed,%a0
 cmp.w scenery_owed_max,%a0
 bls 8f
 move.w %a0,scenery_owed_max
8:
 cmp.w #3,%a0
 bls 8f
 addq.l #1,scenery_late_lines
8:
#endif
 tst.b scenery_busy
 bne 2f
 st scenery_busy
 movem.l %d0-%d2/%a1-%a2,-(%sp)
3:
 /* mask 6 here */
 tst.w scenery_owed
 beq 9f
 subq.w #1,scenery_owed
 move.l scenery_scan,%d0
 beq 9f
 move.l %d0,%a0
 addq.l #4,scenery_scan
#ifdef CBX68_TRACE
 addq.w #1,scenery_line
#endif
 move.l (%a0),%d0
 beq 3b
 move.l %d0,%a1
 move.b scenery_gen,%d2
 move.w #0x2500,%sr
4:
 move.l (%a1)+,%d0
 beq 5f
 cmp.b scenery_gen,%d2          /* a vertical blank came in: the rest belongs to the old frame */
 bne 5f
 move.l %d0,%a2
 move.l (%a1)+,%d1
6:
 move.l (%a1)+,(%a2)+
 dbra %d1,6b
 bra 4b
5:
 move.w #0x2600,%sr
#ifdef CBX68_TRACE
 /* The list of line 16k+3+p (p = 0..10) is for tile row k+1, shown from line 16k+16: it is
    too late when 13-p or more line interrupts have come in since its own. */
 move.w scenery_line,%d0
 subq.w #4,%d0
 and.w #15,%d0
 cmp.w #10,%d0
 bhi 3b
 moveq #13,%d1
 sub.w %d0,%d1
 cmp.w scenery_owed,%d1
 bhi 3b
 addq.l #1,scenery_missed
#endif
 bra 3b
9:
 sf scenery_busy
 tst.b vblank_deferred
 beq 7f
 /* a vertical blank came in while this loop was writing: its work is done here */
 sf vblank_deferred
 move.w #0x2500,%sr
 bsr vblank_work
 move.w #0x2600,%sr
 sf road_vblank_busy
7:
 movem.l (%sp)+,%d0-%d2/%a1-%a2
2:
 move.l (%sp)+,%a0
 rte
