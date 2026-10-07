// world.js - 도로, 하늘, 조명, 비, 카메라 (M2 기본 화질). 현실감 강화는 M3 (docs/sim-plan.md 7절).
// 환경(하늘 밝기, 비)은 입력 값(조도, 강우량)을 연속적으로 비출 뿐, 요구사항 수치(1000, 1500 lx 등)로 판단하지 않는다.
import * as THREE from 'three';
import { createCar } from './car.js';

const LUX_MAX = 60000;
const ROAD_LEN = 400;          // 도로 조각들이 이 길이 안에서 돌아가며 흐른다
const LANE_X = 1.9;            // 차가 달리는 차선 중심
const LANE_SWAY = 1.0;         // ←→로 움직일 수 있는 폭
const MAX_DROPS = 4000;

const SKY_NIGHT = new THREE.Color(0x03050b);
const SKY_DAY = new THREE.Color(0x8cb6e4);
const SKY_RAIN = new THREE.Color(0x56606b);

export function createWorld(canvas) {
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
  renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = THREE.PCFSoftShadowMap;

  const scene = new THREE.Scene();
  scene.background = SKY_DAY.clone();
  scene.fog = new THREE.Fog(SKY_DAY.clone(), 40, 220);

  const camera = new THREE.PerspectiveCamera(58, 1, 0.1, 600);

  const hemi = new THREE.HemisphereLight(0xdfeaff, 0x2a3324, 1);
  const sun = new THREE.DirectionalLight(0xfff4e0, 2.5);
  sun.position.set(-30, 50, -20);
  sun.castShadow = true;
  sun.shadow.mapSize.set(2048, 2048);
  Object.assign(sun.shadow.camera, { left: -15, right: 15, top: 15, bottom: -15, near: 1, far: 150 });
  scene.add(hemi, sun, sun.target);

  // ---- 땅, 도로 ----
  const ground = new THREE.Mesh(new THREE.PlaneGeometry(800, 800),
    new THREE.MeshStandardMaterial({ color: 0x33472b, roughness: 1 }));
  ground.rotation.x = -Math.PI / 2;
  ground.receiveShadow = true;
  scene.add(ground);

  const asphalt = new THREE.MeshStandardMaterial({ color: 0x2a2b2e, roughness: 0.92, metalness: 0 });
  const road = new THREE.Mesh(new THREE.PlaneGeometry(9, 800), asphalt);
  road.rotation.x = -Math.PI / 2;
  road.position.y = 0.01;
  road.receiveShadow = true;
  scene.add(road);

  const paintWhite = new THREE.MeshStandardMaterial({ color: 0xe9e9e9, roughness: 0.6 });
  const paintYellow = new THREE.MeshStandardMaterial({ color: 0xe0b400, roughness: 0.6 });
  for (const x of [-4.3, 4.3]) {            // 양쪽 실선
    const edge = new THREE.Mesh(new THREE.PlaneGeometry(0.15, 800), paintWhite);
    edge.rotation.x = -Math.PI / 2; edge.position.set(x, 0.02, 0); scene.add(edge);
  }

  // 흐르는 것들: 중앙 점선, 가로등, 나무. 차는 제자리, 세상이 뒤로 흐른다
  const movers = [];
  const addMover = (obj, z) => { obj.position.z = z; scene.add(obj); movers.push(obj); };
  for (let z = -ROAD_LEN / 2; z < ROAD_LEN / 2; z += 9) {
    const dash = new THREE.Mesh(new THREE.PlaneGeometry(0.15, 4), paintYellow);
    dash.rotation.x = -Math.PI / 2; dash.position.set(0, 0.02, 0);
    addMover(dash, z);
  }
  const poleMat = new THREE.MeshStandardMaterial({ color: 0x5b6068, metalness: 0.7, roughness: 0.4 });
  const lampMat = new THREE.MeshStandardMaterial({ color: 0x999999, emissive: 0xffd9a0, emissiveIntensity: 0 });
  for (let z = -ROAD_LEN / 2; z < ROAD_LEN / 2; z += 40) {
    const g = new THREE.Group();
    const pole = new THREE.Mesh(new THREE.CylinderGeometry(0.08, 0.1, 7, 10), poleMat);
    pole.position.set(5.6, 3.5, 0);
    const arm = new THREE.Mesh(new THREE.BoxGeometry(1.6, 0.08, 0.08), poleMat);
    arm.position.set(4.9, 6.95, 0);
    const head = new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.12, 0.25), lampMat);
    head.position.set(4.2, 6.88, 0);
    g.add(pole, arm, head);
    g.traverse((o) => { if (o.isMesh) o.castShadow = true; });
    addMover(g, z);
  }
  const trunkMat = new THREE.MeshStandardMaterial({ color: 0x4a3524, roughness: 1 });
  const leafMat = new THREE.MeshStandardMaterial({ color: 0x24461f, roughness: 1 });
  for (let z = -ROAD_LEN / 2; z < ROAD_LEN / 2; z += 14) {
    for (const side of [-1, 1]) {
      const g = new THREE.Group();
      const x = side * (9 + ((z * 7.3) % 6 + 6) % 6);   // 고정된 의사 난수 배치
      const h = 3 + ((z * 3.1) % 2 + 2) % 2;
      const trunk = new THREE.Mesh(new THREE.CylinderGeometry(0.15, 0.2, h * 0.4, 6), trunkMat);
      trunk.position.set(x, h * 0.2, 0);
      const crown = new THREE.Mesh(new THREE.ConeGeometry(h * 0.35, h * 0.8, 8), leafMat);
      crown.position.set(x, h * 0.4 + h * 0.4, 0);
      g.add(trunk, crown);
      g.traverse((o) => { if (o.isMesh) o.castShadow = true; });
      addMover(g, z + side * 5);
    }
  }

  // ---- 비: 짧은 선분. 강우량에 비례해 그리는 개수만 바꾼다 ----
  const dropPos = new Float32Array(MAX_DROPS * 6);
  const dropGeo = new THREE.BufferGeometry();
  dropGeo.setAttribute('position', new THREE.BufferAttribute(dropPos, 3));
  const rainLines = new THREE.LineSegments(dropGeo,
    new THREE.LineBasicMaterial({ color: 0xaab8c8, transparent: true, opacity: 0.55 }));
  rainLines.frustumCulled = false;
  scene.add(rainLines);
  const resetDrop = (i, top) => {
    const x = (Math.random() - 0.5) * 40, z = -Math.random() * 60 + 12, y = top ? 18 + Math.random() * 4 : Math.random() * 22;
    dropPos.set([x, y, z, x, y - 0.45, z], i * 6);
  };
  for (let i = 0; i < MAX_DROPS; i++) resetDrop(i, false);

  // ---- 차 ----
  const car = createCar();
  car.group.position.x = LANE_X;
  scene.add(car.group);
  sun.target = car.group;

  // ---- 카메라 ----
  const CAMERAS = ['추적', '정면', '위'];
  let camMode = 0;
  let camSnap = true;   // 카메라를 바꾼 직후에는 부드럽게 옮기지 않고 바로 이동 (차체를 뚫고 지나가지 않게)
  const camPos = new THREE.Vector3(LANE_X, 3.2, 8.5);
  const lookAt = new THREE.Vector3();
  function placeCamera(dt, carX) {
    const want = new THREE.Vector3();
    if (camMode === 0) { want.set(carX, 2.8, 8.2); lookAt.set(carX, 1.0, -6); }
    else if (camMode === 1) { want.set(carX + 5.5, 2.4, -6.0); lookAt.set(carX, 0.8, -0.5); }   // 앞 대각선(3/4). 전조등 빛 웅덩이 밖에서 앞유리·전조등이 보이게
    else { want.set(carX + 0.01, 16, 3); lookAt.set(carX, 0, -2); }
    if (camSnap) { camPos.copy(want); camSnap = false; } else camPos.lerp(want, Math.min(1, dt * 4));
    camera.position.copy(camPos);
    camera.lookAt(lookAt);
  }

  function resize() {
    const w = canvas.clientWidth, h = canvas.clientHeight;
    renderer.setSize(w, h, false);
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
  }
  addEventListener('resize', resize);
  resize();

  let carX = LANE_X;

  function update(s, dt, lateral) {
    const speedMs = s.in.speed_mkmh / 1000 / 3.6;
    const dir = s.in.gear === 'R' ? -1 : 1;            // 속도는 방향 없는 속력, 후진은 기어 R로 구분
    const dz = speedMs * dt * dir;
    for (const m of movers) {
      m.position.z += dz;
      if (m.position.z > ROAD_LEN / 2) m.position.z -= ROAD_LEN;
      else if (m.position.z < -ROAD_LEN / 2) m.position.z += ROAD_LEN;
    }

    // 차선 안 좌우 이동 (연출). 움직일 때만 조금씩
    if (speedMs > 0.3) carX = THREE.MathUtils.clamp(carX + lateral * dt * 1.5, LANE_X - LANE_SWAY, LANE_X + LANE_SWAY);
    car.group.position.x = carX;
    car.group.rotation.y = THREE.MathUtils.lerp(car.group.rotation.y, speedMs > 0.3 ? -lateral * 0.04 * dir : 0, Math.min(1, dt * 5));

    // 조도 → 하늘 밝기 (로그 눈금, 연속). 강우량 → 흐림·안개
    const lux = THREE.MathUtils.clamp(s.in.lux, 0, LUX_MAX);
    const day = Math.log10(lux + 1) / Math.log10(LUX_MAX + 1);
    const wet = THREE.MathUtils.clamp(s.in.rain, 0, 100) / 100;
    const sky = SKY_NIGHT.clone().lerp(SKY_DAY, day ** 1.6).lerp(SKY_RAIN.clone().multiplyScalar(0.25 + day * 0.75), wet * 0.7);
    scene.background.copy(sky);
    scene.fog.color.copy(sky);
    scene.fog.near = 40 - wet * 30;
    scene.fog.far = 220 - wet * 150;
    hemi.intensity = 0.04 + 1.1 * day ** 1.4;
    sun.intensity = 2.8 * day ** 2 * (1 - wet * 0.7);
    renderer.toneMappingExposure = 0.75 + 0.35 * day;
    lampMat.emissiveIntensity = (1 - day) ** 3 * 2;       // 가로등은 어두울수록 은은하게 (연출)
    asphalt.roughness = 0.92 - wet * 0.55;                 // 젖은 노면: 더 매끈하게
    asphalt.color.setHex(0x2a2b2e).multiplyScalar(1 - wet * 0.35);

    // 비
    const count = Math.round(wet * MAX_DROPS);
    dropGeo.setDrawRange(0, count * 2);
    const fall = 22 * dt;
    for (let i = 0; i < count; i++) {
      const o = i * 6;
      dropPos[o + 1] -= fall; dropPos[o + 4] -= fall;
      dropPos[o + 2] += dz; dropPos[o + 5] += dz;
      if (dropPos[o + 4] < 0 || dropPos[o + 2] > 14 || dropPos[o + 2] < -50) resetDrop(i, true);
    }
    dropGeo.attributes.position.needsUpdate = true;
    rainLines.position.x = carX;

    car.showBeams(camMode !== 1);
    car.update(s, dt, speedMs, dir);
    placeCamera(dt, carX);
    renderer.render(scene, camera);
  }

  return {
    update,
    nextCamera: () => { camMode = (camMode + 1) % CAMERAS.length; camSnap = true; return CAMERAS[camMode]; },
  };
}
