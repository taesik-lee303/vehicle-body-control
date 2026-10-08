// radar-panel.js - 위에서 본 전방 맵 (요구사항 4장). 맵(adas_map)의 검출점·트랙·선행 차량과 레이더 상태를 그린다.
// 판단은 하지 않는다: sim_adas() / sim_points() 값을 그대로 보여 준다. 캔버스를 누르면 보기 거리가 바뀐다.
import { MOUNTS } from './scene/radar-view.js';

const SENSOR_CSS = ['#4dd2ff', '#9cff57', '#ffb347'];
const RANGES = [40, 80, 150];
const Y_HALF = 12;
const HEALTH = { ok: ['정상', 'ok'], unknown: ['확인 중', 'dim'], fault: ['고장', 'bad'] };
const WHY = (w) => [w & 1 ? '새 프레임 없음' : '', w & 2 ? '자체 고장·가림' : '', w & 4 ? '값 고착' : ''].filter(Boolean).join(', ');
const INJECT = { ok: '', cut: '끊김', repeat: '반복', stuck: '고착', block: '가림' };
const MAP = { ok: ['맵 정상', 'ok'], reduced: ['맵 축소', 'warn'], none: ['맵 없음', 'bad'] };

export function createRadarPanel(root) {
  const canvas = root.querySelector('canvas');
  const chips = root.querySelector('.chips');
  const foot = root.querySelector('.foot');
  const ctx = canvas.getContext('2d');
  let range = 1;
  canvas.addEventListener('click', () => { range = (range + 1) % RANGES.length; });
  let lastChips = '';

  function render(adas, pts) {
    root.hidden = !(adas && adas.on) || root.dataset.off === '1';
    if (root.hidden) return;

    // ---- 머리: 레이더 상태 칩 ----
    const r = adas.map.radars;
    const chipHtml = r.map((x, i) => {
      const [label, cls] = HEALTH[x.h] || [x.h, 'dim'];
      const why = x.h === 'fault' && x.why ? ` (${WHY(x.why)})` : '';
      const inj = INJECT[x.inject] ? ` <i>주입: ${INJECT[x.inject]}</i>` : '';
      return `<span class="chip ${cls}" style="--c:${SENSOR_CSS[i]}"><b>${x.name}</b> ${label}${why}${inj}</span>`;
    }).join('') + `<span class="chip ${MAP[adas.map.status][1]}">${MAP[adas.map.status][0]}</span>`;
    if (chipHtml !== lastChips) { chips.innerHTML = chipHtml; lastChips = chipHtml; }

    // ---- 캔버스 ----
    const dpr = Math.min(devicePixelRatio || 1, 2);
    const W = Math.round(canvas.clientWidth * dpr), H = Math.round(canvas.clientHeight * dpr);
    if (canvas.width !== W || canvas.height !== H) { canvas.width = W; canvas.height = H; }
    const R = RANGES[range], bottom = H - 34 * dpr;
    const sx = (W / 2 - 8 * dpr) / Y_HALF, sy = (bottom - 8 * dpr) / R;
    const X = (y) => W / 2 - y * sx, Y = (x) => bottom - x * sy;
    ctx.clearRect(0, 0, W, H);
    ctx.font = `${11 * dpr}px system-ui, sans-serif`;

    // 차로 (내 차로 가운데 y 0, 차로 폭 3.6 m), 오른쪽 가드레일
    ctx.fillStyle = 'rgba(255,255,255,0.025)';
    ctx.fillRect(X(9.0), 0, X(-5.4) - X(9.0), bottom);
    ctx.lineWidth = 1 * dpr;
    for (const [y, color, dash] of [[1.8, '#c9a227', []], [-1.8, '#aab', [6, 6]], [-5.4, '#aab', []], [5.4, '#aab', [6, 6]],
      [9.0, '#aab', []], [-7.45, '#7c838c', [2, 3]]]) {
      ctx.strokeStyle = color; ctx.setLineDash(dash.map((d) => d * dpr));
      ctx.beginPath(); ctx.moveTo(X(y), 0); ctx.lineTo(X(y), bottom); ctx.stroke();
    }
    ctx.setLineDash([]);
    // 경로 폭 (XM8)
    ctx.fillStyle = 'rgba(77,163,255,0.10)';
    ctx.fillRect(X(1.3), 0, X(-1.3) - X(1.3), bottom);
    // 거리 눈금
    const step = R <= 40 ? 5 : R <= 80 ? 10 : 25;
    ctx.fillStyle = '#8d95a5'; ctx.strokeStyle = 'rgba(255,255,255,0.07)';
    for (let x = step; x < R; x += step) {
      ctx.beginPath(); ctx.moveTo(0, Y(x)); ctx.lineTo(W, Y(x)); ctx.stroke();
      ctx.fillText(`${x} m`, 4 * dpr, Y(x) - 3 * dpr);
    }
    // 시야
    MOUNTS.forEach((m, i) => {
      if (r[i].h !== 'ok') return;
      ctx.strokeStyle = SENSOR_CSS[i] + '55';
      ctx.beginPath();
      for (const s of [-1, 1]) {
        const a = (m.yaw + s * m.fov) * Math.PI / 180, len = Math.min(m.range, R * 1.5);
        ctx.moveTo(X(m.y), Y(m.x)); ctx.lineTo(X(m.y + len * Math.sin(a)), Y(m.x + len * Math.cos(a)));
      }
      ctx.stroke();
    });

    // 검출점: [센서, x, y, z, 도플러, 표시]
    const n = pts ? pts.length / 6 : 0;
    const ps = 2.4 * dpr;
    for (let i = 0; i < n; i++) {
      const o = i * 6, x = pts[o + 1], y = pts[o + 2], f = pts[o + 5];
      if (x > R) continue;
      ctx.globalAlpha = f & 2 ? 0.35 : f & 1 ? 0.6 : 1;
      ctx.fillStyle = f & 2 ? '#888' : SENSOR_CSS[pts[o]];
      ctx.fillRect(X(y) - ps / 2, Y(x) - ps / 2, ps, ps);
    }
    ctx.globalAlpha = 1;

    // 트랙: [id, x, y, ymin, ymax, vx, vy, zlo, zhi, 표시]
    let lead = null;
    for (const t of adas.tracks) {
      const [, x, , ymin, ymax, , , , , f] = t;
      if (!(f & 1) || x > R) continue;
      const color = f & 16 ? '#ffd34d' : !(f & 2) ? '#6f7682' : f & 4 ? '#ff6b5c' : '#e8eaee';
      ctx.strokeStyle = color; ctx.lineWidth = (f & 16 ? 2.5 : 1.2) * dpr;
      const x0 = X(ymax), w = Math.max(2 * dpr, X(ymin) - X(ymax)), h = Math.max(3 * dpr, 1.2 * sy);
      ctx.strokeRect(x0, Y(x) - h, w, h);
      if (f & 16) lead = t;
    }
    if (lead) {
      const [id, x, y, , , vx] = lead;
      ctx.strokeStyle = '#ffd34d'; ctx.setLineDash([4 * dpr, 3 * dpr]); ctx.lineWidth = 1 * dpr;
      ctx.beginPath(); ctx.moveTo(X(0), Y(0)); ctx.lineTo(X(y), Y(x)); ctx.stroke(); ctx.setLineDash([]);
      ctx.fillStyle = '#ffd34d';
      ctx.fillText(`선행 #${id}  ${x.toFixed(1)} m  ${(vx * 3.6) >= 0 ? '+' : ''}${(vx * 3.6).toFixed(0)} km/h`,
        Math.min(X(y) + 8 * dpr, W - 150 * dpr), Math.max(14 * dpr, Y(x) - 8 * dpr));
    }

    // 자차
    ctx.fillStyle = '#4da3ff';
    ctx.fillRect(X(0.95), Y(0), X(-0.95) - X(0.95), 4.6 * sy + 2 * dpr);
    ctx.fillStyle = '#e8eaee';
    for (const m of MOUNTS) ctx.fillRect(X(m.y) - 2 * dpr, Y(m.x) - 2 * dpr, 4 * dpr, 4 * dpr);

    const conf = adas.tracks.filter((t) => t[9] & 1).length;
    foot.textContent = `보기 ${R} m (누르면 바뀜) · 검출점 ${n} · 트랙 ${conf}/${adas.tracks.length} · 실제 거리 ${adas.gap >= 0 ? adas.gap.toFixed(1) + ' m' : '-'}`;
  }

  return { render };
}
