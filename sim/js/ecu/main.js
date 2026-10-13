// main.js - ECU 테스트 시뮬레이터 화면: 부팅, 시간 진행, TC 재생, 렌더 루프 (spec 4장).
// 화면은 판단하지 않는다: 명령 문자열을 엔진(modcon WASM)에 보내고, state JSON을 그린다.
import { createEngine } from './engine.js';
import { TcRunner } from './runner.js';
import { createDiagram, FLOW_STAGE_MS } from './diagram.js';
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
const FLOW_TAIL_MS = 3 * FLOW_STAGE_MS; // 시나리오가 끝난 뒤에도 흐름 표시를 유지하는 시간 (입력 → SWC → ECU 세 단계가 끝나도록)
const FLOW_TAIL_DRIVE_MS = 7 * FLOW_STAGE_MS; // drive on이면 운전 조작 → 통합 ECU → drive.c → ego → 입력 단계가 더 있다 (R1~R5)

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

// REAL SIM에서 가져온 시연 시나리오 (ECU 콘솔 전용). 목록 파일 한 줄에 파일 이름 하나 (tools/consoles/scenarios/list.txt)
const SCENARIO_DIR = 'scenarios';
let imported = [];
try {
  const res = await fetch(`../tools/consoles/${SCENARIO_DIR}/list.txt`);
  if (!res.ok) throw new Error(String(res.status));
  imported = (await res.text()).split('\n').map((l) => l.trim()).filter((l) => l && !l.startsWith('#'));
} catch (err) {
  log(`  ! 시나리오 목록을 읽지 못했습니다: tools/consoles/${SCENARIO_DIR}/list.txt (${err.message || err})`, 'err');
}

const runner = new TcRunner(engine, log);
// playing: 시간이 흐르는 중. tc: TC를 불러 둔 상태 (일시정지해도 위치 유지, ■나 TC 끝에서만 해제)
// flowTail: 재생으로 끝난 시나리오의 흐름 표시를 이 벽시계 시각까지 유지 (다른 조작을 하면 0)
const sim = { playing: false, rate: 1, carry: 0, console: 'ecu', tc: false, flowTail: 0 };

