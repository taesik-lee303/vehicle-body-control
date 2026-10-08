// scenario.js - scenarios/*.txt를 화면에서 재생한다. 터미널(./build/vbc 파일)과 같은 명령, 같은 코어.
// DOM을 쓰지 않는다 (Node에서 검증 가능).
//
// 터미널과 다른 점은 wait 하나뿐이다: 터미널은 즉시 시간을 건너뛰지만, 화면에서는 벽시계 × 배속으로
// 조금씩 흘려 보여 준다. 흘린 시간의 합은 wait 값과 정확히 같아서 판정 시각(2.900s 등)은 터미널과 같다.

const TICK_MS = 50;

// "wait 2.9  # 주석" → 2900 (ms). wait가 아니면 null, 잘못된 값이면 NaN
export function parseWait(line) {
  const body = line.split('#')[0].trim();
  const m = /^wait\s+(\S+)$/i.exec(body);
  if (!m) return null;
  const sec = Number(m[1]);
  if (!(sec > 0) || sec > 600) return NaN;
  const ms = Math.round(sec * 1000);
  return ms % TICK_MS === 0 ? ms : NaN;
}

// python http.server 디렉터리 목록에서 .txt 링크를 뽑는다
export async function listScenarios(base = '../scenarios/') {
  const res = await fetch(base);
  if (!res.ok) return [];
  const html = await res.text();
  return [...html.matchAll(/href="([^"]+\.txt)"/g)].map((m) => decodeURIComponent(m[1])).sort();
}

export class ScenarioRunner {
  // core: {init, exec, advance, state}, log(text, cls), onEnd(result)
  // onInit: 코어를 초기화한 직후 (화면이 자차 자세 등을 다시 알려 줄 때)
  constructor(core, log, onEnd = () => {}, onInit = () => {}) {
    this.core = core;
    this.log = log;
    this.onEnd = onEnd;
    this.onInit = onInit;
    this.active = false;
  }

  // 시작할 때 코어를 초기 상태(C5)로 되돌린다. 시나리오 파일은 "초기 상태에서 시작"을 전제로 쓰기 때문
  start(name, text) {
    this.name = name;
    this.lines = text.split(/\r?\n/);
    this.i = 0;
    this.waitLeft = 0;
    this.errors = 0;
    this.frac = 0;
    this.active = true;
    this.core.init(false);
    this.onInit();
    this.log(`== 시나리오 시작: ${name} (초기 상태에서)`, 'info');
    this.run();
  }

  stop() {
    if (!this.active) return;
    this.active = false;
    this.log(`== 시나리오 중단: ${this.name}`, 'info');
  }

  // 다음 wait까지 명령을 실행
  run() {
    while (this.active && this.waitLeft === 0) {
      if (this.i >= this.lines.length) return this.finish();
      const line = this.lines[this.i++];
      const trimmed = line.trim();
      if (!trimmed) continue;
      if (trimmed.startsWith('#')) { this.log(trimmed, 'comment'); continue; }
      this.log('> ' + trimmed, 'cmd');
      const ms = parseWait(trimmed);
      if (ms === null) {
        const r = this.core.exec(trimmed);
        if (r === 1) this.errors++;
        if (r === 2) return this.finish();
      } else if (Number.isNaN(ms)) {
        this.errors++;
        this.log('  ! wait는 0.05초 단위의 양수로 입력하세요', 'err');
      } else {
        this.waitLeft = ms;
      }
    }
  }

  // 메인 루프가 흘려 보낼 시간(ms, 배속 적용 후)을 넘긴다. wait 남은 시간을 넘지 않게 잘라서 진행
  // 코어(sim_advance)는 정수 ms만 받으므로 프레임 간격의 소수(16.6ms의 0.6)는 여기서 모아 둔다.
  // 버리면 wait마다 마지막 한 틱이 모자라 판정 시각이 0.05초 늦어진다.
  update(ms) {
    this.frac = (this.frac || 0) + ms;
    ms = Math.floor(this.frac);
    this.frac -= ms;
    while (this.active && ms > 0 && this.waitLeft > 0) {
      const step = Math.min(ms, this.waitLeft);
      this.core.advance(step);
      this.waitLeft -= step;
      ms -= step;
      if (this.waitLeft === 0) this.run();
    }
  }

  finish() {
    this.active = false;
    const failures = this.core.state().expect_failures;
    const result = { name: this.name, failures, errors: this.errors, pass: failures === 0 && this.errors === 0 };
    this.log(`== ${this.name}: expect 불일치 ${failures}건, 명령 오류 ${this.errors}건 → ${result.pass ? 'PASS' : 'FAIL'}`,
      result.pass ? 'ok' : 'bad');
    this.onEnd(result);
  }
}
