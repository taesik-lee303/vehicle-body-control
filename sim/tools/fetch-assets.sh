#!/usr/bin/env bash
# 현실감 자원을 sim/assets/ 로 내려받는다 (한 번, 인터넷 필요). 모두 Poly Haven CC0 (출처 표기 의무 없음).
# 받은 파일은 저장소에 커밋되어 있으므로 보통은 다시 받을 필요가 없다.
set -euo pipefail
DEST="$(cd "$(dirname "$0")/.." && pwd)/assets"
HDRI="https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr"
TEX="https://dl.polyhaven.org/file/ph-assets/Textures/jpg"
mkdir -p "$DEST/sky" "$DEST/tex"
get() { curl -fsSL -A 'vbc-sim' "$1" -o "$2"; echo "  $2"; }
get "$HDRI/2k/kloofendal_48d_partly_cloudy_puresky_2k.hdr" "$DEST/sky/day.hdr"
get "$HDRI/2k/overcast_soil_puresky_2k.hdr"                "$DEST/sky/overcast.hdr"
get "$HDRI/1k/rogland_clear_night_1k.hdr"                  "$DEST/sky/night.hdr"
get "$TEX/1k/asphalt_04/asphalt_04_diff_1k.jpg"            "$DEST/tex/asphalt_diff.jpg"
get "$TEX/1k/asphalt_04/asphalt_04_nor_gl_1k.jpg"          "$DEST/tex/asphalt_nor.jpg"
get "$TEX/1k/asphalt_04/asphalt_04_rough_1k.jpg"           "$DEST/tex/asphalt_rough.jpg"
get "$TEX/1k/leafy_grass/leafy_grass_diff_1k.jpg"          "$DEST/tex/grass_diff.jpg"
get "$TEX/1k/leafy_grass/leafy_grass_nor_gl_1k.jpg"        "$DEST/tex/grass_nor.jpg"
echo "받음: $DEST ($(du -sh "$DEST" | cut -f1))"
