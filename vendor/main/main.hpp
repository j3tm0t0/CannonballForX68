#pragma once

#include "globals.hpp"
#include "sdl2/audio.hpp"

namespace cannonball
{
    extern Audio audio;

    // Frame counter
	extern int frame;

    // Tick Logic. Used when running at non-standard > 30 fps
    extern bool tick_frame;

    // Cannonball for x68: true while a game step runs whose picture will not be drawn (picture
    // skipping on a slow machine). Code that only prepares what the picture is made from (the
    // road RAM contents) can then leave that out. Set by the port's main loop.
    extern bool skip_render;

    // Millisecond Time Per Frame
    extern double frame_ms;

    // FPS Counter
    extern int fps_counter;

    // Engine Master State
    extern int state;
    
    enum
    {
        STATE_BOOT,
        STATE_INIT_MENU,
        STATE_MENU,
        STATE_INIT_GAME,
        STATE_GAME,
        STATE_QUIT
    };
}

int main(int argc, char* argv[]);
