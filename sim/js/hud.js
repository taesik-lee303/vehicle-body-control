// hud.js - 하단 계기판. 값은 전부 sim_state()의 출력·판정을 그대로 보여 준다 (화면은 판단하지 않음).

const WIPER = { stop: '정지', int: '간헐', low: '저속', high: '고속' };
const WARNING = {
  none: ['경고 없음', 'none'],
  door: ['⚠ 주행 중 도어 열림', 'door'],
  lux: ['⚠ 조도 센서 고장', 'sensor'],
  rain: ['⚠ 강우 센서 고장', 'sensor'],
};
const GEARS = ['P', 'R', 'N', 'D'];
const SG_STATE = { off: ['꺼짐', 'off'], standby: ['대기', 'standby'], active: ['작동', 'active'], hold: ['정지 유지', 'hold'],
  wait: ['재출발 대기', 'wait'] };
const SG_ALERT = { none: ['', ''], takeover: ['⚠ 운전자 개입 요청', 'bad'], radar: ['⚠ 레이더 고장', 'warn'], go: ['▶ 출발 가능', 'go'] };

// Stop & Go 칸 (요구사항 6.3 XD1~XD4). 계기판 경고(D5)와 따로 한 번에 하나만
function sgBlock(a) {
  if (!a || !a.on) return '';
  const sg = a.sg;
  const [label, cls] = SG_STATE[sg.state] || [sg.state, 'off'];
  const [alert, acls] = SG_ALERT[sg.alert] || ['', ''];
  const bars = [1, 2, 3].map((i) => `<i class="${i <= sg.gap ? 'on' : ''}"></i>`).join('');
  const accel = sg.accel === null ? '' : `${sg.accel >= 0 ? '+' : ''}${sg.accel.toFixed(2)} m/s²`;
  return `<div class="sg ${cls}">
      <span class="sg-name">Stop &amp; Go</span><span class="sg-state">${label}</span>
      ${sg.state !== 'off' ? `<span class="sg-set">${sg.set ? sg.set + ' km/h' : '설정 없음'}</span><span class="sg-gap" title="차간 ${sg.gap_s} s">${bars}</span>` : ''}
      <span class="sg-lead ${a.lead ? 'on' : ''}">${a.lead ? `▲ ${a.lead.x.toFixed(1)} m` : '선행 없음'}</span>
      <span class="sg-accel">${sg.hold ? '정지 유지' : accel}</span>
      <span class="sg-alert ${acls}">${alert}</span>
    </div>`;
}

let lastKey = '';

export function renderHud(el, s, sim, scenarioName, adas = null) {
  const speed = Math.round(s.in.speed_mkmh / 1000);
  const sgKey = adas && adas.on ? [adas.sg, adas.lead && adas.lead.x.toFixed(1)] : null;
  const key = JSON.stringify([s.t, s.in.speed_mkmh, s.in.ign, s.in.gear, s.out, s.ctx, sim.paused, sim.rate, scenarioName, sgKey]);
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
    <div class="warning ${warnCls}">${warnText}</div>
    ${sgBlock(adas)}`;
}
