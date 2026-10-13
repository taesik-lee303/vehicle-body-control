// road.js - 실사 노면(아스팔트·풀밭 텍스처), 차선, 가드레일, 가로등, 나무. 차는 제자리에 있고 세상이 +z로 흐른다.
// 같은 간격으로 반복되는 것(차선, 기둥, 가로등)은 그룹을 간격만큼 되돌려 돌리고, 나무는 하나씩 감아 돈다.
import * as THREE from 'three';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';

const LANE = 3.6;                    // 차로 폭
const HALF_ROAD = LANE * 2 + 0.15;   // 가장자리 실선 바깥까지
const SHOULDER = 1.4;
const RAIL_X = HALF_ROAD + SHOULDER + 0.5;
const LEN = 1600;
const ASPHALT_TILE = 4.5, GRASS_TILE = 7;
const DASH_PERIOD = 20, DASH_LEN = 8;           // 흰 점선 (8m 칠, 12m 빈칸)
const POST_PERIOD = 4, LAMP_PERIOD = 50;
const TREE_NEAR = 40, TREE_FAR = -560;          // 나무가 도는 z 범위

function tex(loader, url, repeatX, repeatY, srgb) {
  const t = loader.load(url);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.repeat.set(repeatX, repeatY);
  t.anisotropy = 8;
  if (srgb) t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

// 큰 얼룩 (풀밭 반복 무늬를 감춤)
function blotchTexture() {
  const cv = document.createElement('canvas');
  cv.width = cv.height = 256;
  const g = cv.getContext('2d');
  g.fillStyle = '#d0d0d0'; g.fillRect(0, 0, 256, 256);
  let seed = 7;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
  for (let i = 0; i < 90; i++) {
    const x = rnd() * 256, y = rnd() * 256, r = 10 + rnd() * 50, v = Math.floor(150 + rnd() * 105);
    const grd = g.createRadialGradient(x, y, 0, x, y, r);
    grd.addColorStop(0, `rgba(${v},${v},${v},0.7)`); grd.addColorStop(1, `rgba(${v},${v},${v},0)`);
    g.fillStyle = grd; g.fillRect(x - r, y - r, r * 2, r * 2);
  }
  const t = new THREE.CanvasTexture(cv);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  return t;
}

// 차선 무늬: 칠이 조금 닳은 흰색/노란색. dash면 앞부분만 칠
function lineTexture(dashFrac) {
  const cv = document.createElement('canvas');
  cv.width = 32; cv.height = 512;
  const g = cv.getContext('2d');
  const h = dashFrac ? Math.round(512 * dashFrac) : 512;
  g.fillStyle = '#fff'; g.fillRect(0, 0, 32, h);
  let seed = 3;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
  g.globalCompositeOperation = 'destination-out';
  for (let i = 0; i < 260; i++) { g.fillStyle = `rgba(0,0,0,${0.15 + rnd() * 0.35})`; g.fillRect(rnd() * 32, rnd() * h, 1 + rnd() * 3, 1 + rnd() * 4); }
  const t = new THREE.CanvasTexture(cv);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  return t;
}

// 빛 웅덩이·반사 줄기용 부드러운 원
function glowTexture() {
  const cv = document.createElement('canvas');
  cv.width = cv.height = 128;
  const g = cv.getContext('2d');
  const grd = g.createRadialGradient(64, 64, 0, 64, 64, 64);
  grd.addColorStop(0, 'rgba(255,255,255,1)'); grd.addColorStop(0.4, 'rgba(255,255,255,0.35)'); grd.addColorStop(1, 'rgba(255,255,255,0)');
  g.fillStyle = grd; g.fillRect(0, 0, 128, 128);
  return new THREE.CanvasTexture(cv);
}

// 나뭇잎 덩어리 (알파로 잘라 쓰는 카드)
function leafTexture(conifer) {
  const cv = document.createElement('canvas');
  cv.width = cv.height = 256;
  const g = cv.getContext('2d');
  let seed = conifer ? 11 : 5;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
  for (let i = 0; i < (conifer ? 420 : 260); i++) {
    const a = rnd() * Math.PI * 2, r = Math.sqrt(rnd()) * 110;
    const x = 128 + Math.cos(a) * r, y = 128 + Math.sin(a) * r;
    const l = 22 + rnd() * 26;
    g.fillStyle = `hsl(${conifer ? 100 + rnd() * 25 : 78 + rnd() * 34}, ${28 + rnd() * 22}%, ${conifer ? l * 0.75 : l}%)`;
    g.save(); g.translate(x, y); g.rotate(rnd() * Math.PI);
    g.beginPath();
    if (conifer) g.ellipse(0, 0, 1.5, 9 + rnd() * 6, 0, 0, Math.PI * 2);
    else g.ellipse(0, 0, 5 + rnd() * 4, 9 + rnd() * 6, 0, 0, Math.PI * 2);
    g.fill(); g.restore();
  }
  const t = new THREE.CanvasTexture(cv);
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

// 카드 하나: 법선은 수관 중심에서 바깥으로 (부드러운 음영)
function card(size, pos, rot, center) {
  const geo = new THREE.PlaneGeometry(size, size);
  geo.applyMatrix4(new THREE.Matrix4().makeRotationFromEuler(rot));
  geo.translate(pos.x, pos.y, pos.z);
  const p = geo.attributes.position, n = geo.attributes.normal, v = new THREE.Vector3();
  for (let i = 0; i < p.count; i++) {
    v.fromBufferAttribute(p, i).sub(center).normalize();
    n.setXYZ(i, v.x, v.y * 0.7 + 0.3, v.z);
  }
  return geo;
}

function makeTreePrototype(kind, seed) {
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
  const leaves = [], wood = [];
  if (kind === 'conifer') {
    const h = 9;
    const trunk = new THREE.CylinderGeometry(0.12, 0.25, h * 0.9, 7);
    trunk.translate(0, h * 0.45, 0); wood.push(trunk);
    const center = new THREE.Vector3(0, h * 0.5, 0);
    for (let layer = 0; layer < 9; layer++) {
      const y = 1.6 + layer * (h - 2) / 9, r = (1 - layer / 9) * 2.4 + 0.3;
      for (let k = 0; k < 9; k++) {
        const a = (k / 9) * Math.PI * 2 + rnd();
        const p = new THREE.Vector3(Math.cos(a) * r * 0.6, y, Math.sin(a) * r * 0.6);
        leaves.push(card(r * 1.3 + 0.6, p, new THREE.Euler(-0.6 - rnd() * 0.4, -a + Math.PI / 2, 0, 'YXZ'), center.clone().setY(y - 1)));
      }
    }
  } else {
    const h = 4 + rnd() * 2.5;
    const trunk = new THREE.CylinderGeometry(0.14, 0.26, h, 7);
    trunk.translate(0, h / 2, 0); wood.push(trunk);
    for (let b = 0; b < 4; b++) {
      const br = new THREE.CylinderGeometry(0.05, 0.1, 2, 5);
      br.translate(0, 1, 0);
      br.applyMatrix4(new THREE.Matrix4().makeRotationFromEuler(new THREE.Euler(0.7, b * 1.6 + rnd(), 0, 'YXZ')));
      br.translate(0, h * 0.75, 0); wood.push(br);
    }
    const center = new THREE.Vector3(0, h + 1.2, 0);
    const blobs = 6 + Math.floor(rnd() * 3);
    for (let b = 0; b < blobs; b++) {
      const bc = center.clone().add(new THREE.Vector3((rnd() - 0.5) * 3.4, (rnd() - 0.4) * 2.4, (rnd() - 0.5) * 3.4));
      for (let k = 0; k < 16; k++) {
        const d = new THREE.Vector3(rnd() - 0.5, rnd() - 0.5, rnd() - 0.5).normalize().multiplyScalar(rnd() * 1.3);
        leaves.push(card(1.5 + rnd() * 0.7, bc.clone().add(d), new THREE.Euler(rnd() * 3, rnd() * 3, rnd() * 3), center));
      }
    }
  }
  return { leaves: mergeGeometries(leaves), wood: mergeGeometries(wood.map((g) => g.toNonIndexed())) };
}

// 가로등 (현대식 LED). 원점은 기둥 밑
function makeLamp(metal, headMat) {
  const g = new THREE.Group();
  const pole = new THREE.Mesh(new THREE.CylinderGeometry(0.07, 0.13, 9.5, 12), metal);
  pole.position.y = 4.75;
  const curve = new THREE.QuadraticBezierCurve3(new THREE.Vector3(0, 9.4, 0), new THREE.Vector3(0, 10.2, 0), new THREE.Vector3(-2.6, 10.1, 0));
  const arm = new THREE.Mesh(new THREE.TubeGeometry(curve, 16, 0.055, 8), metal);
  const head = new THREE.Mesh(new THREE.BoxGeometry(0.75, 0.12, 0.32), metal);
  head.position.set(-2.85, 10.05, 0);
  const led = new THREE.Mesh(new THREE.PlaneGeometry(0.6, 0.24), headMat);
  led.rotation.x = Math.PI / 2; led.position.set(-2.85, 9.985, 0);
  const base = new THREE.Mesh(new THREE.CylinderGeometry(0.2, 0.22, 0.5, 12),
    new THREE.MeshStandardMaterial({ color: 0x8a8a86, roughness: 0.9 }));
  base.position.y = 0.25;
  g.add(pole, arm, head, led, base);
  return g;
}

export function createRoad(scene) {
  const loader = new THREE.TextureLoader();
  const movers = [];   // { obj, period }  주기 그룹

  // ---- 풀밭 ----
  const grassMap = tex(loader, 'assets/tex/grass_diff.jpg', LEN / GRASS_TILE, LEN / GRASS_TILE, true);
  const grassNor = tex(loader, 'assets/tex/grass_nor.jpg', LEN / GRASS_TILE, LEN / GRASS_TILE);
  const blotch = blotchTexture(); blotch.repeat.set(LEN / 160, LEN / 160);
  const ground = new THREE.Mesh(new THREE.PlaneGeometry(LEN, LEN),
    new THREE.MeshStandardMaterial({ map: grassMap, normalMap: grassNor, aoMap: blotch, aoMapIntensity: 0.9,
      color: 0xa8b48a, roughness: 1 }));
  ground.rotation.x = -Math.PI / 2;
  ground.receiveShadow = true;
  scene.add(ground);

  // ---- 노면 (갓길 포함) ----
  const roadW = (HALF_ROAD + SHOULDER) * 2;
  const aMap = tex(loader, 'assets/tex/asphalt_diff.jpg', roadW / ASPHALT_TILE, LEN / ASPHALT_TILE, true);
  const aNor = tex(loader, 'assets/tex/asphalt_nor.jpg', roadW / ASPHALT_TILE, LEN / ASPHALT_TILE);
  const aRough = tex(loader, 'assets/tex/asphalt_rough.jpg', roadW / ASPHALT_TILE, LEN / ASPHALT_TILE);
  const asphalt = new THREE.MeshPhysicalMaterial({ map: aMap, normalMap: aNor, roughnessMap: aRough, roughness: 1,
    color: 0x9a9a9a, clearcoat: 0, clearcoatRoughness: 0.06 });
  const road = new THREE.Mesh(new THREE.PlaneGeometry(roadW, LEN), asphalt);
  road.rotation.x = -Math.PI / 2; road.position.y = 0.01;
  road.receiveShadow = true;
  scene.add(road);
  // 갓길 끝 연석 띠
  const curbMat = new THREE.MeshStandardMaterial({ color: 0x8d8b85, roughness: 0.95 });
  for (const s of [-1, 1]) {
    const curb = new THREE.Mesh(new THREE.BoxGeometry(0.25, 0.08, LEN), curbMat);
    curb.position.set(s * (HALF_ROAD + SHOULDER), 0.04, 0);
    curb.receiveShadow = true;
    scene.add(curb);
  }

  // ---- 차선: 중앙 노란 겹실선, 차로 사이 흰 점선, 가장자리 흰 실선 ----
  const solidTex = lineTexture(0), dashTex = lineTexture(DASH_LEN / DASH_PERIOD);
  solidTex.repeat.set(1, LEN / 30); dashTex.repeat.set(1, LEN / DASH_PERIOD);
  const lineMats = [];
  const addLine = (x, w, color, map) => {
    const m = new THREE.MeshStandardMaterial({ color, map, alphaMap: map, transparent: true, roughness: 0.55,
      polygonOffset: true, polygonOffsetFactor: -2, depthWrite: false });
    const mesh = new THREE.Mesh(new THREE.PlaneGeometry(w, LEN), m);
    mesh.rotation.x = -Math.PI / 2; mesh.position.set(x, 0.015, 0);
    mesh.receiveShadow = true;
    scene.add(mesh);
    lineMats.push(m);
  };
  addLine(-0.12, 0.13, 0xe7b416, solidTex); addLine(0.12, 0.13, 0xe7b416, solidTex);
  for (const s of [-1, 1]) {
    addLine(s * LANE, 0.15, 0xf2f2f2, dashTex);
    addLine(s * (LANE * 2), 0.18, 0xf2f2f2, solidTex);
  }

  // ---- 가드레일 (오른쪽, W빔) + 기둥 ----
  const metal = new THREE.MeshStandardMaterial({ color: 0xa9afb5, metalness: 0.85, roughness: 0.42 });
  const prof = new THREE.Shape();
  prof.moveTo(0, -0.16); prof.bezierCurveTo(0.09, -0.12, 0.09, -0.05, 0.02, 0);
  prof.bezierCurveTo(0.09, 0.05, 0.09, 0.12, 0, 0.16); prof.lineTo(-0.01, 0.16);
  prof.bezierCurveTo(0.075, 0.11, 0.075, 0.06, 0.01, 0); prof.bezierCurveTo(0.075, -0.06, 0.075, -0.11, -0.01, -0.16);
  prof.closePath();
  const beam = new THREE.Mesh(new THREE.ExtrudeGeometry(prof, { depth: LEN, bevelEnabled: false, curveSegments: 6 }), metal);
  beam.rotation.y = Math.PI; beam.position.set(RAIL_X, 0.62, LEN / 2);
  beam.castShadow = true; beam.receiveShadow = true;
  scene.add(beam);
  const postGeo = new THREE.BoxGeometry(0.1, 0.75, 0.1);
  postGeo.translate(RAIL_X + 0.12, 0.375, 0);
  const postCount = Math.round(1000 / POST_PERIOD);
  const posts = new THREE.InstancedMesh(postGeo, metal, postCount);
  const mtx = new THREE.Matrix4();
  for (let i = 0; i < postCount; i++) posts.setMatrixAt(i, mtx.makeTranslation(0, 0, -800 + 40 + i * POST_PERIOD));
  posts.castShadow = true;
  const postGroup = new THREE.Group(); postGroup.add(posts);
  scene.add(postGroup);
  movers.push({ obj: postGroup, period: POST_PERIOD });

  // ---- 가로등 (오른쪽, 50m 간격). 밤에는 머리가 빛나고 노면에 빛 웅덩이, 젖으면 반사 줄기 ----
  const headMat = new THREE.MeshStandardMaterial({ color: 0x777777, emissive: 0xffe2b8, emissiveIntensity: 0 });
  const glow = glowTexture();
  const poolMat = new THREE.MeshBasicMaterial({ map: glow, color: 0xffd9a8, transparent: true, opacity: 0,
    blending: THREE.AdditiveBlending, depthWrite: false, fog: true });
  const streakMat = poolMat.clone();
  const lampGroup = new THREE.Group();
  for (let z = -800 + 25; z < 60; z += LAMP_PERIOD) {
    const lamp = makeLamp(metal, headMat);
    lamp.position.set(RAIL_X + 0.9, 0, z);
    lamp.traverse((o) => { if (o.isMesh) o.castShadow = true; });
    const pool = new THREE.Mesh(new THREE.PlaneGeometry(14, 14), poolMat);
    pool.rotation.x = -Math.PI / 2; pool.position.set(RAIL_X + 0.9 - 2.85, 0.03, z);
    const streak = new THREE.Mesh(new THREE.PlaneGeometry(1.0, 22), streakMat);
    streak.rotation.x = -Math.PI / 2; streak.position.set(RAIL_X + 0.9 - 2.85, 0.031, z + 9);
    lampGroup.add(lamp, pool, streak);
  }
  scene.add(lampGroup);
  movers.push({ obj: lampGroup, period: LAMP_PERIOD });

  // ---- 나무 (양쪽, 무작위 배치를 감아 돌림) ----
  const leafMats = [leafTexture(false), leafTexture(true)].map((map) => new THREE.MeshStandardMaterial({
    map, alphaTest: 0.45, side: THREE.DoubleSide, roughness: 0.85 }));
  const bark = new THREE.MeshStandardMaterial({ color: 0x4b3c2e, roughness: 1 });
  const protos = [makeTreePrototype('broad', 13), makeTreePrototype('broad', 29), makeTreePrototype('conifer', 47)];
  let seed = 101;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
  const trees = protos.map(() => []);
  for (let z = TREE_FAR; z < TREE_NEAR; z += 4.5) {
    for (const side of [-1, 1]) {
      if (rnd() < 0.35) continue;
      const near = side > 0 ? RAIL_X + 3 : HALF_ROAD + SHOULDER + 3;
      const p = rnd() < 0.3 ? 2 : (rnd() < 0.5 ? 0 : 1);
      trees[p].push({ x: side * (near + rnd() * 26), z0: z + rnd() * 4, rot: rnd() * 6.28, s: 0.8 + rnd() * 0.6,
        color: new THREE.Color().setHSL(0, 0, 0.75 + rnd() * 0.35) });
    }
  }
  const treeMeshes = protos.map((p, i) => {
    const conifer = i === 2;
    const leaf = new THREE.InstancedMesh(p.leaves, leafMats[conifer ? 1 : 0], trees[i].length);
    const wood = new THREE.InstancedMesh(p.wood, bark, trees[i].length);
    trees[i].forEach((t, k) => leaf.setColorAt(k, t.color));
    scene.add(leaf, wood);
    return { leaf, wood, list: trees[i] };
  });
  const span = TREE_NEAR - TREE_FAR;
  const q = new THREE.Quaternion(), up = new THREE.Vector3(0, 1, 0), pos = new THREE.Vector3(), scl = new THREE.Vector3();
  function placeTrees(dist) {
    for (const t of treeMeshes) {
      t.list.forEach((tr, k) => {
        const z = TREE_FAR + ((((tr.z0 - TREE_FAR + dist) % span) + span) % span);
        mtx.compose(pos.set(tr.x, 0, z), q.setFromAxisAngle(up, tr.rot), scl.setScalar(tr.s));
        t.leaf.setMatrixAt(k, mtx); t.wood.setMatrixAt(k, mtx);
      });
      t.leaf.instanceMatrix.needsUpdate = true; t.wood.instanceMatrix.needsUpdate = true;
    }
  }

  let dist = 0;
  const scrolled = [[aMap, ASPHALT_TILE], [aNor, ASPHALT_TILE], [aRough, ASPHALT_TILE],
    [grassMap, GRASS_TILE], [grassNor, GRASS_TILE], [blotch, 160], [solidTex, 30], [dashTex, DASH_PERIOD]];
  placeTrees(0);

  // dz: 이번 프레임에 세상이 +z로 움직인 거리. dark: 0(낮)~1(밤), wet: 0~1
  function update(dz, dark, wet) {
    dist += dz;
    for (const [t, tile] of scrolled) t.offset.y = (dist / tile) % 1;   // 오프셋이 커지면 무늬가 +z(차 뒤쪽)로 간다
    for (const m of movers) m.obj.position.z = ((dist % m.period) + m.period) % m.period;
    placeTrees(dist);

    headMat.emissiveIntensity = dark ** 2 * 6;
    poolMat.opacity = dark ** 2 * 0.22;
    streakMat.opacity = dark ** 2 * wet * 0.5;
    // 젖은 노면: 물막(클리어코트)이 생기고 더 어둡고 매끈해진다
    asphalt.clearcoat = wet * 0.9;
    asphalt.roughness = 1 - wet * 0.45;
    asphalt.color.setScalar(0.42 - wet * 0.17);
    for (const m of lineMats) m.roughness = 0.55 - wet * 0.35;
  }

  return { update, laneX: LANE / 2 };
}
