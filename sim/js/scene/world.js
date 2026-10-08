// world.js - 장면 조립: 실사 하늘·도로·차, 비, 카메라, 블룸, 화질 단계 (docs/sim-plan.md 7절).
// 환경(하늘 밝기, 비)은 입력 값(조도, 강우량)을 연속적으로 비출 뿐, 요구사항 수치로 판단하지 않는다.
// 도로는 두 가지: 지도(map)가 있으면 실제 도로(city.js) 위 경로를 차가 따라 달리고,
// 없으면 예전 직선 도로(road.js)에서 차는 제자리에 있고 세상이 흐른다.
import * as THREE from 'three';
import { EffectComposer } from 'three/addons/postprocessing/EffectComposer.js';
import { RenderPass } from 'three/addons/postprocessing/RenderPass.js';
import { UnrealBloomPass } from 'three/addons/postprocessing/UnrealBloomPass.js';
import { OutputPass } from 'three/addons/postprocessing/OutputPass.js';
import { createCar } from './car.js';
import { loadGltfCar } from './car-gltf.js';
import { createSky } from './sky.js';
import { createRoad } from './road.js';
import { createCity } from './city.js';
import { createTraffic } from './traffic.js';
import { createRadarView } from './radar-view.js';

const LUX_MAX = 60000;
const LANE_SWAY = 1.0;         // ←→로 움직일 수 있는 폭
const MAX_DROPS = 6000;
const MOON = new THREE.Color(0.62, 0.72, 1.0);
const WHEELBASE = 2.7;         // 경로 곡률 → 앞바퀴 조향 연출, 직접 운전의 회전 반경
const MAX_STEER = 0.5;         // 직접 운전 최대 핸들 각 (rad, 저속). 최소 회전 반경 약 5 m
const MAX_LAT_ACC = 4.5;       // 빠를수록 핸들이 덜 꺾인다: 옆으로 받는 가속도가 이 값(m/s², 약 0.45g)을 넘지 않게
const ROAD_LIFT = 0.03;        // 노면이 땅보다 살짝 떠 있는 만큼 차도 올린다
const CAMERAS = [              // [차 기준 카메라 위치, 바라보는 점]. 차 좌표: 오른쪽 +x, 위 +y, 뒤 +z
  ['추적', [0, 2.4, 7.6], [0, 1.0, -6]],
  ['정면', [4.6, 1.7, -6.2], [0, 0.75, -0.6]],   // 앞 대각선(3/4). 앞유리·전조등이 보이게
  ['위', [0.01, 16, 3], [0, 0, -2]],
  ['측면', [-6.2, 1.3, 0.4], [0, 0.8, 0]],      // 왼쪽 옆 (도어·바퀴)
];
const wrapAngle = (a) => Math.atan2(Math.sin(a), Math.cos(a));

export const QUALITY = {
  low: { label: '기본', ratio: 1, shadows: false, bloom: false, drops: 1600 },
  std: { label: '표준', ratio: 1.5, shadows: true, bloom: true, drops: 3500 },
  high: { label: '높음', ratio: 2, shadows: true, bloom: true, drops: 6000 },
};

const smooth = (a, b, x) => { const t = THREE.MathUtils.clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); };

