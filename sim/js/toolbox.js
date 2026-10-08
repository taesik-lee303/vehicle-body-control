// toolbox.js - 좌상단 툴박스. 조작은 전부 콘솔 명령으로 보내고(docs/sim-plan.md 4.1),
// 표시는 매 프레임 코어 상태(sim_state)에서 다시 그린다 → 키보드·시나리오로 바뀐 값도 맞게 보인다.

const LUX_MAX = 60000;
const LUX_OOR = 70000;   // 조도 범위 밖 주입 값 (C2)
const RAIN_OOR = 101;    // 강우 범위 밖 주입 값 (C2)
const CUT_BITS = { lux: 8, rain: 16, vehicle: 7 };   // console.h CON_* 와 같은 값

// 조도 슬라이더는 로그 눈금 (0 ~ 1000 → 0 ~ 60000 lx). 밤(수백 lx)과 낮(수만 lx)을 한 슬라이더로
export const luxFromSlider = (x) => (x <= 0 ? 0 : Math.round(10 ** ((x / 1000) * Math.log10(LUX_MAX + 1)) - 1));
export const sliderFromLux = (lux) => Math.round((Math.log10(Math.max(0, Math.min(LUX_MAX, lux)) + 1) / Math.log10(LUX_MAX + 1)) * 1000);

const RADAR_MODES = [['ok', '정상'], ['cut', '끊김'], ['repeat', '프레임 반복'], ['stuck', '값 고착'], ['block', '가림']];
const SG_KO = { off: '꺼짐', standby: '대기', active: '작동', hold: '정지 유지', wait: '재출발 대기' };

