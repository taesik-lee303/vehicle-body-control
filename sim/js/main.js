// main.js - 부팅, 시간 진행, 렌더 루프 (docs/sim-plan.md 3절).
// 화면은 판단하지 않는다: 명령 문자열을 코어에 보내고, sim_state() JSON을 그린다.
import { createCore } from './core-wasm.js';
import { setupInput } from './input.js';
import { setupToolbox } from './toolbox.js';
import { renderHud } from './hud.js';
import { ScenarioRunner, listScenarios } from './scenario.js';
import { createWorld } from './scene/world.js';
import { setupMinimap } from './minimap.js';
import { createRadarPanel } from './radar-panel.js';

const RATES = [0.25, 0.5, 1, 2];
const TICK_MS = 50;
const MAX_FRAME_MS = 100;   // 탭이 멈췄다 돌아와도 시간을 한꺼번에 몰아치지 않게

// ---- 이벤트 로그 ----
const logEl = document.getElementById('log');
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
document.getElementById('log-clear').addEventListener('click', () => { logEl.textContent = ''; });

// ---- 부팅 ----
const loading = document.getElementById('loading');
let core;
try {
  core = await createCore();
} catch (err) {
  loading.classList.add('error');
  loading.textContent = `C 코어를 불러오지 못했습니다.\n${err}\n\n저장소 루트에서 make sim 으로 실행했는지, sim/build/vbc.js가 있는지 확인하세요 (make wasm).`;
  throw err;
}
core.onLog((line) => log(line));

const sim = { paused: false, rate: 1, carry: 0 };
let state, adas = null;
let crash = null;   // 건물·지도 끝에 막혀 브레이크를 걸고 있는 중이면 그 이유

function exec(cmd) { log('> ' + cmd, 'cmd'); return core.exec(cmd); }
function sendQuiet(cmd) { return core.exec(cmd); }   // 페달·슬라이더 드래그처럼 자주 바뀌는 것은 로그에 남기지 않음
// 레이더 센서 모델·전방 맵·Stop & Go를 켠다. 실제 지도에는 가드레일이 없으므로 센서 모델의 가드레일 반사를 끈다
function adasOn() {
  sendQuiet('adas on');
  if (mapData) sendQuiet('adas rail off');
}

const scenario = new ScenarioRunner(core, log, () => {
  if (document.getElementById('drive').checked) sendQuiet('drive on');   // 시나리오가 끝나면 다시 운전 가능하게
  adasOn();
  input.resync();
});

// 시뮬레이션 시간을 ms 진행. 시나리오 재생 중이면 시나리오가 wait 남은 시간만큼 쓴다
function advance(ms) {
  if (scenario.active) { scenario.update(ms); return; }
  sim.carry += ms;
  const whole = Math.floor(sim.carry);   // 코어는 정수 ms만 받음. 소수는 모아 둔다
  sim.carry -= whole;
  if (whole > 0) core.advance(whole);
}

const actions = {
  togglePause() { sim.paused = !sim.paused; log(sim.paused ? '일시정지 (시뮬 시각 멈춤)' : '재개', 'info'); },
  tick() { if (sim.paused) advance(TICK_MS); },
  setRate(r) { sim.rate = r; log(`배속 ${r}×`, 'info'); },
  rateStep(d) { actions.setRate(RATES[Math.max(0, Math.min(RATES.length - 1, RATES.indexOf(sim.rate) + d))]); },
  reset() {
    scenario.stop();
    core.init(false);
    sim.carry = 0;
    log('== 리셋: 초기 상태(C5), 시각 0', 'info');
    if (document.getElementById('drive').checked) sendQuiet('drive on');
    crash = null;
    input.setForceBrake(false);
    world.resetPose();   // 차를 출발점으로
    adasOn();
    input.resync();
  },
  resyncInput: () => input.resync(),
  nextCamera() { log(`카메라: ${world.nextCamera()}`, 'info'); },
  toggleToolbox: () => toolbox.toggle(),
  toggleHelp() { const h = document.getElementById('help'); h.hidden = !h.hidden; },
  closeHelp() { document.getElementById('help').hidden = true; },
};

