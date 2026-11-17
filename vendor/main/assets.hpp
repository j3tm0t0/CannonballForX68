/***************************************************************************
    Load OutRun ASSET Set.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#pragma once

#include "assetloader.hpp"

class Assets
{
public:
    // Western ASSETs
    AssetLoader asset0;
    AssetLoader asset1;
    AssetLoader tiles;
    AssetLoader sprites;
    AssetLoader road;
    AssetLoader z80;
    AssetLoader pcm;

    // Japanese ASSETs
    AssetLoader j_asset0;
    AssetLoader j_asset1;

    // Paged Assets (Swap between Jap and Western)
    AssetLoader* asset0p;
    AssetLoader* asset1p;

    // Cannonball for x68: the master CPU and sprite ROMs loaded are Revision A
    bool rev_a;

    Assets();
    ~Assets();
    bool load_revb_assets(bool);
    bool load_japanese_assets();
    int load_master(bool a, bool verbose);
    int load_sprites(bool a);
    int load_pcm_asset(bool);
    bool load_ym_data(const char* filename);

private:
    int jap_asset_status;
    const static bool VERBOSE = true;
};

extern Assets assets;