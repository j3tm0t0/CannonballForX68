/***************************************************************************
    Load OutRun ASSET Set.

    Copyright Chris White.
    See license.txt for more details.
***************************************************************************/

#include <iostream>
#include <cstring>
#include "stdint.hpp"
#include "assets.hpp"

Assets assets;

Assets::Assets()
{
    jap_asset_status = -1;
    asset0p = NULL;
    asset1p = NULL;
}

Assets::~Assets(void)
{
}

// Tidier way to address pointer to member function
// Expanded example: (asset0.*(asset0.load))("epr-10380b.133", 0x00000, 0x10000, 0x1f6cadad, AssetLoader::INTERLEAVE2);
#define LOAD(asset, args) (asset.*(asset.load)) args

bool Assets::load_revb_assets(bool fixed_asset)
{
    // If incremented, a asset has failed to load.
    int status = 0;

    // Load Master CPU ASSETs
    asset0.init(0x40000);
    // Cannonball for x68: the Revision A master CPU ROMs are also accepted (same data at other
    // addresses: oaddresses_rev_a() sets them). Try Revision B first, then A, quietly; if neither
    // set is complete, try B again with the messages.
    rev_a = false;
    if (load_master(false, false) != 0)
    {
        if (load_master(true, false) == 0)
            rev_a = true;
        else
            status += load_master(false, VERBOSE);
    }

    // Load Slave CPU ASSETs
    asset1.init(0x40000);
    status += LOAD(asset1, ("epr-10327a.76", 0x00000, 0x10000, 0xe28a5baf, AssetLoader::INTERLEAVE2, VERBOSE));
    status += LOAD(asset1, ("epr-10329a.58", 0x00001, 0x10000, 0xda131c81, AssetLoader::INTERLEAVE2, VERBOSE));
    status += LOAD(asset1, ("epr-10328a.75", 0x20000, 0x10000, 0xd5ec5e5d, AssetLoader::INTERLEAVE2, VERBOSE));
    status += LOAD(asset1, ("epr-10330a.57", 0x20001, 0x10000, 0xba9ec82a, AssetLoader::INTERLEAVE2, VERBOSE));

    // Load Non-Interleaved Tile ASSETs
    tiles.init(0x30000);
    status += LOAD(tiles, ("opr-10268.99", 0x00000, 0x08000, 0x95344b04, AssetLoader::NORMAL, VERBOSE));
    status += LOAD(tiles, ("opr-10232.102", 0x08000, 0x08000, 0x776ba1eb, AssetLoader::NORMAL, VERBOSE));
    status += LOAD(tiles, ("opr-10267.100", 0x10000, 0x08000, 0xa85bb823, AssetLoader::NORMAL, VERBOSE));
    status += LOAD(tiles, ("opr-10231.103", 0x18000, 0x08000, 0x8908bcbf, AssetLoader::NORMAL, VERBOSE));
    status += LOAD(tiles, ("opr-10266.101", 0x20000, 0x08000, 0x9f6f1a74, AssetLoader::NORMAL, VERBOSE));
    status += LOAD(tiles, ("opr-10230.104", 0x28000, 0x08000, 0x686f5e50, AssetLoader::NORMAL, VERBOSE));

    // Load Non-Interleaved Road ASSETs (2 identical assets, 1 for each road)
    road.init(0x10000);
    status += LOAD(road, ("opr-10185.11", 0x000000, 0x08000, 0x22794426, AssetLoader::NORMAL, VERBOSE));
    status += LOAD(road, ("opr-10186.47", 0x008000, 0x08000, 0x22794426, AssetLoader::NORMAL, VERBOSE));

    // Load Interleaved Sprite ASSETs
    sprites.init(0x100000);
    status += load_sprites(rev_a);

    // Load Z80 Sound ASSET
    // Note: This is a deliberate decision to double the Z80 ASSET Space to accomodate extra FM based music
    z80.init(0x10000);
    status += LOAD(z80, ("epr-10187.88", 0x0000, 0x08000, 0xa10abaa9, AssetLoader::NORMAL, VERBOSE));

    // Load Sega PCM Chip Samples
    pcm.init(0x60000);
    status += LOAD(pcm, ("opr-10193.66", 0x00000, 0x08000, 0xbcd10dde, AssetLoader::NORMAL, VERBOSE));
    status += LOAD(pcm, ("opr-10192.67", 0x10000, 0x08000, 0x770f1270, AssetLoader::NORMAL, VERBOSE));
    status += LOAD(pcm, ("opr-10191.68", 0x20000, 0x08000, 0x20a284ab, AssetLoader::NORMAL, VERBOSE));
    status += LOAD(pcm, ("opr-10190.69", 0x30000, 0x08000, 0x7cab70e2, AssetLoader::NORMAL, VERBOSE));
    status += LOAD(pcm, ("opr-10189.70", 0x40000, 0x08000, 0x01366b54, AssetLoader::NORMAL, VERBOSE));
    status += LOAD(pcm, ("opr-10188.71", 0x50000, 0x08000, 0xbad30ad9, AssetLoader::NORMAL, VERBOSE));
    status += load_pcm_asset(fixed_asset);

    // If status has been incremented, a asset has failed to load.
    return status == 0;
}

