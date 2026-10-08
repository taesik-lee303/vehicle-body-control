// city.js - 실제 도로 지도(OpenStreetMap → sim/tools/fetch-map.py → sim/assets/map/*.json)로 만든 동네.
// 지형(고도), 노면·차선·횡단보도·보도, 건물, 공원·나무, 가로등. 지도는 고정되어 있고 차가 그 위를 움직인다.
// 차가 갈 수 있는지(건물 충돌), 지금 어느 길인지, 땅 높이도 여기서 알려 준다.
// 지도는 연출일 뿐 판단에 쓰지 않는다: 속도·기어는 C 코어 값 그대로이고, 도로 모양은 요구사항과 관계없다.
import * as THREE from 'three';
import { mergeGeometries } from 'three/addons/utils/BufferGeometryUtils.js';
import { tex, lineTexture, glowTexture, leafTexture, makeTreePrototype, makeLamp } from './road.js';

const STEP = 2;                                  // 도로를 나누는 간격 (m)
const ASPHALT_TILE = 4.5, PAVE_TILE = 3, GRASS_TILE = 7;
const DASH_PERIOD = 8, DASH_LEN = 3;             // 시내 차선 점선: 3m 칠, 5m 빈칸
const SIDEWALK = 2.6, CURB_H = 0.13;
const LAMP_GAP = 32, STREET_TREE_GAP = 11;
const MAX_TREES = 1800;
const FLOOR = { w: 3.2, h: 3.3, cells: 8 };      // 건물 창 한 칸 크기 (m), 창 무늬 한 장의 칸 수
// 도로 등급별 높이 순서 (큰 길이 위로): trunk primary secondary tertiary unclassified residential, *_link
const RANK = [6, 5, 4, 3, 2, 1, 4, 4, 3, 3];

const wrapAngle = (a) => Math.atan2(Math.sin(a), Math.cos(a));
const pairs = (flat) => { const out = []; for (let i = 0; i < flat.length; i += 2) out.push([flat[i], flat[i + 1]]); return out; };

// ---- 주행 경로: 닫힌 곡선 위의 거리 s → 위치·방향·곡률 ----
export function makePath(route, names) {
  const p = route.p, n = p.length / 2;
  const X = new Float64Array(n), Z = new Float64Array(n), S = new Float64Array(n + 1);
  const H = new Float64Array(n), K = new Float64Array(n);
  for (let i = 0; i < n; i++) { X[i] = p[i * 2]; Z[i] = p[i * 2 + 1]; }
  for (let i = 0; i < n; i++) { const j = (i + 1) % n; S[i + 1] = S[i] + Math.hypot(X[j] - X[i], Z[j] - Z[i]); }
  const L = S[n];
  for (let i = 0; i < n; i++) {
    const a = (i - 1 + n) % n, b = (i + 1) % n;
    H[i] = Math.atan2(-(X[b] - X[a]), -(Z[b] - Z[a]));      // 차의 rotation.y (앞이 -z)
  }
  const raw = new Float64Array(n);
  for (let i = 0; i < n; i++) {
    const a = (i - 1 + n) % n, b = (i + 1) % n;
    raw[i] = wrapAngle(H[b] - H[a]) / Math.max(0.1, Math.hypot(X[b] - X[a], Z[b] - Z[a]));   // 왼쪽으로 돌면 +
  }
  for (let i = 0; i < n; i++) { let k = 0; for (let d = -4; d <= 4; d++) k += raw[(i + d + n) % n]; K[i] = k / 9; }

  function sample(s) {
    s = ((s % L) + L) % L;
    let lo = 0, hi = n;
    while (hi - lo > 1) { const m = (lo + hi) >> 1; if (S[m] <= s) lo = m; else hi = m; }
    const i = lo, j = (i + 1) % n, t = (s - S[i]) / Math.max(1e-6, S[i + 1] - S[i]);
    return {
      s, x: X[i] + (X[j] - X[i]) * t, z: Z[i] + (Z[j] - Z[i]) * t,
      heading: H[i] + wrapAngle(H[j] - H[i]) * t, curvature: K[i] + (K[j] - K[i]) * t,
      name: names[route.n[i]] || '',
    };
  }
  return { length: L, sample, X, Z };
}

// ---- 기하 도우미 ----
function densify(pts, step) {
  const out = [pts[0]];
  for (let i = 1; i < pts.length; i++) {
    const [x0, z0] = pts[i - 1], [x1, z1] = pts[i];
    const k = Math.max(1, Math.ceil(Math.hypot(x1 - x0, z1 - z0) / step));
    for (let j = 1; j <= k; j++) out.push([x0 + (x1 - x0) * j / k, z0 + (z1 - z0) * j / k]);
  }
  return out;
}

// 각 점의 진행 방향(t)과 오른쪽(r), 처음부터의 거리(s). 오른쪽은 차 기준 +x와 같다
function frames(pts) {
  const n = pts.length, out = new Array(n);
  let s = 0;
  for (let i = 0; i < n; i++) {
    const a = pts[Math.max(0, i - 1)], b = pts[Math.min(n - 1, i + 1)];
    let tx = b[0] - a[0], tz = b[1] - a[1];
    const len = Math.hypot(tx, tz) || 1;
    tx /= len; tz /= len;
    if (i > 0) s += Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]);
    out[i] = { x: pts[i][0], z: pts[i][1], tx, tz, rx: -tz, rz: tx, s };
  }
  return out;
}

