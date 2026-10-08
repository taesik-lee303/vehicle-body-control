// world.js - 장면 조립: 실사 하늘·도로·차, 비, 카메라, 블룸, 화질 단계 (docs/sim-plan.md 7절).
// 환경(하늘 밝기, 비)은 입력 값(조도, 강우량)을 연속적으로 비출 뿐, 요구사항 수치로 판단하지 않는다.
import * as THREE from 'three';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { createCar } from './car.js';
import { loadGltfCar } from './car-gltf.js';
import { createSky } from './sky.js';
import { createRoad } from './road.js';

const LUX_MAX = 60000;
const LANE_SWAY = 1.0;         // ←→로 움직일 수 있는 폭
const MAX_DROPS = 6000;
const MOON = new THREE.Color(0.62, 0.72, 1.0);

export const QUALITY = {
  low: { label: '기본', ratio: 1, shadows: false, bloom: false, drops: 1600 },
  std: { label: '표준', ratio: 1.5, shadows: true, bloom: true, drops: 3500 },
  high: { label: '높음', ratio: 2, shadows: true, bloom: true, drops: 6000 },
};

const smooth = (a, b, x) => { const t = THREE.MathUtils.clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); };

export function createWorld(canvas, { log = () => {}, quality = 'std', carModel = 'auto' } = {}) {
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: 'high-performance' });
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = THREE.PCFSoftShadowMap;

  const scene = new THREE.Scene();
  scene.fog = new THREE.Fog(0x9fb4c8, 60, 520);
  const camera = new THREE.PerspectiveCamera(56, 1, 0.1, 1600);

  const sky = createSky(renderer);
  scene.add(sky.dome);

  const sun = new THREE.DirectionalLight(0xfff1dc, 3);
  sun.castShadow = true;
  sun.shadow.mapSize.set(2048, 2048);
  sun.shadow.bias = -0.0004;
  sun.shadow.normalBias = 0.02;
  Object.assign(sun.shadow.camera, { left: -12, right: 12, top: 12, bottom: -12, near: 1, far: 200 });
  scene.add(sun, sun.target);
  const fill = new THREE.HemisphereLight(0xbfd4ff, 0x30352a, 0);   // 하늘 HDRI를 못 불러왔을 때와 밤의 최소 밝기
  scene.add(fill);

  const road = createRoad(scene);
  const LANE_X = road.laneX;

  // ---- 비: 짧은 선분. 강우량에 비례해 그리는 개수만 바꾼다 ----
  const dropPos = new Float32Array(MAX_DROPS * 6), dropCol = new Float32Array(MAX_DROPS * 6);
  const dropGeo = new THREE.BufferGeometry();
  dropGeo.setAttribute('position', new THREE.BufferAttribute(dropPos, 3));
  dropGeo.setAttribute('color', new THREE.BufferAttribute(dropCol, 3));
  const rainMat = new THREE.LineBasicMaterial({ vertexColors: true, transparent: true, opacity: 0.55 });
  const rainLines = new THREE.LineSegments(dropGeo, rainMat);
  rainLines.frustumCulled = false;
  scene.add(rainLines);
  const resetDrop = (i, top) => {
    const x = (Math.random() - 0.5) * 40, z = -Math.random() * 60 + 12, y = top ? 18 + Math.random() * 4 : Math.random() * 22;
    dropPos.set([x, y, z, x - 0.02, y - 0.55, z + 0.06], i * 6);
  };
  for (let i = 0; i < MAX_DROPS; i++) resetDrop(i, false);

  // ---- 차: 코드 모델로 시작하고, sim/assets/car.glb가 있으면 바꿔 끼운다 ----
  let car = createCar();
  scene.add(car.group);
  if (carModel !== 'code') {
    fetch('assets/car.glb', { method: 'HEAD' }).then((r) => {
      if (!r.ok) { if (carModel === 'glb') log('  ! sim/assets/car.glb가 없습니다. 코드 모델을 씁니다.', 'err'); return; }
      return loadGltfCar('assets/').then((c) => {
        scene.remove(car.group);
        car = c;
        scene.add(car.group);
        log(`차 모델: car.glb (${c.info})`, 'info');
      });
    }).catch((err) => log(`  ! car.glb를 불러오지 못했습니다: ${err.message}. 코드 모델을 씁니다.`, 'err'));
  }

  // ---- 후처리: 블룸 (전조등·가로등·브레이크등 번짐) ----
  const composer = new EffectComposer(renderer);
  composer.addPass(new RenderPass(scene, camera));
  const bloom = new UnrealBloomPass(new THREE.Vector2(256, 256), 0.4, 0.55, 1.0);
  composer.addPass(bloom);
  composer.addPass(new OutputPass());

  // ---- 카메라 ----
  const CAMERAS = ['추적', '정면', '위', '측면'];
  let camMode = 0;
  let camSnap = true;   // 카메라를 바꾼 직후에는 부드럽게 옮기지 않고 바로 이동 (차체를 뚫고 지나가지 않게)
  const camPos = new THREE.Vector3(LANE_X, 3.2, 8.5);
  const lookAt = new THREE.Vector3();
  function placeCamera(dt, carX, speedKmh) {
    const want = new THREE.Vector3();
    if (camMode === 0) { want.set(carX, 2.4, 7.6); lookAt.set(carX, 1.0, -6); }
    else if (camMode === 1) { want.set(carX + 4.6, 1.7, -6.2); lookAt.set(carX, 0.75, -0.6); }   // 앞 대각선(3/4). 앞유리·전조등이 보이게
    else if (camMode === 2) { want.set(carX + 0.01, 16, 3); lookAt.set(carX, 0, -2); }
    else { want.set(carX - 6.2, 1.3, 0.4); lookAt.set(carX, 0.8, 0); }                              // 왼쪽 옆 (도어·바퀴)
    if (camSnap) { camPos.copy(want); camSnap = false; } else camPos.lerp(want, Math.min(1, dt * 4));
    camera.position.copy(camPos);
    camera.lookAt(lookAt);
    const fov = 56 + Math.min(8, speedKmh * 0.04);   // 빠를수록 조금 넓게
    if (Math.abs(camera.fov - fov) > 0.05) { camera.fov = fov; camera.updateProjectionMatrix(); }
  }

  // ---- 화질 ----
  let q = QUALITY[quality] ? quality : 'std';
  function applyQuality() {
    const Q = QUALITY[q];
    renderer.setPixelRatio(Math.min(devicePixelRatio, Q.ratio));
    sun.castShadow = Q.shadows;
    resize();
  }
  function resize() {
    const w = canvas.clientWidth, h = canvas.clientHeight;
    renderer.setSize(w, h, false);
    composer.setPixelRatio(renderer.getPixelRatio());
    composer.setSize(w, h);
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
  }
  addEventListener('resize', resize);
  applyQuality();

  let carX = LANE_X, clock = 0;

  function update(s, dt, lateral) {
    clock += dt;
    const speedMs = s.in.speed_mkmh / 1000 / 3.6;
    const dir = s.in.gear === 'R' ? -1 : 1;            // 속도는 방향 없는 속력, 후진은 기어 R로 구분
    const dz = speedMs * dt * dir;

    // 조도 → 낮·밤 (로그 눈금, 연속). 강우량 → 흐림·젖은 노면
    const lux = THREE.MathUtils.clamp(s.in.lux, 0, LUX_MAX);
    const day = Math.log10(lux + 1) / Math.log10(LUX_MAX + 1);
    const light = smooth(0.42, 0.92, day);              // 0 밤 ~ 1 한낮 (연출 곡선)
    const wet = THREE.MathUtils.clamp(s.in.rain, 0, 100) / 100;
    const dark = 1 - light;

    const env = sky.set(light, wet, 0.3 + 0.7 * smooth(0, 0.45, day), clock);
    scene.environment = env;
    scene.fog.color.copy(sky.fogColor);
    scene.fog.near = 60 - wet * 50;
    scene.fog.far = 520 - wet * 400;
    renderer.toneMappingExposure = 0.62 + 0.25 * dark;
    // 낮에는 해, 밤에는 약한 달빛 (같은 방향 광원을 색·세기만 바꿔 쓴다)
    sun.intensity = 3.2 * light ** 1.5 * (1 - wet * 0.85) + 0.18 * dark * (1 - wet * 0.6);
    sun.color.setRGB(1, 0.95, 0.87).lerp(MOON, dark);
    fill.intensity = 0.12 * dark + (env ? 0 : 0.8 * light);   // 밤에도 차 윤곽이 보일 정도
    bloom.strength = 0.15 + 0.35 * dark;

    road.update(dz, dark, wet);

    // 비
    const Q = QUALITY[q];
    const count = Math.round(wet * Q.drops);
    dropGeo.setDrawRange(0, count * 2);
    const fall = 22 * dt;
    const base = 0.12 + 0.6 * light, beamOn = !!s.out.headlight;
    for (let i = 0; i < count; i++) {
      const o = i * 6;
      dropPos[o + 1] -= fall; dropPos[o + 4] -= fall;
      dropPos[o + 2] += dz; dropPos[o + 5] += dz;
      if (dropPos[o + 4] < 0 || dropPos[o + 2] > 14 || dropPos[o + 2] < -50) resetDrop(i, true);
      // 전조등 빛 안의 빗방울은 밝게 (빗속 빛줄기)
      let b = base;
      const ahead = -dropPos[o + 2] - 2.3;
      if (beamOn && ahead > 0 && ahead < 45 && Math.abs(dropPos[o]) < 1.1 + ahead * 0.32 && dropPos[o + 1] < 1.0 + ahead * 0.12) {
        b += 1.6 * (1 - ahead / 45) ** 1.5;
      }
      for (let k = 0; k < 6; k += 3) { dropCol[o + k] = b * 0.92; dropCol[o + k + 1] = b * 0.93; dropCol[o + k + 2] = b; }
    }
    dropGeo.attributes.position.needsUpdate = true;
    dropGeo.attributes.color.needsUpdate = true;
    rainLines.position.x = carX;

    // 차선 안 좌우 이동 (연출). 움직일 때만 조금씩
    if (speedMs > 0.3) carX = THREE.MathUtils.clamp(carX + lateral * dt * 1.5, LANE_X - LANE_SWAY, LANE_X + LANE_SWAY);
    car.group.position.x = carX;
    car.group.rotation.y = THREE.MathUtils.lerp(car.group.rotation.y, speedMs > 0.3 ? -lateral * 0.04 * dir : 0, Math.min(1, dt * 5));

    // 해: HDRI 속 해 방향에서 차를 비춘다 (그림자는 차 주변만)
    sun.position.copy(car.group.position).addScaledVector(sky.sunDir, 80);
    sun.target.position.copy(car.group.position);

    car.update(s, dt, speedMs, dir, speedMs > 0.3 ? lateral : 0, wet);
    placeCamera(dt, carX, speedMs * 3.6);
    if (QUALITY[q].bloom) composer.render(); else renderer.render(scene, camera);
  }

  return {
    update,
    nextCamera: () => { camMode = (camMode + 1) % CAMERAS.length; camSnap = true; return CAMERAS[camMode]; },
    setQuality: (name) => { if (QUALITY[name]) { q = name; applyQuality(); } return QUALITY[q].label; },
    quality: () => q,
  };
}
