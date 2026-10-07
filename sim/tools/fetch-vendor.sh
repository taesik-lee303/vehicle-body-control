#!/usr/bin/env bash
# three.js를 sim/vendor/three/ 로 내려받는다 (한 번, 인터넷 필요). 발표장에서는 로컬 파일만 쓴다.
set -euo pipefail
VERSION=0.170.0
BASE="https://cdn.jsdelivr.net/npm/three@${VERSION}"
DEST="$(cd "$(dirname "$0")/.." && pwd)/vendor/three"
mkdir -p "$DEST"
curl -fsSL "$BASE/build/three.module.js" -o "$DEST/three.module.js"
curl -fsSL "$BASE/LICENSE" -o "$DEST/LICENSE"
echo "three@${VERSION}" > "$DEST/VERSION"
echo "받음: $DEST ($(du -sh "$DEST" | cut -f1))"