// 거리 s에서의 틀 (가장 가까운 점에서 접선 방향으로 밀어 낸다)
function frameAt(f, s) {
  let lo = 0, hi = f.length - 1;
  while (hi - lo > 1) { const m = (lo + hi) >> 1; if (f[m].s <= s) lo = m; else hi = m; }
  const p = (s - f[lo].s) < (f[hi].s - s) ? f[lo] : f[hi];
  const d = s - p.s;
  return { ...p, x: p.x + p.tx * d, z: p.z + p.tz * d, s };
}

function pointInPoly(x, z, ring) {
  let inside = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const [xi, zi] = ring[i], [xj, zj] = ring[j];
    if ((zi > z) !== (zj > z) && x < xi + (z - zi) * (xj - xi) / (zj - zi)) inside = !inside;
  }
  return inside;
}

function makeGrid(cell) {
  const m = new Map();
  const key = (cx, cz) => cx * 100003 + cz;
  return {
    add(x, z, item) { const k = key(Math.floor(x / cell), Math.floor(z / cell)); (m.get(k) ?? m.set(k, []).get(k)).push(item); },
    near(x, z) {
      const cx = Math.floor(x / cell), cz = Math.floor(z / cell), out = [];
      for (let i = -1; i <= 1; i++) for (let j = -1; j <= 1; j++) { const a = m.get(key(cx + i, cz + j)); if (a) for (const v of a) out.push(v); }
      return out;
    },
  };
}

// 위를 보는 띠·벽을 모아 BufferGeometry 하나로
class Builder {
  // h(x, z): 그 자리 땅 높이. 띠·사각형·벽의 y는 땅 위로 얼마나 떠 있는지
  constructor(h = () => 0) { this.h = h; this.pos = []; this.nor = []; this.uv = []; this.idx = []; }
  // 띠: 틀 f를 따라 오른쪽 오프셋 a~b (왼쪽이 -), 높이 y. uScale이 null이면 u는 0~1 (차선 무늬)
  strip(f, a, b, y, uScale, vScale) {
    const base = this.pos.length / 3;
    for (const p of f) {
      const ax = p.x + p.rx * a, az = p.z + p.rz * a, bx = p.x + p.rx * b, bz = p.z + p.rz * b;
      this.pos.push(ax, y + this.h(ax, az), az, bx, y + this.h(bx, bz), bz);
      this.nor.push(0, 1, 0, 0, 1, 0);
      if (uScale === null) this.uv.push(0, p.s * vScale, 1, p.s * vScale);
      else this.uv.push(a * uScale, p.s * vScale, b * uScale, p.s * vScale);
    }
    for (let i = 0; i < f.length - 1; i++) { const k = base + i * 2; this.idx.push(k, k + 1, k + 2, k + 1, k + 3, k + 2); }
  }
  // 한 틀 기준의 직사각형 (횡단보도 줄, 정지선): 오프셋 a~b, 거리 s0~s1
  quad(fr, a, b, s0, s1, y) {
    const base = this.pos.length / 3;
    for (const s of [s0, s1]) {
      const d = s - fr.s;
      for (const o of [a, b]) {
        const x = fr.x + fr.rx * o + fr.tx * d, z = fr.z + fr.rz * o + fr.tz * d;
        this.pos.push(x, y + this.h(x, z), z);
        this.nor.push(0, 1, 0);
        this.uv.push(o === a ? 0 : 1, s / 30);
      }
    }
    this.idx.push(base, base + 1, base + 2, base + 1, base + 3, base + 2);
  }
  // 연석 옆면: 오프셋 off에서 y0~y1. 법선은 도로 쪽
  wall(f, off, y0, y1) {
    const base = this.pos.length / 3, sgn = off > 0 ? -1 : 1;
    for (const p of f) {
      const x = p.x + p.rx * off, z = p.z + p.rz * off;
      const g = this.h(x, z);
      this.pos.push(x, y0 + g, z, x, y1 + g, z);
      this.nor.push(p.rx * sgn, 0, p.rz * sgn, p.rx * sgn, 0, p.rz * sgn);
      this.uv.push(0, p.s / PAVE_TILE, 1, p.s / PAVE_TILE);
    }
    for (let i = 0; i < f.length - 1; i++) { const k = base + i * 2; this.idx.push(k, k + 1, k + 2, k + 1, k + 3, k + 2); }
  }
  geometry() {
    const g = new THREE.BufferGeometry();
    g.setAttribute('position', new THREE.Float32BufferAttribute(this.pos, 3));
    g.setAttribute('normal', new THREE.Float32BufferAttribute(this.nor, 3));
    g.setAttribute('uv', new THREE.Float32BufferAttribute(this.uv, 2));
    g.setIndex(this.idx);
    return g;
  }
}

// 조건을 만족하는 연속 구간들 [시작, 끝] (점 2개 이상)
function runs(ok) {
  const out = [];
  let a = -1;
  for (let i = 0; i <= ok.length; i++) {
    if (i < ok.length && ok[i]) { if (a < 0) a = i; } else if (a >= 0) { if (i - 1 > a) out.push([a, i - 1]); a = -1; }
  }
  return out;
}

