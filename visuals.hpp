/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
#pragma once
#include "road_bridge.hpp"
struct BackgroundAssets {uint16_t palette[256];};
extern BackgroundAssets background_assets;
extern unsigned visual_mode;
extern uint32_t background_columns,car_overflows,car_tiles_total,car_frames,car_text_frames,visual_frames[4];
extern unsigned car_peak_tiles,car_peak_patterns,car_peak_line;
bool visuals_load();
void visuals_build(RoadFrame&,const uint16_t*);
void visuals_preview(const RoadFrame&,uint16_t*);
void visuals_native_init();
void visuals_set_line_budget(unsigned longs);   // longs a raster interrupt may write (scenery jobs)
unsigned visuals_line_budget();
void visuals_handoff(RoadFrame&f);   // r029
void visuals_stream(const RoadFrame&);
extern "C" void visuals_native_commit(const RoadFrame*);

#ifndef OR_X68K
void visuals_selftest();
#endif

void visuals_print();
