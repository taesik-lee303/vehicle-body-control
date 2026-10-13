// car.js - 코드로 만든 곡면 차 (M3). sim/assets/car.glb가 있으면 car-gltf.js가 같은 인터페이스로 대신한다.
// 차의 앞은 -z. 전조등·와이퍼는 코어 출력(s.out), 도어·브레이크등·후진등은 입력(s.in)을 비춘다.
import * as THREE from 'three';
import { RoundedBoxGeometry } from 'three/addons/geometries/RoundedBoxGeometry.js';
import { monotone, superellipse, evenAngles, stations, loft } from './loft.js';

export const DOOR_OPEN_RAD = THREE.MathUtils.degToRad(14);   // "덜 닫힘" 연출 각도
const WIPER_SWEEP_RAD = THREE.MathUtils.degToRad(95);

// ---- 치수 (m) ----
const ZF = -2.32, ZR = 2.30;                  // 앞·뒤 끝
const HALF_W = 0.92;
const AXLES = [-1.42, 1.38];
const WHEEL_R = 0.335, WHEEL_X = 0.785;
const ARCH_R = 0.40, ARCH_Y = 0.36;          // 휠하우스 반지름, 중심 높이
const WS0 = -0.85, ROOF0 = -0.06, ROOF1 = 0.98, RG0 = 1.45;   // 앞유리 아래, 지붕 앞·뒤, 뒷유리 아래
const BODY_N = 5, GH_N = 4;                   // 단면 초타원 지수 (클수록 각짐)
const GAP = 0.006;                            // 도어 틈
const DOORS = [                               // FL FR RL RR
  { side: -1, z0: -0.92, z1: 0.33 }, { side: 1, z0: -0.92, z1: 0.33 },
  { side: -1, z0: 0.37, z1: 1.20 }, { side: 1, z0: 0.37, z1: 1.20 },
];

// 옆에서 본 윗선·아랫선, 위에서 본 폭
const top = monotone([[ZF, 0.58], [ZF + 0.07, 0.70], [ZF + 0.3, 0.79], [-1.4, 0.86], [WS0, 0.965],
  [0.4, 0.99], [RG0, 1.02], [2.0, 1.035], [ZR - 0.08, 0.99], [ZR, 0.88]]);
const bottomBase = monotone([[ZF, 0.42], [ZF + 0.18, 0.27], [-1.9, 0.22], [1.9, 0.23], [ZR - 0.15, 0.31], [ZR, 0.45]]);
function bottom(z) {
  let y = bottomBase(z);
  for (const a of AXLES) {
    const d = z - a;
    if (Math.abs(d) < ARCH_R) y = Math.max(y, ARCH_Y + Math.sqrt(ARCH_R * ARCH_R - d * d));
  }
  return y;
}
function halfW(z) {
  const fr = -2.0, rr = 1.98, p = 4.5;   // 여기서부터 앞뒤 모서리가 둥글게 줄어듦
  let t = 0;
  if (z < fr) t = (fr - z) / (fr - ZF); else if (z > rr) t = (z - rr) / (ZR - rr);
  t = Math.min(1, t);
  return HALF_W * Math.pow(Math.max(0, 1 - t ** p), 1 / p);
}
const belt = (z) => top(z) - 0.004;
const roof = monotone([[WS0, belt(WS0)], [ROOF0, 1.425], [0.45, 1.45], [ROOF1, 1.425], [RG0, belt(RG0)]]);
const ghW = monotone([[WS0, 0.80], [0.3, 0.815], [RG0, 0.755]]);
const TUMBLE = 0.22;

function bodyPoint(z, a) {
  const yb = bottom(z), yt = top(z), [x, y] = superellipse(a, BODY_N);
  return [halfW(z) * x, (yb + yt) / 2 + (yt - yb) / 2 * y, z];
}
function ghPoint(z, a) {
  const [x, y] = superellipse(a, GH_N), b = belt(z);
  return [ghW(z) * x * (1 - TUMBLE * y), b + (roof(z) - b) * y, z];
}
// 차체 옆면 x (도어 손잡이·미러 위치용)
function sideX(z, y) {
  const yb = bottom(z), yt = top(z), s = Math.min(1, Math.abs((y - (yb + yt) / 2) / ((yt - yb) / 2)));
  const sin = s ** (BODY_N / 2), cos = Math.sqrt(1 - sin * sin);
  return halfW(z) * cos ** (2 / BODY_N);
}