// ExtrudeGeometry를 재질 묶음(0 지붕·바닥, 1 벽)별로 나눈다
function splitGroups(geo) {
  const out = [[], []];
  for (const g of geo.groups) {
    const part = new THREE.BufferGeometry();
    for (const name of ['position', 'normal', 'uv']) {
      const a = geo.attributes[name];
      part.setAttribute(name, new THREE.BufferAttribute(a.array.slice(g.start * a.itemSize, (g.start + g.count) * a.itemSize), a.itemSize));
    }
    out[g.materialIndex].push(part);
  }
  return out.map((list) => (list.length > 1 ? mergeGeometries(list) : list[0]));
}

function paint(geo, color) {
  const n = geo.attributes.position.count, c = new Float32Array(n * 3);
  for (let i = 0; i < n; i++) c.set([color.r, color.g, color.b], i * 3);
  geo.setAttribute('color', new THREE.BufferAttribute(c, 3));
  return geo;
}

// 건물 벽 무늬: 칸마다 창 하나. lit=true면 밤에 켜진 창만 (발광 맵)
function windowTexture(lit) {
  const cv = document.createElement('canvas');
  const C = 32, N = FLOOR.cells;
  cv.width = cv.height = C * N;
  const g = cv.getContext('2d');
  g.fillStyle = lit ? '#000' : '#fff';
  g.fillRect(0, 0, cv.width, cv.height);
  let seed = lit ? 91 : 37;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
  let seed2 = 5;
  const rnd2 = () => ((seed2 = (seed2 * 16807) % 2147483647) / 2147483647);   // 창 배치는 두 장이 같아야 한다
  for (let r = 0; r < N; r++) {
    for (let c = 0; c < N; c++) {
      if (rnd2() < 0.12) continue;                      // 창 없는 칸
      const x = c * C + 5, y = r * C + 8, w = C - 10, h = C - 15;
      if (lit) {
        if (rnd() < 0.62) continue;                     // 꺼진 창
        const v = 150 + rnd() * 105;
        g.fillStyle = `rgb(${v},${v * 0.86},${v * 0.62})`;
      } else {
        const grd = g.createLinearGradient(x, y, x + w, y + h);
        grd.addColorStop(0, '#5d6b78'); grd.addColorStop(1, '#2c343d');
        g.fillStyle = grd;
      }
      g.fillRect(x, y, w, h);
    }
  }
  const t = new THREE.CanvasTexture(cv);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.repeat.set(1 / (FLOOR.w * N), 1 / (FLOOR.h * N));
  if (!lit) t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

// ---- 지형: 높이 격자(cm, base64 Int16) → 높이 함수와 메시 ----
export function makeTerrain(t) {
  if (!t) return { heightAt: () => 0, contains: () => true, geo: null, lowest: 0 };
  const bin = atob(t.cm);
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
  const cm = new Int16Array(bytes.buffer);
  const { nx, ny, step, x0, n0 } = t;
  const at = (i, j) => cm[j * nx + i] / 100;
  // 화면 좌표 z = -북쪽
  function heightAt(x, z) {
    const fi = THREE.MathUtils.clamp((x - x0) / step, 0, nx - 1.001), fj = THREE.MathUtils.clamp((-z - n0) / step, 0, ny - 1.001);
    const i = Math.floor(fi), j = Math.floor(fj), u = fi - i, v = fj - j;
    return (at(i, j) * (1 - u) + at(i + 1, j) * u) * (1 - v) + (at(i, j + 1) * (1 - u) + at(i + 1, j + 1) * u) * v;
  }
  const xMax = x0 + (nx - 1) * step, nMax = n0 + (ny - 1) * step;
  const contains = (x, z, m = 0) => x > x0 + m && x < xMax - m && -z > n0 + m && -z < nMax - m;
  // 메시: 격자점마다 꼭짓점. uv = 바닥 무늬(6m 반복), uv1 = 격자 전체 0~1 (공원·물 마스크)
  const pos = new Float32Array(nx * ny * 3), uv = new Float32Array(nx * ny * 2), uv1 = new Float32Array(nx * ny * 2);
  for (let j = 0; j < ny; j++) {
    for (let i = 0; i < nx; i++) {
      const k = j * nx + i, x = x0 + i * step, z = -(n0 + j * step);
      pos.set([x, at(i, j), z], k * 3);
      uv.set([x / 6, z / 6], k * 2);
      uv1.set([i / (nx - 1), j / (ny - 1)], k * 2);
    }
  }
  const idx = [];
  for (let j = 0; j < ny - 1; j++) {
    for (let i = 0; i < nx - 1; i++) {
      const a = j * nx + i, b = a + 1, c = a + nx, d = c + 1;
      idx.push(a, b, c, b, d, c);
    }
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  geo.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
  geo.setAttribute('uv1', new THREE.BufferAttribute(uv1, 2));
  geo.setIndex(idx);
  geo.computeVertexNormals();
  let lowest = Infinity;
  for (let i = 0; i < cm.length; i++) lowest = Math.min(lowest, cm[i] / 100);
  return { heightAt, contains, geo, lowest, extent: { x0, n0, xMax, nMax } };
}

// 공원·물 마스크: 지형 격자 전체를 덮는 캔버스에 다각형을 흰색으로 (alphaMap은 초록 채널을 쓴다)
function maskTexture(polys, ext) {
  const W = 1024, Hpx = Math.round(W * (ext.nMax - ext.n0) / (ext.xMax - ext.x0));
  const cv = document.createElement('canvas');
  cv.width = W; cv.height = Hpx;
  const g = cv.getContext('2d');
  g.fillStyle = '#000'; g.fillRect(0, 0, W, Hpx);
  g.fillStyle = '#fff';
  for (const ring of polys) {
    g.beginPath();
    ring.forEach(([x, z], i) => {
      const u = (x - ext.x0) / (ext.xMax - ext.x0), v = (-z - ext.n0) / (ext.nMax - ext.n0);
      g[i ? 'lineTo' : 'moveTo'](u * W, (1 - v) * Hpx);
    });
    g.closePath();
    g.fill();
  }
  const t = new THREE.CanvasTexture(cv);
  t.channel = 1;
  return t;
}

export function createCity(scene, data) {
  const loader = new THREE.TextureLoader();
  const path = makePath(data.route, data.names);
  const terrain = makeTerrain(data.terrain);
  const H = terrain.heightAt;

  // ---- 위치 찾기 도우미: 교차로까지 거리, 경로까지 거리, 건물 안인지 ----
  const jgrid = makeGrid(40);
  for (let i = 0; i < data.junctions.length; i += 3) {
    const j = { x: data.junctions[i], z: data.junctions[i + 1], r: data.junctions[i + 2] };
    jgrid.add(j.x, j.z, j);
  }
  const jDist = (x, z) => { let d = 99; for (const j of jgrid.near(x, z)) d = Math.min(d, Math.hypot(x - j.x, z - j.z) - j.r); return d; };
  const coarse = [];
  for (let s = 0; s < path.length; s += 10) coarse.push(path.sample(s));
  const routeDist = (x, z) => { let d = Infinity; for (const p of coarse) d = Math.min(d, Math.abs(p.x - x) + Math.abs(p.z - z)); return d; };
  // 건물은 바닥 외곽선이 걸치는 모든 칸에 넣는다 (큰 건물도 빠짐없이 찾게)
  const BCELL = 40;
  const bgrid = makeGrid(BCELL);
  const rings = data.buildings.map((b) => pairs(b.p));
  rings.forEach((ring) => {
    const xs = ring.map((p) => p[0]), zs = ring.map((p) => p[1]);
    const box = { x0: Math.min(...xs), x1: Math.max(...xs), z0: Math.min(...zs), z1: Math.max(...zs) };
    const item = { ring, box };
    for (let cx = Math.floor(box.x0 / BCELL); cx <= Math.floor(box.x1 / BCELL); cx++) {
      for (let cz = Math.floor(box.z0 / BCELL); cz <= Math.floor(box.z1 / BCELL); cz++) bgrid.add(cx * BCELL + 1, cz * BCELL + 1, item);
    }
  });
  const buildingsNear = (x, z) => [...new Set(bgrid.near(x, z))];
  const inBuilding = (x, z) => buildingsNear(x, z).some((b) => pointInPoly(x, z, b.ring));

  // ---- 재질 ----
  const aMap = tex(loader, 'assets/tex/asphalt_diff.jpg', 1, 1, true);
  const aNor = tex(loader, 'assets/tex/asphalt_nor.jpg', 1, 1);
  const aRough = tex(loader, 'assets/tex/asphalt_rough.jpg', 1, 1);
  const gMap = tex(loader, 'assets/tex/grass_diff.jpg', 1, 1, true);
  const gNor = tex(loader, 'assets/tex/grass_nor.jpg', 1, 1);
  const asphalt = new THREE.MeshPhysicalMaterial({ map: aMap, normalMap: aNor, roughnessMap: aRough, roughness: 1,
    color: 0x9a9a9a, clearcoat: 0, clearcoatRoughness: 0.06, polygonOffset: true, polygonOffsetFactor: -1, polygonOffsetUnits: -2 });
  const paveBase = new THREE.Color(0xd9d2c6);
  const pave = new THREE.MeshStandardMaterial({ map: aMap, normalMap: aNor, color: paveBase.clone(), roughness: 0.95 });
  const groundBase = new THREE.Color(0xa59f95);
  const groundMat = new THREE.MeshStandardMaterial({ map: aMap, normalMap: aNor, color: groundBase.clone(), roughness: 1 });
  const curbMat = new THREE.MeshStandardMaterial({ color: 0xa3a19b, roughness: 0.9, side: THREE.DoubleSide });
  const grassMat = new THREE.MeshStandardMaterial({ map: gMap, normalMap: gNor, color: 0xa8b48a, roughness: 1,
    polygonOffset: true, polygonOffsetFactor: -0.5, polygonOffsetUnits: -1 });
  const waterMat = new THREE.MeshStandardMaterial({ color: 0x334c5a, roughness: 0.12, metalness: 0.2,
    polygonOffset: true, polygonOffsetFactor: -0.8, polygonOffsetUnits: -1.5 });
  const solidTex = lineTexture(0), dashTex = lineTexture(DASH_LEN / DASH_PERIOD);
  const lineMat = (color, map) => new THREE.MeshStandardMaterial({ color, map, alphaMap: map, transparent: true, roughness: 0.55,
    polygonOffset: true, polygonOffsetFactor: -4, polygonOffsetUnits: -4, depthWrite: false });
  const lineMats = [lineMat(0xf2f2f2, solidTex), lineMat(0xe7b416, solidTex), lineMat(0xf2f2f2, dashTex)];

  // ---- 땅: 실제 고도 지형. 도로·공원 밖은 포장된 땅, 지형 밖은 안개 속 평지 ----
  if (terrain.geo) {
    const ground = new THREE.Mesh(terrain.geo, groundMat);
    ground.receiveShadow = true;
    scene.add(ground);
  }
  const farGeo = new THREE.PlaneGeometry(9000, 9000);
  farGeo.rotateX(-Math.PI / 2);
  const far = new THREE.Mesh(farGeo, terrain.geo ? new THREE.MeshStandardMaterial({ color: 0x6f6b64, roughness: 1 }) : groundMat);
  far.position.y = terrain.lowest - (terrain.geo ? 0.5 : 0);
  far.receiveShadow = !terrain.geo;
  scene.add(far);

  // ---- 도로 ----
  // 다른 도로 노면 위인지 (가로등·가로수·보도가 옆 도로나 진입로 위에 놓이지 않게)
  const roadFrames = data.roads.map((r) => frames(densify(pairs(r.p), STEP)));
  const rgrid = makeGrid(30);
  roadFrames.forEach((f, ri) => { const hw = data.roads[ri].w / 2; for (const p of f) rgrid.add(p.x, p.z, { x: p.x, z: p.z, hw, ri }); });
  const onOtherRoad = (x, z, ri, margin = 1.0) => rgrid.near(x, z).some((p) => p.ri !== ri && Math.hypot(x - p.x, z - p.z) < p.hw + margin);

  const B = { road: new Builder(H), pave: new Builder(H), curb: new Builder(H), white: new Builder(H), yellow: new Builder(H), dash: new Builder(H) };
  const streetTrees = [], lamps = [];
  data.roads.forEach((r, ri) => {
    const f = roadFrames[ri];
    const hw = r.w / 2, link = r.c >= 6, major = r.c <= 3;
    const y = 0.02 + RANK[r.c] * 0.004 + (ri % 3) * 0.001;
    B.road.strip(f, -hw, hw, y, 1 / ASPHALT_TILE, 1 / ASPHALT_TILE);
    if (!major && !link) return;                       // 골목은 노면만
    const d = f.map((p) => jDist(p.x, p.z));
    const ly = y + 0.006;

    // 차선: 교차로 안에서는 긋지 않는다
    const lw = r.w / r.l;
    const line = (bld, seg, off, w = 0.15, period = 30) => bld.strip(seg, off - w / 2, off + w / 2, ly, null, 1 / period);
    for (const [a, b] of runs(d.map((v) => v > 6))) {
      const seg = f.slice(a, b + 1);
      if (link) { line(B.white, seg, -hw + 0.2); line(B.white, seg, hw - 0.2); continue; }
      if (r.o) {
        line(r.c <= 2 ? B.yellow : B.white, seg, -hw + 0.2);   // 중앙분리 쪽은 노란 실선
        line(B.white, seg, hw - 0.2);
        for (let k = 1; k < r.l; k++) line(B.dash, seg, -hw + k * lw, 0.15, DASH_PERIOD);
      } else {
        if (r.l >= 2) { line(B.yellow, seg, -0.12, 0.13); line(B.yellow, seg, 0.12, 0.13); }
        for (let k = 1; k < Math.floor(r.l / 2); k++) { line(B.dash, seg, k * lw, 0.15, DASH_PERIOD); line(B.dash, seg, -k * lw, 0.15, DASH_PERIOD); }
        line(B.white, seg, -hw + 0.2); line(B.white, seg, hw - 0.2);
      }
    }
    if (link) return;

    // 횡단보도 + 정지선: 교차로 경계에서 도로 쪽으로
    if (r.w >= 5) {
      for (let i = 1; i < f.length; i++) {
        const inPrev = d[i - 1] <= 0, inCur = d[i] <= 0;
        if (inPrev === inCur) continue;
        const t = d[i - 1] / (d[i - 1] - d[i]);
        const sb = f[i - 1].s + (f[i].s - f[i - 1].s) * t;
        const leaving = inPrev && !inCur, dir = leaving ? 1 : -1;
        const c = sb + dir * 2.4;
        if (c < f[0].s + 1 || c > f[f.length - 1].s - 1) continue;
        const fr = frameAt(f, c);
        for (let o = -hw + 0.35; o + 0.5 <= hw - 0.35; o += 1.0) B.white.quad(fr, o, o + 0.5, c - 1.7, c + 1.7, ly);
        const sl = sb + dir * 4.9;
        if (sl < f[0].s || sl > f[f.length - 1].s) continue;
        const fs = frameAt(f, sl);
        // 정지선은 교차로로 들어가는 쪽 차로에만 (오른쪽 통행)
        if (r.o) { if (!leaving) B.white.quad(fs, -hw + 0.2, hw - 0.2, sl - 0.25, sl + 0.25, ly); }
        else if (leaving) B.white.quad(fs, -hw + 0.2, 0, sl - 0.25, sl + 0.25, ly);
        else B.white.quad(fs, 0, hw - 0.2, sl - 0.25, sl + 0.25, ly);
      }
    }

    // 보도 (높이 CURB_H) + 연석. 일방통행 차도(중앙분리대가 있는 큰길의 한쪽)는 바깥쪽만
    const sides = r.o ? [1] : [-1, 1];
    for (const side of sides) {
      const mid = side * (hw + SIDEWALK / 2);
      const ok = f.map((p, i) => d[i] > 2.5 && !onOtherRoad(p.x + p.rx * mid, p.z + p.rz * mid, ri, 0.3));
      for (const [a, b] of runs(ok)) {
        const seg = f.slice(a, b + 1);
        const edge = side * hw, outer = side * (hw + SIDEWALK);
        B.pave.strip(seg, Math.min(edge, outer), Math.max(edge, outer), CURB_H, 1 / PAVE_TILE, 1 / PAVE_TILE);
        B.curb.wall(seg, edge, 0, CURB_H + 0.002);
      }
    }
    // 가로수·가로등: 경로 근처 큰길의 보도 위
    for (const side of sides) {
      let nextTree = 5 + (ri % 7), nextLamp = 12 + (ri % 11);
      for (let i = 0; i < f.length; i++) {
        const p = f[i];
        if (p.s >= nextTree) {
          nextTree = p.s + STREET_TREE_GAP;
          const o = side * (hw + 1.4), x = p.x + p.rx * o, z = p.z + p.rz * o;
          if (d[i] > 8 && routeDist(x, z) < 380 && !onOtherRoad(x, z, ri) && !inBuilding(x, z)) streetTrees.push({ x, z, kind: (ri + i) % 2 });
        }
        if (p.s >= nextLamp) {
          nextLamp = p.s + LAMP_GAP;
          const o = side * (hw + 0.5), x = p.x + p.rx * o, z = p.z + p.rz * o;
          if (d[i] > 4 && routeDist(x, z) < 260 && !onOtherRoad(x, z, ri, 3.2)) {   // 팔(2.85m)까지 다른 길 위로 나가지 않게
            const heading = Math.atan2(-p.tx, -p.tz) + (side > 0 ? 0 : Math.PI);   // 팔(-x)이 도로 쪽으로
            lamps.push({ x, z, heading });
          }
        }
      }
    }
  });
  const roadMesh = new THREE.Mesh(B.road.geometry(), asphalt);
  roadMesh.receiveShadow = true;
  const paveMesh = new THREE.Mesh(B.pave.geometry(), pave);
  paveMesh.receiveShadow = true;
  scene.add(roadMesh, paveMesh, new THREE.Mesh(B.curb.geometry(), curbMat));
  [B.white, B.yellow, B.dash].forEach((b, i) => { const m = new THREE.Mesh(b.geometry(), lineMats[i]); m.receiveShadow = true; scene.add(m); });

  // ---- 공원·물: 지형 위에 마스크로 칠한다 (지형을 그대로 따라감) ----
  const parkPolys = [], grassRings = [], waterRings = [];
  for (const gpoly of data.green) {
    const ring = pairs(gpoly.p);
    (gpoly.k === 'water' ? waterRings : grassRings).push(ring);
    if (gpoly.k === 'wood' || gpoly.k === 'park') parkPolys.push({ ring, density: gpoly.k === 'wood' ? 1 / 45 : 1 / 170 });
  }
  if (terrain.geo) {
    for (const [list, mat] of [[grassRings, grassMat], [waterRings, waterMat]]) {
      if (!list.length) continue;
      mat.alphaMap = maskTexture(list, terrain.extent);
      mat.alphaTest = 0.5;
      const m = new THREE.Mesh(terrain.geo, mat);
      m.receiveShadow = true;
      scene.add(m);
    }
  }

  // ---- 건물: 바닥 외곽선을 높이만큼 세운다. 벽은 창 무늬, 밤에는 일부 창이 켜진다 ----
  const walls = [], roofs = [];
  const col = new THREE.Color();
  let seed = 17;
  const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
  data.buildings.forEach((b, i) => {
    // 비탈에 선 건물: 가장 낮은 모서리 조금 아래부터 세운다 (지붕은 가장 높은 모서리 + 높이)
    const hs = rings[i].map(([x, z]) => H(x, z));
    const lo = Math.min(...hs), hi = Math.max(...hs);
    const geo = new THREE.ExtrudeGeometry(new THREE.Shape(rings[i].map(([x, z]) => new THREE.Vector2(x, -z))),
      { depth: b.h + (hi - lo) + 0.4, bevelEnabled: false });
    geo.rotateX(-Math.PI / 2);
    geo.translate(0, lo - 0.4, 0);
    col.setHSL(0.07 + rnd() * 0.06, 0.04 + rnd() * 0.12, 0.6 + rnd() * 0.25);
    const [roof, wall] = splitGroups(geo);
    geo.dispose();
    if (roof) roofs.push(paint(roof, col));
    if (wall) walls.push(paint(wall, col));
  });
  const wallMat = new THREE.MeshStandardMaterial({ map: windowTexture(false), emissiveMap: windowTexture(true),
    emissive: 0xffd2a0, emissiveIntensity: 0, vertexColors: true, roughness: 0.82 });
  const roofMat = new THREE.MeshStandardMaterial({ color: 0x9a958e, vertexColors: true, roughness: 0.95 });
  if (walls.length) scene.add(new THREE.Mesh(mergeGeometries(walls), wallMat));
  if (roofs.length) scene.add(new THREE.Mesh(mergeGeometries(roofs), roofMat));

  // ---- 나무: 가로수 먼저, 남은 수만큼 경로에 가까운 공원 나무 ----
  const parkTrees = [];
  let ts = 4242;
  const trnd = () => ((ts = (ts * 16807) % 2147483647) / 2147483647);
  for (const { ring, density } of parkPolys) {
    const xs = ring.map((p) => p[0]), zs = ring.map((p) => p[1]);
    const x0 = Math.min(...xs), x1 = Math.max(...xs), z0 = Math.min(...zs), z1 = Math.max(...zs);
    let area = 0;
    for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) area += (ring[j][0] + ring[i][0]) * (ring[j][1] - ring[i][1]);
    const want = Math.min(4000, Math.abs(area / 2) * density);
    for (let k = 0, placed = 0; k < want * 4 && placed < want; k++) {
      const x = x0 + trnd() * (x1 - x0), z = z0 + trnd() * (z1 - z0);
      if (!pointInPoly(x, z, ring)) continue;
      placed++;
      const dr = routeDist(x, z);
      if (dr < 12 || dr > 450 || jDist(x, z) < 6 || onOtherRoad(x, z, -1, 1.5) || inBuilding(x, z)) continue;
      parkTrees.push({ x, z, kind: trnd() < 0.3 ? 2 : (trnd() < 0.5 ? 0 : 1), dr });
    }
  }
  parkTrees.sort((a, b) => a.dr - b.dr);
  const trees = streetTrees.slice(0, MAX_TREES).concat(parkTrees.slice(0, Math.max(0, MAX_TREES - streetTrees.length)));
  const leafMats = [leafTexture(false), leafTexture(true)].map((map) => new THREE.MeshStandardMaterial({
    map, alphaTest: 0.45, side: THREE.DoubleSide, roughness: 0.85 }));
  const bark = new THREE.MeshStandardMaterial({ color: 0x4b3c2e, roughness: 1 });
  const protos = [makeTreePrototype('broad', 13), makeTreePrototype('broad', 29), makeTreePrototype('conifer', 47)];
  const mtx = new THREE.Matrix4(), q = new THREE.Quaternion(), up = new THREE.Vector3(0, 1, 0);
  const v3 = new THREE.Vector3(), scl = new THREE.Vector3();
  protos.forEach((p, i) => {
    const list = trees.filter((t) => t.kind === i);
    if (!list.length) return;
    const leaf = new THREE.InstancedMesh(p.leaves, leafMats[i === 2 ? 1 : 0], list.length);
    const wood = new THREE.InstancedMesh(p.wood, bark, list.length);
    list.forEach((t, k) => {
      mtx.compose(v3.set(t.x, H(t.x, t.z) - 0.05, t.z), q.setFromAxisAngle(up, trnd() * 6.28), scl.setScalar(0.8 + trnd() * 0.6));
      leaf.setMatrixAt(k, mtx); wood.setMatrixAt(k, mtx);
      leaf.setColorAt(k, col.setHSL(0, 0, 0.75 + trnd() * 0.35));
    });
    scene.add(leaf, wood);
  });

  // ---- 가로등: 부품을 재질별로 합쳐 인스턴스로. 밤에 머리가 빛나고 노면에 빛 웅덩이 ----
  const metal = new THREE.MeshStandardMaterial({ color: 0xa9afb5, metalness: 0.85, roughness: 0.42 });
  const headMat = new THREE.MeshStandardMaterial({ color: 0x777777, emissive: 0xffe2b8, emissiveIntensity: 0 });
  const proto = makeLamp(metal, headMat);
  proto.updateMatrixWorld(true);
  const parts = new Map();
  proto.traverse((o) => {
    if (!o.isMesh) return;
    const g = o.geometry.clone().applyMatrix4(o.matrixWorld);
    const flatGeo = g.index ? g.toNonIndexed() : g;
    (parts.get(o.material) ?? parts.set(o.material, []).get(o.material)).push(flatGeo);
  });
  const poolMat = new THREE.MeshBasicMaterial({ map: glowTexture(), color: 0xffd9a8, transparent: true, opacity: 0,
    blending: THREE.AdditiveBlending, depthWrite: false, fog: true });
  if (lamps.length) {
    for (const [mat, list] of parts) {
      const im = new THREE.InstancedMesh(mergeGeometries(list), mat, lamps.length);
      lamps.forEach((l, k) => im.setMatrixAt(k, mtx.compose(v3.set(l.x, H(l.x, l.z) + CURB_H, l.z), q.setFromAxisAngle(up, l.heading), scl.setScalar(1))));
      scene.add(im);
    }
    const poolGeo = new THREE.PlaneGeometry(14, 14);
    poolGeo.rotateX(-Math.PI / 2);
    const pools = new THREE.InstancedMesh(poolGeo, poolMat, lamps.length);
    lamps.forEach((l, k) => {
      const ax = -Math.cos(l.heading) * 2.85, az = Math.sin(l.heading) * 2.85;   // 팔 끝 (로컬 -x)
      pools.setMatrixAt(k, mtx.compose(v3.set(l.x + ax, H(l.x + ax, l.z + az) + 0.08, l.z + az), q.identity(), scl.setScalar(1)));
    });
    scene.add(pools);
  }

  // dark: 0(낮)~1(밤), wet: 0~1
  function update(dark, wet) {
    headMat.emissiveIntensity = dark ** 2 * 6;
    poolMat.opacity = dark ** 2 * 0.22;
    wallMat.emissiveIntensity = dark ** 1.5 * 1.3;
    asphalt.clearcoat = wet * 0.9;
    asphalt.roughness = 1 - wet * 0.45;
    asphalt.color.setScalar(0.42 - wet * 0.17);
    for (const m of lineMats) m.roughness = 0.55 - wet * 0.35;
    pave.color.copy(paveBase).multiplyScalar(1 - wet * 0.3);
    pave.roughness = 0.95 - wet * 0.4;
    groundMat.color.copy(groundBase).multiplyScalar(1 - wet * 0.3);
  }

  // ---- 차가 갈 수 있는지: 차 바닥 직사각형(앞이 -z)이 건물과 겹치면 충돌. 지형 밖도 막는다 ----
  function collide(x, z, heading, halfL = 2.3, halfW = 0.95) {
    if (!terrain.contains(x, z, 20)) return 'edge';
    const c = Math.cos(heading), sn = Math.sin(heading);
    const W = (lx, lz) => [x + lx * c + lz * sn, z - lx * sn + lz * c];   // 차 좌표(오른쪽 lx, 뒤 lz) → 세상
    const probes = [];
    for (const lz of [-halfL, -halfL / 2, 0, halfL / 2, halfL]) for (const lx of [-halfW, halfW]) probes.push(W(lx, lz));
    probes.push(W(0, -halfL), W(0, halfL));
    for (const b of buildingsNear(x, z)) {
      if (x < b.box.x0 - 6 || x > b.box.x1 + 6 || z < b.box.z0 - 6 || z > b.box.z1 + 6) continue;
      if (probes.some(([px, pz]) => pointInPoly(px, pz, b.ring))) return 'building';
      for (const [bx, bz] of b.ring) {                       // 차보다 작은 건물 모서리가 차 안으로 들어온 경우
        const dx = bx - x, dz = bz - z;
        const lx = dx * c - dz * sn, lz = dx * sn + dz * c;
        if (Math.abs(lx) < halfW && Math.abs(lz) < halfL) return 'building';
      }
    }
    return null;
  }
  // 지금 자리의 도로 이름 (도로 밖이면 '')
  function roadAt(x, z) {
    let best = null, bd = Infinity;
    for (const p of rgrid.near(x, z)) {
      const d = Math.hypot(x - p.x, z - p.z) - p.hw;
      if (d < bd) { bd = d; best = p; }
    }
    return best && bd < 1.5 ? data.names[data.roads[best.ri].n] || '이름 없는 길' : '';
  }

  // ---- 경로 점마다 차로 범위 (레이더 실험의 교통 차량용, 경로 기준 y 왼쪽 +) ----
  // [같은 방향 오른쪽 끝, 같은 방향 왼쪽 끝, 반대 방향 왼쪽 끝, 차로 폭]. 양방향이면 가운데선까지가 같은 방향
  function laneTable() {
    const n = path.X.length, out = new Float32Array(n * 4);
    const raw = [];
    for (let i = 0; i < n; i++) {
      const a = (i - 1 + n) % n, b = (i + 1) % n;
      let fx = path.X[b] - path.X[a], fz = path.Z[b] - path.Z[a];
      const fl = Math.hypot(fx, fz) || 1; fx /= fl; fz /= fl;
      const rx = -fz, rz = fx;                       // 경로 오른쪽
      const px = path.X[i], pz = path.Z[i];
      const want = data.route.n[i];
      let best = null, bd = Infinity, bestAny = null, bdAny = Infinity;
      for (const q of rgrid.near(px, pz)) {
        const d = Math.hypot(px - q.x, pz - q.z);
        if (d < bdAny) { bdAny = d; bestAny = q; }
        if (data.roads[q.ri].n === want && d < bd) { bd = d; best = q; }
      }
      const q = best || bestAny;
      if (!q || Math.hypot(px - q.x, pz - q.z) > q.hw + 1) { raw.push(null); continue; }
      const road = data.roads[q.ri], hw = road.w / 2, lw = road.w / Math.max(1, road.l);
      const off = (px - q.x) * rx + (pz - q.z) * rz;   // 경로가 도로 중심선보다 오른쪽으로 몇 m
      raw.push(road.o ? [off - hw, off + hw, off + hw, lw] : [off - hw, off, off + hw, lw]);
    }
    // 교차로에서 다른 길을 잡는 튐을 줄인다 (앞뒤 5점 중앙값), 못 찾은 곳은 내 차로만
    for (let i = 0; i < n; i++) {
      for (let k = 0; k < 4; k++) {
        const vals = [];
        for (let d = -5; d <= 5; d++) { const r = raw[(i + d + n) % n]; if (r) vals.push(r[k]); }
        vals.sort((x, y) => x - y);
        out[i * 4 + k] = vals.length ? vals[vals.length >> 1] : [-1.5, 1.5, 1.5, 3.0][k];
      }
    }
    return out;
  }

  const counts = { roads: data.roads.length, buildings: data.buildings.length, trees: trees.length, lamps: lamps.length };
  return { path, update, counts, heightAt: H, collide, roadAt, laneTable };
}
