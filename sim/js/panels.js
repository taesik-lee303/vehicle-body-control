// panels.js - 화면의 창(툴박스, 이벤트 로그, 전방 맵, 미니맵, 계기판)을 접고 크기를 바꾼다.
// 접기: 창 머리의 ▾/▸ 버튼. 크기: 화면 안쪽 모서리의 손잡이를 대각선으로 끈다. 바꾼 크기·접힘은 이 브라우저에 기억한다.

const KEY = 'vbc.panels';
const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));

// specs: [{ key, el, corner: 'bl'|'br'|'tr'|'tl', center?: 가운데 정렬(폭을 양쪽으로), height?: false면 폭만, minW, minH, label? }]
export function setupPanels(specs) {
  let saved = {};
  try { saved = JSON.parse(localStorage.getItem(KEY)) || {}; } catch { /* 저장소를 못 쓰면 기본 크기 */ }
  const save = () => { try { localStorage.setItem(KEY, JSON.stringify(saved)); } catch { /* 이번 실행에만 */ } };
  const panels = {};

  for (const sp of specs) {
    const el = sp.el;
    el.classList.add('panel');
    const st = () => (saved[sp.key] ||= {});

    // ---- 접기 버튼 ----
    const btn = document.createElement('button');
    btn.className = 'pbtn';
    btn.title = '접기 / 펴기';
    const header = el.querySelector(':scope > header');
    if (header) header.append(btn);
    else { btn.classList.add('float'); el.prepend(btn); }
    btn.addEventListener('click', (e) => { e.stopPropagation(); toggle(); btn.blur(); });

    // ---- 크기 손잡이 ----
    const rz = document.createElement('div');
    rz.className = `rz rz-${sp.corner}`;
    rz.title = '끌어서 크기 조절';
    el.append(rz);
    rz.addEventListener('pointerdown', (e) => {
      e.preventDefault();
      e.stopPropagation();
      rz.setPointerCapture(e.pointerId);
      const r = el.getBoundingClientRect(), x0 = e.clientX, y0 = e.clientY;
      const move = (ev) => {
        const dx = ev.clientX - x0, dy = ev.clientY - y0;
        const s = st();
        s.w = Math.round(clamp(r.width + (sp.corner.includes('l') ? -dx : dx) * (sp.center ? 2 : 1), sp.minW || 180, innerWidth - 24));
        if (sp.height !== false) s.h = Math.round(clamp(r.height + (sp.corner.includes('t') ? -dy : dy), sp.minH || 80, innerHeight - 24));
        s.c = false;
        apply();
      };
      const up = () => { rz.removeEventListener('pointermove', move); rz.removeEventListener('pointerup', up); save(); };
      rz.addEventListener('pointermove', move);
      rz.addEventListener('pointerup', up);
    });
    // 손잡이를 두 번 누르면 기본 크기로
    rz.addEventListener('dblclick', () => { delete st().w; delete st().h; save(); apply(); });

    function apply() {
      const s = st();
      el.classList.toggle('collapsed', !!s.c);
      btn.textContent = s.c ? (header ? '▸' : `${sp.label || ''} ▸`) : '▾';
      el.style.width = s.w ? `${s.w}px` : '';
      if (sp.height !== false) {
        const h = s.h && !s.c;
        el.style.height = h ? `${s.h}px` : '';
        el.style.maxHeight = h ? 'none' : '';
        el.style.flex = h ? 'none' : '';
      }
    }
    function toggle() { st().c = !st().c; save(); apply(); }
    apply();
    panels[sp.key] = { toggle, apply };
  }

  return {
    toggle: (key) => panels[key]?.toggle(),
    reset() { saved = {}; save(); for (const p of Object.values(panels)) p.apply(); },
  };
}
