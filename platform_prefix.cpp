/* Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
   Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
   released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were. */
#include "draw_profile.hpp"
#include "render_schedule.hpp"
#include "palette_cache.hpp"
#ifdef OR_X68K
#include "native_platform.hpp"
#endif
// Headless Cannonball driver (player mode): drives stage 1 with scripted keys and dumps the
// complete road/car state per logic tick (reference for the ORCOURSE game engine port).
// usage: hl NTICKS dump.bin SHOT_EVERY ASSET_DIR/
// Built by build_hl.sh against the Cannonball source (https://github.com/djyt/cannonball).
#include <cstdio>
#include <cstring>
#include <string>
#include "main.hpp"
#include "frontend/config.hpp"
#include "sdl2/input.hpp"
#include "sdl2/rendersurface.hpp"
#include "assetloader.hpp"
#include "assets.hpp"
#include "video.hpp"
#include "engine/outrun.hpp"
#include "engine/oroad.hpp"
#include "engine/oinitengine.hpp"
#include "engine/ostats.hpp"
#include "engine/oferrari.hpp"
#include "engine/oinputs.hpp"
#include "hwvideo/hwroad.hpp"
#include "engine/omusic.hpp"
#include "engine/otiles.hpp"
#include "engine/ocrash.hpp"
#include "trackloader.hpp"
#include "engine/osprites.hpp"
#include "engine/olevelobjs.hpp"
#include "engine/oanimseq.hpp"

// ---------------------------------------------------------------- globals normally in main.cpp
namespace cannonball { Audio audio; int frame = 0; bool tick_frame = true; bool skip_render = false; double frame_ms = 0; int fps_counter = 0; int state = 0; }
Config config;
Input input;
namespace forcefeedback { void set(int, int) {} }

// ---------------------------------------------------------------- Config stub
#include "engine/audio/commands.hpp"
Config::Config() {
    music_t m; m.type = music_t::IS_YM_INT;
    m.cmd = sound::MUSIC_MAGICAL; m.title = "MAGICAL SOUND SHOWER"; sound.music.push_back(m);
    m.cmd = sound::MUSIC_BREEZE;  m.title = "PASSING BREEZE";       sound.music.push_back(m);
    m.cmd = sound::MUSIC_SPLASH;  m.title = "SPLASH WAVE";          sound.music.push_back(m);
}
Config::~Config() {}
void Config::load_scores(bool) {}
void Config::save_scores(bool) {}
void Config::load_tiletrial_scores() {}
void Config::save_tiletrial_scores() {}

// ---------------------------------------------------------------- Audio stub
Audio::Audio() {}
Audio::~Audio() {}
void Audio::clear_wav() {}
void Audio::load_wav(const char*) {}

// ---------------------------------------------------------------- Input stub
Input::Input() { memset(keys, 0, sizeof keys); memset(keys_old, 0, sizeof keys_old); }
Input::~Input() {}
bool Input::is_pressed(presses p) { return keys[p]; }
bool Input::is_pressed_clear(presses p) { bool b = keys[p]; keys[p] = false; return b; }
bool Input::has_pressed(presses p) { return keys[p] && !keys_old[p]; }

// ---------------------------------------------------------------- Render stub
static uint32_t g_rgb[S16_PALETTE_ENTRIES * 2];
RenderBase::RenderBase() { surface = NULL; screen_pixels = NULL; orig_width = orig_height = 0; }
void RenderBase::convert_palette(uint32_t adr, uint32_t r, uint32_t g, uint32_t b)
{ adr >>= 1; g_rgb[adr] = (r*8)<<16 | (g*8)<<8 | b*8;
  g_rgb[adr + S16_PALETTE_ENTRIES] = ((r*shadow_multi/31)<<16) | ((g*shadow_multi/31)<<8) | (b*shadow_multi/31);
  display_palette_update(adr,g_rgb[adr]);
  display_palette_update(adr+S16_PALETTE_ENTRIES,g_rgb[adr+S16_PALETTE_ENTRIES]); }
void RenderBase::set_shadow_intensity(float f) { shadow_multi = int(255.0f*f); }
static uint16_t* g_pixels = 0;
Render::Render() {}
Render::~Render() {}
bool Render::init(int, int, int, int, int) { return true; }
void Render::disable() {}
bool Render::start_frame() { return true; }
bool Render::finalize_frame() { return true; }
void Render::draw_frame(uint16_t* pixels) { g_pixels = pixels;
#ifdef OR_X68K
 platform_present(pixels, g_rgb);
#endif
}


#ifdef OR_X68K
static bool platform_keys(int frame,int ingame,bool demo){
    if(native_key(1))return false;
    if(!demo){
        input.keys[Input::ACCEL]=native_key(0x3c)||native_key(0x2a);
        input.keys[Input::BRAKE]=native_key(0x3e)||native_key(0x2b);
        input.keys[Input::LEFT]=native_key(0x3b);
        input.keys[Input::RIGHT]=native_key(0x3d);
        input.keys[Input::GEAR1]=native_key(0x35);
        input.keys[Input::COIN]=input.keys[Input::COIN]||native_key(0x2c);
        input.keys[Input::START]=input.keys[Input::START]||native_key(0x1d);
    }
    (void)frame;(void)ingame;return true;
}

#endif
