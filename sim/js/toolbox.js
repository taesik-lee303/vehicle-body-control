// toolbox.js - 좌상단 툴박스. 조작은 전부 콘솔 명령으로 보내고(docs/sim-plan.md 4.1),
// 표시는 매 프레임 코어 상태(sim_state)에서 다시 그린다 → 키보드·시나리오로 바뀐 값도 맞게 보인다.

const LUX_MAX = 60000;
const LUX_OOR = 70000;   // 조도 범위 밖 주입 값 (C2)
const RAIN_OOR = 101;    // 강우 범위 밖 주입 값 (C2)
const CUT_BITS = { lux: 8, rain: 16, vehicle: 7 };   // console.h CON_* 와 같은 값

// 조도 슬라이더는 로그 눈금 (0 ~ 1000 → 0 ~ 60000 lx). 밤(수백 lx)과 낮(수만 lx)을 한 슬라이더로
export const luxFromSlider = (x) => (x <= 0 ? 0 : Math.round(10 ** ((x / 1000) * Math.log10(LUX_MAX + 1)) - 1));
export const sliderFromLux = (lux) => Math.round((Math.log10(Math.max(0, Math.min(LUX_MAX, lux)) + 1) / Math.log10(LUX_MAX + 1)) * 1000);

export function setupToolbox({ exec, sendQuiet, sim, actions, scenarios }) {
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

  // ---- 기타 ----
  $('#reset').addEventListener('click', actions.reset);
  $('#help-btn').addEventListener('click', actions.toggleHelp);

  // ---- 코어 상태 → 표시 ----
  function sync(s) {
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

  return { sync, toggle: () => root.classList.toggle('collapsed') };
}
