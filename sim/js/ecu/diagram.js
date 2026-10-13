// diagram.js - BCU system 구성도 (SVG). state JSON 값을 그대로 그린다 (spec 2장 구성도, 4장 표시 규칙).
// 색·밝기·라벨은 값 → 표시 매핑일 뿐이다. 요구사항 판단(임계값 비교 등)은 하지 않는다. 판정은 모두 C가 한다.

// SWC별로 VBC → SWC 화살표에 싣는 VbcContext 필드 (spec 2.1 표). 화면 표시용이며 판단에 쓰지 않는다.
// 모듈 코드가 읽는 필드가 바뀌면 함께 고친다 (spec 2.1)
export const SWC_FIELDS = {
  lighting: ['ignition_on', 'light_sw', 'lux_valid', 'lux', 'lux_fault'],
  wiper: ['ignition_on', 'wiper_sw', 'rain_valid', 'rain', 'rain_fault', 'speed_valid', 'speed_kmh'],
  doors: ['rx_lost', 'ignition_on', 'gear', 'speed_valid', 'speed_kmh'],
  dashboard: ['speed_valid', 'speed_kmh', 'any_door_open', 'lux_fault', 'rain_fault'],
};

const NS = 'http://www.w3.org/2000/svg';
const PULSE_MS = 400; // 라벨이 바뀐 화살표를 깜박이는 시간 (수동·일시정지)
const FLOW_STAGE_MS = 500; // 시나리오 재생 중 데이터 흐름 한 단계 (벽시계, 배속과 무관): 입력 → VBC → SWC → ECU

// 코드 → 이름 (modcon.c 별칭과 같은 순서). 이름 배열 밖은 C의 name_or처럼 "INVALID"
const IGN = ['off', 'on'];
const GEAR = ['P', 'R', 'N', 'D'];
const DOOR = ['close', 'open'];
const LIGHT_SW = ['OFF', 'AUTO', 'ON'];
const WIPER_SW = ['OFF', 'AUTO', 'LOW', 'HIGH'];
const nameOr = (names, v) => (v >= 0 && v < names.length ? names[v] : 'INVALID');
const code = (names) => (v) => `${v} ${nameOr(names, v)}`;
const unit = (u) => (v) => `${v} ${u}`;

// 입력 화살표 11개 (VbcInputs → VBC). fault: 이 화살표를 빨강으로 그리는 C 판정 (C2·C4)
const INPUTS = [
  { key: 'ign', label: '시동', fmt: code(IGN), periodic: true, fault: (c) => c.rx_lost },
  { key: 'gear', label: '기어', fmt: code(GEAR), periodic: true, fault: (c) => c.rx_lost },
  { key: 'speed', label: '속도', fmt: unit('km/h'), periodic: true, fault: (c) => c.rx_lost },
  { key: 'lux', label: '조도', fmt: unit('lx'), periodic: true, fault: (c) => c.lux_fault },
  { key: 'rain', label: '강우', fmt: unit('%'), periodic: true, fault: (c) => c.rain_fault },
  { key: 'fl', label: '도어', fmt: code(DOOR), get: (i) => i.doors[0] },
  { key: 'fr', label: '도어', fmt: code(DOOR), get: (i) => i.doors[1] },
  { key: 'rl', label: '도어', fmt: code(DOOR), get: (i) => i.doors[2] },
  { key: 'rr', label: '도어', fmt: code(DOOR), get: (i) => i.doors[3] },
  { key: 'light', label: '조명 SW', fmt: code(LIGHT_SW), get: (i) => i.light_sw },
  { key: 'wiper', label: '와이퍼 SW', fmt: code(WIPER_SW), get: (i) => i.wiper_sw },
];

