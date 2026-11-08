#!/bin/bash
# Builds ClayFighter: Sculptor's Cut for the Sega Dreamcast and its disc image, cfsc.cdi.
#
#     source /opt/toolchains/dc/kos/environ.sh
#     ./build.sh
#
# Needs your own ROM of the game, ClayFighter: Sculptor's Cut (USA), in .z64 byte order, as
# baserom.us.z64 in this folder. The game's data is made from it on your computer: nothing
# from the ROM is in this project.
set -e
cd "$(dirname "$0")"
B=build
mkdir -p $B/obj $B/cd

# ---- 0. your ROM, KallistiOS
if [ ! -f baserom.us.z64 ]; then
    echo "baserom.us.z64 not found: copy your own ClayFighter: Sculptor's Cut (USA) ROM into this folder"
    echo "with that name (.z64 byte order)."
    exit 1
fi
SUM=$( (shasum -a 1 baserom.us.z64 2>/dev/null || sha1sum baserom.us.z64) | cut -d' ' -f1)
if [ "$SUM" != "28697fba130ded056bf7c497deaa3e154cce8022" ]; then
    echo "baserom.us.z64 is not ClayFighter: Sculptor's Cut (USA) in .z64 byte order (SHA-1 $SUM)"
    exit 1
fi
if [ -z "$KOS_BASE" ]; then
    echo "KallistiOS isn't set up in this window: run   source /opt/toolchains/dc/kos/environ.sh   first"
    exit 1
fi

# ---- 1. the game's data, from your ROM
echo "1/4 making the game's data from your ROM"
HOSTCC=$(command -v cc || command -v clang)
"$HOSTCC" -std=gnu99 -O2 -w -Isrc/include -Isrc/mpf tools/mpf_unpack.c src/mpf/mpfcomp.c src/mpf/mpfcomp2.c \
    src/mpf/mpfbstrm.c src/mpf/mpfrle_enc.c src/mpf/mpfrle_dec.c src/mpf/mpfplat.c -o $B/mpf_unpack
CFSC_LAYOUT=tools/data/data_layout.txt \
    python3 tools/port_assets.py baserom.us.z64 tools/data/asset_trace.txt $B/mpf_unpack $B/cd/CFSC.DAT
CFSC_LAYOUT=tools/data/data_layout.txt \
    python3 tools/port_sound.py baserom.us.z64 $B/mpf_unpack $B/cd/SOUND.DAT

# ---- 2. compile
echo "2/4 compiling"
SHZ_INC=""
for d in "$KOS_PORTS/include" /opt/toolchains/dc/sh4zam/include "$HOME/sh4zam/include"; do
    if [ -f "$d/sh4zam/shz_sh4zam.h" ]; then SHZ_INC="-I$d"; break; fi
done
if [ -z "$SHZ_INC" ]; then
    echo "SH4ZAM not found: install it into KallistiOS (see the README) and run this again"
    exit 1
fi
SHZ_LIB=""
for d in "$KOS_PORTS/lib" /opt/toolchains/dc/sh4zam/build; do
    if [ -f "$d/libsh4zam.a" ]; then SHZ_LIB="-L$d -lsh4zam"; break; fi
done
CF="-O2 -fno-math-errno -DTARGET_PORT -fno-strict-aliasing -fsingle-precision-constant -w $SHZ_INC
    -Isrc/include -Isrc/mpf -Isrc/char -Isrc/game -Isrc/n64 -Isrc/arena -Iplatform/dc"
OBJS=""
for f in $(ls src/*/*.c platform/dc/*.c | sort); do
    o=$B/obj/$(echo $f | tr / _ | sed 's/\.c$/.o/')
    FLAGS="$CF"
    case "$f" in platform/dc/*) FLAGS="$CF -O3";; esac      # (the port's own code: the game's stays at -O2)
    [ "$f" = platform/dc/gfx.c ] && FLAGS="$CF -O3 -ffast-math"
    kos-cc $FLAGS -c $f -o $o
    OBJS="$OBJS $o"
done

# ---- 3. the game's variables (little-endian), from your ROM; then link
echo "3/4 linking"
sh-elf-nm $OBJS | awk '$2=="T"{print substr($3,2)}' | sort -u > $B/defined.txt
CFSC_ROM=baserom.us.z64 python3 tools/port_data.py tools/data/data_layout.txt tools/data/data_units.txt \
    $B/defined.txt $B/data.S 2>/dev/null
python3 tools/port_bss.py tools/data/bss_layout.s $B/bss.S
kos-cc -x assembler-with-cpp -c $B/data.S -o $B/obj/data.o
kos-cc -x assembler-with-cpp -c $B/bss.S -o $B/obj/bss.o
kos-cc -o $B/cfsc.elf $OBJS $B/obj/data.o $B/obj/bss.o $SHZ_LIB -lpthread -lm

# ---- 4. the disc image
echo "4/4 making the disc image"
M=$(command -v mkdcdisc || find "$HOME" /opt /usr/local -name mkdcdisc -type f 2>/dev/null | head -1)
if [ -z "$M" ]; then
    echo "mkdcdisc not found: the game is built ($B/cfsc.elf) but not the disc image"
    exit 1
fi
"$M" -e $B/cfsc.elf -D $B/cd -n "CLAYFIGHTER SC" -o cfsc.cdi --allow-overwrite
echo "built cfsc.cdi"
