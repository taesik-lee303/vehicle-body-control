// main.js - ECU 테스트 시뮬레이터 화면: 부팅, 시간 진행, TC 재생, 렌더 루프 (spec 4장).
// 화면은 판단하지 않는다: 명령 문자열을 엔진(modcon WASM)에 보내고, state JSON을 그린다.
import { createEngine } from './engine.js';
import { TcRunner } from './runner.js';
import { createDiagram } from './diagram.js';
import { createPanel } from './panel.js';

const CONSOLES = [
  ['ecu', 'ECU'],
  ['doors', '도어'],
  ['dashboard', '계기판'],
  ['lighting', '조명'],
  ['wipers', '와이퍼'],
];
const RATES = [0.25, 0.5, 1, 2, 5, 10];
const MANUAL = ''; // 시나리오 select의 (수동)
const MAX_FRAME_MS = 100; // 탭이 멈췄다 돌아와도 시간을 한꺼번에 몰아치지 않게 (sim/js/main.js와 같음)

const $ = (id) => document.getElementById(id);

// ---- 이벤트 로그 (sim/js/main.js와 같은 규칙) ----
const logEl = $('log');
function classify(line) {
  const t = line.trim();
  if (t.startsWith('>')) return 'cmd';
  if (t.startsWith('✔')) return 'ok';
  if (t.startsWith('✘')) return 'bad';
  if (t.startsWith('!')) return 'err';
  if (t.startsWith('#') || t.startsWith('==')) return 'info';
  return 'change';
}
function log(text, cls = classify(text)) {
  const div = document.createElement('div');
  div.className = cls;
  div.textContent = text;
  logEl.append(div);
  while (logEl.childElementCount > 600) logEl.firstChild.remove();
  logEl.scrollTop = logEl.scrollHeight;
}
$('log-clear').addEventListener('click', () => { logEl.textContent = ''; });

// ---- 부팅 ----
const loading = $('loading');
let engine;
try {
  engine = await createEngine();
} catch (err) {
  loading.classList.add('error');
  loading.textContent = `ECU 엔진을 불러오지 못했습니다.\n${err}\n\n저장소 루트에서 make sim-ecu로 실행했는지, sim/build/ecu.js가 있는지 확인하세요 (make wasm-ecu).`;
  throw err;
}
engine.onLog((line) => log(line));

const runner = new TcRunner(engine, log);
const sim = { playing: false, rate: 1, carry: 0, console: 'ecu', tc: false }; // tc: TC 재생으로 시작한 흐름

function exec(cmd) {
  log('> ' + cmd, 'cmd');
  return engine.exec(cmd);
}

const diagram = createDiagram($('diagram'));
const panel = createPanel($('panel'), exec);

// ---- 상단 바 ----
const consoleSel = $('console');
const scenarioSel = $('scenario');
const rateSel = $('rate');
const playBtn = $('play');
for (const [name, label] of CONSOLES) consoleSel.append(new Option(label, name));
for (const r of RATES) rateSel.append(new Option(`${r}×`, String(r)));

// 시나리오 목록: (수동) + 그 콘솔의 TC 파일. 고른 종류(수동/TC)는 콘솔을 바꿔도 유지
function fillScenarios() {
  const manual = scenarioSel.value === MANUAL && scenarioSel.options.length > 0;
  scenarioSel.textContent = '';
  scenarioSel.append(new Option('(수동)', MANUAL));
  scenarioSel.append(new Option(`${sim.console}.txt`, sim.console));
  scenarioSel.value = manual ? MANUAL : sim.console;
}

function setPlaying(on, tc = false) {
  sim.playing = on;
  sim.tc = on && tc;
  sim.carry = 0;
  playBtn.classList.toggle('active', on);
}

function selectConsole(name) {
  runner.stop();
  setPlaying(false);
  if (engine.select(name) !== 0) {
    log(`  ! 없는 콘솔입니다: ${name}`, 'err');
    return;
  }
  sim.console = name;
  consoleSel.value = name;
  fillScenarios();
  log(`== 콘솔: ${engine.state().title} (처음 상태, 시각 0)`, 'info');
}