// SWC 4개. 위→아래 배치는 spec 2장 그림 순서, order는 vbc_step() 실행 순서 (와이퍼 → 조명 → 도어 → 계기판)
const SWCS = [
  { key: 'lighting', name: 'Lighting SWC', fn: 'lighting_step()', order: 2, ecu: '조명 ECU' },
  { key: 'wiper', name: 'Wiper SWC', fn: 'wipers_step()', order: 1, ecu: '와이퍼 ECU' },
  { key: 'doors', name: 'Doors SWC', fn: 'doors_step()', order: 3, ecu: '도어 ECU' },
  { key: 'dashboard', name: 'Dashboard SWC', fn: 'dashboard_step()', order: 4, ecu: '계기판 ECU' },
];

const onOff = (b) => (b ? 'on' : 'off');
const upper = (s) => String(s).toUpperCase();

// SWC → ECU 화살표 라벨 (spec 2.2 받는 값)
const OUT_LINES = {
  lighting: (o) => [`headlight=${onOff(o.headlight)}`, `lux_lamp=${onOff(o.lux_lamp)}`],
  wiper: (o) => [`wiper=${o.wiper}`, `rain_lamp=${onOff(o.rain_lamp)}`],
  doors: (o) => [`locked=${onOff(o.locked)}`],
  dashboard: (o) => [`warning=${o.warning}`],
};

// 계기판 경고 이름 (spec 2.2)
const WARN_NAME = { none: '경고 없음', door: '주행 중 도어 열림', lux: '조도 센서 고장', rain: '강우 센서 고장' };

// ECU 표시 (spec 2.2): on = 밝게, mode = 배지 글자, kind = 추가 클래스, lamp = 블록 안 고장등
const ECU_VIEW = {
  lighting: (o) => ({ on: o.headlight, mode: o.headlight ? 'ON' : 'OFF', lamp: o.lux_lamp }),
  wiper: (o) => ({ on: o.wiper !== 'stop', mode: upper(o.wiper), lamp: o.rain_lamp }),
  doors: (o) => ({ on: null, mode: o.locked ? 'LOCKED' : 'UNLOCKED', kind: o.locked ? 'locked' : 'unlocked' }),
  dashboard: (o) => ({ on: o.warning !== 'none', mode: WARN_NAME[o.warning] || upper(o.warning) }),
};
const LAMP_NAME = { lighting: '조도 고장등', wiper: '강우 고장등' };

// ---- 좌표 (viewBox 1160 × 660) ----
const W = 1160;
const H = 660;
const IN_X1 = 70;
const VBC = { x: 270, y: 50, w: 130, h: 590 };
const SWC_X = 640;
const SWC_W = 145;
const ECU_X = 950;
const ECU_W = 200;
const SLOT_Y0 = 50;
const SLOT_H = 147.5;
const slotMid = (i) => SLOT_Y0 + i * SLOT_H + 80; // SWC·ECU 박스 가운데 = 화살표 높이

function el(tag, attrs, parent) {
  const e = document.createElementNS(NS, tag);
  for (const [k, v] of Object.entries(attrs || {})) e.setAttribute(k, String(v));
  if (parent) parent.appendChild(e);
  return e;
}

function text(parent, x, y, s, cls) {
  const t = el('text', { x, y, class: cls || '' }, parent);
  t.textContent = s;
  return t;
}

function setText(t, s) {
  if (t.textContent !== s) t.textContent = s;
}

// 여러 줄 라벨: 줄마다 tspan 하나. 마지막 줄의 기준선이 y
function writeLines(t, lines, lineH) {
  while (t.childNodes.length > lines.length) t.removeChild(t.lastChild);
  while (t.childNodes.length < lines.length) el('tspan', { x: t.getAttribute('x') }, t);
  lines.forEach((s, i) => {
    const sp = t.childNodes[i];
    sp.setAttribute('y', String(Number(t.getAttribute('data-y')) - (lines.length - 1 - i) * lineH));
    if (sp.textContent !== s) sp.textContent = s;
  });
}