// ---- 재질 ----
const M = { paint: 0, glass: 1, trim: 2, plastic: 3, head: 4, tail: 5, rev: 6, under: 7 };

function makeMaterials(color) {
  const mats = [];
  mats[M.paint] = new THREE.MeshPhysicalMaterial({ color, metalness: 0.55, roughness: 0.34, clearcoat: 1, clearcoatRoughness: 0.03 });
  mats[M.glass] = new THREE.MeshPhysicalMaterial({ color: 0x0b1117, metalness: 0.1, roughness: 0.04, clearcoat: 1,
    clearcoatRoughness: 0, transparent: true, opacity: 0.74, envMapIntensity: 1.6 });
  mats[M.trim] = new THREE.MeshPhysicalMaterial({ color: 0x0a0a0b, roughness: 0.3, clearcoat: 0.6 });
  mats[M.plastic] = new THREE.MeshStandardMaterial({ color: 0x161718, roughness: 0.82 });
  mats[M.head] = new THREE.MeshPhysicalMaterial({ color: 0xd4dae1, metalness: 0.85, roughness: 0.12, clearcoat: 1,
    emissive: 0xfff3dc, emissiveIntensity: 0 });
  mats[M.tail] = new THREE.MeshPhysicalMaterial({ color: 0x7a090c, roughness: 0.12, clearcoat: 1, emissive: 0xff1208, emissiveIntensity: 0 });
  mats[M.rev] = new THREE.MeshPhysicalMaterial({ color: 0x8c8c8c, roughness: 0.15, clearcoat: 1, emissive: 0xffffff, emissiveIntensity: 0 });
  mats[M.under] = new THREE.MeshStandardMaterial({ color: 0x0b0b0b, roughness: 1 });
  return mats;
}

// ---- 곡면 나누기 ----
function doorOf(c, n, z, pad, upper) {
  for (let k = 0; k < DOORS.length; k++) {
    const d = DOORS[k];
    if (Math.sign(c.x) !== d.side || z <= d.z0 - pad || z >= d.z1 + pad) continue;
    if (upper ? Math.abs(n.x) > 0.62 : (Math.abs(c.x) > ghW(z) && c.y > bottom(z) + 0.07)) return k;
  }
  return -1;
}

function bodyMaterial(c, n) {
  const yb = bottom(c.z), yt = top(c.z), ax = Math.abs(c.x);
  if (n.y < -0.6) return M.under;
  if (c.y < yb + 0.07 && Math.abs(n.x) > 0.3) return M.plastic;          // 사이드 스커트, 휠아치
  if (n.z < -0.3 && c.z < -1.6) {                                        // 앞면: 램프·그릴은 보닛 윤곽선 기준 띠
    if (c.y > yt - 0.13 && c.y < yt - 0.035 && ax > 0.36 && ax < 0.9) return M.head;
    if (ax < 0.36 && c.y > yt - 0.24 && c.y < yt - 0.05) return M.trim;  // 그릴
    if (c.y < 0.40) return M.plastic;
  }
  if (n.z > 0.45 && c.z > 1.6) {                                          // 뒷면: 트렁크 윤곽선 기준 띠
    if (c.y > yt - 0.15 && c.y < yt - 0.04 && ax > 0.28) return ax < 0.4 ? M.rev : M.tail;
    if (c.y < 0.46) return M.plastic;
  }
  return M.paint;
}

function glassMaterial(c, n, door) {
  const z = c.z, b = belt(z), r = roof(z);
  if (c.y < b + 0.025) return M.trim;                                    // 벨트 몰딩
  if (door) {
    if (z > door.z0 + 0.06 && z < door.z1 - 0.05 && c.y < r - 0.07) return M.glass;
    return M.trim;                                                       // 창틀 (B필러가 검게 보임)
  }
  if (Math.abs(n.x) > 0.62) {                                            // 뒷도어 뒤 쪽유리
    return z > 1.24 && z < RG0 - 0.12 && c.y < r - 0.06 ? M.glass : M.paint;
  }
  if (Math.abs(n.x) > 0.42) return M.paint;                              // A·C 필러
  if (z < ROOF0 + 0.02) return z > ROOF0 - 0.1 || Math.abs(n.x) > 0.34 ? M.trim : M.glass;   // 앞유리 (테두리 검정)
  if (z > ROOF1 - 0.02) return Math.abs(n.x) > 0.34 ? M.trim : M.glass;  // 뒷유리
  return M.paint;                                                        // 지붕
}