export function createWorld(canvas, { log = () => {}, quality = 'std', carModel = 'auto', map = null, startAt = 0 } = {}) {
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, powerPreference: 'high-performance' });
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = THREE.PCFSoftShadowMap;

  const scene = new THREE.Scene();
  scene.fog = new THREE.Fog(0x9fb4c8, 60, 520);
  const camera = new THREE.PerspectiveCamera(56, 1, 0.25, 1600);   // near를 조금 키워 겹친 노면의 깊이 정밀도 확보

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

  const city = map ? createCity(scene, map) : null;
  const road = city ? null : createRoad(scene);
  const LANE_X = road ? road.laneX : 0;
  if (city) log(`지도: ${map.name}, 한 바퀴 ${(city.path.length / 1000).toFixed(2)} km (도로 ${city.counts.roads}, 건물 ${city.counts.buildings}, 나무 ${city.counts.trees}, 가로등 ${city.counts.lamps})`, 'info');
  const traffic = createTraffic(scene);     // 시뮬레이터 세계의 차·보행자·육교 (정답)
  const radarView = createRadarView(scene); // 레이더가 본 것 (맵의 검출점·트랙)

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
  // 카메라 위치는 차 기준 좌표에서 부드럽게 옮기고, 방향은 차를 조금 늦게 따라 돈다 (차가 빨라도 뒤처지지 않게)
  let camMode = 0;
  let camSnap = true;   // 카메라를 바꾼 직후에는 부드럽게 옮기지 않고 바로 이동 (차체를 뚫고 지나가지 않게)
  const camLocal = new THREE.Vector3(), lookLocal = new THREE.Vector3(), want = new THREE.Vector3();
  let camYaw = 0;
  const toWorld = (v, pos, yaw, out) => {   // 차 좌표 → 세상 좌표 (y축 회전)
    const c = Math.cos(yaw), s = Math.sin(yaw);
    return out.set(pos.x + v.x * c + v.z * s, pos.y + v.y, pos.z - v.x * s + v.z * c);
  };
  const tmp = new THREE.Vector3();
  function placeCamera(dt, rigPos, heading, speedKmh) {
    const [, w, l] = CAMERAS[camMode];
    want.set(...w);
    lookLocal.set(...l);
    if (camSnap) { camLocal.copy(want); camYaw = heading; camSnap = false; }
    else { camLocal.lerp(want, Math.min(1, dt * 4)); camYaw += wrapAngle(heading - camYaw) * Math.min(1, dt * 3); }
    camera.position.copy(toWorld(camLocal, rigPos, camYaw, tmp));
    camera.lookAt(toWorld(lookLocal, rigPos, camYaw, tmp));
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

  // 차의 자리 (지도가 있을 때 두 가지):
  //  - 자동: 경로 위 거리 dist를 따라가고, ←→는 차로 안에서 옆으로 side만큼 비키는 연출
  //  - 직접 운전: ←→가 핸들. 위치 rigPos·방향 heading을 속도와 핸들 각으로 적분 (자전거 모형). 건물에 막힌다
  // 속도는 언제나 C 코어 값. 화면은 방향만 정한다 (사양서에 조향 입력 없음)
  let dist = startAt || 0, side = 0, yaw = 0, clock = 0;
  const rigPos = new THREE.Vector3();
  let heading = 0, curvature = 0, here = null;
  let manual = false, steerAng = 0, odo = 0, pitch = 0, roll = 0;

  function placeOnRoute() {
    here = city.path.sample(dist);
    heading = here.heading; curvature = here.curvature;
    rigPos.set(here.x + Math.cos(heading) * side, city.heightAt(here.x, here.z), here.z - Math.sin(heading) * side);
  }
  // 직접 운전에서 자동으로 바뀔 때: 지금 자리에서 가장 가까운 경로 지점으로
  function nearestOnRoute(x, z) {
    let best = 0, bd = Infinity;
    for (let s = 0; s < city.path.length; s += 2) {
      const p = city.path.sample(s), d = (p.x - x) ** 2 + (p.z - z) ** 2;
      if (d < bd) { bd = d; best = s; }
    }
    return best;
  }
  function resetPose() {
    dist = startAt || 0; side = 0; steerAng = 0; odo = 0; manual = false;
    if (city) placeOnRoute();
  }
  resetPose();

  // ---- 도로를 편 좌표 → 화면 좌표 (레이더·교통 물체용) ----
  // C 코어의 세계·레이더·맵은 도로를 편 좌표(x 앞 = 경로를 따라 잰 거리, y 왼쪽, z 위)에서 계산한다 (요구사항 1.1 직진 가정).
  // 지도 자동 주행이면 그 좌표를 경로를 따라 휘어 놓고, 직선 도로·직접 운전이면 차 앞 직선으로 놓는다.
  // 원점은 자차 앞 범퍼 (차 가운데에서 FRONT m 앞)
  const FRONT = 2.3;
  function mapPoint(x, y, z, out) {
    if (city && !manual) {
      const p = city.path.sample(dist + FRONT + x), c = Math.cos(p.heading), sn = Math.sin(p.heading);
      const wx = p.x - c * y, wz = p.z + sn * y;   // 경로 오른쪽 = (cos, -sin) 이므로 왼쪽 y는 반대
      return out.set(wx, city.heightAt(wx, wz) + ROAD_LIFT + z, wz);
    }
    const c = Math.cos(heading), sn = Math.sin(heading), f = FRONT + x;   // 앞 = (-sin, -cos), 왼쪽 = (-cos, sin)
    return out.set(rigPos.x - sn * f - c * y, (city ? rigPos.y : 0) + z, rigPos.z - c * f + sn * y);
  }
  const mapHeading = (x) => (city && !manual ? city.path.sample(dist + FRONT + x).heading : heading);
  const mapper = { point: mapPoint, heading: mapHeading, egoHeading: () => heading };

  // opts.manual: 직접 운전(주행 모드, 시나리오 재생 아님)이면 true. opts.adas/pts: 레이더·Stop & Go 상태와 검출점.
  // 돌려주는 값: 이번 프레임에 막힌 이유('building'·'edge') 또는 null
  function update(s, dt, lateral, opts = {}) {
    const adas = opts.adas && opts.adas.on ? opts.adas : null;
    // 레이더 장면(물체가 있거나 Stop & Go가 켜짐)에서는 경로를 따라가고 차로 가운데에 둔다 (맵의 자차 위치와 맞춤)
    const radarScene = !!(adas && (adas.objs.length || adas.sg.state !== 'off'));
    if (radarScene) { lateral = 0; side = 0; }
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

    let moved = dz, blocked = null;
    const wantManual = !!(city && opts.manual && !radarScene);
    if (city && wantManual !== manual) {
      manual = wantManual;
      steerAng = 0;
      if (!manual) { dist = nearestOnRoute(rigPos.x, rigPos.z); side = 0; }   // 자동으로: 가까운 경로 지점부터
    }
    if (city && manual) {
      // 직접 운전: ←(-1)는 왼쪽(+각). 빠를수록 덜 꺾이고, 손을 떼면 핸들이 가운데로 돌아온다
      const v = speedMs * dir;
      const maxA = Math.min(MAX_STEER, Math.atan(WHEELBASE * MAX_LAT_ACC / Math.max(0.01, speedMs * speedMs)));
      const rate = (lateral ? 1.4 : 2.4) * dt;
      steerAng += THREE.MathUtils.clamp(-lateral * maxA - steerAng, -rate, rate);
      curvature = Math.tan(steerAng) / WHEELBASE;
      moved = 0;
      if (v !== 0 && dt > 0) {
        const nh = heading + v * curvature * dt;
        const nx = rigPos.x - Math.sin(nh) * v * dt, nz = rigPos.z - Math.cos(nh) * v * dt;
        blocked = city.collide(nx, nz, nh);
        if (!blocked) { rigPos.x = nx; rigPos.z = nz; heading = nh; moved = v * dt; odo += Math.abs(moved); }
      }
      here = { name: city.roadAt(rigPos.x, rigPos.z) };
      rigPos.y = city.heightAt(rigPos.x, rigPos.z);
    } else if (city) {
      // 자동: 차가 경로를 따라 간다. 경로 오른쪽(+x)이 차 오른쪽. ←→는 차로 안에서 비키기
      if (speedMs > 0.3) side = THREE.MathUtils.clamp(side + lateral * dt * 1.5, -LANE_SWAY, LANE_SWAY);
      dist += dz;
      placeOnRoute();
    } else {
      // 직선 도로: 차는 제자리, 세상이 +z로 흐른다
      if (speedMs > 0.3) side = THREE.MathUtils.clamp(side + lateral * dt * 1.5, -LANE_SWAY, LANE_SWAY);
      dist += dz;
      heading = 0; curvature = 0;
      rigPos.set(LANE_X + side, 0, 0);
      road.update(dz, dark, wet);
    }
    if (city) {
      // 비탈: 차 앞뒤·좌우 땅 높이 차로 기울인다
      const H = city.heightAt, fx = -Math.sin(heading), fz = -Math.cos(heading), rx = Math.cos(heading), rz = -Math.sin(heading);
      const k = Math.min(1, dt * 8);
      pitch += (Math.atan2(H(rigPos.x + fx * 1.4, rigPos.z + fz * 1.4) - H(rigPos.x - fx * 1.4, rigPos.z - fz * 1.4), 2.8) - pitch) * k;
      roll += (Math.atan2(H(rigPos.x + rx * 0.8, rigPos.z + rz * 0.8) - H(rigPos.x - rx * 0.8, rigPos.z - rz * 0.8), 1.6) - roll) * k;
      rigPos.y += ROAD_LIFT;
      city.update(dark, wet);
    }

    // 비 (차 기준 좌표: 앞이 -z)
    const Q = QUALITY[q];
    const count = Math.round(wet * Q.drops);
    dropGeo.setDrawRange(0, count * 2);
    const fall = 22 * dt;
    const base = 0.12 + 0.6 * light, beamOn = !!s.out.headlight;
    for (let i = 0; i < count; i++) {
      const o = i * 6;
      dropPos[o + 1] -= fall; dropPos[o + 4] -= fall;
      dropPos[o + 2] += moved; dropPos[o + 5] += moved;
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
    rainLines.position.copy(rigPos);
    rainLines.rotation.y = heading;

    car.group.position.copy(rigPos);
    yaw = THREE.MathUtils.lerp(yaw, speedMs > 0.3 && !manual ? -lateral * 0.04 * dir : 0, Math.min(1, dt * 5));
    car.group.rotation.order = 'YXZ';
    car.group.rotation.set(pitch, heading + yaw, roll);

    // 해: HDRI 속 해 방향에서 차를 비춘다 (그림자는 차 주변만)
    sun.position.copy(car.group.position).addScaledVector(sky.sunDir, 80);
    sun.target.position.copy(car.group.position);

    // 앞바퀴 (car.update의 조향 값 1 ≈ 0.22 rad, +가 오른쪽): 직접 운전은 핸들 각, 자동은 경로가 휘는 만큼 + ←→
    const steer = manual ? -steerAng / 0.22 : (speedMs > 0.3 ? lateral : 0) - Math.atan(WHEELBASE * curvature) / 0.22;
    car.update(s, dt, speedMs, dir, THREE.MathUtils.clamp(steer, -2.6, 2.6), wet);
    traffic.update(adas ? adas.objs : [], dark, mapper);
    radarView.update(adas, opts.pts, mapper);
    placeCamera(dt, rigPos, heading, speedMs * 3.6);
    if (QUALITY[q].bloom) composer.render(); else renderer.render(scene, camera);
    return blocked;
  }

  return {
    update,
    resetPose,
    nextCamera: () => { camMode = (camMode + 1) % CAMERAS.length; camSnap = true; return CAMERAS[camMode][0]; },
    // 지도 모드의 현재 위치 (미니맵용). 직선 도로면 null
    where: () => {
      if (!city || !here) return null;
      const p = { x: rigPos.x, z: rigPos.z, heading, name: here.name, manual };
      return manual ? { ...p, odo } : { ...p, s: here.s, length: city.path.length };
    },
    map: city ? { path: city.path, data: map } : null,
    setQuality: (name) => { if (QUALITY[name]) { q = name; applyQuality(); } return QUALITY[q].label; },
    quality: () => q,
    radarShow: radarView.show,
  };
}