const input = setupInput({ exec, sendQuiet, getState: () => state, actions, getAdas: () => adas });
const toolbox = setupToolbox({
  exec, sendQuiet, sim, actions, getAdas: () => adas, getRadarShow: () => world.radarShow,
  scenarios: {
    list: () => listScenarios('../scenarios/'),
    async play(name) {
      const res = await fetch('../scenarios/' + encodeURIComponent(name));
      if (!res.ok) { log(`  ! 시나리오를 읽지 못했습니다: ${name}`, 'err'); return; }
      sim.carry = 0;
      if (sim.paused) actions.togglePause();
      scenario.start(name, await res.text());
    },
    stop: () => scenario.stop(),
  },
});
// URL로 화질·차 모델 지정: ?q=low|std|high&car=code|glb (기본: 저장된 화질, car.glb가 있으면 그것)
const params = new URLSearchParams(location.search);
let savedQuality = null;
try { savedQuality = localStorage.getItem('vbc.quality'); } catch { /* 저장소를 못 쓰면 기본값 */ }
// 지도: 기본은 세종대학교 주변 실제 도로 (sim/assets/map/sejong.json, sim/tools/fetch-map.py가 만듦).
// ?map=straight 이면 예전 직선 도로. 지도를 못 불러와도 직선 도로로 계속한다
const mapName = params.get('map') || 'sejong';
let mapData = null;
if (mapName !== 'straight') {
  loading.textContent = '지도 불러오는 중…';
  try {
    const res = await fetch(`assets/map/${encodeURIComponent(mapName)}.json`);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    mapData = await res.json();
  } catch (err) {
    log(`  ! 지도 ${mapName}을(를) 불러오지 못했습니다 (${err.message}). 직선 도로로 그립니다.`, 'err');
  }
}
const world = createWorld(document.getElementById('view'), {
  log, quality: params.get('q') || savedQuality || 'std', carModel: params.get('car') || 'auto', map: mapData,
  startAt: Number(params.get('at')) || 0 });   // ?at=1200: 경로의 1.2 km 지점에서 출발 (발표·확인용)
const minimap = mapData ? setupMinimap(document.getElementById('minimap'), mapData) : null;
const hudEl = document.getElementById('hud');
const radarPanel = createRadarPanel(document.getElementById('radar-panel'));

// 화질: 기본(그림자·블룸 없음) / 표준 / 높음. 고른 값은 이 브라우저에 기억
const qualityBtns = [...document.querySelectorAll('#quality [data-q]')];
function showQuality() { qualityBtns.forEach((b) => b.classList.toggle('active', b.dataset.q === world.quality())); }
qualityBtns.forEach((b) => b.addEventListener('click', () => {
  log(`화질: ${world.setQuality(b.dataset.q)}`, 'info');
  try { localStorage.setItem('vbc.quality', b.dataset.q); } catch { /* 저장 못 해도 이번 실행에는 적용됨 */ }
  showQuality();
  b.blur();
}));
showQuality();

core.init(false);
sendQuiet('drive on');
adasOn();
state = core.state();
adas = core.adas();
loading.remove();
log('준비됨. I 시동 → D 기어 → ↑ 가속. H 도움말', 'info');

// URL로 시작 장면 지정 (발표용·확인용): ?cmd=lux 300&cmd=rain 70&wait=3&scenario=R5_top_speed.txt&cam=1
for (const c of params.getAll('cmd')) exec(c);
if (params.has('wait')) core.advance(Math.round(Number(params.get('wait')) * 1000));
for (let i = 0; i < Number(params.get('cam') || 0); i++) actions.nextCamera();
if (params.has('scenario')) toolbox.playScenario(params.get('scenario'));

// ---- 루프: 벽시계 × 배속만큼 시뮬 시간을 흘리고, 상태를 읽어 그린다 ----
let last = performance.now();
function frame(now) {
  const dt = Math.min(now - last, MAX_FRAME_MS);
  last = now;
  if (!sim.paused) advance(dt * sim.rate);
  state = core.state();
  adas = core.adas();
  const pts = adas.on ? core.points() : null;
  document.body.classList.toggle('adas', !!adas.on);
  toolbox.sync(state, adas);
  renderHud(hudEl, state, sim, scenario.active ? scenario.name : '', adas);
  // 직접 운전(주행 모드, 시나리오 재생 아님)이면 ←→가 핸들. 속도 직접 입력·시나리오는 경로를 자동으로 따라간다.
  // 레이더 장면(물체가 있거나 Stop & Go가 켜짐)도 경로를 따라간다: 레이더·맵은 도로를 편 좌표에서 계산하기 때문
  const blocked = world.update(state, sim.paused ? 0 : (dt / 1000) * sim.rate, input.lateral(),
    { manual: state.in.drive && !scenario.active, adas, pts });
  // 충돌: 화면이 차를 막고, 코어에는 브레이크를 밟은 것으로 보낸다(R4 경로). 속도가 0이 되면 놓는다
  if (blocked && state.in.speed_mkmh > 0 && !crash) {
    crash = blocked;
    input.setForceBrake(true);
    log(`  ! 충돌: ${blocked === 'edge' ? '지도 끝' : '건물'} → 멈출 때까지 브레이크`, 'err');
  }
  if (crash && state.in.speed_mkmh === 0) { crash = null; input.setForceBrake(false); }
  minimap?.update(world.where(), now);
  radarPanel.render(adas, pts);
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