function buildShell(mats) {
  const extraZ = [];
  for (const d of DOORS) extraZ.push(d.z0 - GAP, d.z0, d.z1, d.z1 + GAP);
  for (const a of AXLES) extraZ.push(a - ARCH_R - 0.002, a - ARCH_R + 0.002, a + ARCH_R - 0.002, a + ARCH_R + 0.002);

  const lower = loft({
    zs: stations(ZF, ZR, 150, extraZ),
    angles: evenAngles(160, BODY_N, 0.36, -Math.PI / 2, Math.PI * 1.5),   // 이음매는 바닥
    point: bodyPoint,
    inside: (c) => { const z = THREE.MathUtils.clamp(c.z, -1.5, 1.5); return new THREE.Vector3(0, (bottom(z) + top(z)) / 2, z); },
    materials: mats,
    regions: [
      (c, n) => (doorOf(c, n, c.z, GAP, false) >= 0 ? -1 : bodyMaterial(c, n)),
      ...DOORS.map((_, k) => (c, n) => (doorOf(c, n, c.z, 0, false) === k ? bodyMaterial(c, n) : -1)),
    ],
  });
  const upper = loft({
    zs: stations(WS0, RG0, 110, extraZ),
    angles: evenAngles(90, GH_N, 0.6, 0, Math.PI),
    point: ghPoint,
    inside: (c) => { const z = THREE.MathUtils.clamp(c.z, -0.3, 0.9); return new THREE.Vector3(0, belt(z) - 0.3, z); },
    materials: mats,
    regions: [
      (c, n) => (doorOf(c, n, c.z, GAP, true) >= 0 ? -1 : glassMaterial(c, n, null)),
      ...DOORS.map((d, k) => (c, n) => (doorOf(c, n, c.z, 0, true) === k ? glassMaterial(c, n, d) : -1)),
    ],
  });
  return { body: [lower[0], upper[0]], doors: DOORS.map((_, k) => [lower[k + 1], upper[k + 1]]) };
}

// 실내: 문틈·유리 너머로 보이는 어두운 안쪽 면과 좌석, 대시보드, 운전대
function buildInterior() {
  const g = new THREE.Group();
  const dark = new THREE.MeshStandardMaterial({ color: 0x141416, roughness: 0.9, side: THREE.BackSide });
  const tub = loft({
    zs: stations(-1.0, 1.4, 40), angles: evenAngles(48, BODY_N, 0.36, -Math.PI / 2, Math.PI * 1.5),
    point: (z, a) => { const p = bodyPoint(z, a); return [p[0] * 0.95, Math.min(p[1], belt(z) - 0.01), z]; },
    inside: (c) => new THREE.Vector3(0, 0.6, c.z), materials: [dark], regions: [() => 0],
  })[0];
  const head = loft({
    zs: stations(WS0 + 0.05, RG0 - 0.05, 40), angles: evenAngles(32, GH_N, 0.6, 0, Math.PI),
    point: (z, a) => { const p = ghPoint(z, a); return [p[0] * 0.95, belt(z) + (p[1] - belt(z)) * 0.95, z]; },
    inside: (c) => new THREE.Vector3(0, 0.6, c.z), materials: [dark], regions: [() => 0],
  })[0];
  g.add(tub, head);
  const seatMat = new THREE.MeshStandardMaterial({ color: 0x2a2725, roughness: 0.85 });
  for (const [x, z] of [[-0.38, 0.05], [0.38, 0.05], [-0.38, 0.95], [0.0, 0.95], [0.38, 0.95]]) {
    if (z > 0.5 && x === 0) continue;
    const base = new THREE.Mesh(new RoundedBoxGeometry(0.48, 0.14, 0.5, 3, 0.05), seatMat);
    base.position.set(x, 0.5, z);
    const back = new THREE.Mesh(new RoundedBoxGeometry(0.46, 0.62, 0.12, 3, 0.05), seatMat);
    back.position.set(x, 0.86, z + 0.27); back.rotation.x = -0.18;
    g.add(base, back);
  }
  const dash = new THREE.Mesh(new RoundedBoxGeometry(1.5, 0.22, 0.45, 3, 0.06), seatMat);
  dash.position.set(0, 0.88, -0.62);
  const wheel = new THREE.Mesh(new THREE.TorusGeometry(0.17, 0.018, 8, 32),
    new THREE.MeshStandardMaterial({ color: 0x111111, roughness: 0.6 }));
  wheel.position.set(-0.38, 0.98, -0.32); wheel.rotation.x = -0.45;
  g.add(dash, wheel);
  return g;
}

