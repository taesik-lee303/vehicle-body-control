// panel.js - 입력 패널. 조작을 콘솔 명령 문자열(`speed 15`, `cut lux`)로 만들어 exec에 넘긴다 (spec 4장).
// 수동 조작과 TC 재생이 같은 경로(엔진 exec)를 쓴다. 값 검사는 하지 않는다 (테스트 도구는 입력을 거르지 않음).

// 신호별 표시. words: 버튼 이름 = 명령 단어, 순서 = 코드 (modcon.c 별칭과 같음)
const DEFS = {
  ign: { label: '시동', words: ['off', 'on'], periodic: true },
  gear: { label: '기어', words: ['p', 'r', 'n', 'd'], periodic: true },
  speed: { label: '속도 (km/h)', num: true, periodic: true },
  lux: { label: '조도 (lx)', num: true, periodic: true },
  rain: { label: '강우량 (%)', num: true, periodic: true },
  light: { label: '조명 스위치', words: ['off', 'auto', 'on'], get: (i) => i.light_sw },
  wiper: { label: '와이퍼 스위치', words: ['off', 'auto', 'low', 'high'], get: (i) => i.wiper_sw },
  level: { label: '와이퍼 단계 (L6 주입)', words: ['stop', 'int', 'low', 'high'], get: (i) => i.level },
};
const DOORS = ['fl', 'fr', 'rl', 'rr'];
const VEHICLE = ['ign', 'gear', 'speed']; // cut vehicle = 속도·기어·시동

function h(tag, attrs, parent, textContent) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) e.setAttribute(k, String(v));
  if (textContent !== undefined) e.textContent = textContent;
  if (parent) parent.appendChild(e);
  return e;
}

function put(e, s) {
  if (e.textContent !== s) e.textContent = s;
}