function setRate(r) {
  if (!(r > 0)) return;
  if (!RATES.includes(r)) rateSel.append(new Option(`${r}×`, String(r)));
  sim.rate = r;
  rateSel.value = String(r);
}

async function play() {
  if (scenarioSel.value === MANUAL) {
    runner.stop();
    setPlaying(true);
    log('시간 흐름 시작 (벽시계 × 배속)', 'info');
    return;
  }
  const name = sim.console;
  const res = await fetch(`../tools/consoles/tc/${encodeURIComponent(name)}.txt`);
  if (!res.ok) {
    log(`  ! TC 파일을 읽지 못했습니다: tools/consoles/tc/${name}.txt (${res.status})`, 'err');
    return;
  }
  const text = await res.text();
  if (name !== sim.console) return; // 읽는 사이 콘솔이 바뀜
  runner.start(name, text);
  setPlaying(true, true);
}

function stop() {
  const was = runner.active || sim.playing;
  runner.stop();
  setPlaying(false);
  if (was) log('■ 정지 (시각 멈춤)', 'info');
}

// ⏭: 재생 중이면 다음 줄, 아니면 판단 주기 하나(step_ms)
function step() {
  if (runner.active) runner.next();
  else engine.advance(engine.state().step_ms || 1);
}

consoleSel.addEventListener('change', () => selectConsole(consoleSel.value));
rateSel.addEventListener('change', () => setRate(Number(rateSel.value)));
playBtn.addEventListener('click', () => { play(); });
$('stop').addEventListener('click', stop);
$('step').addEventListener('click', step);

// ---- 시간 진행 ----
// 수동: 벽시계 × 배속을 step_ms 배수로 잘라 advance, 나머지는 다음 프레임으로 (판단 시각이 터미널과 같게)
function advance(ms, st) {
  if (runner.active) {
    runner.update(ms);
    return;
  }
  const stepMs = st.step_ms || 1;
  sim.carry += ms;
  const n = Math.floor(sim.carry / stepMs) * stepMs;
  if (n > 0) {
    engine.advance(n);
    sim.carry -= n;
  }
}

// ---- 상태 표시 ----
const clockEl = $('clock');
const tcEl = $('tc-id');
const scoreEl = $('score');
function put(e, s) {
  if (e.textContent !== s) e.textContent = s;
}
function renderBar(st) {
  put(clockEl, `t = ${Math.floor(st.t / 1000)}.${String(st.t % 1000).padStart(3, '0')} s`);
  put(tcEl, st.tc.id || '');
  const tc = st.tc;
  put(scoreEl, `PASS ${tc.checks - tc.failures} / FAIL ${tc.failures}`);
  scoreEl.classList.toggle('bad', tc.failures > 0);
  scoreEl.title = `확인 ${tc.checks}건 기준. TC ${tc.count}개 (FAIL 있는 TC ${tc.failed}개, 끝난 TC 기준), 관찰 ${tc.observations}건`;
}

// ---- 시작: URL ?console=ecu&tc=1&speed=10 (발표용·확인용) ----
const params = new URLSearchParams(location.search);
const want = params.get('console');
fillScenarios();
selectConsole(CONSOLES.some(([n]) => n === want) ? want : 'ecu');
const speed = Number(params.get('speed'));
setRate(speed > 0 ? speed : 1);
loading.remove();
log('준비됨. 콘솔과 시나리오를 고르고 ▶ (수동이면 시간만 흐름). 입력 패널 조작은 콘솔 명령으로 실행됩니다', 'info');
const tcParam = params.get('tc');
if (tcParam && tcParam !== '0') {
  scenarioSel.value = sim.console;
  play();
}

// ---- 루프: 벽시계 × 배속만큼 시뮬 시간을 흘리고, 상태를 읽어 그린다 ----
let last = performance.now();
function frame(now) {
  const dt = Math.min(now - last, MAX_FRAME_MS);
  last = now;
  if (sim.playing) advance(dt * sim.rate, engine.state());
  if (sim.tc && !runner.active) setPlaying(false); // TC가 끝나면 시각을 멈춰 끝 상태를 보여 준다
  const st = engine.state();
  renderBar(st);
  diagram.render(st);
  panel.render(st);
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