// 바퀴: 타이어(회전체) + 5스포크 휠 + 디스크·캘리퍼. 바깥면이 +x
function buildWheel(mats) {
  const tireMat = new THREE.MeshStandardMaterial({ color: 0x171717, roughness: 0.88 });
  const rimMat = new THREE.MeshPhysicalMaterial({ color: 0xc9ccd0, metalness: 1, roughness: 0.22, clearcoat: 0.5 });
  const darkMetal = new THREE.MeshStandardMaterial({ color: 0x2b2d30, metalness: 0.8, roughness: 0.45 });
  const g = new THREE.Group();
  const profile = [[0.232, -0.115], [0.30, -0.12], [0.325, -0.112], [0.334, -0.085], [0.336, 0],
    [0.334, 0.085], [0.325, 0.112], [0.30, 0.12], [0.232, 0.115]].map(([r, y]) => new THREE.Vector2(r, y));
  const tire = new THREE.Mesh(new THREE.LatheGeometry(profile, 48), tireMat);
  tire.rotation.z = Math.PI / 2;
  const barrel = new THREE.Mesh(new THREE.CylinderGeometry(0.232, 0.232, 0.22, 40, 1, true), darkMetal);
  barrel.rotation.z = Math.PI / 2;
  const lip = new THREE.Mesh(new THREE.TorusGeometry(0.228, 0.01, 6, 48), rimMat);
  lip.rotation.y = Math.PI / 2; lip.position.x = 0.105;
  const disc = new THREE.Mesh(new THREE.CylinderGeometry(0.17, 0.17, 0.025, 32), darkMetal);
  disc.rotation.z = Math.PI / 2; disc.position.x = -0.01;
  const hub = new THREE.Mesh(new THREE.CylinderGeometry(0.055, 0.06, 0.04, 20), rimMat);
  hub.rotation.z = Math.PI / 2; hub.position.x = 0.095;
  g.add(tire, barrel, lip, disc, hub);
  const spokeShape = new THREE.Shape();
  spokeShape.moveTo(-0.018, 0.05); spokeShape.lineTo(0.018, 0.05); spokeShape.lineTo(0.03, 0.226); spokeShape.lineTo(-0.03, 0.226); spokeShape.closePath();
  const spokeGeo = new THREE.ExtrudeGeometry(spokeShape, { depth: 0.02, bevelEnabled: true, bevelSize: 0.006, bevelThickness: 0.006, bevelSegments: 2 });
  for (let i = 0; i < 10; i++) {
    const s = new THREE.Mesh(spokeGeo, rimMat);
    s.rotation.set(Math.PI * 2 * Math.floor(i / 2) / 5 + (i % 2 ? 0.1 : -0.1), Math.PI / 2, 0, 'XYZ');   // 바퀴 축(x) 둘레로
    s.position.x = 0.075;
    g.add(s);
  }
  // 캘리퍼는 돌지 않는다 (바깥 그룹에 붙인다)
  const caliper = new THREE.Mesh(new RoundedBoxGeometry(0.06, 0.16, 0.09, 2, 0.02), new THREE.MeshStandardMaterial({ color: 0x8a1010, roughness: 0.5 }));
  caliper.position.set(0.02, 0.1, 0.1);
  return { spin: g, caliper };
}

export function buildWiper() {
  const mat = new THREE.MeshStandardMaterial({ color: 0x0e0e0e, roughness: 0.5, metalness: 0.3 });
  const pivot = new THREE.Group();
  const arm = new THREE.Mesh(new THREE.BoxGeometry(0.56, 0.012, 0.018), mat);
  arm.position.set(0.28, 0, -0.012);
  const blade = new THREE.Mesh(new THREE.BoxGeometry(0.6, 0.022, 0.016), mat);
  blade.position.set(0.32, -0.02, -0.006);
  const cap = new THREE.Mesh(new THREE.CylinderGeometry(0.022, 0.022, 0.03, 12), mat);
  cap.rotation.x = Math.PI / 2;
  pivot.add(arm, blade, cap);
  return pivot;
}

