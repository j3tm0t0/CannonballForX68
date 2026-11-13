#!/bin/sh
# Cannonball for x68 - X68000 port by Mattsuan-Z@ColtMaz.
# Part of a derivative work of Cannonball (Copyright Chris White and the Cannonball team),
# released under the Cannonball licence (LICENSE / LICENSE.txt). Other notices below are kept as they were.
set -eu
cd "$(dirname "$0")"
mkdir -p build/host
find vendor/main/engine vendor/main/hwvideo -name '*.cpp' ! -path 'vendor/main/engine/audio/*' > build/sources.txt
for f in video.cpp assets.cpp trackloader.cpp utils.cpp frontend/ttrial.cpp frontend/cabdiag.cpp; do echo "vendor/main/$f" >> build/sources.txt; done
echo sprite_reference.cpp >> build/sources.txt
echo sprite_stats.cpp >> build/sources.txt
echo render_reference.cpp >> build/sources.txt
echo visuals.cpp >> build/sources.txt
echo road_bridge.cpp >> build/sources.txt
echo draw_profile.cpp >> build/sources.txt
echo audio_silent.cpp >> build/sources.txt
# r031: PC check of the X68000 sound (CBX68_AUDIO_REPLAY=AUDIO.BIN)
echo vendor/main/engine/audio/osound.cpp >> build/sources.txt
echo audio_replay_host.cpp >> build/sources.txt
echo assetloader_local.cpp >> build/sources.txt
cat build/sources.txt | xargs -P 4 -I{} sh -c 'f="$1"; o="build/host/$(echo "$f" | tr / _).o"; g++ -std=c++14 -O2 -w -Istub -Ivendor/main -c "$f" -o "$o"' sh {}
g++ -std=c++14 -O2 -w -Istub -Ivendor/main main.cpp $(while IFS= read -r f; do printf "build/host/%s.o " "$(echo "$f" | tr / _)"; done < build/sources.txt) -o build/host/stage1
./build/host/stage1 test 300   # display test, needs no ROM
