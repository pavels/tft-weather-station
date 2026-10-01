#!/bin/sh
# Regenerates fonts/dejavu_*.c from DejaVuSans.ttf with LVGL's built-in
# FontAwesome symbols (LV_SYMBOL_*) merged in, for LVGL v9. Needs node/npx.
#
# The _14/_20/_30/_40 names are line heights in px, while lv_font_conv's --size
# is the em size -- hence the smaller --size values below.
#
# Output is RLE-compressed (lv_font_conv's default), which needs
# LV_USE_FONT_COMPRESSED 1 in lv_conf.h.
#
# BPP=2 (default): 4 anti-aliasing levels, ~48KB less flash than BPP=4 (16
# levels, slightly smoother edges, no faster), which the app image can't
# afford with HTTPS OTA in it. BPP=1 is no faster on the device and has no
# anti-aliasing.
# Usage: [BPP=4] resources/generate_fonts.sh
set -e
BPP=${BPP:-2}
cd "$(dirname "$0")"

TTF=DejaVuSans.ttf # DejaVu Sans 2.37, license: DejaVuSans-LICENSE.txt
FA=../lvgl/lvgl/scripts/generators/built_in_font/FontAwesome5-Solid+Brands+Regular.woff
SYMS=$(grep -oP '^syms = "\K[^"]+' ../lvgl/lvgl/scripts/generators/built_in_font/built_in_font_gen.py)

gen() { # $1 = name suffix (line height), $2 = lv_font_conv --size
    npx -y lv_font_conv --font "$TTF" -r 32-126 -r 160-383 --font "$FA" -r "$SYMS" \
        --size "$2" --bpp "$BPP" --format lvgl --lv-font-name "dejavu_$1" -o "fonts/dejavu_$1.c"
}

gen 14 11
gen 20 15
gen 30 24
gen 40 33

# Update mode's screen draws without LVGL (components/ota_update/update_screen.c
# reads the glyph tables directly): ASCII only, uncompressed, 1bpp.
npx -y lv_font_conv --font "$TTF" -r 32-126 --size 15 --bpp 1 --no-compress --no-kerning \
    --format lvgl --lv-font-name update_font -o ../components/ota_update/update_font.c
