// runner.js - TC 파일 재생기 (DOM 없음). 줄을 엔진에 넘기고, `t <초>`만 예산에 맞춰 잘라 진행한다.
// 판단 시각이 터미널 `t`와 같도록 조각은 step_ms의 배수로 자른다 (마지막 나머지 조각만 예외).

/** `t <초>`면 ms, `t`가 아닌 줄이면 null, 형식 오류면 NaN (C parse_seconds와 같은 반올림) */
export function parseT(line) {
  const parts = line.trim().split(/\s+/);
  if (parts[0].toLowerCase() !== 't') return null;
  if (parts.length !== 2 || !/^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$/.test(parts[1])) return NaN;
  const sec = Number(parts[1]);
  if (!(sec > 0) || sec * 1000 > 600000) return NaN;
  const ms = Math.floor(sec * 1000 + 0.5);
  return ms > 0 ? ms : NaN;
}

export class TcRunner {
  constructor(engine, log) {
    this.engine = engine;
    this.log = log; // log(text, cls) cls: 'cmd' | 'comment' | 'info'
    this.lines = [];
    this.pos = 0;
    this.active = false;
    this.remaining = 0; // 진행 중인 t 줄의 남은 ms
    this.total = 0;     // 그 t 줄의 전체 ms
    this.changes = 0;   // 그 t 줄 동안 advance 반환 합
    this.step = 1;
    this.carry = 0;
  }

  // label: 로그에 보일 파일 이름 (기본은 그 콘솔의 TC 파일)
  start(consoleName, text, label = `${consoleName}.txt`) {
    this.engine.select(consoleName);
    this.lines = text.split('\n').map((l) => l.replace(/\r$/, ''));
    if (this.lines.length && this.lines[this.lines.length - 1] === '') this.lines.pop();
    this.pos = 0;
    this.remaining = this.total = this.changes = 0;
    this.carry = 0;
    this.active = true;
    this.log(`== 시나리오 시작: ${label} (초기 상태에서)`, 'info');
  }

  stop() {
    this.active = false;
    this.remaining = 0;
  }

  // 예산(ms)만큼 재생한다. t가 없는 줄은 예산 없이 이어서 실행한다.
  update(budgetMs) {
    if (!this.active) return;
    this.carry += budgetMs;
    for (;;) {
      if (this.remaining > 0) {
        this.advanceRemaining(false);
        if (this.remaining > 0) return; // 예산 소진
      }
      if (!this.runLine()) return; // 끝
    }
  }

  // 남은 t가 있으면 끝까지, 없으면 다음 줄 하나만 실행한다.
  next() {
    if (!this.active) return;
    if (this.remaining > 0) {
      this.advanceRemaining(true);
      return;
    }
    this.runLine();
  }

  // 다음 줄 하나 실행. 계속할 수 있으면 true, 끝났으면 false
  runLine() {
    if (this.pos >= this.lines.length) {
      this.engine.finish();
      this.log('== 시나리오 끝', 'info');
      this.active = false;
      return false;
    }
    const line = this.lines[this.pos++];
    if (/^\s*#/.test(line)) {
      this.log(line, 'comment');
      return true;
    }
    if (line.trim() === '') return true;
    this.log(`> ${line}`, 'cmd');
    const ms = parseT(line);
    if (ms !== null && !Number.isNaN(ms)) {
      this.step = this.engine.state().step_ms || 1;
      this.remaining = this.total = ms;
      this.changes = 0;
      return true;
    }
    if (this.engine.exec(line) === 2) {
      this.pos = this.lines.length;
    }
    return true;
  }

  advanceRemaining(all) {
    const step = this.step;
    while (this.remaining > 0) {
      let n;
      if (all || this.remaining < step) n = this.remaining; // 마지막 나머지 조각
      else {
        n = Math.min(this.remaining - (this.remaining % step), Math.floor(this.carry / step) * step);
        if (n <= 0) break;
      }
      this.changes += this.engine.advance(n);
      this.remaining -= n;
      this.carry = Math.max(0, this.carry - n);
    }
    if (this.remaining === 0 && this.changes === 0) {
      const ms = this.total;
      this.log(`  → ${Math.floor(ms / 1000)}.${String(ms % 1000).padStart(3, '0')}초 동안 변화 없음`, 'info');
    }
  }
}
