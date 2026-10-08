#!/usr/bin/env bash
# three.js와 쓰는 부가 모듈(addons)을 sim/vendor/three/ 로 내려받는다 (한 번, 인터넷 필요). 발표장에서는 로컬 파일만 쓴다.
set -euo pipefail
VERSION=0.170.0
BASE="https://cdn.jsdelivr.net/npm/three@${VERSION}"
DEST="$(cd "$(dirname "$0")/.." && pwd)/vendor/three"
ADDONS=(
  loaders/GLTFLoader.js loaders/RGBELoader.js utils/BufferGeometryUtils.js
  geometries/RoundedBoxGeometry.js loaders/DRACOLoader.js
  libs/draco/gltf/draco_decoder.js libs/draco/gltf/draco_decoder.wasm libs/draco/gltf/draco_wasm_wrapper.js
  postprocessing/EffectComposer.js postprocessing/RenderPass.js postprocessing/ShaderPass.js
  postprocessing/MaskPass.js postprocessing/Pass.js postprocessing/UnrealBloomPass.js postprocessing/OutputPass.js
  shaders/CopyShader.js shaders/LuminosityHighPassShader.js shaders/OutputShader.js
)
mkdir -p "$DEST"
curl -fsSL "$BASE/build/three.module.js" -o "$DEST/three.module.js"
curl -fsSL "$BASE/LICENSE" -o "$DEST/LICENSE"
for f in "${ADDONS[@]}"; do
  mkdir -p "$DEST/addons/$(dirname "$f")"
  curl -fsSL "$BASE/examples/jsm/$f" -o "$DEST/addons/$f"
done
echo "three@${VERSION}" > "$DEST/VERSION"
echo "받음: $DEST ($(du -sh "$DEST" | cut -f1))"
