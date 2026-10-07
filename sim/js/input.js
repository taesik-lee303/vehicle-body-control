// input.js - 키보드 → 콘솔 명령. 페달은 키 상태가 바뀔 때만 보낸다 (drive.c가 상태를 기억).
// ←→는 화면 연출(차선 안 좌우 이동)이라 코어로 보내지 않는다 (사양서에 조향 입력 없음).

const LIGHT = ['off', 'auto', 'on'];
const WIPER = ['off', 'auto', 'low', 'high'];
const DOORS = ['fl', 'fr', 'rl', 'rr'];

// 글자를 입력하는 곳에서는 단축키를 쓰지 않는다
function isTyping(el) {
  if (!el) return false;
  if (el.tagName === 'SELECT' || el.tagName === 'TEXTAREA') return true;
  return el.tagName === 'INPUT' && ['text', 'number', 'search'].includes(el.type);
}

export function setupInput({ exec, sendQuiet, getState, actions }) {
  const keys = new Set();
  let lastPedal = '';

  function updatePedal() {
    if (!getState().in.drive) { lastPedal = ''; return; }    // 주행 모드가 아니면 페달 없음
    const throttle = keys.has('Shift') ? 2 : keys.has('ArrowUp') ? 1 : 0;
    const brake = keys.has('ArrowDown') ? 1 : 0;
    const cmd = `pedal ${throttle} ${brake}`;
    if (cmd !== lastPedal) { sendQuiet(cmd); lastPedal = cmd; }
  }

  addEventListener('keydown', (e) => {
    if (isTyping(e.target)) return;
    if (e.key === 'Escape') { actions.closeHelp(); return; }
    if (e.key.startsWith('Arrow') || e.key === ' ' || e.key === 'Tab') e.preventDefault();  // 슬라이더·스크롤·포커스 이동 막기
    keys.add(e.key);
    updatePedal();
    if (e.repeat) return;

    const s = getState();
    const k = e.key.toLowerCase();
    if (k === 'i') exec(`ign ${s.in.ign ? 'off' : 'on'}`);
    else if (['p', 'r', 'n', 'd'].includes(k)) exec(`gear ${k}`);
    else if (k === 'l') exec(`light ${LIGHT[(LIGHT.indexOf(s.in.light.toLowerCase()) + 1) % LIGHT.length]}`);
    else if (k === 'w') exec(`wiper ${WIPER[(WIPER.indexOf(s.in.wiper.toLowerCase()) + 1) % WIPER.length]}`);
    else if (['1', '2', '3', '4'].includes(e.key)) {
      const i = Number(e.key) - 1;
      exec(`door ${DOORS[i]} ${s.in.doors[i] ? 'close' : 'open'}`);
    }
    else if (k === ' ') actions.togglePause();
    else if (k === '.') actions.tick();
    else if (k === '[') actions.rateStep(-1);
    else if (k === ']') actions.rateStep(+1);
    else if (k === 'c') actions.nextCamera();
    else if (k === 'tab') actions.toggleToolbox();
    else if (k === 'h') actions.toggleHelp();
  });

  addEventListener('keyup', (e) => { keys.delete(e.key); updatePedal(); });
  // 창을 떠나면 키를 다 놓은 것으로 (페달이 눌린 채 남지 않게)
  addEventListener('blur', () => { keys.clear(); updatePedal(); });

  return {
    lateral: () => (keys.has('ArrowRight') ? 1 : 0) - (keys.has('ArrowLeft') ? 1 : 0),
    resync: () => { lastPedal = ''; updatePedal(); },  // 리셋·주행 모드 전환 뒤 페달 상태를 다시 보냄
  };
}
