/***************************************************************************
    68000 Program Code Addresses 
    Addresses to data within the Master and Sub CPU Program ASSETs.
    
    These are typically large blocks of data that we don't want to include
    in the codebase. 
    
    Where needed addresses from the separate Japanese version are also
    included. These are denoted by entries ending _J
    
    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once

// ----------------------------------------------------------------------------
// Text Structures
// ----------------------------------------------------------------------------

// Text1 = Use BlitText1 routine | Text2 = Use BlitText2 routine

// Text: Credits
extern uint16_t TEXT1_CREDIT;  // 0x6D38 (Revision B)
extern uint16_t TEXT1_CREDITS; // 0x6D48 (Revision B)
extern uint16_t TEXT1_CLEAR_CREDITS; // 0x6D58 (Revision B)
extern uint16_t TEXT1_FREEPLAY; // 0x6D6C (Revision B)

// Text: Course Map
extern uint16_t TEXT2_COURSEMAP; // 0xBBC2 (Revision B)

// Text: Press Start
extern uint16_t TEXT1_PRESS_START; // 0xBBD0 (Revision B)
extern uint16_t TEXT1_CLEAR_START; // 0xBBEC (Revision B)

// Text: Insert Coins
extern uint16_t TEXT1_INSERT_COINS; // 0xBC08 (Revision B)
extern uint16_t TEXT1_CLEAR_COINS;  // 0xBC1E (Revision B)

// Text: Game Over
extern uint16_t TEXT2_GAMEOVER; // 0xBCB0 (Revision B)

// Text: Select Music By Steering
extern uint16_t TEXT2_SELECT_MUSIC; // 0xBCBE (Revision B)

// Text: 1986 Sega
extern uint16_t TEXT1_1986_SEGA; // 0xBCF2 (Revision B)

// Text: Copyright Symbol
extern uint16_t TEXT1_COPYRIGHT; // 0xBD04 (Revision B)

// Text: Magical Sound Shower
extern uint16_t TEXT2_MAGICAL; // 0xCE04 (Revision B)

// Text: Passing Breeze
extern uint16_t TEXT2_BREEZE; // 0xCE1E (Revision B)

// Text: Splash Wave
extern uint16_t TEXT2_SPLASH; // 0xCE38 (Revision B)

// Text: Your Score
extern uint16_t TEXT1_YOURSCORE; // 0xD5E0 (Revision B)

// Text: Best OutRunners
extern uint16_t TEXT2_BEST_OR; // 0xD5F2 (Revision B)

// Text: Score, Name, Route, Record
extern uint16_t TEXT1_SCORE_ETC; // 0xD606 (Revision B)

// Text: ABCDEFGHIJKLMNOPQRSTUVWXYZ
extern uint16_t TEXT2_ALPHABET; // 0xD5C2 (Revision B)

// Text: Extend Time
extern uint16_t TEXT1_EXTEND1; // 0x90DC (Revision B)
extern uint16_t TEXT1_EXTEND2; // 0x90F6 (Revision B)

// Text: Clear Extend Time
extern uint16_t TEXT1_EXTEND_CLEAR1; // 0x9110 (Revision B)
extern uint16_t TEXT1_EXTEND_CLEAR2; // 0x912A (Revision B)

// Text: Laptime
extern uint16_t TEXT1_LAPTIME1; // 0x9144 (Revision B)
extern uint16_t TEXT1_LAPTIME2; // 0x9150 (Revision B)

// Text: Clear Laptime
extern uint16_t TEXT1_LAPTIME_CLEAR1; // 0x915C (Revision B)
extern uint16_t TEXT1_LAPTIME_CLEAR2; // 0x917A (Revision B)

// Text: Easter Egg
extern uint16_t TEXT1_EASTER;       // 0x91B4 (Revision B)
extern uint16_t TEXT1_EASTER_CLEAR; // 0x91D6 (Revision B)

// Text: Bonus Points Section
extern uint16_t TEXT2_BONUS_POINTS; // 0x9C0C (Revision B)
extern uint16_t TEXT1_BONUS_STOP;   // 0x9C1C (Revision B)
extern uint16_t TEXT1_BONUS_SEC;    // 0x9C26 (Revision B)
extern uint16_t TEXT1_BONUS_X;      // 0x9C34 (Revision B)
extern uint16_t TEXT1_BONUS_PTS;    // 0x9C3E (Revision B)
extern uint16_t TEXT1_BONUS_100K;   // 0x9C4A (Revision B)
extern uint16_t TEXT2_BONUS_CLEAR1; // 0x9C52 (Revision B)
extern uint16_t TEXT2_BONUS_CLEAR2; // 0x9C64 (Revision B)
extern uint16_t TEXT2_BONUS_CLEAR3; // 0x9C78 (Revision B)

// ----------------------------------------------------------------------------
// HUD
// ----------------------------------------------------------------------------

// SCORE Graphic (2 Lines)
extern uint16_t HUD_SCORE1; // 0xBC3E (Revision B)
extern uint16_t HUD_SCORE2; // 0xBC4C (Revision B)

// TIME Graphic (2 Lines)
extern uint16_t HUD_TIME1; // 0xBC5A (Revision B)
extern uint16_t HUD_TIME2; // 0xBC66 (Revision B)

// KPH (2 Lines)
extern uint16_t HUD_KPH1; // 0xBC72 (Revision B)
extern uint16_t HUD_KPH2; // 0xBC7E (Revision B)

// STAGE (2 Lines)
extern uint16_t HUD_STAGE1; // 0xBC8A (Revision B)
extern uint16_t HUD_STAGE2; // 0xBC98 (Revision B)

// Number "1" to appear after stage
extern uint16_t HUD_ONE; // 0xBCA6 (Revision B)

// LAP (2 Lines)
extern uint16_t HUD_LAP1; // 0xBCDA (Revision B)
extern uint16_t HUD_LAP2; // 0xBCE6 (Revision B)

// ----------------------------------------------------------------------------
// Tilemaps
// ----------------------------------------------------------------------------

// Tilemap hardware addresses
const uint32_t HW_FG_PSEL       = 0xE80;
const uint32_t HW_BG_PSEL       = 0xE82;
const uint32_t HW_BG_HSCROLL    = 0xE9A;
const uint32_t HW_FG_VSCROLL    = 0xE90;
const uint32_t HW_BG_VSCROLL    = 0xE92;
const uint32_t HW_FG_HSCROLL    = 0xE98;

// In-Game Tilemap Defaults
extern uint32_t TILES_PAGE_FG1;     // 0x17E4C (Revision B)
extern uint32_t TILES_PAGE_BG1;     // 0x17E5C (Revision B)
extern uint32_t TILES_PAGE_FG2;     // 0x17E68 (Revision B)  // Used for road split
extern uint32_t TILES_PAGE_BG2;     // 0x17E78 (Revision B)  // Used for road split
extern uint32_t TILES_DEF_LOOKUP;   // 0x17E84 (Revision B)  // Tilemap default lookup indexes, for values in table below
const uint32_t TILES_DEF_LOOKUP_J = 0x178E4;
extern uint32_t TILES_TABLE;        // 0x17EAC (Revision B)  // Stage Tilemap Default Values
const uint32_t TILES_TABLE_J      = 0x1790c;

extern uint32_t TILES_MINIMAP;    // 0x8C04 (Revision B)

// Table of h_scroll offsets (words) for road split
// Note the h_scroll is set manually during the road split from the actual road position
extern uint32_t H_SCROLL_TABLE;   // 0x30B00 (Revision B)

// Tilemap: Music Selection Screen
extern uint32_t TILEMAP_MUSIC_SELECT; // 0x383F2 (Revision B)

// ----------------------------------------------------------------------------
// Palettes
// ----------------------------------------------------------------------------

// In-Game Tilemap Palettes
extern uint32_t TILEMAP_PALS; // 0xDF9C (Revision B)

// Palette: Music Select Screen
extern uint32_t PAL_MUSIC_SELECT; // 0x175CC (Revision B)

// Table of long addresses of ground colours
extern uint32_t PAL_GND_TABLE; // 0x17350 (Revision B)

// Table of palette addresses
extern uint32_t PAL_SKY_TABLE; // 0x17590 (Revision B)

// Palette Data: Best Outrunners Name Entry
extern uint32_t PAL_BESTOR; // 0x17DCC (Revision B)

// Cannonball for x68: palette data that otiles.cpp read at fixed addresses (HUD palette, tilemap
// palette indexes and tilemap palette data)
extern uint32_t PAL_HUD;            // 0x16ED8 (Revision B)
extern uint32_t PAL_TILEMAP_INDEX;  // 0x16FD8 (Revision B)
extern uint32_t PAL_TILEMAP_DATA;   // 0x17050 (Revision B)

// ----------------------------------------------------------------------------
// Sprites
// ----------------------------------------------------------------------------

// Sprite Animation Sequences For Crashes
//
// +00 [Long] Sprite Data Frame Address
// +04 [Byte] Bit 7: Set to H-Flip Sprite
//            Bit 0: Set Sprite to Sprite Priority Higher (Unused so far?)
// +05 [Byte] Sprite Colour Palette
// +06 [Byte] Passenger Frame
//            OR FOR Passenger Sprites: X Offset
// +07 [Byte] Set to denote end of frame sequence
//            OR FOR Passenger Sprites: Y Offset

extern uint32_t SPRITE_CRASH_SPIN1;        // 0x2294 (Revision B)
extern uint32_t SPRITE_CRASH_SPIN2;        // 0x22D4 (Revision B)
extern uint32_t SPRITE_BUMP_DATA1;         // 0x2314 (Revision B)
extern uint32_t SPRITE_BUMP_DATA2;         // 0x232C (Revision B)
extern uint32_t SPRITE_CRASH_MAN1;         // 0x2344 (Revision B)
extern uint32_t SPRITE_CRASH_GIRL1;        // 0x23B4 (Revision B)
extern uint32_t SPRITE_CRASH_FLIP;         // 0x2424 (Revision B) // Flip: Car
extern uint32_t SPRITE_CRASH_FLIP_MAN1;    // 0x2464 (Revision B) // Flip: Man
extern uint32_t SPRITE_CRASH_FLIP_GIRL1;   // 0x255C (Revision B) // Flip: Girl
extern uint32_t SPRITE_CRASH_FLIP_MAN2;    // 0x24DC (Revision B) // Post Flip: Man
extern uint32_t SPRITE_CRASH_FLIP_GIRL2;   // 0x25D4 (Revision B) // Post Flip: Girl
extern uint32_t SPRITE_CRASH_MAN2;         // 0x2604 (Revision B)
extern uint32_t SPRITE_CRASH_GIRL2;        // 0x2660 (Revision B)

const uint32_t SPRITE_CRASH_SPIN1_J      = SPRITE_CRASH_SPIN1 - 12;
const uint32_t SPRITE_CRASH_SPIN2_J      = SPRITE_CRASH_SPIN2 - 12;
const uint32_t SPRITE_BUMP_DATA1_J       = SPRITE_BUMP_DATA1  - 12;
const uint32_t SPRITE_BUMP_DATA2_J       = SPRITE_BUMP_DATA2  - 12;
const uint32_t SPRITE_CRASH_MAN1_J       = SPRITE_CRASH_MAN1  - 12;
const uint32_t SPRITE_CRASH_GIRL1_J      = SPRITE_CRASH_GIRL1 - 12;
const uint32_t SPRITE_CRASH_FLIP_J       = SPRITE_CRASH_FLIP  - 12;      // Flip: Car
const uint32_t SPRITE_CRASH_FLIP_MAN1_J  = SPRITE_CRASH_FLIP_MAN1 - 12;  // Flip: Man
const uint32_t SPRITE_CRASH_FLIP_GIRL1_J = SPRITE_CRASH_FLIP_GIRL1 - 12; // Flip: Girl
const uint32_t SPRITE_CRASH_FLIP_MAN2_J  = SPRITE_CRASH_FLIP_MAN2 - 12;  // Post Flip: Man
const uint32_t SPRITE_CRASH_FLIP_GIRL2_J = SPRITE_CRASH_FLIP_GIRL2 - 12; // Post Flip: Girl
const uint32_t SPRITE_CRASH_MAN2_J       = SPRITE_CRASH_MAN2 - 12;
const uint32_t SPRITE_CRASH_GIRL2_J      = SPRITE_CRASH_GIRL2 - 12;

// Sprite Default Properties
//
// +0: Sprite Properties
// +1: Draw Properties
// +2: Sprite Priority
// +3: Sprite Palette
// +4: Sprite Type
// +6: Sprite X World
// +8: Sprite Y World
// +A: Sprite Z
// +C: Routine Address
extern uint32_t SPRITE_DEF_PROPS1;   // 0x2B70 (Revision B)
const uint32_t SPRITE_DEF_PROPS1_J = 0x2B64;

// Best OutRunners Sprites
extern uint32_t SPRITE_DEF_PROPS2;   // 0x2FB2 (Revision B)
const uint32_t SPRITE_DEF_PROPS2_J = 0x2FA6;

// Sprite: Cloud Frames
extern uint32_t SPRITE_CLOUD_FRAMES;   // 0x4246 (Revision B)
const uint32_t SPRITE_CLOUD_FRAMES_J = 0x423A;

// Sprite: Mini Tree Frames
extern uint32_t SPRITE_MINITREE_FRAMES;   // 0x435C (Revision B)
const uint32_t SPRITE_MINITREE_FRAMES_J = 0x4350;

// Sprite: Grass Frames (Vary in thickness. Closer to the camera = Need Thicker Sprite.)
extern uint32_t SPRITE_GRASS_FRAMES;   // 0x4548 (Revision B)
const uint32_t SPRITE_GRASS_FRAMES_J = 0x453C;

// Sprite: Sand Frames
extern uint32_t SPRITE_SAND_FRAMES;   // 0x4588 (Revision B)
const uint32_t SPRITE_SAND_FRAMES_J = 0x457C;

// Sprite: Stone Frames
extern uint32_t SPRITE_STONE_FRAMES;   // 0x45C8 (Revision B)
const uint32_t SPRITE_STONE_FRAMES_J = 0x45BC;

// Sprite: Water Frames (Vary in thickness. Closer to the camera = Need Thicker Sprite.)
extern uint32_t SPRITE_WATER_FRAMES;   // 0x4608 (Revision B)
const uint32_t SPRITE_WATER_FRAMES_J = 0x45FC;

// Sprite: Shadow Frames
extern uint32_t SPRITE_SHDW_FRAMES;   // 0x7862 (Revision B)
const uint32_t SPRITE_SHDW_FRAMES_J = 0x7794;

// Sprite: Ferrari Frames, Offsets
extern uint32_t SPRITE_FERRARI_FRAMES;   // 0x9ECC (Revision B)
const uint32_t SPRITE_FERRARI_FRAMES_J = 0x9CF2;

// Sprite: Frame Data For Ferrari Skid
extern uint32_t SPRITE_SKID_FRAMES;   // 0x9F1C (Revision B)
const uint32_t SPRITE_SKID_FRAMES_J = 0x9D3A;

// Sprite: Passenger Frames (2 Frames for each, hair up and hair down)
extern uint32_t SPRITE_PASS_FRAMES;   // 0xA6EC (Revision B)
const uint32_t SPRITE_PASS_FRAMES_J = 0xA512;

// Table of smoke data from wheels
extern uint32_t SMOKE_DATA;   // 0xACC6 (Revision B)
const uint32_t SMOKE_DATA_J = 0xAAEC;

// Table of spray data from wheels
extern uint32_t SPRAY_DATA;   // 0xAD06 (Revision B)
const uint32_t SPRAY_DATA_J = 0xAB2C;

// Sprite: Shadow Data
extern uint32_t SPRITE_SHADOW_DATA;   // 0x103B6 (Revision B)
const uint32_t SPRITE_SHADOW_DATA_J = 0xFE16;

// Sprite: Passenger Skid Frames
extern uint32_t SPRITE_PASS1_SKIDL;   // 0x1107C (Revision B)
extern uint32_t SPRITE_PASS1_SKIDR;   // 0x110C2 (Revision B)
extern uint32_t SPRITE_PASS2_SKIDL;   // 0x110CC (Revision B)
extern uint32_t SPRITE_PASS2_SKIDR;   // 0x11112 (Revision B)
const uint32_t SPRITE_PASS1_SKIDL_J = 0x10ADC;
const uint32_t SPRITE_PASS1_SKIDR_J = 0x10B22;
const uint32_t SPRITE_PASS2_SKIDL_J = 0x10B2C;
const uint32_t SPRITE_PASS2_SKIDR_J = 0x10B72;

// Long addresses of sprite data for hardware
extern uint32_t SPRITE_TYPE_TABLE;   // 0x11ED2 (Revision B)
const uint32_t SPRITE_TYPE_TABLE_J = 0x11932;

// Master Sprite Table
//
// Each one of the following addresses contains the following:
//
// [+0] Sprite Frequency Value Bitmask [Word]
// [+2] Reload Value For Sprite Info Offset [Word]
// [+4] Start of table with x,y,type,palette etc.
extern uint32_t SPRITE_MASTER_TABLE;   // 0x1A43C (Revision B)
const uint32_t SPRITE_MASTER_TABLE_J = 0x19C96;

// OutRun logo data
extern uint32_t SPRITE_LOGO_BG;      // 0x11162 (Revision B)
extern uint32_t SPRITE_LOGO_CAR;     // 0x1128E (Revision B)
extern uint32_t SPRITE_LOGO_BIRD1;   // 0x112C0 (Revision B)
extern uint32_t SPRITE_LOGO_BIRD2;   // 0x112F2 (Revision B)
extern uint32_t SPRITE_LOGO_BASE;    // 0x1125C (Revision B)
extern uint32_t SPRITE_LOGO_TEXT;    // 0x11194 (Revision B)
extern uint32_t SPRITE_LOGO_PALM1;   // 0x111C6 (Revision B)
extern uint32_t SPRITE_LOGO_PALM2;   // 0x111F8 (Revision B)
extern uint32_t SPRITE_LOGO_PALM3;   // 0x1122A (Revision B)
const uint32_t SPRITE_LOGO_BG_J    = SPRITE_LOGO_BG - 0x5A0;
const uint32_t SPRITE_LOGO_CAR_J   = SPRITE_LOGO_CAR - 0x5A0;
const uint32_t SPRITE_LOGO_BIRD1_J = SPRITE_LOGO_BIRD1 - 0x5A0;
const uint32_t SPRITE_LOGO_BIRD2_J = SPRITE_LOGO_BIRD2 - 0x5A0;
const uint32_t SPRITE_LOGO_BASE_J  = SPRITE_LOGO_BASE - 0x5A0;
const uint32_t SPRITE_LOGO_TEXT_J  = SPRITE_LOGO_TEXT - 0x5A0;
const uint32_t SPRITE_LOGO_PALM1_J = SPRITE_LOGO_PALM1 - 0x5A0;
const uint32_t SPRITE_LOGO_PALM2_J = SPRITE_LOGO_PALM2 - 0x5A0;
const uint32_t SPRITE_LOGO_PALM3_J = SPRITE_LOGO_PALM3 - 0x5A0;

// Music Selection Screen - Sprite Data
extern uint32_t SPRITE_FM_LEFT;       // 0x11892 (Revision B)
extern uint32_t SPRITE_FM_CENTRE;     // 0x1189C (Revision B)
extern uint32_t SPRITE_FM_RIGHT;      // 0x118A6 (Revision B)
extern uint32_t SPRITE_DIAL_LEFT;     // 0x118B0 (Revision B)
extern uint32_t SPRITE_DIAL_CENTRE;   // 0x118BA (Revision B)
extern uint32_t SPRITE_DIAL_RIGHT;    // 0x118C4 (Revision B)
extern uint32_t SPRITE_EQ;            // 0x118CE (Revision B) // EQ Sprite, Part of Radio
extern uint32_t SPRITE_RADIO;         // 0x118D8 (Revision B)
extern uint32_t SPRITE_HAND_LEFT;     // 0x118E2 (Revision B)
extern uint32_t SPRITE_HAND_CENTRE;   // 0x118EC (Revision B)
extern uint32_t SPRITE_HAND_RIGHT;    // 0x118F6 (Revision B)

const uint32_t SPRITE_FM_LEFT_J     = SPRITE_FM_LEFT - 0x5A0;
const uint32_t SPRITE_FM_CENTRE_J   = SPRITE_FM_CENTRE - 0x5A0;
const uint32_t SPRITE_FM_RIGHT_J    = SPRITE_FM_RIGHT - 0x5A0;
const uint32_t SPRITE_DIAL_LEFT_J   = SPRITE_DIAL_LEFT - 0x5A0;
const uint32_t SPRITE_DIAL_CENTRE_J = SPRITE_DIAL_CENTRE - 0x5A0;
const uint32_t SPRITE_DIAL_RIGHT_J  = SPRITE_DIAL_RIGHT - 0x5A0;
const uint32_t SPRITE_EQ_J          = SPRITE_EQ - 0x5A0;
const uint32_t SPRITE_RADIO_J       = SPRITE_RADIO - 0x5A0;
const uint32_t SPRITE_HAND_LEFT_J   = SPRITE_HAND_LEFT - 0x5A0;
const uint32_t SPRITE_HAND_CENTRE_J = SPRITE_HAND_CENTRE - 0x5A0;
const uint32_t SPRITE_HAND_RIGHT_J  = SPRITE_HAND_RIGHT - 0x5A0;

// Shadow data
extern uint32_t SPRITE_SHDW_SMALL;   // 0x1193C (Revision B)
const uint32_t SPRITE_SHDW_SMALL_J = 0x1139C;

// Sprite Collision X Offsets [Signed]
// Table is indexed with the type of sprite. 
//
// Format:
// Word 1: X-Left Offset
// Word 2: X-Right Offset
extern uint32_t SPRITE_X_OFFS; // 0x1212A (Revision B)

// Sprite Zoom Lookup Table.
// Table Of Longs that represent X & Y Zoom Value
extern uint32_t SPRITE_ZOOM_LOOKUP; // 0x28000 (Revision B)

extern uint32_t MOVEMENT_LOOKUP_Z; // 0x30900 (Revision B)

// Table to alter sprite based on its y position.
//
// Input = Y Position
//
// Format:
//
// +0: Frame Number To Use
// +2: Entry In Zoom Lookup Table
extern uint32_t MAP_Y_TO_FRAME; // 0x30A00 (Revision B)

// ----------------------------------------------------------------------------
// Road
// ----------------------------------------------------------------------------

// Road Data Lookup Table
// Long Addresses where road data is looked up from.
// Note this only contains the road data itself. Namely the x position, and the length of each segment.
//
// The height and width are fetched from elsewhere.
// The sprites etc are fetched from elsewhere.
//
// Same location in both Jap and USA Assets.
//
// Note: Although 1AD92 might not be used, this seems to be a section of road with road split / checkpoint sign
const uint32_t ROAD_DATA_LOOKUP = 0x1224;

// Lookup table of road height information.
// Each entry is a long address into this asset.
const uint32_t ROAD_HEIGHT_LOOKUP   = 0x220A;
const uint32_t ROAD_HEIGHT_LOOKUP_J = 0x21F6;

// Data for Road Split (Same Address for Jap and USA Assets)
const uint32_t ROAD_DATA_SPLIT = 0x3A33E;

// Data for Road Bonus (Same Address for Jap and USA Assets)
const uint32_t ROAD_DATA_BONUS = 0x3ACA0;

// Data for Road Background Colour (Same Address for Jap and USA Assets)
const uint32_t ROAD_BGCOLOR = 0x109EE;

// Segment data for End Sequences (Master CPU Code)
extern uint32_t ROAD_SEG_TABLE_END;   // 0xE514 (Revision B)
const uint32_t ROAD_SEG_TABLE_END_J = 0xE1FE;

// Road Segment Table Information (Master CPU Code)
extern uint32_t ROAD_SEG_TABLE;   // 0xE528 (Revision B)
const uint32_t ROAD_SEG_TABLE_J = 0xE212;

// Segment data for Road Split (Master CPU Code)
extern uint32_t ROAD_SEG_TABLE_SPLIT;   // 0x1DFA4 (Revision B)
const uint32_t ROAD_SEG_TABLE_SPLIT_J = 0x1D5A6;


// ----------------------------------------------------------------------------
// Traffic Data
// ----------------------------------------------------------------------------

// Traffic Property Table
extern uint32_t TRAFFIC_PROPS;   // 0x4CFA (Revision B)
const uint32_t TRAFFIC_PROPS_J = 0x4CDE;

// There are six types of traffic in OutRun:
// Lorry, Pickup, Beetle, BMW, Corvette, Porsche.
//
// Each has 5 directional frames, including horizontal flipping.
// Some vehicles have different frames for inclines.
//
// Format is [0x20 byte boundaries]:
//
// [+0] Straight Frame
// [+4] Straight Frame (same as above)
// [+8] Right Frame
// [+C] Rightmost Frame 
//
// [+10] Straight Frame  [uphill version]
// [+14] Straight Frame  [uphill version] (same as above)
// [+18] Right Frame     [uphill version]
// [+1C] Rightmost Frame [uphill version]

extern uint32_t TRAFFIC_DATA;     // 0x5424 (Revision B)
const uint32_t TRAFFIC_DATA_J   = 0x5362;

extern uint32_t SPRITE_PORSCHE;   // 0xF290 (Revision B)
const uint32_t SPRITE_PORSCHE_J = 0xED22;

// ----------------------------------------------------------------------------
// Animation Sequences
// ----------------------------------------------------------------------------

// Flag Waver
extern uint32_t ANIM_SEQ_FLAG;   // 0x12382 (Revision B)
const uint32_t ANIM_SEQ_FLAG_J = ANIM_SEQ_FLAG - 0x5A0;

// Ferrari Drive-In Sequence
extern uint32_t ANIM_FERRARI_CURR;   // 0x12970 (Revision B)
extern uint32_t ANIM_FERRARI_NEXT;   // 0x129C0 (Revision B)
const uint32_t ANIM_FERRARI_CURR_J = ANIM_FERRARI_CURR - 0x5A0;
const uint32_t ANIM_FERRARI_NEXT_J = ANIM_FERRARI_NEXT - 0x5A0;

extern uint32_t ANIM_PASS1_CURR;   // 0x129C8 (Revision B)
extern uint32_t ANIM_PASS1_NEXT;   // 0x12A18 (Revision B)
const uint32_t ANIM_PASS1_CURR_J = ANIM_PASS1_CURR - 0x5A0;
const uint32_t ANIM_PASS1_NEXT_J = ANIM_PASS1_NEXT - 0x5A0;

extern uint32_t ANIM_PASS2_CURR;   // 0x12A20 (Revision B)
extern uint32_t ANIM_PASS2_NEXT;   // 0x12A70 (Revision B)
const uint32_t ANIM_PASS2_CURR_J = ANIM_PASS2_CURR - 0x5A0;
const uint32_t ANIM_PASS2_NEXT_J = ANIM_PASS2_NEXT - 0x5A0;

// ----------------------------------------------------------------------------
// End Sequence / Bonus Mode
// ----------------------------------------------------------------------------

// Ferrari Sprite Frame Data For Bonus Mode
//
// +0 [Long]: Address of frame
// +4 [Byte]: Passenger Offset (always 0!)
// +5 [Byte]: Ferrari X Change
// +6 [Byte]: Sprite Colour Palette
// +7 [Byte]: H-Flip
extern uint32_t ANIM_FERRARI_FRAMES;   // 0xA2F0 (Revision B)
const uint32_t ANIM_FERRARI_FRAMES_J = 0xA116;

// Note each table is used by every end sequence animation.
// And contains the variance for that particular animation.
extern uint32_t ANIM_ENDSEQ_OBJ1;      // 0x124B0 (Revision B) // Ferrari Door Opening
extern uint32_t ANIM_ENDSEQ_OBJ2;      // 0x124D8 (Revision B) // Ferrari Interior
extern uint32_t ANIM_ENDSEQ_OBJ3;      // 0x12500 (Revision B) // Car Shadow & Man Sprite
extern uint32_t ANIM_ENDSEQ_OBJ4;      // 0x12528 (Revision B) // Man Shadow & Female Sprite
extern uint32_t ANIM_ENDSEQ_OBJ5;      // 0x12550 (Revision B) // Female Shadow & Person Presenting Trophy
extern uint32_t ANIM_ENDSEQ_OBJ6;      // 0x12578 (Revision B)
extern uint32_t ANIM_ENDSEQ_OBJ7;      // 0x125A0 (Revision B)
extern uint32_t ANIM_ENDSEQ_OBJ8;      // 0x125C8 (Revision B) // Animation Data Sequence For Trophy Person
extern uint32_t ANIM_ENDSEQ_OBJA;      // 0x125F0 (Revision B)
extern uint32_t ANIM_ENDSEQ_OBJB;      // 0x12618 (Revision B)
extern uint32_t ANIM_END_TABLE;        // 0x123A2 (Revision B) // Start/End Animation Sequence Markers (Only End Markers Appear To Be Used)
const uint32_t ANIM_ENDSEQ_OBJ1_J    = ANIM_ENDSEQ_OBJ1 - 0x5A0;
const uint32_t ANIM_ENDSEQ_OBJ2_J    = ANIM_ENDSEQ_OBJ2 - 0x5A0;
const uint32_t ANIM_ENDSEQ_OBJ3_J    = ANIM_ENDSEQ_OBJ3 - 0x5A0;
const uint32_t ANIM_ENDSEQ_OBJ4_J    = ANIM_ENDSEQ_OBJ4 - 0x5A0;
const uint32_t ANIM_ENDSEQ_OBJ5_J    = ANIM_ENDSEQ_OBJ5 - 0x5A0;
const uint32_t ANIM_ENDSEQ_OBJ6_J    = ANIM_ENDSEQ_OBJ6 - 0x5A0;
const uint32_t ANIM_ENDSEQ_OBJ7_J    = ANIM_ENDSEQ_OBJ7 - 0x5A0;
const uint32_t ANIM_ENDSEQ_OBJ8_J    = ANIM_ENDSEQ_OBJ8 - 0x5A0;
const uint32_t ANIM_ENDSEQ_OBJA_J    = ANIM_ENDSEQ_OBJA - 0x5A0;
const uint32_t ANIM_ENDSEQ_OBJB_J    = ANIM_ENDSEQ_OBJB - 0x5A0;
const uint32_t ANIM_END_TABLE_J      = ANIM_END_TABLE   - 0x5A0;
// ----------------------------------------------------------------------------
// Course Map Data
// ----------------------------------------------------------------------------

// Course Map Sprite Data
extern uint32_t SPRITE_COURSEMAP;   // 0x26BE (Revision B)
const uint32_t SPRITE_COURSEMAP_J = 0x26B2; // Japanese ASSETs

// Coursemap Sprite Components
extern uint32_t SPRITE_COURSEMAP_TOP;   // 0x3784 (Revision B)
extern uint32_t SPRITE_COURSEMAP_BOT;   // 0x386C (Revision B)
extern uint32_t SPRITE_COURSEMAP_END;   // 0x3954 (Revision B)
const uint32_t SPRITE_COURSEMAP_TOP_J = SPRITE_COURSEMAP_TOP - 12;
const uint32_t SPRITE_COURSEMAP_BOT_J = SPRITE_COURSEMAP_BOT - 12;
const uint32_t SPRITE_COURSEMAP_END_J = SPRITE_COURSEMAP_END - 12;

// Minicar
extern uint32_t SPRITE_MINICAR_RIGHT;   // 0x10C58 (Revision B)
extern uint32_t SPRITE_MINICAR_UP;      // 0x10C62 (Revision B)
extern uint32_t SPRITE_MINICAR_DOWN;    // 0x10C6C (Revision B)
const uint32_t SPRITE_MINICAR_RIGHT_J = SPRITE_MINICAR_RIGHT - 0x5A0;
const uint32_t SPRITE_MINICAR_UP_J    = SPRITE_MINICAR_UP - 0x5A0;
const uint32_t SPRITE_MINICAR_DOWN_J  = SPRITE_MINICAR_DOWN - 0x5A0;

// Convert route information to piece index to colour
extern uint32_t MAP_ROUTE_LOOKUP; // 0x3636 (Revision B)

// X and Y Movement table for Minicar
extern uint32_t MAP_MOVEMENT_LEFT; // 0x3A34 (Revision B)
extern uint32_t MAP_MOVEMENT_RIGHT; // 0x3AB4 (Revision B)

// ----------------------------------------------------------------------------
// Best OutRunners Data
// ----------------------------------------------------------------------------

// Tiles for minicars that run across the text layer
extern uint32_t TILES_MINICARS1; // 0xD62A (Revision B)
extern uint32_t TILES_MINICARS2; // 0xD670 (Revision B) // smoke tiles

// Default Score Data
extern uint32_t DEFAULT_SCORES;  // 0xD676 (Revision B)

// Alphabet characters for initial entry
extern uint32_t TILES_ALPHABET;  // 0xD5A4 (Revision B)

// ----------------------------------------------------------------------------
// Music Selection Data
// ----------------------------------------------------------------------------

// Palette to cycle the graphical equalizer on the radio
extern uint32_t MUSIC_EQ_PAL; // 0xCCAA (Revision B)

// ----------------------------------------------------------------------------
// Data
// ----------------------------------------------------------------------------

// X,Y Offsets of passenger 1 (Man) from car
//
// Format 
//
// Word 1: Frame 1 X Offset
// Word 2: Frame 1 Y Offset 
// Word 3: Frame 1 X Offset H-FLIP
// Word 4: Frame 1 Y Offset H-FLIP
//
// Word 5: Frame 2 X Offset
// Word 6: Frame 2 Y Offset
//
// etc.
extern uint32_t PASS1_OFFSET; // 0xA6FC (Revision B)
extern uint32_t PASS2_OFFSET; // 0xA75C (Revision B)

// Map stage ID to number 0-14
extern uint32_t MAP_STAGE_ID; // 0xDDB4 (Revision B)

// Width / Height Table for sprites
extern uint32_t WH_TABLE; // 0x20000 (Revision B)

// Movement Lookup Table
// Used by:
// - Logo Bird Animation In Attract Mode
// - Car Flip In Crash Scenario
//
// Entries 
// 0     - 0xFF : X Position
// 0x100 - 0x1FF: Y Position
extern uint32_t DATA_MOVEMENT; // 0x30800 (Revision B)