/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
#pragma once
#include <stdint.h>
struct CarPacket {
 uint16_t count,patterns,palettes,overflow,line_max;
 uint16_t palette[13][16],registers[128][4];
 uint8_t pcg[128][128];   // r027: up to 128 (patterns come from the shared pool, reused down the screen)
 uint16_t groups[13],pattern_index[128],text_mode,text_bank;
 uint8_t text_bits[4][224*40];
};
// r026: roadside objects and other sprites (all sprites except the player car) as hardware
// sprites. Sprite numbers and PCG patterns are reused down the screen: a resource used by tile
// row r is rewritten during row r+1 and shown again in row r+2 (raster interrupt jobs).
// Job list: {address, longs-1, data...} ... 0. Addresses on the X68000, offsets+1 on the PC.
enum {SCENERY_MAX=448,SCENERY_JOB_LONGS=18000,SCENERY_LINES=256};
struct ScenerySprite {int16_t x,y;uint8_t block,row,kept,pad;};
struct SceneryPacket {
 uint16_t count,kept,dropped,blocks,first_block,dropped_banks,reused,line_longs_max,shared,car_dropped;
 uint16_t palette[13][16];
 uint32_t start_job,restart_job,line_job[SCENERY_LINES];   // restart_job: the part of start_job a pass of the line jobs has overwritten
 uint32_t jobs[SCENERY_JOB_LONGS];
 ScenerySprite sprite[SCENERY_MAX];
 uint32_t pcg[SCENERY_MAX][32];   // r026c: 8 pixels a long, first pixel in the top nibble (PCG byte order)
};
// r027: changed text-layer cells for the X68000 text plane (4 planes x 8 rows each).
enum {HUD_CELLS=1120};
struct HudPacket {uint16_t count,full,overflow,active,bands;uint8_t row_band[28],band_first[28];uint16_t band_pal[28][16];uint16_t pos[HUD_CELLS];uint8_t planes[HUD_CELLS][32];};
struct VisualFrame {uint16_t flags,pad;int32_t near_world,far_world;CarPacket car;SceneryPacket scenery;HudPacket hud;};