function plateTexture(text) {
  const cv = document.createElement('canvas');
  cv.width = 520; cv.height = 110;
  const g = cv.getContext('2d');
  g.fillStyle = '#f4f4f0'; g.fillRect(0, 0, 520, 110);
  g.strokeStyle = '#222'; g.lineWidth = 6; g.strokeRect(5, 5, 510, 100);
  g.fillStyle = '#111'; g.font = 'bold 74px "Malgun Gothic", "Noto Sans KR", sans-serif';
  g.textAlign = 'center'; g.textBaseline = 'middle'; g.fillText(text, 260, 60);
  const t = new THREE.CanvasTexture(cv);
  t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 4;
  return t;
}

function shadowTexture() {
  const cv = document.createElement('canvas');
  cv.width = 128; cv.height = 256;
  const g = cv.getContext('2d');
  const grd = g.createRadialGradient(64, 64, 4, 64, 64, 64);
  grd.addColorStop(0, 'rgba(0,0,0,0.9)'); grd.addColorStop(0.55, 'rgba(0,0,0,0.6)'); grd.addColorStop(1, 'rgba(0,0,0,0)');
  g.setTransform(1, 0, 0, 2, 0, 0); g.fillStyle = grd; g.fillRect(0, 0, 128, 128);
  return new THREE.CanvasTexture(cv);
}

// 바닥 그림자 (주변광 차폐 흉내. 밤에도 차가 땅에 붙어 보이게)
export function makeContactShadow(w, l) {
  const m = new THREE.Mesh(new THREE.PlaneGeometry(w, l),
    new THREE.MeshBasicMaterial({ map: shadowTexture(), transparent: true, depthWrite: false, opacity: 0.85 }));
  m.rotation.x = -Math.PI / 2; m.position.y = 0.026;
  return m;
}

// 와이퍼 왕복 (주기는 연출값. 사양서는 단계만 정한다)
export function makeWiperAnimator() {
  let phase = 0, pause = 0;
  return (level, dt) => {
    if (phase === 0) {
      if (level === 'stop') return 0;
      if (pause > 0) { pause -= dt; return 0; }
      phase = 1e-6;
    }
    const cycle = level === 'high' ? 0.5 : 1.0;
    phase += dt / cycle;
    if (phase >= 1) { phase = 0; if (level === 'int') pause = 2.0; return 0; }
    return Math.sin(phase * Math.PI);
  };
}

// 전조등 빛: 스포트라이트 2개 (빗속 빛줄기는 world.js가 빗방울을 밝혀 그린다)
export function makeBeams(group, positions) {
  const spots = positions.map((p) => {
    const spot = new THREE.SpotLight(0xfff1d8, 0, 120, 0.32, 0.75, 1.4);
    spot.position.copy(p);
    spot.target.position.set(p.x * 2, 0, -60);
    group.add(spot, spot.target);
    return spot;
  });
  return { set(on) { spots.forEach((s) => { s.intensity = on ? 1500 : 0; }); } };
}

