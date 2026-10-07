// hud.js - 하단 계기판. 값은 전부 sim_state()의 출력·판정을 그대로 보여 준다 (화면은 판단하지 않음).

const WIPER = { stop: '정지', int: '간헐', low: '저속', high: '고속' };
const WARNING = {
  none: ['경고 없음', 'none'],
  door: ['⚠ 주행 중 도어 열림', 'door'],
  lux: ['⚠ 조도 센서 고장', 'sensor'],
  rain: ['⚠ 강우 센서 고장', 'sensor'],
};
const GEARS = ['P', 'R', 'N', 'D'];

let lastKey = '';

export function renderHud(el, s, sim, scenarioName) {
  const speed = Math.round(s.in.speed_mkmh / 1000);
  const key = JSON.stringify([s.t, s.in.speed_mkmh, s.in.ign, s.in.gear, s.out, s.ctx, sim.paused, sim.rate, scenarioName]);
  if (key === lastKey) return;   // 바뀐 게 없으면 다시 그리지 않음
  lastKey = key;

  const lamp = (label, on, cls = 'on') => `<span class="lamp ${on ? cls : ''}">${label}</span>`;
  const [warnText, warnCls] = WARNING[s.out.warning] || [s.out.warning, 'sensor'];

  el.innerHTML = `
    <div class="speedo">
      <div class="num">${speed}</div><div class="unit">km/h</div>
      <div class="gear">${GEARS.map((g) => `<span class="${g === s.in.gear ? 'cur' : ''}">${g}</span>`).join('')}</div>
    </div>
    <div class="lamps">
      ${lamp('시동', s.in.ign)}
      ${lamp('전조등', s.out.headlight)}
      ${lamp(`와이퍼 ${WIPER[s.out.wiper] || s.out.wiper}`, s.out.wiper !== 'stop')}
      ${lamp(s.out.locked ? '🔒 잠김' : '🔓 풀림', s.out.locked)}
      ${lamp('조도 고장', s.out.lux_lamp, 'bad')}
      ${lamp('강우 고장', s.out.rain_lamp, 'bad')}
      ${lamp('수신 중단', s.ctx.rx_lost, 'bad')}
    </div>
    <div class="clock">
      <div class="t">${(s.t / 1000).toFixed(2)} s</div>
      <div>${sim.paused ? '<span class="paused">일시정지</span>' : `배속 ${sim.rate}×`}</div>
      ${scenarioName ? `<div class="scen">▶ ${scenarioName.replace(/\.txt$/, '')}</div>` : ''}
    </div>
    <div class="warning ${warnCls}">${warnText}</div>`;
}
