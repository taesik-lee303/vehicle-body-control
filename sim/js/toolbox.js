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

export function setupToolbox({ exec, drag, sim, actions, scenarios, getAdas = () => null, getRadarShow = () => ({}),
  log = () => {} }) {
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

  // ---- 슬라이더: 드래그 중에는 100ms마다 조용히(출력이 바뀌면 그 값을 먼저 로그에), 놓을 때 로그에 남김 ----
  function slider(el, name, toValue, isOor) {
    el.addEventListener('pointerdown', () => { dragging = el; });
    el.addEventListener('input', () => {
      dragging = el;
      if (isOor()) return;
      const now = performance.now();
      if (now - lastQuiet > 100) { drag.send(name, `${name} ${toValue(Number(el.value))}`); lastQuiet = now; }
    });
    el.addEventListener('change', () => {
      dragging = null;
      el.blur();
      if (!isOor()) drag.commit(name, `${name} ${toValue(Number(el.value))}`);
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
  scenarios.list().then(async (names) => {
    select.innerHTML = names.length
      ? names.map((n) => `<option value="${n}">${n.replace(/\.txt$/, '')}</option>`).join('')
      : '<option value="">(scenarios/ 없음)</option>';
    // 파일 첫 줄 '# ...'을 보이는 이름으로 (요구사항 묶음과 무엇을 보는지)
    for (const opt of select.options) {
      if (!opt.value) continue;
      try {
        const first = (await (await fetch('../scenarios/' + encodeURIComponent(opt.value))).text()).split(/\r?\n/)[0];
        if (first.startsWith('#')) { const t = first.replace(/^#\s*/, ''); opt.textContent = t.split(':')[0]; opt.title = t; }
      } catch { /* 이름 그대로 */ }
    }
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

  // 교통 장면: 지금 자차 위치·속도 기준으로 물체를 놓는다 (요구사항 8.4 obj 명령).
  // 차로는 이름(same·side·opp·curbr·curbl)으로 보내고 C 세계가 그 자리의 실제 차로로 바꾼다. 그 차로가 없으면 명령이 실패한다
  let egoMs = 0;   // 자차 속도 m/s (sync에서 갱신)
  const newestId = () => (getAdas()?.objs || []).reduce((m, o) => Math.max(m, o[0]), 0);
  const ok = (cmd) => exec(cmd) === 0;
  const info = (text) => log(text, 'info');
  // 물체: [id, 종류, 가운데 x, 가운데 y, 길이, ...] (자차 기준)
  const laneLead = () => {
    let best = null;
    for (const o of getAdas()?.objs || []) {
      const [, kind, xc, y, len] = o;
      if ((kind === 'car' || kind === 'truck') && Math.abs(y) < 1.3 && xc - len / 2 > 0 && (!best || xc < best[2])) best = o;
    }
    return best;
  };
  const SCENES = {
    jam() {
      exec('obj clear');
      for (const c of ['car 18 same 30', 'car 31 same 30', 'truck 44 same 30']) exec(`obj add ${c}`);
      ok('obj add car 12 side 33') && ok('obj add car 36 side 31');
      ok('obj add car 120 opp -40');
      info('▶ 정체 행렬: 내 차로 앞에 차 3대가 30 km/h. G(스위치) → S(SET)로 Stop & Go를 켜고, 아래 "앞차 멈춤/출발"로 정체를 만들어 보세요 (XS4~XS7)');
    },
    cutin() {
      const v = egoMs, lead = Math.max(30, v * 2.5 + 20);
      exec('obj clear');
      exec(`obj add car ${lead.toFixed(0)} same ${Math.max(25, v * 3.6).toFixed(0)}`);
      if (!ok(`obj add car -6 side ${Math.max(30, v * 3.6 + 15).toFixed(0)}`)) {
        info('  ! 여기에는 같은 방향 옆 차로가 없어 끼어들기를 만들 수 없습니다. 직선 도로(?map=straight)나 능동로·광나루로 구간에서 해 보세요');
        return;
      }
      exec(`obj ${newestId()} lane same 1.2 at 8`);
      info('▶ 끼어들기: 옆 차로 차가 내 차를 추월해 8 m 앞서면 내 차로로 들어옵니다. 0.3초 뒤 선행 차량이 그 차로 바뀌는지 보세요 (XM9)');
    },
    ped() {
      const v = egoMs;
      if (v < 1) {   // 서 있을 때: 바로 앞에서 건넌다 → 앞차가 떠나도 출발하지 않음
        exec('obj add ped 2 -3 0');
        exec(`obj ${newestId()} lane curbl 1.2`);
        info('▶ 보행자 횡단(정지 중): 바로 앞을 건너는 동안 Stop & Go는 다시 출발하지 않습니다 (XS9). 앞차가 있으면 "출발"을 눌러 보세요');
        return;
      }
      // 달릴 때: 경로에 닿기까지 약 4초, 그동안 자차가 가는 거리 + 편하게 설 거리 앞에서 건너기 시작
      const at = v * 4.2 + 8 + (v * v) / 4;
      exec(`obj add ped ${(at + 25).toFixed(0)} curbr 0`);
      exec(`obj ${newestId()} lane curbl 1.3 at ${at.toFixed(0)}`);
      info(`▶ 보행자 횡단: 오른쪽 길가 보행자가 내 차가 ${at.toFixed(0)} m 안으로 오면 건너기 시작합니다. Stop & Go가 속도를 줄이거나 서는지 보세요 (XM9, XS5)`);
    },
    bridge() {
      exec('obj clear');
      for (const c of ['manhole 30 same', 'sign 55 same', 'bridge 90 same', 'car 94 same 0']) exec(`obj add ${c}`);
      info('▶ 높이 거르기: 맨홀(바닥 0.03 m)·문형 표지판(5~6.5 m)·육교(4.5~5.5 m)는 4D 레이더가 높이로 걸러 무시하고(회색 상자), 육교 아래 서 있는 차(노란 상자)에만 섭니다 (XM6)');
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
    egoMs = s.in.speed_mkmh / 3600;
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