export function createCar({ color = 0x1b3766 } = {}) {
  const group = new THREE.Group();
  const mats = makeMaterials(color);
  const shell = buildShell(mats);
  group.add(...shell.body, buildInterior());

  // 도어: 앞 모서리(경첩)를 축으로 바깥으로 열림. 순서 FL FR RL RR
  const doors = DOORS.map((d, k) => {
    const hinge = new THREE.Group();
    hinge.position.set(d.side * halfW(d.z0), 0, d.z0);
    for (const m of shell.doors[k]) { m.position.copy(hinge.position).negate(); hinge.add(m); }
    const handle = new THREE.Mesh(new RoundedBoxGeometry(0.025, 0.035, 0.17, 2, 0.01),
      new THREE.MeshPhysicalMaterial({ color: 0xbfc3c8, metalness: 1, roughness: 0.2 }));
    const hz = d.z1 - 0.2;
    handle.position.set(d.side * (sideX(hz, 0.9) + 0.008), 0.9, hz).sub(hinge.position);
    hinge.add(handle);
    if (k < 2) {                                         // 사이드미러는 앞도어에
      const mirror = new THREE.Group();
      const housing = new THREE.Mesh(new RoundedBoxGeometry(0.2, 0.13, 0.09, 3, 0.035), mats[M.paint]);
      housing.position.x = d.side * 0.1;
      const glass = new THREE.Mesh(new THREE.PlaneGeometry(0.17, 0.1),
        new THREE.MeshStandardMaterial({ color: 0x8d969f, metalness: 1, roughness: 0.08 }));
      glass.position.set(d.side * 0.1, 0, 0.046);
      const stalk = new THREE.Mesh(new THREE.BoxGeometry(0.06, 0.03, 0.05), mats[M.trim]);
      stalk.position.x = d.side * 0.0;
      mirror.add(housing, glass, stalk);
      mirror.position.set(d.side * (halfW(-0.74) - 0.02), 1.03, -0.74).sub(hinge.position);
      hinge.add(mirror);
    }
    hinge.userData.side = d.side;
    group.add(hinge);
    return hinge;
  });

  // 바퀴: 앞바퀴는 조향 그룹 안에. 왼쪽 바퀴는 바깥면이 -x가 되게 뒤집는다
  const wheels = [];
  for (const z of AXLES) {
    for (const side of [-1, 1]) {
      const steer = new THREE.Group();
      steer.position.set(side * WHEEL_X, WHEEL_R, z);
      const mount = new THREE.Group();
      if (side < 0) mount.rotation.y = Math.PI;
      const { spin, caliper } = buildWheel(mats);
      mount.add(spin, caliper);
      steer.add(mount);
      group.add(steer);
      wheels.push({ steer, spin, side, front: z < 0 });
    }
  }

  // 와이퍼: 앞유리 면 위에 놓고 그 면 안에서 회전
  const wz = WS0 + 0.08, wy = roof(wz) + 0.012;
  const slope = Math.atan2(roof(wz + 0.05) - roof(wz - 0.05), 0.1);   // 앞유리 기울기
  const wipers = [-0.6, 0.06].map((x) => {
    const plane = new THREE.Group();
    plane.position.set(x, wy, wz);
    plane.rotation.x = Math.PI / 2 - slope;   // 로컬 y가 유리 위쪽(+z, +y)으로
    const pivot = buildWiper();
    plane.add(pivot);
    group.add(plane);
    return pivot;
  });

  // 번호판
  const plateMat = new THREE.MeshStandardMaterial({ map: plateTexture('12가 3456'), roughness: 0.4 });
  const rearPlate = new THREE.Mesh(new THREE.PlaneGeometry(0.52, 0.11), plateMat);
  rearPlate.position.set(0, 0.6, ZR + 0.004);
  const frontPlate = new THREE.Mesh(new THREE.PlaneGeometry(0.52, 0.11), plateMat);
  frontPlate.position.set(0, 0.36, ZF - 0.004); frontPlate.rotation.y = Math.PI;
  group.add(rearPlate, frontPlate);

  const contact = makeContactShadow(2.3, 5.0);
  group.add(contact);

  group.traverse((o) => { if (o.isMesh && o !== contact) { o.castShadow = true; o.receiveShadow = true; } });

  const beams = makeBeams(group, [new THREE.Vector3(-0.64, 0.74, ZF + 0.12), new THREE.Vector3(0.64, 0.74, ZF + 0.12)]);
  const wiperAmount = makeWiperAnimator();

  function update(s, dt, speedMs, dir, steerInput = 0, wet = 0) {
    const on = !!s.out.headlight;
    mats[M.head].emissiveIntensity = on ? 2.5 : 0;
    beams.set(on);
    mats[M.tail].emissiveIntensity = s.in.brake ? 6 : on ? 1.2 : 0;
    mats[M.rev].emissiveIntensity = s.in.gear === 'R' ? 3 : 0;

    const a = wiperAmount(s.out.wiper, dt) * WIPER_SWEEP_RAD;
    wipers.forEach((p) => { p.rotation.z = a; });

    doors.forEach((d, i) => {
      const target = s.in.doors[i] ? d.userData.side * DOOR_OPEN_RAD : 0;
      d.rotation.y += (target - d.rotation.y) * Math.min(1, dt * 8);
    });

    const spin = (speedMs * dt * dir) / WHEEL_R;
    for (const w of wheels) {
      w.spin.rotation.x -= spin * w.side;
      if (w.front) w.steer.rotation.y += (-steerInput * 0.22 - w.steer.rotation.y) * Math.min(1, dt * 6);
    }
  }

  return { group, update, paint: mats[M.paint] };
}