export function createPanel(el, exec) {
  let key = '';
  let syncs = []; // 매 렌더마다 state → 컨트롤 반영

  function value(name, st) {
    const d = DEFS[name];
    return d.periodic ? st.in[name].tx : d.get(st.in);
  }

  // 주기 신호 ✂ 토글: cut x / resume x
  function cutButton(parent, name, title) {
    const b = h('button', { class: 'cut', title: `${title} 송신 끊기/재개 (cut/resume ${name})` }, parent, '✂');
    let cut = false;
    b.addEventListener('click', () => exec(`${cut ? 'resume' : 'cut'} ${name}`));
    return (on) => {
      cut = on;
      b.classList.toggle('active', on);
    };
  }

  function signalGroup(name) {
    const d = DEFS[name];
    const g = h('div', { class: 'group' }, el);
    const head = h('div', { class: 'label' }, g);
    h('span', {}, head, `${d.label} · ${name}`);
    const out = h('output', {}, head);
    const row = h('div', { class: 'row' }, g);
    let setCut = null;
    let sync;
    if (d.num) {
      const input = h('input', { type: 'number', step: 1 }, row);
      input.addEventListener('change', () => {
        if (input.value.trim() !== '') exec(`${name} ${input.value.trim()}`);
      });
      sync = (v) => {
        if (document.activeElement !== input && input.value !== String(v)) input.value = String(v);
      };
    } else {
      const seg = h('div', { class: 'seg' }, row);
      const buttons = d.words.map((w) => {
        const b = h('button', {}, seg, w.toUpperCase());
        b.addEventListener('click', () => exec(`${name} ${w}`));
        return b;
      });
      sync = (v) => buttons.forEach((b, i) => b.classList.toggle('active', i === v));
    }
    if (d.periodic) setCut = cutButton(row, name, d.label);
    syncs.push((st) => {
      const v = value(name, st);
      put(out, d.periodic && st.in[name].cut ? `${v} (끊김)` : String(v));
      sync(v);
      if (setCut) setCut(st.in[name].cut);
    });
  }

  function doorGroup(names) {
    const g = h('div', { class: 'group' }, el);
    const head = h('div', { class: 'label' }, g);
    h('span', {}, head, '도어 (체크 = 열림)');
    const out = h('output', {}, head);
    const row = h('div', { class: 'checks' }, g);
    const boxes = names.map((n) => {
      const label = h('label', {}, row);
      const cb = h('input', { type: 'checkbox' }, label);
      label.append(` ${n.toUpperCase()}`);
      cb.addEventListener('change', () => exec(`${n} ${cb.checked ? 'open' : 'close'}`));
      return { n, cb, i: DOORS.indexOf(n) };
    });
    syncs.push((st) => {
      put(out, boxes.map((b) => st.in.doors[b.i]).join(' '));
      for (const b of boxes) {
        const v = st.in.doors[b.i];
        b.cb.checked = v === 1;
        b.cb.indeterminate = v !== 0 && v !== 1; // 정의 밖 코드는 그대로 두고 표시만
      }
    });
  }

  // 차량 상태(속도·기어·시동) 한꺼번에 끊기 (C4 시연용)
  function vehicleGroup() {
    const g = h('div', { class: 'group' }, el);
    const row = h('div', { class: 'row' }, g);
    h('span', { class: 'dim' }, row, '차량 상태 한꺼번에');
    const setCut = cutButton(row, 'vehicle', '차량 상태(속도·기어·시동)');
    syncs.push((st) => setCut(VEHICLE.every((n) => st.in[n].cut)));
  }

  // 모듈 상태 토글 (도어 콘솔: lock on|off)
  function stateGroup(s) {
    const g = h('div', { class: 'group' }, el);
    const head = h('div', { class: 'label' }, g);
    h('span', {}, head, `${s.label} · ${s.cmd}`);
    const out = h('output', {}, head);
    const seg = h('div', { class: 'seg' }, g);
    const buttons = s.words.map((w, i) => {
      const b = h('button', {}, seg, w);
      b.addEventListener('click', () => exec(`${s.cmd} ${i ? 'on' : 'off'}`));
      return b;
    });
    syncs.push((st) => {
      put(out, s.words[st.state.value] ?? String(st.state.value));
      buttons.forEach((b, i) => b.classList.toggle('active', i === st.state.value));
    });
  }

  // 주행 모델 (ECU 콘솔, R1~R5): drive on|off 토글과 페달 (pedal <스로틀 0|1|2> <브레이크 0|1>).
  // 페달은 누르고 있는 상태라 스로틀을 바꿀 때 지금 브레이크를, 브레이크를 바꿀 때 지금 스로틀을 함께 보낸다
  function driveGroup() {
    const THROTTLE = [['none', '없음'], ['normal', '보통'], ['full', '풀']];
    let cur = { throttle: 0, brake: 0 };
    const g = h('div', { class: 'group drive' }, el);
    const head = h('div', { class: 'label' }, g);
    h('span', {}, head, '주행 모델 (R1~R5) · drive');
    const out = h('output', {}, head);
    const seg = h('div', { class: 'seg' }, g);
    const onOff = ['off', 'on'].map((w) => {
      const b = h('button', {}, seg, w.toUpperCase());
      b.addEventListener('click', () => exec(`drive ${w}`));
      return b;
    });
    const phead = h('div', { class: 'label' }, g);
    h('span', {}, phead, '페달 · pedal');
    const pout = h('output', {}, phead);
    const row = h('div', { class: 'row' }, g);
    const tseg = h('div', { class: 'seg' }, row);
    const tbtn = THROTTLE.map(([, label], i) => {
      const b = h('button', { title: `스로틀 ${i} (pedal ${i} <브레이크>)` }, tseg, label);
      b.addEventListener('click', () => exec(`pedal ${i} ${cur.brake}`));
      return b;
    });
    const brake = h('button', { class: 'brake', title: '브레이크 밟기/떼기 (pedal <스로틀> 1|0)' }, row, '브레이크');
    brake.addEventListener('click', () => exec(`pedal ${cur.throttle} ${cur.brake ? 0 : 1}`));
    const note = h('div', { class: 'dim' }, g);
    syncs.push((st) => {
      const d = st.drive;
      cur = { throttle: Math.max(0, THROTTLE.findIndex(([w]) => w === d.throttle)), brake: d.brake ? 1 : 0 };
      put(out, d.on ? `on · ${d.speed_kmh} km/h` : 'off');
      onOff.forEach((b, i) => b.classList.toggle('active', i === (d.on ? 1 : 0)));
      put(pout, `스로틀 ${cur.throttle} · 브레이크 ${cur.brake}`);
      tbtn.forEach((b, i) => b.classList.toggle('active', d.on && i === cur.throttle));
      brake.classList.toggle('active', d.on && d.brake);
      g.classList.toggle('off', !d.on);
      put(note, d.on ? 'ign·gear는 정지 상태에서만 바뀜 (R1, R2). speed는 페달이 정함' : 'drive on이면 페달로 운전 (speed 직접 입력 대신)');
    });
  }

  function build(st) {
    el.textContent = '';
    syncs = [];
    h('h2', {}, el, '입력 패널');
    const sigs = new Set(st.signals);
    for (const name of st.signals) {
      if (DEFS[name]) signalGroup(name);
      else if (name === DOORS.find((n) => sigs.has(n))) doorGroup(DOORS.filter((n) => sigs.has(n)));
    }
    if (VEHICLE.every((n) => sigs.has(n))) vehicleGroup();
    if (st.drive) driveGroup();
    if (st.state) stateGroup(st.state);
  }

  function render(st) {
    const k = `${st.console}|${st.signals.join(',')}|${st.state ? st.state.cmd : ''}|${st.drive ? 'drive' : ''}`;
    if (k !== key) {
      key = k;
      build(st);
    }
    for (const s of syncs) s(st);
  }

  return { render };
}
