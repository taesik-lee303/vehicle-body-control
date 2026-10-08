// core-wasm.js - 전송 계층(1안): C 코어를 WebAssembly로 브라우저 안에서 부른다.
// 같은 인터페이스 {init, exec, advance, state, onLog}를 2안(core-ws.js)도 따른다.
import createVbc from '../build/vbc.js';

export async function createCore() {
  let logHandler = () => {};
  const mod = await createVbc({ print: (s) => logHandler(s), printErr: (s) => logHandler(s) });
  const exec = mod.cwrap('sim_exec', 'number', ['string']);
  return {
    init: (quiet = false) => mod._sim_init(quiet ? 1 : 0),
    exec: (line) => exec(line),             // 0 OK, 1 오류, 2 quit
    advance: (ms) => mod._sim_advance(ms),
    state: () => JSON.parse(mod.UTF8ToString(mod._sim_state())),
    // 레이더·Stop & Go: 맵·트랙·세계 물체 JSON, 맵이 차량 좌표로 바꾼 검출점 (점마다 6개: 센서 x y z 도플러 표시)
    adas: () => JSON.parse(mod.UTF8ToString(mod._sim_adas())),
    points: () => {
      const ptr = mod._sim_points() >> 2, n = mod._sim_points_count();
      return mod.HEAPF32.slice(ptr, ptr + n * 6);
    },
    onLog: (fn) => { logHandler = fn; },
  };
}
