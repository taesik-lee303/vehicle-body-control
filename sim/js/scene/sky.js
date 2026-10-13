// sky.js - 실사 하늘(HDRI 낮·흐림·밤 3장)을 조도·강우량에 따라 섞어 배경과 반사 환경(차 도장·유리·젖은 노면)에 쓴다.
// 섞는 비율은 입력 값을 연속적으로 비출 뿐, 요구사항 수치로 판단하지 않는다.
import * as THREE from 'three';
import { RGBELoader } from 'three/addons/loaders/RGBELoader.js';

const FILES = { day: 'assets/sky/day.hdr', over: 'assets/sky/overcast.hdr', night: 'assets/sky/night.hdr' };
const GAIN = { day: 0.9, over: 0.55, night: 1.2 };   // 세 장의 밝기 맞춤 (연출값)

const vert = /* glsl */`
varying vec3 vDir;
void main() {
  vDir = (modelMatrix * vec4(position, 0.0)).xyz;
  gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(position, 1.0);
  gl_Position.z = gl_Position.w;
}`;
const frag = /* glsl */`
uniform sampler2D tDay, tOver, tNight;
uniform float uDay, uWet, uDark, gDay, gOver, gNight, uHaze;
uniform vec3 uFog;
varying vec3 vDir;
vec2 eq(vec3 d) { return vec2(atan(d.z, d.x) * 0.15915494 + 0.5, asin(clamp(d.y, -1.0, 1.0)) * 0.31830988 + 0.5); }
void main() {
  vec3 d = normalize(vDir);
  vec2 uv = eq(d);
  // 해 픽셀은 반정밀도 범위를 넘어 무한대가 될 수 있다 → 잘라서 NaN(검은 번짐)을 막는다
  vec3 day = min(texture2D(tDay, uv).rgb, vec3(300.0)) * gDay;
  vec3 over = min(texture2D(tOver, uv).rgb, vec3(300.0)) * gOver;
  vec3 night = min(texture2D(tNight, uv).rgb, vec3(300.0)) * gNight;
  vec3 col = mix(night, day, uDay);
  col = mix(col, over * mix(0.04, 1.0, uDay), uWet);
  col *= uDark;
  float h = smoothstep(-0.02, uHaze, d.y);      // 지평선은 안개색으로 (땅·안개와 이음매 없이)
  col = mix(uFog, col, h);
  gl_FragColor = vec4(col, 1.0);
  #include <tonemapping_fragment>
  #include <colorspace_fragment>
}`;

// 지평선 근처 평균색과 해 방향 (HDRI 픽셀에서 계산)
function analyze(tex, findSun) {
  const { data, width, height } = tex.image;
  const half = tex.type === THREE.HalfFloatType;
  const px = (i) => (half ? THREE.DataUtils.fromHalfFloat(data[i]) : data[i]);
  const ch = data.length / (width * height);
  const avg = new THREE.Color(0, 0, 0);
  let n = 0, best = -1, bi = 0;
  for (let r = 0; r < height; r++) {
    const lat = Math.abs(0.5 - (r + 0.5) / height) * 180;   // 위도 (도)
    const horizon = lat < 8 && r < height / 2;               // 지평선 바로 위 띠
    for (let c = 0; c < width; c += 2) {
      const i = (r * width + c) * ch;
      if (horizon) { avg.r += px(i); avg.g += px(i + 1); avg.b += px(i + 2); n++; }
      if (findSun && r < height / 2) { const l = px(i) + px(i + 1) + px(i + 2); if (l > best) { best = l; bi = r * width + c; } }
    }
  }
  avg.multiplyScalar(1 / Math.max(1, n));
  let sun = null;
  if (findSun) {
    const r = Math.floor(bi / width), c = bi % width;
    const phi = ((c + 0.5) / width - 0.5) * Math.PI * 2, theta = (0.5 - (r + 0.5) / height) * Math.PI;
    sun = new THREE.Vector3(Math.cos(phi) * Math.cos(theta), Math.abs(Math.sin(theta)), Math.sin(phi) * Math.cos(theta)).normalize();
  }
  return { horizon: avg, sun };
}

