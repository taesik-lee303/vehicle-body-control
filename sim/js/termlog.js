// termlog.js - 이벤트 로그를 make sim 터미널에도 찍는다 (sim/tools/serve.py의 POST /log).
// 화면 로그와 같은 줄을 모아 보낸다. 서버가 받지 않으면(python3 -m http.server 등) 조용히 끈다.

const FLUSH_MS = 200;
const MAX_QUEUE = 500;
let queue = [];
let off = false, sending = false, timer = 0;

async function flush() {
  timer = 0;
  if (off || sending || queue.length === 0) return;
  const batch = queue;
  queue = [];
  sending = true;   // 한 번에 하나만 보내 순서를 지킨다
  try {
    const res = await fetch('/log', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(batch) });
    if (!res.ok) off = true;
  } catch {
    off = true;
  }
  sending = false;
  if (queue.length && !timer) timer = setTimeout(flush, FLUSH_MS);
}

export function termLog(cls, text) {
  if (off) return;
  queue.push({ c: cls, t: text });
  if (queue.length > MAX_QUEUE) queue.splice(0, queue.length - MAX_QUEUE);
  if (!timer && !sending) timer = setTimeout(flush, FLUSH_MS);
}
