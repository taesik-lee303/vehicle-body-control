// wasm_check.mjs - 터미널 콘솔과 WASM 엔진(재생기 포함)의 출력이 같은지 확인한다 (C1~C5).
// 실행: make wasm-check  (build/con_*, sim/build/ecu.js 필요)
import { readFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { createEngine } from '../../sim/js/ecu/engine.js';
import { TcRunner } from '../../sim/js/ecu/runner.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const KEEP = /^  (→ \[|✔ |✘ |◎ |▶ )|^== /;
const CONSOLES = ['ecu', 'doors', 'dashboard', 'lighting', 'wipers'];

const keep = (lines) => lines.filter((l) => KEEP.test(l));

// 시드 고정 의사난수 (LCG) → 1~37ms
function makeBudget(seed) {
  let s = seed >>> 0;
  return () => {
    s = (Math.imul(s, 1664525) + 1013904223) >>> 0;
    return 1 + (s >>> 8) % 37;
  };
}

function terminalLines(name) {
  const text = readFileSync(path.join(root, 'tools/consoles/tc', `${name}.txt`));
  let out;
  try {
    out = execFileSync(path.join(root, 'build', `con_${name}`), { input: text, maxBuffer: 1 << 28 });
  } catch (e) {
    if (e.stdout) out = e.stdout; // FAIL이 있으면 종료 코드가 1이다
    else throw e;
  }
  return keep(out.toString('utf8').split('\n'));
}

async function wasmLines(name) {
  const engine = await createEngine();
  const got = [];
  engine.onLog((s) => got.push(s));
  const runner = new TcRunner(engine, () => {});
  const text = readFileSync(path.join(root, 'tools/consoles/tc', `${name}.txt`), 'utf8');
  runner.start(name, text);
  const budget = makeBudget(12345);
  let guard = 0;
  while (runner.active) {
    runner.update(budget());
    if (++guard > 50_000_000) throw new Error('재생이 끝나지 않음');
  }
  return keep(got);
}

let failed = false;
for (const name of CONSOLES) {
  const a = terminalLines(name);
  const b = await wasmLines(name);
  const n = Math.max(a.length, b.length);
  let diff = -1;
  for (let i = 0; i < n; i++) if (a[i] !== b[i]) { diff = i; break; }
  if (diff < 0) {
    console.log(`OK   동일성 ${name}: ${a.length}줄`);
  } else {
    failed = true;
    console.log(`FAIL 동일성 ${name}: ${diff + 1}번째 줄부터 다름\n  터미널: ${a[diff]}\n  WASM  : ${b[diff]}`);
  }
}
process.exit(failed ? 1 : 0);
