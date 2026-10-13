// loft.js - 단면(초타원)을 z 방향으로 이어 붙여 곡면을 만든다. 차체·유리·도어가 같은 곡면을 나눠 갖는다.
import * as THREE from 'three';

// 넘침 없는 매끄러운 보간 (Fritsch–Carlson). keys: [[x, y], ...] x 오름차순
export function monotone(keys) {
  const n = keys.length, xs = keys.map((k) => k[0]), ys = keys.map((k) => k[1]);
  const d = [], m = new Array(n);
  for (let i = 0; i < n - 1; i++) d.push((ys[i + 1] - ys[i]) / (xs[i + 1] - xs[i]));
  m[0] = d[0]; m[n - 1] = d[n - 2];
  for (let i = 1; i < n - 1; i++) m[i] = d[i - 1] * d[i] <= 0 ? 0 : (d[i - 1] + d[i]) / 2;
  for (let i = 0; i < n - 1; i++) {
    if (d[i] === 0) { m[i] = 0; m[i + 1] = 0; continue; }
    const a = m[i] / d[i], b = m[i + 1] / d[i], s = a * a + b * b;
    if (s > 9) { const t = 3 / Math.sqrt(s); m[i] = t * a * d[i]; m[i + 1] = t * b * d[i]; }
  }
  return (x) => {
    if (x <= xs[0]) return ys[0];
    if (x >= xs[n - 1]) return ys[n - 1];
    let i = 0;
    while (x > xs[i + 1]) i++;
    const h = xs[i + 1] - xs[i], t = (x - xs[i]) / h, t2 = t * t, t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * ys[i] + (t3 - 2 * t2 + t) * h * m[i]
      + (-2 * t3 + 3 * t2) * ys[i + 1] + (t3 - t2) * h * m[i + 1];
  };
}

// 단위 초타원 위의 점. n이 클수록 네모에 가깝다
export function superellipse(a, n) {
  const c = Math.cos(a), s = Math.sin(a), e = 2 / n;
  return [Math.sign(c) * Math.abs(c) ** e, Math.sign(s) * Math.abs(s) ** e];
}

// 둘레 길이가 고르게 나뉘는 각도 count+1개 (폭 1, 높이 aspect인 대표 단면 기준)
export function evenAngles(count, n, aspect, from, to) {
  const N = 4000, ang = [], len = [0];
  let [px, py] = superellipse(from, n);
  for (let i = 0; i <= N; i++) {
    const a = from + (to - from) * (i / N);
    const [x, y] = superellipse(a, n);
    if (i > 0) len.push(len[i - 1] + Math.hypot(x - px, (y - py) * aspect));
    ang.push(a); px = x; py = y;
  }
  const total = len[N], out = [];
  let k = 0;
  for (let j = 0; j <= count; j++) {
    const want = total * (j / count);
    while (k < N && len[k + 1] < want) k++;
    const f = len[k + 1] > len[k] ? (want - len[k]) / (len[k + 1] - len[k]) : 0;
    out.push(ang[k] + (ang[Math.min(k + 1, N)] - ang[k]) * f);
  }
  return out;
}

// 양 끝이 촘촘한 z 위치 + 꼭 넣어야 할 위치(도어 틈, 휠아치 끝)
export function stations(z0, z1, count, extra = []) {
  const s = [];
  for (let i = 0; i <= count; i++) s.push(z0 + (z1 - z0) * (0.5 - 0.5 * Math.cos(Math.PI * (i / count))));
  for (const z of extra) if (z > z0 && z < z1) s.push(z);
  s.sort((a, b) => a - b);
  return s.filter((z, i) => i === 0 || z - s[i - 1] > 1e-4);
}

// 곡면 만들기. point(z, a) → [x, y, z]. inside(c) → 안쪽 점(바깥 방향 판정용).
// regions: 함수 배열. 각 함수 (중심점, 바깥 법선) → 재질 번호, -1이면 그 곡면에서 뺀다. 함수마다 BufferGeometry 하나를 돌려준다.
export function loft({ zs, angles, point, inside, regions, materials }) {
  const S = zs.length, A = angles.length;
  const pos = new Float32Array(S * A * 3), uv = new Float32Array(S * A * 2);
  for (let i = 0; i < S; i++) {
    for (let j = 0; j < A; j++) {
      const k = i * A + j;
      pos.set(point(zs[i], angles[j]), k * 3);
      uv[k * 2] = i / (S - 1); uv[k * 2 + 1] = j / (A - 1);
    }
  }
  const P = (k) => new THREE.Vector3(pos[k * 3], pos[k * 3 + 1], pos[k * 3 + 2]);
  const quads = [];
  const c = new THREE.Vector3(), n = new THREE.Vector3(), e1 = new THREE.Vector3(), e2 = new THREE.Vector3();
  for (let i = 0; i < S - 1; i++) {
    for (let j = 0; j < A - 1; j++) {
      const a = i * A + j, b = (i + 1) * A + j, cc = (i + 1) * A + j + 1, d = i * A + j + 1;
      const pa = P(a), pb = P(b), pc = P(cc), pd = P(d);
      e1.subVectors(pc, pa); e2.subVectors(pd, pb);
      n.crossVectors(e1, e2);
      const len = n.length();
      if (len < 1e-10) continue;   // 끝에서 한 점으로 모인 곳
      n.divideScalar(len);
      c.copy(pa).add(pb).add(pc).add(pd).multiplyScalar(0.25);
      const flip = n.dot(c.clone().sub(inside(c))) < 0;
      if (flip) n.negate();
      quads.push({ a, b, cc, d, flip, c: c.clone(), n: n.clone() });
    }
  }
  return regions.map((region) => {
    const buckets = materials.map(() => []);
    for (const q of quads) {
      const m = region(q.c, q.n);
      if (m < 0) continue;
      if (q.flip) buckets[m].push(q.a, q.cc, q.b, q.a, q.d, q.cc);
      else buckets[m].push(q.a, q.b, q.cc, q.a, q.cc, q.d);
    }
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(pos.slice(), 3));
    geo.setAttribute('uv', new THREE.BufferAttribute(uv.slice(), 2));
    const index = [];
    buckets.forEach((b, m) => { if (b.length) { geo.addGroup(index.length, b.length, m); index.push(...b); } });
    geo.setIndex(index);
    geo.computeVertexNormals();
    return new THREE.Mesh(geo, materials);
  });
}