function exec(cmd) {
  sim.flowTail = 0; // 입력 패널 조작은 지금처럼 깜박임으로 보인다
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

// 시나리오 목록: (수동) + 그 콘솔의 TC 파일 (+ ECU면 가져온 시연 시나리오).
// option 값 = tools/consoles/ 아래 경로. 고른 것이 새 목록에도 있으면 유지, 없으면 수동/TC 종류만 유지
function fillScenarios() {
  const prev = scenarioSel.options.length > 0 ? scenarioSel.value : sim.console;
  scenarioSel.textContent = '';
  scenarioSel.append(new Option('(수동)', MANUAL));
  scenarioSel.append(new Option(`${sim.console}.txt`, `tc/${sim.console}.txt`));
  if (sim.console === 'ecu') for (const f of imported) scenarioSel.append(new Option(f, `${SCENARIO_DIR}/${f}`));
  const values = [...scenarioSel.options].map((o) => o.value);
  scenarioSel.value = values.includes(prev) ? prev : prev === MANUAL ? MANUAL : `tc/${sim.console}.txt`;
}

// ▶/∥ 한 버튼: 흐르는 중이면 ∥(일시정지), 멈춰 있으면 ▶(재생·재개)
function setPlaying(on) {
  sim.playing = on;
  sim.flowTail = 0;
  sim.carry = 0;
  playBtn.textContent = on ? '∥' : '▶';
  playBtn.title = on ? '일시정지 (시나리오 위치 유지)' : '재생 (수동이면 시간 흐름 시작, 일시정지 중이면 이어서)';
  playBtn.classList.toggle('active', on);
}

function selectConsole(name) {
  runner.stop();
  sim.tc = false;
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

// ▶/∥: 흐르는 중이면 일시정지, 불러 둔 TC가 있으면 그 자리에서 재개, 아니면 새로 시작
function togglePlay() {
  if (sim.playing) {
    setPlaying(false);
    log(runner.active ? '∥ 일시정지 (시나리오 위치 유지, ⏭ 다음 줄)' : '∥ 일시정지 (⏭ 판단 주기 하나)', 'info');
  } else if (runner.active) {
    setPlaying(true);
    log('▶ 이어서 재생', 'info');
  } else {
    play();
  }
}

async function play() {
  if (scenarioSel.value === MANUAL) {
    runner.stop();
    setPlaying(true);
    log('시간 흐름 시작 (벽시계 × 배속)', 'info');
    return;
  }
  const name = sim.console;
  const path = scenarioSel.value; // tc/<콘솔>.txt 또는 scenarios/<파일>
  const res = await fetch(`../tools/consoles/${path.split('/').map(encodeURIComponent).join('/')}`);
  if (!res.ok) {
    log(`  ! 시나리오 파일을 읽지 못했습니다: tools/consoles/${path} (${res.status})`, 'err');
    return;
  }
  const text = await res.text();
  if (name !== sim.console || path !== scenarioSel.value) return; // 읽는 사이 콘솔·시나리오가 바뀜
  runner.start(name, text, path.split('/').pop());
  sim.tc = true;
  setPlaying(true);
}

function stop() {
  const was = runner.active || sim.playing;
  runner.stop();
  sim.tc = false;
  setPlaying(false);
  if (was) log('■ 정지 (시나리오 끝냄, 시각 멈춤)', 'info');
}

// ⏭: 시나리오를 불러 두었으면 다음 줄(진행 중인 t가 있으면 그 t를 끝까지), 아니면 판단 주기 하나(step_ms).
// 일시정지 중 ⏭로 1ms짜리 TC 시점(TC-ECU-L6-02, C4-02 등)을 한 줄씩 본다. 누를 때마다 바로 다시 그린다
function step() {
  sim.flowTail = 0;
  if (runner.active) runner.next();
  else engine.advance(engine.state().step_ms || 1);
  draw();
}

consoleSel.addEventListener('change', () => selectConsole(consoleSel.value));
rateSel.addEventListener('change', () => setRate(Number(rateSel.value)));
playBtn.addEventListener('click', togglePlay);
$('stop').addEventListener('click', stop);
// 시나리오를 바꾸면 이전 모드(재생 중이던 TC)를 이어 가지 않도록 정지
scenarioSel.addEventListener('change', stop);
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

// ---- 시작: URL ?console=ecu&tc=1&speed=10 (발표용·확인용). tc=<파일 이름>이면 그 시나리오 (예: tc=Z01_L_lighting.txt) ----
const params = new URLSearchParams(location.search);
const want = params.get('console');
fillScenarios();
selectConsole(CONSOLES.some(([n]) => n === want) ? want : 'ecu');
const speed = Number(params.get('speed'));
setRate(speed > 0 ? speed : 1);
loading.remove();
log('준비됨. 콘솔과 시나리오를 고르고 ▶ (수동이면 시간만 흐름). ∥ 일시정지 중 ⏭는 한 줄씩. 입력 패널 조작은 콘솔 명령으로 실행됩니다', 'info');
const tcParam = params.get('tc');
if (tcParam && tcParam !== '0') {
  // 목록에 있는 파일 이름이면 그것, 아니면(tc=1 등) 그 콘솔의 TC 파일
  const opt = [...scenarioSel.options].find((o) => o.value !== MANUAL && o.textContent === tcParam);
  if (!opt && tcParam !== '1') log(`  ! 이 콘솔의 시나리오 목록에 없습니다: ${tcParam} (${sim.console}.txt 재생)`, 'err');
  scenarioSel.value = opt ? opt.value : `tc/${sim.console}.txt`;
  play();
}

// 상태를 읽어 그린다. TC가 끝났으면 시각을 멈춰 끝 상태를 보여 준다
function draw() {
  const now = performance.now();
  // 시나리오가 흐르는 중에만 데이터 흐름 표시 (수동·일시정지 ⏭는 지금처럼 깜박임).
  // 끝나기 전에 정한다: 마지막 t·남은 줄·파일 끝이 한 프레임에 실행되어도 그 변화는 흐름으로 보인다
  const playing = sim.tc && sim.playing;
  if (sim.tc && !runner.active) {
    sim.tc = false;
    setPlaying(false);
    if (playing) sim.flowTail = now + (engine.state().drive?.on ? FLOW_TAIL_DRIVE_MS : FLOW_TAIL_MS); // 진행 중인 흐름(SWC·ECU 단계)이 끝까지 가도록
  }
  const st = engine.state();
  renderBar(st);
  diagram.render(st, { flow: playing || now < sim.flowTail });
  panel.render(st);
}

// ---- 루프: 벽시계 × 배속만큼 시뮬 시간을 흘리고, 상태를 읽어 그린다 ----
let last = performance.now();
function frame(now) {
  const dt = Math.max(0, Math.min(now - last, MAX_FRAME_MS)); // 음수 진행 방어
  last = now;
  if (sim.playing) advance(dt * sim.rate, engine.state());
  draw();
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