bool Assets::load_japanese_assets()
{
    // Only attempt to initalize the arrays once.
    if (jap_asset_status == -1)
    {
        j_asset0.init(0x40000);
        j_asset1.init(0x40000);
    }

    // If incremented, a asset has failed to load.
    jap_asset_status = 0;

    // Load Master CPU ASSETs     
    jap_asset_status += LOAD(j_asset0, ("epr-10380.133", 0x00000, 0x10000, 0xe339e87a, AssetLoader::INTERLEAVE2, VERBOSE));
    jap_asset_status += LOAD(j_asset0, ("epr-10382.118", 0x00001, 0x10000, 0x65248dd5, AssetLoader::INTERLEAVE2, VERBOSE));
    jap_asset_status += LOAD(j_asset0, ("epr-10381.132", 0x20000, 0x10000, 0xbe8c412b, AssetLoader::INTERLEAVE2, VERBOSE));
    jap_asset_status += LOAD(j_asset0, ("epr-10383.117", 0x20001, 0x10000, 0xdcc586e7, AssetLoader::INTERLEAVE2, VERBOSE));

    // Load Slave CPU ASSETs        
    jap_asset_status += LOAD(j_asset1, ("epr-10327.76", 0x00000, 0x10000, 0xda99d855, AssetLoader::INTERLEAVE2, VERBOSE));
    jap_asset_status += LOAD(j_asset1, ("epr-10329.58", 0x00001, 0x10000, 0xfe0fa5e2, AssetLoader::INTERLEAVE2, VERBOSE));
    jap_asset_status += LOAD(j_asset1, ("epr-10328.75", 0x20000, 0x10000, 0x3c0e9a7f, AssetLoader::INTERLEAVE2, VERBOSE));
    jap_asset_status += LOAD(j_asset1, ("epr-10330.57", 0x20001, 0x10000, 0x59786e99, AssetLoader::INTERLEAVE2, VERBOSE));
    // If status has been incremented, a asset has failed to load.
    return jap_asset_status == 0;
}

int Assets::load_master(bool a, bool verbose)
{
    int status = 0;
    status += LOAD(asset0, (a ? "epr-10380a.133" : "epr-10380b.133", 0x00000, 0x10000, a ? 0x58b84d83 : 0x1f6cadad, AssetLoader::INTERLEAVE2, verbose));
    status += LOAD(asset0, (a ? "epr-10382a.118" : "epr-10382b.118", 0x00001, 0x10000, a ? 0x39387c63 : 0xc4c3fa1a, AssetLoader::INTERLEAVE2, verbose));
    status += LOAD(asset0, (a ? "epr-10381a.132" : "epr-10381b.132", 0x20000, 0x10000, 0xbe8c412b, AssetLoader::INTERLEAVE2, verbose));
    status += LOAD(asset0, (a ? "epr-10383a.117" : "epr-10383b.117", 0x20001, 0x10000, a ? 0xdcc586e7 : 0x10a2014a, AssetLoader::INTERLEAVE2, verbose));
    return status;
}

int Assets::load_sprites(bool a)
{
    static const uint32_t CRC_B[8] = { 0x7cc86208, 0xb0d26ac9, 0x59b60bd7, 0x17a1b04a, 0xb557078c, 0x8051e517, 0xf3b8f318, 0xa1062984 };
    static const uint32_t CRC_A[8] = { 0x8ad7c647, 0xce7d3841, 0xccf042a0, 0xc14f1d68, 0x6f49eae7, 0xe19e7d1f, 0xf83116ab, 0xe531855a };
    static const char* const NAME[8] = { "mpr-10371.9", "mpr-10373.10", "mpr-10375.11", "mpr-10377.12", "mpr-10372.13", "mpr-10374.14", "mpr-10376.15", "mpr-10378.16" };
    int status = 0;
    for (int i = 0; i < 8; i++)
        status += LOAD(sprites, (NAME[i], (i >> 2) * 0x80000 + (i & 3), 0x20000, (int)(a ? CRC_A[i] : CRC_B[i]), AssetLoader::INTERLEAVE4, VERBOSE));
    return status;
}

int Assets::load_pcm_asset(bool fixed_asset)
{
    int status = 0;
    if (fixed_asset)
    {
        status = LOAD(pcm, ("opr-10188.71f", 0x50000, 0x08000, 0x37598616, AssetLoader::NORMAL, false));
        if (status == 1)
            status = LOAD(pcm, ("opr-10188.71f", 0x50000, 0x08000, 0xC2DE09B2, AssetLoader::NORMAL, VERBOSE));
    }
    else
    {
        status = LOAD(pcm, ("opr-10188.71", 0x50000, 0x08000, 0xbad30ad9, AssetLoader::NORMAL, VERBOSE));
    }

    //return LOAD(pcm, (fixed_asset ? "opr-10188.71f" : "opr-10188.71", 0x50000, 0x08000, fixed_asset ? 0x37598616 : 0xbad30ad9, AssetLoader::NORMAL)) == 0;
    return status;
}

bool Assets::load_ym_data(const char* filename)
{
    AssetLoader data;
    if (data.load_binary(filename) == 0)
    {
        if (data.length < 0x8000)
        {
            memcpy(z80.asset + 0x8000, data.asset, data.length);
            data.unload();
            return true;
        }
        else
        {
            std::cout << "YM Data is too large: " << filename << std::endl;
        }
    }
    return false;
}