export function createSky(renderer) {
  const black = new THREE.DataTexture(new Uint8Array([0, 0, 0, 255]), 1, 1);
  black.needsUpdate = true;
  const uniforms = {
    tDay: { value: black }, tOver: { value: black }, tNight: { value: black },
    uDay: { value: 1 }, uWet: { value: 0 }, uDark: { value: 1 }, uHaze: { value: 0.12 },
    gDay: { value: GAIN.day }, gOver: { value: GAIN.over }, gNight: { value: GAIN.night },
    uFog: { value: new THREE.Color(0x9fb4c8) },
  };
  const material = new THREE.ShaderMaterial({ uniforms, vertexShader: vert, fragmentShader: frag,
    side: THREE.BackSide, depthWrite: false, fog: false });
  const dome = new THREE.Mesh(new THREE.SphereGeometry(1, 64, 32), material);
  dome.scale.setScalar(900);
  dome.frustumCulled = false;
  dome.renderOrder = -10;
  dome.onBeforeRender = (r, s, cam) => dome.position.copy(cam.position);

  // 반사 환경: 하늘만 있는 장면을 PMREM으로 굽는다. 섞는 비율이 바뀔 때만 다시 굽는다
  const envScene = new THREE.Scene();
  const envDome = new THREE.Mesh(new THREE.SphereGeometry(100, 32, 16), material);
  envScene.add(envDome);
  const pmrem = new THREE.PMREMGenerator(renderer);
  let envRT = null, bakedKey = '', lastBake = -1e9;

  const horizon = { day: new THREE.Color(0x9fb4c8), over: new THREE.Color(0x6d757d), night: new THREE.Color(0x05070c) };
  const sunDir = new THREE.Vector3(-0.5, 0.75, -0.4).normalize();
  let ready = false;

  const loader = new RGBELoader();
  const load = (url) => new Promise((res, rej) => loader.load(url, res, undefined, rej));
  const loaded = Promise.all([load(FILES.day), load(FILES.over), load(FILES.night)]).then(([d, o, n]) => {
    for (const t of [d, o, n]) { t.minFilter = THREE.LinearFilter; t.magFilter = THREE.LinearFilter; t.generateMipmaps = false; }
    uniforms.tDay.value = d; uniforms.tOver.value = o; uniforms.tNight.value = n;
    const a = analyze(d, true);
    horizon.day.copy(a.horizon).multiplyScalar(GAIN.day);
    horizon.over.copy(analyze(o, false).horizon).multiplyScalar(GAIN.over);
    horizon.night.copy(analyze(n, false).horizon).multiplyScalar(GAIN.night);
    sunDir.copy(a.sun);
    ready = true; bakedKey = '';
  }).catch((err) => console.warn('하늘 HDRI를 불러오지 못했습니다. 단색 하늘로 그립니다.', err));

  const fogColor = new THREE.Color();
  // day: 0(밤)~1(낮), wet: 0~1, dark: 전체 밝기 배율
  function set(day, wet, dark, t) {
    uniforms.uDay.value = day; uniforms.uWet.value = wet; uniforms.uDark.value = dark;
    fogColor.copy(horizon.night).lerp(horizon.day, day)
      .lerp(horizon.over.clone().multiplyScalar(0.04 + 0.96 * day), wet).multiplyScalar(dark);
    uniforms.uFog.value.copy(fogColor);
    uniforms.uHaze.value = 0.06 + wet * 0.25;
    const key = `${ready}|${day.toFixed(2)}|${wet.toFixed(2)}|${dark.toFixed(2)}`;
    if (key !== bakedKey && t - lastBake > 0.25) {
      const old = envRT;
      envRT = pmrem.fromScene(envScene, 0, 0.1, 200);
      old?.dispose();
      bakedKey = key; lastBake = t;
    }
    return envRT?.texture ?? null;
  }

  return { dome, set, fogColor, sunDir, loaded };
}
