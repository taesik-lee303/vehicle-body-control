// engine.js - ECU 시뮬레이터 엔진: modcon(C)을 WebAssembly로 부른다. 판정은 모두 C가 한다.
// core-wasm.js와 같은 모양: print/printErr 한 줄씩을 onLog 핸들러로 보낸다.
import createEcu from '../../build/ecu.js';

export async function createEngine() {
  let logHandler = () => {};
  const mod = await createEcu({ print: (s) => logHandler(s), printErr: (s) => logHandler(s) });
  const select = mod.cwrap('ecu_select', 'number', ['string']);
  const exec = mod.cwrap('ecu_exec', 'number', ['string']);
  const engine = {
    select: (name) => select(name),       // 0 OK, 1 없는 이름. 성공하면 처음 상태로 시작
    exec: (line) => exec(line),           // 0 OK, 1 오류, 2 quit
    advance: (ms) => mod._ecu_advance(ms), // 바뀐 출력·판정 수
    state: () => JSON.parse(mod.UTF8ToString(mod._ecu_state())),
    finish: () => mod._ecu_finish(),      // 요약 줄을 내고 FAIL이 있으면 1
    onLog: (fn) => { logHandler = fn; },
  };
  engine.select('ecu');
  return engine;
}
