# Cannonball for x68

An unofficial X68000 port of [Cannonball](https://github.com/djyt/cannonball), the OutRun engine
by Chris White. X68000 port: **Mattsuan-Z@ColtMaz**.

It contains no game data. Included is a demo that needs no ROM: the road, backgrounds and
roadside objects are all made by the program and shown with the X68000's display hardware
(`CBX68 TEST`). **No ROMs are included.**

**Runs in practice only on the XM6 emulator** (X68030, 100MHz, 68060 x4, 12MB, 060turbo
memory; see the README below). It is far too heavy for a real X68000 / X68030, WebX68k
and the X68000 Z.

The program for the X68000 is on the Releases page. The text below is the README included with it.

## Building

- X68000: `M68K_XELF=<folder of the m68k-xelf toolchain> sh build_x68k.sh` makes `STAGE1.X`
  (rename it to `CBX68.X`). Needs m68k-xelf-gcc with the Human68k libraries.
- PC check (Linux, g++): `sh build_host.sh` builds `build/host/stage1` and runs the demo (`CBX68 TEST`).
  With ROMs: `CBX68_ROMS=<rom folder>/ ./build/host/stage1 2000`.

## README (as included with the program)

```
Cannonball for x68  v0.2
========================

An unofficial port of Cannonball (the OutRun engine by Chris White) to the X68000.
X68000 port: Mattsuan-Z@ColtMaz
This is a non-commercial hobby project. It is not affiliated with, endorsed by or
connected to SEGA or the Cannonball team.
OutRun is a trademark of the SEGA Corporation.

It contains no game data. Included is a demo that needs no ROM: the road,
backgrounds and roadside objects are all made by the program and shown with the
X68000's display hardware (CBX68 TEST).
No ROMs are included.

Requirements
------------
IMPORTANT: this program is far too heavy for a real X68000 / X68030, for WebX68k
and for the X68000 Z. In practice it runs only on the XM6 emulator with these settings:
- Model X68030, 100MHz, MPU 68060 with clock x4, FPU on, extended ROM on
  (needed to start with the 68060), "MPU speed-up" on
- Main memory 12MB, extended memory 060turbo 256MB
- MPU / VM no-wait off, "VM sync to PC VSYNC" off
- Human68k (3.02)
Even with these settings the speed drops in busy scenes. Real hardware, WebX68k and
the X68000 Z have not been tested and are not expected to run it at a usable speed
(not supported).

How to use
----------
1. Put CBX68.X, TILEMAP.BIN and TILEPAT.BIN in one folder.
2. Make a folder ROMS in that folder and put the OutRun Revision B ROM files in it
   (the set Cannonball uses: epr-10380b.133, mpr-10371.9, opr-10185.11 ...).
   As in Cannonball, the files are found by their CRC32, so their names do not matter
   (8.3 names such as 10380B.133 are fine).
   The Revision A set also works: its master CPU and sprite ROMs (the other ROMs are the same
   as in Revision B) are found by their CRC32 in the same way. The game then plays with the
   Revision A data (for example the stage 1 hills and the default best scores differ).
3. Start:  CBX68 0 30 manual both
   (a 5th argument gives another ROM folder, e.g.  CBX68 0 30 manual both A:\OUTRUN\)
If a file is missing or wrong, the program lists it (as Cannonball does) and stops.

Demo (no ROMs needed)
---------------------
  CBX68 TEST   (or TEST.BAT)
runs without any ROM file: a road that bends and rises, two background layers
scrolling at different speeds, shapes coming up the road and text. All pictures
are simple shapes made by the program. Keys: 1-4 display, V 30/60 frames, L light/heavy load,
ESC quit (the speed figures are printed at the end, as in the game; press any key
when the screen is full).

Keys
----
C credit / ENTER start / UP or Z accelerate / DOWN or X brake / LEFT RIGHT steer /
SPACE gear / 1-4 display / G roadside objects / H score display / R restart / ESC quit
Joystick (port 1 or 2): LEFT RIGHT steer / A accelerate / B brake / UP high gear /
DOWN low gear; out of the game B also gives a credit and A also starts
(T: time stays at 99, A: drives by itself - check aids)

Not in v0.2: sound. Shadows are not shown. Where more than 32 sprites meet on one
line, parts of objects can be missing.

Changes in v0.2
---------------
- Overall speed-up.

Credits
-------
X68000 port: Mattsuan-Z@ColtMaz
Cannonball (OutRun engine): Chris White and the Cannonball team
Video and sound hardware code in Cannonball: based on MAME (Aaron Giles and others)
The files in SOURCE/vendor come from Cannonball; some of them were changed for this port.
Their original notices are kept.

Licence
-------
Cannonball is copyright Chris White and the Cannonball team; this derivative is
released under the same licence (LICENSE.txt): not for sale or commercial use, and
the complete source is included (SOURCE).

----------------------------------------------------------------------------
Cannonball for x68 (日本語)

X68000向けの、Cannonball（Chris White氏によるOutRunエンジン）の非公式移植です。
X68000への移植: Mattsuan-Z@ColtMaz
非営利の個人制作で、SEGAおよびCannonballの開発者とは関係ありません。
OutRunはSEGAの商標です。

ゲームのデータは一切含まれていません。ROMなしで動くデモを収録しています。道・背景・
景物はすべてプログラムが作り、X68000の表示機能で動かします（CBX68 TEST）。
ROMは含まれていません。

動作環境（重要）
このプログラムは非常に重く、実機のX68000／X68030、WebX68k、X68000 Z では
まともに動きません。実際に動くのは、次の設定のエミュレーター XM6 だけです。
- 機種 X68030、100MHz、MPU 68060・クロック4倍、FPU有効、拡張ROM有効
  （68060で起動するのに必要）、「MPU高速化を許可」オン
- メインメモリ 12MB、拡張メモリ 060turbo 256MB
- MPU・VMのノーウェイトはオフ、「VMをPCのVSYNCに同期」はオフ
- Human68k（3.02）
この設定でも、景物の多い場面では処理落ちします。実機・WebX68k・X68000 Z では
確認しておらず、実用的な速度で動く見込みはありません（非対応）。

使い方
1. CBX68.X、TILEMAP.BIN、TILEPAT.BIN を同じフォルダーに置く
2. そのフォルダーに ROMS フォルダーを作り、ROMファイル（Cannonballと同じセット）を入れる
   （Cannonballと同じく中身のCRC32で探すので、ファイル名は自由です。例: 10380B.133）
   Revision A のセットでも動きます。メインCPUとスプライトのROMが Revision A なら
   （ほかのROMは Revision B と同じ）、それを使って Revision A のデータで遊べます
   （例: ステージ1の起伏や、最初のハイスコアが違います）。
3. CBX68 0 30 manual both で起動
   （5番目の引数でROMフォルダーを指定できます）
ファイルが無い・違う場合は、Cannonballと同様に一覧を表示して終了します。

デモ（ROM不要）
   CBX68 TEST （または TEST.BAT）
ROMなしで動きます。曲がりと起伏のある道、速さの違う2枚の背景、道を近づいてくる図形、
文字が表示されます。絵はすべてプログラムが作った簡単な図形です。1〜4 表示切替、V 30/60、
L 負荷の軽い/重いの切替、ESC 終了（終了時に速さの数字を表示します。画面がいっぱいに
なると止まるので、何かキーを押すと続きを表示します）。

操作: C クレジット / Enter スタート / ↑かZ アクセル / ↓かX ブレーキ / ←→ ハンドル /
      Space ギア / 1〜4 表示 / G 景物 / H 点数表示 / R 再開始 / ESC 終了
ジョイスティック（ポート1か2）: ←→ ハンドル / A アクセル / B ブレーキ / ↑ 高速ギア /
      ↓ 低速ギア（ゲーム中以外は B でもクレジット、A でもスタート）
v0.2では音は出ません。

v0.2での変更
- 全体の高速化。

クレジット: X68000への移植 Mattsuan-Z@ColtMaz ／ Cannonball Chris White氏とCannonballチーム ／
      Cannonballの表示・音源ハードウェア部分はMAME（Aaron Giles氏ほか）が元です。
      SOURCE/vendor はCannonballのファイルで、一部を移植のために変更しています（元の表記は残しています）。
許諾: Cannonballの許諾条件（LICENSE.txt）に従います。販売・商用利用は禁止、
      ソース一式（SOURCE）を同梱しています。
```
