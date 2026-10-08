// traffic.js - 시뮬레이터 세계의 교통 참여자·고정 물체를 그린다 (요구사항 8.3).
// 위치는 C 코어(env_world)가 정한 정답을 그대로 그릴 뿐이다. 레이더가 본 것은 radar-view.js가 따로 그린다.
// C 코어 좌표는 도로를 편 좌표(x 앞, y 왼쪽)다. 화면 위치는 world.js의 mapper가 정한다 (지도면 경로를 따라 휨).
import * as THREE from 'three';
import { RoundedBoxGeometry } from 'three/addons/geometries/RoundedBoxGeometry.js';

const PAINT = [0xd8dde3, 0x2b3440, 0x8c1c1c, 0x2d5a8c, 0x6b6f75, 0xb9a27a, 0x1f5c45, 0xe6e6e0];

function std(color, extra = {}) {
  return new THREE.MeshStandardMaterial({ color, roughness: 0.55, metalness: 0.2, ...extra });
}

function shadowed(obj) {
  obj.traverse((o) => { if (o.isMesh) { o.castShadow = true; o.receiveShadow = true; } });
  return obj;
}

// 차는 -z(화면 안쪽)를 앞으로 만든다. 후미등은 +z 쪽
function makeCar(id) {
  const g = new THREE.Group();
  const paint = std(PAINT[id % PAINT.length], { metalness: 0.5, roughness: 0.35 });
  const body = new THREE.Mesh(new RoundedBoxGeometry(1.86, 0.78, 4.55, 3, 0.18), paint);
  body.position.y = 0.72;
  const cabin = new THREE.Mesh(new RoundedBoxGeometry(1.62, 0.56, 2.5, 3, 0.2),
    std(0x1b232c, { metalness: 0.1, roughness: 0.15 }));
  cabin.position.set(0, 1.32, 0.25);
  const wheelGeo = new THREE.CylinderGeometry(0.33, 0.33, 0.24, 18).rotateZ(Math.PI / 2);
  const tire = std(0x15171a, { roughness: 0.9, metalness: 0 });
  for (const [x, z] of [[-0.82, -1.45], [0.82, -1.45], [-0.82, 1.45], [0.82, 1.45]]) {
    const w = new THREE.Mesh(wheelGeo, tire);
    w.position.set(x, 0.33, z);
    g.add(w);
  }
  const tail = new THREE.MeshStandardMaterial({ color: 0x400000, emissive: 0xff1a10, emissiveIntensity: 0.6 });
  for (const x of [-0.65, 0.65]) {
    const t = new THREE.Mesh(new THREE.BoxGeometry(0.4, 0.12, 0.05), tail);
    t.position.set(x, 0.86, 2.29);
    g.add(t);
  }
  g.add(body, cabin);
  g.userData.tail = tail;
  return shadowed(g);
}

function makeTruck(id) {
  const g = new THREE.Group();
  const cab = new THREE.Mesh(new RoundedBoxGeometry(2.4, 2.6, 2.2, 2, 0.15), std(PAINT[(id + 3) % PAINT.length]));
  cab.position.set(0, 1.6, -3.9);
  const box = new THREE.Mesh(new THREE.BoxGeometry(2.5, 3.0, 7.6), std(0xe9e9e4, { metalness: 0.05 }));
  box.position.set(0, 2.0, 1.2);
  const wheelGeo = new THREE.CylinderGeometry(0.5, 0.5, 0.35, 18).rotateZ(Math.PI / 2);
  const tire = std(0x15171a, { roughness: 0.9, metalness: 0 });
  for (const z of [-3.8, 1.0, 3.6]) for (const x of [-1.05, 1.05]) {
    const w = new THREE.Mesh(wheelGeo, tire);
    w.position.set(x, 0.5, z);
    g.add(w);
  }
  g.add(cab, box);
  return shadowed(g);
}

