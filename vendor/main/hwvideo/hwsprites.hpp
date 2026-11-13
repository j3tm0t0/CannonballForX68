#pragma once

#include "stdint.hpp"
#ifndef OR_X68K
#include <cstdio>
#endif

class video;

class hwsprites
{
public:
    hwsprites();
    ~hwsprites();
    void init(const uint8_t*);
    void reset();
    void set_x_clip(bool);
    void swap();
    uint8_t read(const uint16_t adr);
    void write(const uint16_t adr, const uint16_t data);
    void render(const uint8_t);
    void capture_entry(const uint16_t*,uint16_t*);
    void capture_entry_table(const uint16_t*,uint16_t*,const uint16_t* table,int* xbounds=nullptr);
    void capture_entry_table8(const uint16_t*,uint8_t*,const uint16_t* table,int* xbounds=nullptr);   // 8-bit pixels   // r026e (xbounds: see SpriteFastParams)
#ifndef OR_X68K
    void render_reference(const uint8_t);
    void save_render_state(uint16_t*) const;
    void load_render_state(const uint16_t*);
    void verify_sprite_cases();
    void verify_sprite_extra_cases();
    void count_sprite_work();
    void sprite_work_paths(unsigned long long*) const;
    void print_sprite_work(FILE*);
#endif

private:
    // Clip values.
    uint16_t x1, x2;

    // 128 sprites, 16 bytes each (0x400)
    static const uint16_t SPRITE_RAM_SIZE = 128 * 8;
    static const uint32_t SPRITES_LENGTH = 0x100000 >> 2;
    static const uint16_t COLOR_BASE = 0x800;

    uint32_t sprites[SPRITES_LENGTH]; // Converted sprites
    
    // Two halves of RAM
    // Cannonball for x68: exchanged by their pointers (swap), not by copying
    uint16_t ramStore[2][SPRITE_RAM_SIZE];
    uint16_t* ram = ramStore[0];
    uint16_t* ramBuff = ramStore[1];
};

