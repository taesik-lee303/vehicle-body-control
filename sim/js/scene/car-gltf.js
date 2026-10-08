// car-gltf.js - 직접 받은 실사 차 모델(sim/assets/car.glb)을 car.js와 같은 인터페이스로 감싼다.
// 노드 이름 규칙과 car.json 설정은 sim/assets/README.md. 도어·바퀴의 회전축은 모델 원점과 상관없이 경계 상자로 잡는다.
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { DRACOLoader } from 'three/addons/loaders/DRACOLoader.js';
import { DOOR_OPEN_RAD, makeWiperAnimator, makeBeams, buildWiper, makeContactShadow } from './car.js';

const LENGTH = 4.6;
const WIPER_SWEEP_RAD = THREE.MathUtils.degToRad(95);
// 앞뒤+좌우(fl) 순서와 좌우+앞뒤(lf, GTA식) 순서를 모두 받는다. rr은 어느 쪽이든 뒤 오른쪽
const CORNER = { fl: 0, fr: 1, rl: 2, rr: 3, lf: 0, rf: 1, lr: 2 };
const norm = (s) => (s || '').toLowerCase().replace(/[^a-z0-9]+/g, '_');

// 'door_fl', 'door_lf', 'FL_Door', 'Door Front Left' → 0..3, 아니면 -1
function cornerOf(name, kind) {
  const n = norm(name);
  if (!n.includes(kind)) return -1;
  const m = n.match(/(?:^|_)(fl|fr|rl|rr|lf|rf|lr)(?:_|$)/);
  if (m) return CORNER[m[1]];
  const f = /front/.test(n) ? 'f' : /rear|back/.test(n) ? 'r' : '';
  const s = /left/.test(n) ? 'l' : /right/.test(n) ? 'r' : '';
  return f && s ? CORNER[f + s] : -1;
}
const matches = (o, re) => re.test(norm(o.name)) || [].concat(o.material || []).some((m) => re.test(norm(m.name)));

function boxOf(obj) { return new THREE.Box3().setFromObject(obj); }

// 바퀴 4개가 한 메시로 합쳐진 모델(TYRES, RIMS 등): 삼각형을 차 가운데 기준 네 구역으로 나눠 구역마다 그룹을 만든다.
// 나눈 메시는 그 구역의 정점만 가진다 (경계 상자로 회전축을 잡으므로). 비어 있는 구역은 그룹을 만들지 않는다
function splitWheels(root, meshes) {
  const groups = [];
  const v = new THREE.Vector3(), c = new THREE.Vector3();
  for (const o of meshes) {
    const g = o.geometry, pos = g.attributes.position;
    const idx = g.index ? g.index.array : Array.from({ length: pos.count }, (_, i) => i);
    const parts = [[], [], [], []];
    for (let t = 0; t + 2 < idx.length; t += 3) {
      c.set(0, 0, 0);
      for (let k = 0; k < 3; k++) c.add(v.fromBufferAttribute(pos, idx[t + k]).applyMatrix4(o.matrixWorld));
      parts[(c.z < 0 ? 0 : 2) + (c.x < 0 ? 0 : 1)].push(idx[t], idx[t + 1], idx[t + 2]);   // fl fr rl rr
    }
    parts.forEach((p, i) => {
      if (!p.length) return;
      const sub = new THREE.BufferGeometry();
      for (const [name, attr] of Object.entries(g.attributes)) sub.setAttribute(name, attr);
      sub.setIndex(p);
      const part = new THREE.Mesh(sub.toNonIndexed(), o.material);
      part.name = `${o.name}_${['fl', 'fr', 'rl', 'rr'][i]}`;
      part.castShadow = true; part.receiveShadow = true;
      if (!groups[i]) { groups[i] = new THREE.Group(); root.add(groups[i]); }
      part.applyMatrix4(o.matrixWorld);   // root는 원점이라 matrixWorld가 곧 root 좌표
      groups[i].add(part);
    });
    o.removeFromParent();
  }
  return groups;
}

