#!/bin/sh
# Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
# Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
# released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were.
set -eu
cd "$(dirname "$0")"
mkdir -p build/x68k
# m68k-xelf toolchain (gcc for X68000 / Human68k): set M68K_XELF to its folder
M68K_XELF=${M68K_XELF:-/root/afterburner-m68k-xelf}
export PATH=$M68K_XELF/bin:$PATH
CXX=$M68K_XELF/bin/m68k-xelf-g++
export CXX
find vendor/main/engine vendor/main/hwvideo -name '*.cpp' ! -path 'vendor/main/engine/audio/*' > build/sources_x68k.txt
for f in video.cpp assets.cpp trackloader.cpp utils.cpp frontend/ttrial.cpp frontend/cabdiag.cpp; do echo "vendor/main/$f" >> build/sources_x68k.txt; done
echo visuals.cpp >> build/sources_x68k.txt
echo road_bridge.cpp >> build/sources_x68k.txt
echo road_irq.S >> build/sources_x68k.txt
echo scenery_tiles.S >> build/sources_x68k.txt
echo pcm_mix.S >> build/sources_x68k.txt
echo draw_profile.cpp >> build/sources_x68k.txt
# r031: the sound code of Cannonball with the X68000's FM chip (audio_x68k.cpp) instead of audio_silent.cpp
echo vendor/main/engine/audio/osound.cpp >> build/sources_x68k.txt
echo vendor/main/engine/audio/osoundint.cpp >> build/sources_x68k.txt
echo audio_x68k.cpp >> build/sources_x68k.txt
echo assetloader_local.cpp >> build/sources_x68k.txt
cat build/sources_x68k.txt | xargs -P 4 -I{} sh -c 'f="$1"; o="build/x68k/$(echo "$f" | tr / _).o"; "$CXX" -DOR_X68K ${CBX68_DEFS:-} -m68060 -msoft-float -std=c++14 ${CBX68_OPT:--O3} -w -fno-strict-aliasing -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections -Istub -Ivendor/main -c "$f" -o "$o"' sh {}
# r025b: code compiled for 68060 (soft float); linked with the 68000 libraries (crt0, libc, libstdc++, libgcc) known to work
"$CXX" -DOR_X68K ${CBX68_DEFS:-} -m68060 -msoft-float -std=c++14 ${CBX68_OPT:--O3} -w -fno-strict-aliasing -fno-exceptions -fno-rtti -DOR_X68K -Istub -Ivendor/main -ffunction-sections -fdata-sections -c main.cpp -o build/x68k/main.cpp.o
"$CXX" -DOR_X68K -m68000 -std=c++14 ${CBX68_OPT:--O3} -w -fno-strict-aliasing -fno-exceptions -fno-rtti -DOR_X68K -Istub -Ivendor/main build/x68k/main.cpp.o $(while IFS= read -r f; do printf "build/x68k/%s.o " "$(echo "$f" | tr / _)"; done < build/sources_x68k.txt) -Wl,-Map=build/STAGE1.map -o STAGE1.X