function makePed(id) {
  const g = new THREE.Group();
  const cloth = std([0x3a5a8c, 0x8c3a3a, 0x3a8c5a, 0x8c7a3a][id % 4], { metalness: 0 });
  const body = new THREE.Mesh(new THREE.CapsuleGeometry(0.2, 0.75, 4, 10), cloth);
  body.position.y = 1.05;
  const legs = new THREE.Mesh(new THREE.CapsuleGeometry(0.13, 0.55, 4, 8), std(0x22262c, { metalness: 0 }));
  legs.position.y = 0.42;
  const head = new THREE.Mesh(new THREE.SphereGeometry(0.13, 14, 10), std(0xd9b08c, { metalness: 0 }));
  head.position.y = 1.62;
  g.add(body, legs, head);
  return shadowed(g);
}

function makeBridge() {
  const g = new THREE.Group();
  const concrete = std(0x9a9890, { roughness: 0.95, metalness: 0 });
  const deck = new THREE.Mesh(new THREE.BoxGeometry(30, 1.0, 10), concrete);
  deck.position.y = 5.0;
  const rail = new THREE.Mesh(new THREE.BoxGeometry(30, 0.9, 0.25), concrete);
  rail.position.set(0, 5.95, -4.85);
  const rail2 = rail.clone();
  rail2.position.z = 4.85;
  g.add(deck, rail, rail2);
  for (const x of [-13.5, 13.5]) for (const z of [-3, 3]) { /* 기둥은 도로 밖 */
    const p = new THREE.Mesh(new THREE.BoxGeometry(1.0, 4.5, 1.0), concrete);
    p.position.set(x, 2.25, z);
    g.add(p);
  }
  return shadowed(g);
}

function makeSign() {
  const g = new THREE.Group();
  const steel = std(0x8d939a, { metalness: 0.7, roughness: 0.4 });
  for (const x of [-8.2, 8.2]) {
    const post = new THREE.Mesh(new THREE.CylinderGeometry(0.15, 0.18, 6.6, 10), steel);
    post.position.set(x, 3.3, 0);
    g.add(post);
  }
  const beam = new THREE.Mesh(new THREE.BoxGeometry(16.4, 0.3, 0.3), steel);
  beam.position.y = 6.35;
  const panelMat = std(0x0f6b3a, { metalness: 0.1, roughness: 0.6 });
  for (const x of [-4, 4]) {
    const panel = new THREE.Mesh(new THREE.BoxGeometry(6.5, 1.5, 0.12), panelMat);
    panel.position.set(x, 5.75, 0.1);
    g.add(panel);
  }
  g.add(beam);
  return shadowed(g);
}

function makeManhole() {
  const m = new THREE.Mesh(new THREE.CylinderGeometry(0.4, 0.4, 0.03, 24), std(0x3c3a36, { metalness: 0.6, roughness: 0.7 }));
  m.position.y = 0.017;
  m.receiveShadow = true;
  return m;
}

const MAKERS = { car: makeCar, truck: makeTruck, ped: makePed, bridge: makeBridge, sign: makeSign, manhole: makeManhole };

export function createTraffic(scene) {
  const group = new THREE.Group();
  scene.add(group);
  const items = new Map();   // id → { obj, kind }

  const at = new THREE.Vector3();
  // objs: sim_adas().objs = [id, 종류, 가운데 x, 가운데 y, 길이, 폭, z0, z1, 속도, 가로 속도, 자차 기준 방향] (자차 기준)
  // mapper: point(x, y, z, out) 자차 기준 → 화면, egoHeading() 자차 방향
  function update(objs, dark, mapper) {
    const seen = new Set();
    for (const o of objs || []) {
      const [id, kind, xc, y, , , , , , , yaw] = o;
      let it = items.get(id);
      if (!it || it.kind !== kind) {
        if (it) group.remove(it.obj);
        it = { obj: (MAKERS[kind] || makeCar)(id), kind };
        items.set(id, it);
        group.add(it.obj);
      }
      it.obj.position.copy(mapper.point(xc, y, 0, at));
      it.obj.rotation.y = mapper.egoHeading() + yaw;   // 반대 방향 차는 C가 이미 180° 돌려 준다
      it.obj.visible = xc > -80 && xc < 400;
      if (it.obj.userData.tail) it.obj.userData.tail.emissiveIntensity = 0.4 + 1.6 * dark;
      seen.add(id);
    }
    for (const [id, it] of items) if (!seen.has(id)) { group.remove(it.obj); items.delete(id); }
  }

  return { update };
}