export function setupToolbox({ exec, sendQuiet, sim, actions, scenarios, getAdas = () => null, getRadarShow = () => ({}),
  getLanes = () => ({ right: -3.6, left: null, opp: 3.6 }), isMap = () => false }) {
  const $ = (sel) => document.querySelector(sel);
  const $$ = (sel) => [...document.querySelectorAll(sel)];
  const root = $('#toolbox');
  const rain = $('#rain');
  const lux = $('#lux');
  let dragging = null;     // 드래그 중인 슬라이더는 상태로 덮어쓰지 않는다
  let lastQuiet = 0;

  // 버튼·체크박스는 누른 뒤 포커스를 놓는다 (Space·방향키가 단축키로 가게)
  root.addEventListener('click', (e) => { if (e.target.closest('button, input[type=checkbox]')) e.target.blur(); });

  // ---- 주행 모드 ----
  $('#drive').addEventListener('change', (e) => {
    exec(`drive ${e.target.checked ? 'on' : 'off'}`);
    actions.resyncInput();
  });
  $('#speed-input').addEventListener('change', (e) => exec(`speed ${Math.round(Number(e.target.value) || 0)}`));

  // ---- 슬라이더: 드래그 중에는 100ms마다 조용히, 놓을 때 로그에 남김 ----
  function slider(el, name, toValue, isOor) {
    el.addEventListener('pointerdown', () => { dragging = el; });
    el.addEventListener('input', () => {
      dragging = el;
      if (isOor()) return;
      const now = performance.now();
      if (now - lastQuiet > 100) { sendQuiet(`${name} ${toValue(Number(el.value))}`); lastQuiet = now; }
    });
    el.addEventListener('change', () => {
      dragging = null;
      el.blur();
      if (!isOor()) exec(`${name} ${toValue(Number(el.value))}`);
    });
  }
  const oor = (name) => $(`[data-oor=${name}]`).checked;
  slider(rain, 'rain', (v) => v, () => oor('rain'));
  slider(lux, 'lux', luxFromSlider, () => oor('lux'));

  $$('#lux-presets [data-lux]').forEach((b) => b.addEventListener('click', () => {
    lux.value = sliderFromLux(Number(b.dataset.lux));
    if (!oor('lux')) exec(`lux ${b.dataset.lux}`);
  }));

  // ---- 도어, 스위치 ----
  $$('[data-door]').forEach((cb) => cb.addEventListener('change', () =>
    exec(`door ${['fl', 'fr', 'rl', 'rr'][cb.dataset.door]} ${cb.checked ? 'open' : 'close'}`)));
  $$('[data-light]').forEach((b) => b.addEventListener('click', () => exec(`light ${b.dataset.light}`)));
  $$('[data-wiper]').forEach((b) => b.addEventListener('click', () => exec(`wiper ${b.dataset.wiper}`)));

  // ---- 고장 주입 ----
  $$('[data-cut]').forEach((cb) => cb.addEventListener('change', () =>
    exec(`${cb.checked ? 'cut' : 'resume'} ${cb.dataset.cut}`)));
  $$('[data-oor]').forEach((cb) => cb.addEventListener('change', () => {
    const name = cb.dataset.oor;
    const normal = name === 'lux' ? luxFromSlider(Number(lux.value)) : Number(rain.value);
    exec(`${name} ${cb.checked ? (name === 'lux' ? LUX_OOR : RAIN_OOR) : normal}`);
  }));

  // ---- 시간 ----
  $('#play').addEventListener('click', actions.togglePause);
  $('#tick').addEventListener('click', actions.tick);
  $$('[data-rate]').forEach((b) => b.addEventListener('click', () => actions.setRate(Number(b.dataset.rate))));

  // ---- 시나리오 ----
  const select = $('#scenario');
  scenarios.list().then((names) => {
    select.innerHTML = names.length
      ? names.map((n) => `<option value="${n}">${n.replace(/\.txt$/, '')}</option>`).join('')
      : '<option value="">(scenarios/ 없음)</option>';
  }).catch(() => { select.innerHTML = '<option value="">(목록 실패)</option>'; });
  $('#scenario-play').addEventListener('click', () => select.value && scenarios.play(select.value));
  $('#scenario-stop').addEventListener('click', scenarios.stop);

  // ---- 레이더·Stop & Go (exp/radar-stop-and-go) ----
  $$('[data-radar]').forEach((sel) => {
    sel.innerHTML = RADAR_MODES.map(([v, t]) => `<option value="${v}">${t}</option>`).join('');
    sel.addEventListener('change', () => { exec(`radar ${sel.dataset.radar} ${sel.value}`); sel.blur(); });
  });
  $('#sg-sw').addEventListener('click', () => { const a = getAdas(); exec(`sg ${a && a.sg.sw ? 'off' : 'on'}`); });
  $$('[data-sg]').forEach((b) => b.addEventListener('click', () => exec(`sg ${b.dataset.sg}`)));
  $$('[data-gap]').forEach((b) => b.addEventListener('click', () => exec(`sg gap ${b.dataset.gap}`)));

  // 교통 장면: 지금 자차 위치 기준으로 물체를 놓는다 (요구사항 8.4 obj 명령)
  const pending = [];   // { t: 시뮬 시각 ms, run }
  let simT = 0;
  const later = (ms, run) => pending.push({ t: simT + ms, run });
  const newestId = () => (getAdas()?.objs || []).reduce((m, o) => Math.max(m, o[0]), 0);
  // 물체: [id, 종류, 가운데 x, 가운데 y, 길이, ...] (자차 기준)
  const laneLead = () => {
    let best = null;
    for (const o of getAdas()?.objs || []) {
      const [, kind, xc, y, len] = o;
      if ((kind === 'car' || kind === 'truck') && Math.abs(y) < 1.3 && xc - len / 2 > 0 && (!best || xc < best[2])) best = o;
    }
    return best;
  };
  // 옆 차로는 그 자리에 실제로 있을 때만 쓴다 (지도는 구간마다 차로 수가 다르다)
  const side = (ahead) => { const L = getLanes(ahead); return L.right ?? L.left; };
  const SCENES = {
    jam() {
      exec('obj clear');
      for (const c of ['car 18 0 30', 'car 31 0 30', 'truck 44 0 30']) exec(`obj add ${c}`);
      for (const [ahead, v] of [[12, 33], [36, 31]]) { const y = side(ahead); if (y !== null) exec(`obj add car ${ahead} ${y.toFixed(2)} ${v}`); }
      const opp = getLanes(120).opp;
      if (opp !== null) exec(`obj add car 120 ${opp.toFixed(2)} -40`);
    },
    cutin() {
      const y = side(6);
      if (y === null) { exec('obj clear'); exec('obj add car 45 0 30'); return; }   // 옆 차로가 없는 곳
      exec('obj clear');
      exec('obj add car 45 0 30');
      exec(`obj add car 6 ${y.toFixed(2)} 32`);
      const id = newestId();
      later(3000, () => exec(`obj ${id} lane 0 1.2`));
    },
    ped() {
      const L = getLanes(14), from = (L.opp ?? L.left ?? 1.5) + 3, to = (L.right ?? 0) - 4.5;
      exec(`obj add ped 14 ${from.toFixed(1)} 0`);
      exec(`obj ${newestId()} lane ${to.toFixed(1)} 1.3`);
    },
    bridge() {
      exec('obj clear');
      for (const c of ['manhole 30 0', 'sign 55 0', 'bridge 90 0', 'car 94 0 0']) exec(`obj add ${c}`);
    },
    clear() { exec('obj clear'); },
  };
  $$('[data-scene]').forEach((b) => b.addEventListener('click', () => SCENES[b.dataset.scene]()));
  const LEAD_CMD = { stop: 'speed 0 2', go: 'speed 30 1.5', slow: 'speed 10 1.5' };
  $$('[data-lead]').forEach((b) => b.addEventListener('click', () => {
    const o = laneLead();
    if (o) exec(`obj ${o[0]} ${LEAD_CMD[b.dataset.lead]}`);
  }));
  $$('[data-show]').forEach((cb) => cb.addEventListener('change', () => {
    if (cb.dataset.show === 'panel') document.getElementById('radar-panel').dataset.off = cb.checked ? '0' : '1';
    else getRadarShow()[cb.dataset.show] = cb.checked;
  }));

  // ---- 기타 ----
  $('#reset').addEventListener('click', actions.reset);
  $('#help-btn').addEventListener('click', actions.toggleHelp);

  // ---- 코어 상태 → 표시 ----
  function sync(s, adas = null) {
    simT = s.t;
    for (let i = pending.length - 1; i >= 0; i--) if (pending[i].t <= simT) pending.splice(i, 1)[0].run();
    const on = !!(adas && adas.on);
    $$('.group.adas').forEach((g) => { g.hidden = !on; });
    if (on) {
      const st = $('#sg-state');
      st.textContent = SG_KO[adas.sg.state] || adas.sg.state;
      st.className = adas.sg.state;
      $('#sg-sw').classList.toggle('active', !!adas.sg.sw);
      $$('[data-gap]').forEach((b) => b.classList.toggle('active', Number(b.dataset.gap) === adas.sg.gap));
      $$('[data-radar]').forEach((sel, i) => { if (document.activeElement !== sel) sel.value = adas.map.radars[i].inject; });
    }
    const drive = !!s.in.drive;
    $('#drive').checked = drive;
    $('#speed-field').hidden = drive;
    const speedInput = $('#speed-input');
    if (document.activeElement !== speedInput) speedInput.value = s.in.speed;

    $('#rain-val').textContent = `${s.in.rain} %`;
    $('#lux-val').textContent = `${s.in.lux} lx`;
    const rainOor = s.in.rain < 0 || s.in.rain > 100;
    const luxOor = s.in.lux < 0 || s.in.lux > LUX_MAX;
    $('[data-oor=rain]').checked = rainOor;
    $('[data-oor=lux]').checked = luxOor;
    if (dragging !== rain && !rainOor) rain.value = s.in.rain;
    if (dragging !== lux && !luxOor && luxFromSlider(Number(lux.value)) !== s.in.lux) lux.value = sliderFromLux(s.in.lux);

    $$('[data-door]').forEach((cb) => { cb.checked = !!s.in.doors[cb.dataset.door]; });
    $$('[data-light]').forEach((b) => b.classList.toggle('active', b.dataset.light.toUpperCase() === s.in.light));
    $$('[data-wiper]').forEach((b) => b.classList.toggle('active', b.dataset.wiper.toUpperCase() === s.in.wiper));
    $$('[data-cut]').forEach((cb) => { const m = CUT_BITS[cb.dataset.cut]; cb.checked = (s.in.cut & m) === m; });

    $('#play').textContent = sim.paused ? '▶' : '∥';
    $$('[data-rate]').forEach((b) => b.classList.toggle('active', Number(b.dataset.rate) === sim.rate));
  }

  return { sync, toggle: () => root.classList.toggle('collapsed'), playScenario: scenarios.play,
    scene: (name) => SCENES[name] && SCENES[name]() };
}