// 화살표 = 선 + 머리 + 라벨. 머리는 marker 대신 path로 그려 CSS 클래스(색)를 그대로 따른다
function arrow(parent, x1, y1, x2, y2, labelX, labelY, cls) {
  const g = el('g', { class: `arrow ${cls}` }, parent);
  const ang = Math.atan2(y2 - y1, x2 - x1);
  const bx = x2 - 9 * Math.cos(ang);
  const by = y2 - 9 * Math.sin(ang);
  const px = -Math.sin(ang) * 5;
  const py = Math.cos(ang) * 5;
  const r = (v) => Math.round(v * 10) / 10;
  el('line', { class: 'shaft', x1, y1, x2: r(bx), y2: r(by) }, g);
  el('line', { class: 'flow', x1, y1, x2: r(bx), y2: r(by) }, g); // 데이터 흐름 (시나리오 재생 중만 보임)
  el('path', { class: 'head', d: `M${x2} ${y2}L${r(bx + px)} ${r(by + py)}L${r(bx - px)} ${r(by - py)}Z` }, g);
  const label = el('text', { class: 'lbl', x: labelX, 'data-y': labelY }, g);
  return { g, label, prev: undefined, until: 0, from: 0, to: 0 };
}

// 라벨을 쓰고, 이전 렌더와 달라졌으면 true. pulse면 PULSE_MS 동안 pulse 클래스 (수동·일시정지의 깜박임)
function setLabel(a, lines, now, pulse, lineH = 13) {
  const s = lines.join('\n');
  let changed = false;
  if (s !== a.prev) {
    changed = a.prev !== undefined;
    if (changed && pulse) a.until = now + PULSE_MS;
    writeLines(a.label, lines, lineH);
    a.prev = s;
  }
  a.g.classList.toggle('pulse', pulse && now < a.until);
  return changed;
}

// 흐름 예약: from부터 FLOW_STAGE_MS 동안. 이미 흐르는 중이거나 예약이 남아 있으면 구간을 늘리기만 한다
// (처음부터 다시 시작하지 않으므로 10배속처럼 변화가 잦아도 끊기지 않고 이어서 흐른다)
function flowAt(a, from, now) {
  const to = from + FLOW_STAGE_MS;
  if (a.to > now) {
    a.from = Math.min(a.from, from);
    a.to = Math.max(a.to, to);
  } else {
    a.from = from;
    a.to = to;
  }
}

function showFlow(a, now, flow) {
  if (!flow) a.from = a.to = 0;
  a.g.classList.toggle('flowing', flow && a.from <= now && now < a.to);
}

function box(parent, x, y, w, h, cls) {
  const g = el('g', { class: cls }, parent);
  el('rect', { class: 'body', x, y, width: w, height: h, rx: 8 }, g);
  return g;
}

// 두 필드씩 한 줄로 묶는다 (화살표 사이 폭에 맞추기)
function pairs(items) {
  const out = [];
  for (let i = 0; i < items.length; i += 2) out.push(items.slice(i, i + 2).join('  '));
  return out;
}