function pivotAt(root, obj, point) {
  const p = new THREE.Group();
  p.position.copy(point);
  root.add(p);
  p.updateMatrixWorld(true);
  p.attach(obj);
  return p;
}

export async function loadGltfCar(base = 'assets/') {
  const res = await fetch(base + 'car.json').catch(() => null);
  const cfg = res && res.ok ? await res.json() : {};
  const loader = new GLTFLoader();
  const draco = new DRACOLoader();
  draco.setDecoderPath('vendor/three/addons/libs/draco/gltf/');
  loader.setDRACOLoader(draco);
  const gltf = await loader.loadAsync(base + (cfg.file || 'car.glb'));

  // 차 좌표로 맞춤: 앞 -z, 바닥 y=0, 가운데 x=z=0, 길이 4.6m
  const root = new THREE.Group();
  const fit = new THREE.Group();
  fit.add(gltf.scene);
  root.add(fit);
  fit.rotation.y = THREE.MathUtils.degToRad(cfg.rotationY || 0);
  fit.updateMatrixWorld(true);
  const size = boxOf(fit).getSize(new THREE.Vector3());
  fit.scale.setScalar(cfg.scale || (cfg.length || LENGTH) / Math.max(size.x, size.z));
  fit.updateMatrixWorld(true);
  let box = boxOf(fit);
  const center = box.getCenter(new THREE.Vector3());
  fit.position.set(-center.x, -box.min.y, -center.z);
  root.updateMatrixWorld(true);
  box = boxOf(root);
  const dim = box.getSize(new THREE.Vector3());

  // 이름으로 찾기. 이미 고른 노드의 자식은 건너뛴다 (도어 그룹 안의 유리 등)
  const doors = [], wheels = [], heads = [], tails = [], revs = [], oldWipers = [];
  const picked = new Set();
  const underPicked = (o) => { for (let p = o.parent; p; p = p.parent) if (picked.has(p)) return true; return false; };
  gltf.scene.traverse((o) => {
    if (underPicked(o)) return;
    const d = cornerOf(o.name, 'door'), w = cornerOf(o.name, 'wheel');
    if (d >= 0 && !doors[d]) { doors[d] = o; picked.add(o); return; }
    if (w >= 0 && !wheels[w]) { wheels[w] = o; picked.add(o); return; }
    if (/wiper/.test(norm(o.name))) { oldWipers.push(o); picked.add(o); }
  });
  // 이름으로 4개를 다 못 찾으면, 합쳐진 타이어·휠 메시(tyres, rims ...)를 네 구역으로 나눈다
  if (wheels.filter(Boolean).length < 4) {
    const merged = [];
    gltf.scene.traverse((o) => {
      if (o.isMesh && !underPicked(o) && /(^|_)(tyres?|tires?|rims?|wheels)(_|$)/.test(norm(o.name))) merged.push(o);
    });
    splitWheels(root, merged).forEach((g, i) => { if (g && !wheels[i]) wheels[i] = g; });
  }
  gltf.scene.traverse((o) => {
    if (!o.isMesh) return;
    o.castShadow = true; o.receiveShadow = true;
    const kind = matches(o, /head_?light|head_?lamp|front_?light|projector/) ? heads : matches(o, /reverse|backup/) ? revs
      : matches(o, /tail_?light|tail_?lamp|rear_?light|brake_?light|stop_?lamp/) ? tails : null;
    if (kind) { o.material = Array.isArray(o.material) ? o.material.map((m) => m.clone()) : o.material.clone(); kind.push(o); }
  });
  const emissives = (list, color) => list.flatMap((o) => [].concat(o.material)).filter((m) => 'emissive' in m)
    .map((m) => { m.emissive = new THREE.Color(color); m.emissiveIntensity = 0; return m; });
  const headMats = emissives(heads, 0xfff3dc), tailMats = emissives(tails, 0xff1208), revMats = emissives(revs, 0xffffff);

  // 도어: 바깥 앞 모서리를 경첩으로
  const doorPivots = doors.map((o) => {
    if (!o) return null;
    const b = boxOf(o), c = b.getCenter(new THREE.Vector3()), side = Math.sign(c.x) || 1;
    const p = pivotAt(root, o, new THREE.Vector3(side > 0 ? b.max.x : b.min.x, c.y, b.min.z));
    p.userData.side = side;
    return p;
  });
  // 바퀴: 경계 상자 가운데를 축으로. 앞바퀴는 조향 그룹 안에
  const wheelRigs = wheels.map((o) => {
    if (!o) return null;
    const b = boxOf(o), c = b.getCenter(new THREE.Vector3());
    const steer = new THREE.Group();
    steer.position.copy(c);
    root.add(steer);
    const spin = new THREE.Group();
    steer.add(spin);
    steer.updateMatrixWorld(true);
    spin.attach(o);
    return { steer, spin, radius: b.getSize(new THREE.Vector3()).y / 2, front: c.z < 0 };
  });

  // 와이퍼: 모델 것은 숨기고 같은 규칙의 와이퍼를 붙인다 (위치는 car.json "wiper" 또는 추정)
  oldWipers.forEach((o) => { o.visible = false; });
  const wc = cfg.wiper || {};
  const slope = THREE.MathUtils.degToRad(wc.slopeDeg ?? 28);
  const wipers = (wc.x || [-dim.x * 0.33, dim.x * 0.03]).map((x) => {
    const plane = new THREE.Group();
    plane.position.set(x, wc.y ?? dim.y * 0.66, wc.z ?? box.min.z + dim.z * 0.36);
    plane.rotation.x = Math.PI / 2 - slope;
    const pivot = buildWiper();
    plane.add(pivot);
    root.add(plane);
    return pivot;
  });

  root.add(makeContactShadow(dim.x * 1.2, dim.z * 1.08));
  const beamPos = heads.length
    ? [-1, 1].map((s) => { const hs = heads.map((o) => boxOf(o).getCenter(new THREE.Vector3())).filter((c) => Math.sign(c.x) === s);
      return hs.length ? hs[0] : new THREE.Vector3(s * dim.x * 0.33, dim.y * 0.48, box.min.z + 0.1); })
    : [-1, 1].map((s) => new THREE.Vector3(s * dim.x * 0.33, dim.y * 0.48, box.min.z + 0.1));
  const beams = makeBeams(root, beamPos);
  const wiperAmount = makeWiperAnimator();

  function update(s, dt, speedMs, dir, steerInput = 0, wet = 0) {
    const on = !!s.out.headlight;
    headMats.forEach((m) => { m.emissiveIntensity = on ? 4 : 0; });
    beams.set(on);
    tailMats.forEach((m) => { m.emissiveIntensity = s.in.brake ? 6 : on ? 1.2 : 0; });
    revMats.forEach((m) => { m.emissiveIntensity = s.in.gear === 'R' ? 3 : 0; });
    const a = wiperAmount(s.out.wiper, dt) * WIPER_SWEEP_RAD;
    wipers.forEach((p) => { p.rotation.z = a; });
    doorPivots.forEach((d, i) => {
      if (!d) return;
      const target = s.in.doors[i] ? d.userData.side * DOOR_OPEN_RAD : 0;
      d.rotation.y += (target - d.rotation.y) * Math.min(1, dt * 8);
    });
    for (const w of wheelRigs) {
      if (!w) continue;
      w.spin.rotation.x -= (speedMs * dt * dir) / w.radius;
      if (w.front) w.steer.rotation.y += (-steerInput * 0.22 - w.steer.rotation.y) * Math.min(1, dt * 6);
    }
  }

  const found = (arr) => arr.filter(Boolean).length;
  const info = `도어 ${found(doors)}/4, 바퀴 ${found(wheels)}/4, 전조등 재질 ${headMats.length}, 후미등 재질 ${tailMats.length}`;
  return { group: root, update, info };
}
