/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
#pragma once
#include <stdint.h>
#include "hwvideo/hwroad.hpp"
#include "visual_types.hpp"
struct RoadRow { uint32_t bg0,bg1,gpage;uint16_t front[4],back[4],ground[5],bg_y,bg_far_y,control,road_enable,next; };
static_assert(sizeof(RoadRow)==48,"Raster entry layout");
// row_changed[y]: which register groups of row y differ from row y-1 (bit 0 bg0, 1 bg1, 2 gpage,
// 3 front, 4 back, 5 ground, 6 bg_y, 7 bg_far_y, 8 control, 9 road_enable); the raster interrupt
// writes only those (road_irq.S). Row 0 has every bit. Set by road_rows_changed().
// Bits 10-15: how many rows directly after this one need no raster interrupt (no register group
// differs and the scenery has no job list on them, at most 63); the interrupt of this row steps
// over them while the job lists run.
struct RoadFrame {RoadRow rows[224];unsigned visible,hidden,variant_miss,exact_rows,atlas_rows;uint8_t mode[224];uint16_t gv_base;int16_t gv_line[224];uint16_t row_changed[224];VisualFrame visual;};
void road_rows_changed(RoadFrame&);
unsigned road_rows_check(const RoadFrame&);   // PC only
struct RoadAtlas {uint16_t tiles,nr;uint8_t lrow[256],pcg[8192],ground[512*512];uint16_t vtab[832],map0[4096],map1[4096];};
extern RoadAtlas road_atlas;
bool road_assets_load();
void road_build(const HWRoad&,const uint16_t*,RoadFrame&);
void road_preview(const RoadFrame&,uint16_t*);
void road_stream(const RoadFrame&);