export function createDiagram(svgEl) {
  svgEl.setAttribute('viewBox', `0 0 ${W} ${H}`);
  svgEl.setAttribute('preserveAspectRatio', 'xMidYMid meet');
  while (svgEl.firstChild) svgEl.removeChild(svgEl.firstChild);

  text(svgEl, 12, 26, '입력 신호 (VbcInputs)', 'col-title');
  text(svgEl, ECU_X, 26, '출력 ECU', 'col-title');
  el('rect', { class: 'bcu-frame', x: 240, y: 34, width: 660, height: H - 42, rx: 12 }, svgEl);
  const bcuTitle = text(svgEl, 252, 52, 'BCU system', 'bcu-title');

  // VBC 상위 제어기
  const vbc = box(svgEl, VBC.x, VBC.y + 12, VBC.w, VBC.h - 12, 'vbc');
  text(vbc, VBC.x + VBC.w / 2, H / 2 - 22, 'VBC 상위 제어기', 'box-title mid');
  text(vbc, VBC.x + VBC.w / 2, H / 2, 'C2~C4 판정', 'box-sub mid');
  text(vbc, VBC.x + VBC.w / 2, H / 2 + 18, '→ VbcContext', 'box-sub mid');

  // 입력 화살표 11개
  const inputs = INPUTS.map((d, i) => {
    const y = 84 + i * 52.5;
    const a = arrow(svgEl, IN_X1, y, VBC.x, y, IN_X1 + 8, y - 6, 'in');
    text(a.g, 12, y + 4, d.key, 'in-key');
    text(a.g, 12, y + 17, d.label, 'in-sub');
    return { d, a };
  });

  // SWC 4개 + ctx 화살표 + 출력 화살표 + ECU
  const swcs = SWCS.map((d, i) => {
    const top = SLOT_Y0 + i * SLOT_H;
    const mid = slotMid(i);
    const ctxArrow = arrow(svgEl, VBC.x + VBC.w, mid, SWC_X, mid, VBC.x + VBC.w + 10, mid - 7, 'ctx');
    const swc = box(svgEl, SWC_X, mid - 35, SWC_W, 70, 'swc');
    text(swc, SWC_X + 10, mid - 12, d.name, 'box-title');
    text(swc, SWC_X + 10, mid + 6, d.fn, 'box-sub');
    text(swc, SWC_X + 10, mid + 23, `실행 ${d.order}번째`, 'box-sub');

    const outArrow = arrow(svgEl, SWC_X + SWC_W, mid, ECU_X, mid, SWC_X + SWC_W + 10, mid - 7, 'out');

    const ecuTop = top + 30;
    const ecu = box(svgEl, ECU_X, ecuTop, ECU_W, 100, `ecu ecu-${d.key}`);
    text(ecu, ECU_X + 12, ecuTop + 22, d.ecu, 'box-title');
    const badge = el('g', { class: 'badge' }, ecu);
    el('rect', { x: ECU_X + 12, y: ecuTop + 32, width: ECU_W - 24, height: 30, rx: 6 }, badge);
    const mode = text(badge, ECU_X + ECU_W / 2, ecuTop + 53, '', 'mode mid');
    let lamp = null;
    if (LAMP_NAME[d.key]) {
      lamp = el('g', { class: 'lamp' }, ecu);
      el('circle', { cx: ECU_X + 20, cy: ecuTop + 81, r: 6 }, lamp);
      text(lamp, ECU_X + 32, ecuTop + 85, LAMP_NAME[d.key], 'lamp-name');
    }
    return { d, swc, ctxArrow, outArrow, ecu, mode, lamp, top };
  });

  // Wiper → Lighting 링크 (L6): 와이퍼 박스 위 → 조명 박스 아래
  const lx = SWC_X + SWC_W / 2;
  const link = arrow(svgEl, lx, slotMid(1) - 35, lx, slotMid(0) + 35, lx + 8, (slotMid(0) + slotMid(1)) / 2 + 12, 'link');
  text(link.g, lx + 8, (slotMid(0) + slotMid(1)) / 2 - 4, 'L6 와이퍼 단계', 'link-cap');

  const allArrows = [...inputs.map((x) => x.a), ...swcs.flatMap((s) => [s.ctxArrow, s.outArrow]), link];

  // opts.flow: 시나리오 재생 중(main.js가 정함). 깜박임 대신 0.5초 단계 데이터 흐름 + ECU 부드러운 전환.
  // 아니면(수동·일시정지) 지금처럼 값이 바뀐 화살표를 깜박인다. 둘 다 표시만 바꾸며 엔진 호출·시각과 무관하다
  function render(st, opts = {}) {
    const now = performance.now();
    const flow = Boolean(opts.flow);
    const pulse = !flow;
    const swcOn = new Set(st.swc);
    const sigOn = new Set(st.signals);
    svgEl.classList.toggle('flow-mode', flow);
    setText(bcuTitle, `BCU system — ${st.title}`);
    let inChanged = false; // 이번 렌더에 입력 화살표 값이 바뀜 → SWC 쪽은 다음 단계부터

    for (const { d, a } of inputs) {
      let lines;
      let cut = false;
      let fault = false;
      if (d.periodic) {
        const s = st.in[d.key];
        cut = s.cut;
        fault = Boolean(d.fault(st.ctx));
        if (!s.received) lines = ['수신 전'];
        else lines = [s.tx !== s.rx ? `${d.fmt(s.rx)}  (tx ${s.tx})` : d.fmt(s.rx)];
      } else {
        lines = [d.fmt(d.get(st.in))];
      }
      // 끊긴 입력은 VBC로 가지 않으므로 흐름을 그리지 않는다 (라벨의 tx 표시만 바뀜)
      if (setLabel(a, lines, now, pulse) && flow && !cut && sigOn.has(d.key)) {
        flowAt(a, now, now);
        inChanged = true;
      }
      a.g.classList.toggle('cut', cut);
      a.g.classList.toggle('fault', fault);
      a.g.classList.toggle('idle', !sigOn.has(d.key));
    }

    let linkNext = null; // 와이퍼 → ECU와 같은 단계에 L6 링크도 흐름
    for (const s of swcs) {
      const on = swcOn.has(s.d.key);
      const fields = SWC_FIELDS[s.d.key].map((f) => `${f}=${st.ctx[f]}`);
      const ctxCh = setLabel(s.ctxArrow, pairs(fields), now, pulse);
      const outCh = setLabel(s.outArrow, OUT_LINES[s.d.key](st.out), now, pulse);
      // 흐름 단계: 입력이 바뀌었으면 VBC → SWC는 0.5초 뒤, SWC → ECU는 1초 뒤.
      // 입력 변화 없이 바뀐 것(VBC 판정·SWC 시간 조건)은 그 화살표부터 바로 시작
      if (flow && on) {
        let next = null;
        if (ctxCh) {
          const t0 = inChanged ? now + FLOW_STAGE_MS : now;
          flowAt(s.ctxArrow, t0, now);
          next = t0 + FLOW_STAGE_MS;
        } else if (outCh) {
          next = now;
        }
        if (next !== null) {
          flowAt(s.outArrow, next, now);
          if (s.d.key === 'wiper') linkNext = next;
        }
      }
      s.swc.classList.toggle('active', on);
      s.swc.classList.toggle('idle', !on);
      s.ctxArrow.g.classList.toggle('idle', !on);
      s.outArrow.g.classList.toggle('idle', !on);

      const v = ECU_VIEW[s.d.key](st.out);
      s.ecu.classList.toggle('on', v.on === true);
      s.ecu.classList.toggle('off', v.on === false);
      s.ecu.classList.toggle('locked', v.kind === 'locked');
      s.ecu.classList.toggle('unlocked', v.kind === 'unlocked');
      s.ecu.classList.toggle('idle', !on);
      setText(s.mode, v.mode);
      if (s.lamp) s.lamp.classList.toggle('lit', Boolean(v.lamp));
    }

    // 링크: source가 none이면 숨김, inject면 "주입" (조명 모듈 콘솔에서 level로 넣은 값)
    const src = st.link.source;
    link.g.classList.toggle('hidden', src === 'none');
    link.g.classList.toggle('inject', src === 'inject');
    if (setLabel(link, [src === 'inject' ? `주입 ${st.link.wiper_level}` : st.link.wiper_level], now, pulse) && flow && src !== 'none')
      flowAt(link, linkNext ?? now, now);

    for (const a of allArrows) showFlow(a, now, flow);
  }

  return { render };
}
