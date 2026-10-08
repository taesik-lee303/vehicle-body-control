// minimap.js - 지도 모드의 작은 지도: 주변 도로, 주행 경로, 차 위치·방향, 지금 달리는 도로 이름.
// 지도 데이터 출처(OpenStreetMap) 표시도 여기서 한다 (ODbL 조건).

const SIZE = 210;          // CSS px
const MARGIN = 70;         // 경로 바깥으로 더 보여 줄 거리 (m)

export function setupMinimap(root, data) {
  root.hidden = false;
  root.querySelector('.mm-attr').textContent = data.attribution;
  const cap = root.querySelector('.mm-cap');
  const cv = root.querySelector('canvas');
  const dpr = Math.min(2, devicePixelRatio || 1);
  cv.width = cv.height = SIZE * dpr;
  cv.style.width = cv.style.height = `${SIZE}px`;
  const g = cv.getContext('2d');

  // 경로를 감싸는 범위에 맞춘다 (위가 북쪽: 화면 아래 = +z = 남쪽)
  const rp = data.route.p;
  let x0 = Infinity, x1 = -Infinity, z0 = Infinity, z1 = -Infinity;
  for (let i = 0; i < rp.length; i += 2) { x0 = Math.min(x0, rp[i]); x1 = Math.max(x1, rp[i]); z0 = Math.min(z0, rp[i + 1]); z1 = Math.max(z1, rp[i + 1]); }
  const span = Math.max(x1 - x0, z1 - z0) + MARGIN * 2;
  const k = (SIZE * dpr) / span, cx = (x0 + x1) / 2, cz = (z0 + z1) / 2;
  const P = (x, z) => [(x - cx) * k + (SIZE * dpr) / 2, (z - cz) * k + (SIZE * dpr) / 2];

  // 움직이지 않는 부분은 한 번만 그려 둔다
  const base = document.createElement('canvas');
  base.width = base.height = SIZE * dpr;
  const b = base.getContext('2d');
  b.fillStyle = 'rgba(20, 24, 31, 0.92)';
  b.fillRect(0, 0, base.width, base.height);
  const poly = (flat) => { b.beginPath(); for (let i = 0; i < flat.length; i += 2) b[i ? 'lineTo' : 'moveTo'](...P(flat[i], flat[i + 1])); b.closePath(); };
  for (const gr of data.green) { poly(gr.p); b.fillStyle = gr.k === 'water' ? '#2b4656' : '#2c4a2e'; b.fill(); }
  b.fillStyle = '#3a3f49';
  for (const bd of data.buildings) { poly(bd.p); b.fill(); }
  b.lineCap = 'round';
  for (const r of data.roads) {
    b.beginPath();
    for (let i = 0; i < r.p.length; i += 2) b[i ? 'lineTo' : 'moveTo'](...P(r.p[i], r.p[i + 1]));
    b.strokeStyle = r.c <= 3 ? '#8b919c' : '#5b616b';
    b.lineWidth = Math.max(0.8 * dpr, r.w * k);
    b.stroke();
  }
  b.beginPath();
  for (let i = 0; i < rp.length; i += 2) b[i ? 'lineTo' : 'moveTo'](...P(rp[i], rp[i + 1]));
  b.closePath();
  b.strokeStyle = 'rgba(77, 163, 255, 0.85)';
  b.lineWidth = 2.2 * dpr;
  b.stroke();

  let last = 0, lastCap = '';
  return {
    // w: world.where() (x, z, heading, name, manual, 자동이면 s·length, 직접 운전이면 odo)
    update(w, now) {
      if (!w || now - last < 100) return;   // 1초에 10번이면 충분
      last = now;
      g.drawImage(base, 0, 0);
      const [px, py] = P(w.x, w.z);
      const fx = -Math.sin(w.heading), fz = -Math.cos(w.heading);   // 차 앞 방향
      const s = 7 * dpr;
      g.beginPath();
      g.moveTo(px + fx * s, py + fz * s);
      g.lineTo(px - fx * s * 0.6 + fz * s * 0.55, py - fz * s * 0.6 - fx * s * 0.55);
      g.lineTo(px - fx * s * 0.6 - fz * s * 0.55, py - fz * s * 0.6 + fx * s * 0.55);
      g.closePath();
      g.fillStyle = '#ffd34d';
      g.strokeStyle = '#000';
      g.lineWidth = 1.5 * dpr;
      g.fill(); g.stroke();
      const text = w.manual
        ? `${w.name || '도로 밖'} · 직접 운전 ${(w.odo / 1000).toFixed(2)} km`
        : `${w.name || '연결로'} · 자동 ${(w.s / 1000).toFixed(2)} / ${(w.length / 1000).toFixed(2)} km`;
      if (text !== lastCap) { cap.textContent = text; lastCap = text; }
    },
  };
}
