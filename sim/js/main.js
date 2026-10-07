// main.js - M1: C 코어와 키보드 연결. 3D 화면·툴박스는 M2에서 추가 (docs/sim-plan.md).
// 화면은 판단하지 않는다: 명령을 보내고 sim_state() JSON을 그린다.
import { createCore } from './core-wasm.js';

const logEl = document.getElementById('log');
const log = (s) => { logEl.textContent += s + '\n'; logEl.scrollTop = logEl.scrollHeight; };

const api = await createCore();
api.onLog(log);

function exec(cmd) {
  log('> ' + cmd);
  if (api.exec(cmd) === 1) document.getElementById('err').textContent = '명령 오류: ' + cmd;
}

api.init(0);
exec('drive on');

// ---- 키보드 ----
const keys = new Set();
let lastPedal = '';
const LIGHT = ['off', 'auto', 'on'];
const WIPER = ['off', 'auto', 'low', 'high'];
const DOORS = ['fl', 'fr', 'rl', 'rr'];
let paused = false, rate = 1;
const RATES = [0.25, 0.5, 1, 2];

function updatePedal() {
  const throttle = keys.has('Shift') ? 2 : keys.has('ArrowUp') ? 1 : 0;   // Shift 단독도 풀 스로틀
  const brake = keys.has('ArrowDown') ? 1 : 0;
  const p = `pedal ${throttle} ${brake}`;
  if (p !== lastPedal) { api.exec(p); lastPedal = p; }   // 페달은 로그에 남기지 않음 (자주 바뀜)
}

addEventListener('keydown', (e) => {
  if (e.target.tagName === 'INPUT') return;
  keys.add(e.key);
  if (e.repeat) { updatePedal(); return; }
  const s = state;
  const k = e.key.toLowerCase();
  if (k === 'i') exec(`ign ${s.in.ign ? 'off' : 'on'}`);
  else if ('prnd'.includes(k) && k.length === 1) exec(`gear ${k}`);
  else if (k === 'l') exec(`light ${LIGHT[(LIGHT.indexOf(s.in.light.toLowerCase()) + 1) % 3]}`);
  else if (k === 'w') exec(`wiper ${WIPER[(WIPER.indexOf(s.in.wiper.toLowerCase()) + 1) % 4]}`);
  else if ('1234'.includes(k) && k.length === 1) {
    const i = Number(k) - 1;
    exec(`door ${DOORS[i]} ${s.in.doors[i] ? 'close' : 'open'}`);
  } else if (k === ' ') { paused = !paused; log(paused ? '일시정지' : '재개'); }
  else if (k === '.' && paused) api.advance(50);                     // 일시정지 중 한 틱
  else if (k === '[' || k === ']') {
    rate = RATES[Math.max(0, Math.min(3, RATES.indexOf(rate) + (k === ']' ? 1 : -1)))];
    log(`배속 ${rate}×`);
  }
  if (e.key.startsWith('Arrow') || k === ' ') e.preventDefault();
  updatePedal();
});
addEventListener('keyup', (e) => { keys.delete(e.key); updatePedal(); });
addEventListener('blur', () => { keys.clear(); updatePedal(); });  // 창을 떠나면 페달 놓기

document.getElementById('cmdform').addEventListener('submit', (e) => {
  e.preventDefault();
  const input = document.getElementById('cmd');
  if (input.value.trim()) exec(input.value.trim());
  input.value = '';
});

// ---- 시간 진행: 벽시계 × 배속만큼 시뮬레이션 시간을 흘린다 ----
let state = api.state();
let last = performance.now();
let carry = 0;
function frame(now) {
  const dt = Math.min(now - last, 250); last = now;           // 탭이 멈췄다 돌아와도 한꺼번에 몰아치지 않게
  if (!paused) {
    carry += dt * rate;
    const ms = Math.floor(carry); carry -= ms;
    if (ms > 0) api.advance(ms);
  }
  state = api.state();
  render(state);
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);

// ---- 표시 ----
const yes = (v, cls = 'on') => v ? `<span class="${cls}">켜짐</span>` : '꺼짐';
function rows(el, list) { el.innerHTML = list.map(([k, v]) => `<tr><td>${k}</td><td>${v}</td></tr>`).join(''); }
function render(s) {
  const doors = ['FL', 'FR', 'RL', 'RR'].map((n, i) => s.in.doors[i] ? `<span class="bad">${n}</span>` : n).join(' ');
  rows(document.getElementById('in'), [
    ['시각', `${(s.t / 1000).toFixed(2)} s ${paused ? '(일시정지)' : rate !== 1 ? `(${rate}×)` : ''}`],
    ['시동 · 기어', `${s.in.ign ? '<span class="on">ON</span>' : 'OFF'} · ${s.in.gear}`],
    ['속도', `${(s.in.speed_mkmh / 1000).toFixed(1)} km/h`],
    ['페달', `스로틀 ${s.in.throttle} · 브레이크 ${s.in.brake}`],
    ['조도 · 강우', `${s.in.lux} lux · ${s.in.rain} %`],
    ['도어 열림', doors],
    ['조명 SW · 와이퍼 SW', `${s.in.light} · ${s.in.wiper}`],
    ['송신 중단', s.in.cut ? `<span class="bad">${s.in.cut}</span>` : '없음'],
  ]);
  rows(document.getElementById('out'), [
    ['전조등', yes(s.out.headlight)],
    ['와이퍼', s.out.wiper],
    ['도어 잠금', s.out.locked ? '<span class="on">잠김</span>' : '풀림'],
    ['계기판 경고', s.out.warning === 'none' ? '없음' : `<span class="bad">${s.out.warning}</span>`],
    ['조도 고장 표시', yes(s.out.lux_lamp, 'bad')],
    ['강우 고장 표시', yes(s.out.rain_lamp, 'bad')],
    ['(판정) 수신 중단', s.ctx.rx_lost ? '<span class="bad">예</span>' : '아니오'],
    ['(판정) 유효한 속도', s.ctx.speed_valid ? '<span class="ok">예</span>' : '아니오'],
  ]);
}
