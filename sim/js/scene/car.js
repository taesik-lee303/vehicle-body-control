// car.js - 박스로 만든 차 (M2). M3에서 glTF 모델로 바꿔도 update(s, ...) 인터페이스는 같다.
// 차의 앞은 -z 방향. 전조등·와이퍼·잠금은 코어 출력(s.out), 도어·브레이크등·후진등은 입력(s.in)을 비춘다.
import * as THREE from 'three';

const DOOR_OPEN_RAD = THREE.MathUtils.degToRad(14);   // "덜 닫힘" 연출 각도
const WIPER_SWEEP_RAD = THREE.MathUtils.degToRad(100);
const WHEEL_R = 0.34;

function box(w, h, d, mat) {
  return new THREE.Mesh(new THREE.BoxGeometry(w, h, d), mat);
}

export function createCar() {
  const group = new THREE.Group();
  const paint = new THREE.MeshStandardMaterial({ color: 0x9e1b24, metalness: 0.6, roughness: 0.32 });
  const glass = new THREE.MeshStandardMaterial({ color: 0x14202c, metalness: 0.2, roughness: 0.05, transparent: true, opacity: 0.85 });
  const black = new THREE.MeshStandardMaterial({ color: 0x111111, roughness: 0.8 });
  const bladeMat = new THREE.MeshStandardMaterial({ color: 0x8a9099, metalness: 0.6, roughness: 0.4 });   // 어두운 유리 위에서 보이게
  const chrome = new THREE.MeshStandardMaterial({ color: 0xcfd3d8, metalness: 1, roughness: 0.25 });

  // 차체: 아래 몸통 + 실내(유리) + 보닛 경사
  const body = box(1.82, 0.62, 4.5, paint); body.position.y = 0.62; group.add(body);
  const cabin = box(1.62, 0.56, 2.3, glass); cabin.position.set(0, 1.21, 0.25); group.add(cabin);
  const roof = box(1.6, 0.06, 2.0, paint); roof.position.set(0, 1.52, 0.35); group.add(roof);
  const bumperF = box(1.86, 0.22, 0.2, black); bumperF.position.set(0, 0.42, -2.3); group.add(bumperF);
  const bumperR = box(1.86, 0.22, 0.2, black); bumperR.position.set(0, 0.42, 2.3); group.add(bumperR);

  // 전조등 (출력 headlight)
  const headMat = new THREE.MeshStandardMaterial({ color: 0xdfe6ee, emissive: 0xfff3d6, emissiveIntensity: 0 });
  const beams = [];
  for (const x of [-0.62, 0.62]) {
    const lamp = box(0.42, 0.14, 0.06, headMat); lamp.position.set(x, 0.74, -2.26); group.add(lamp);
    // 실제 전조등처럼 빛의 중심은 멀리(60m), 가까운 노면은 부드러운 가장자리 빛만 받게
    const spot = new THREE.SpotLight(0xfff1d8, 0, 110, 0.3, 0.8, 1.5);
    spot.position.set(x, 0.74, -2.3);
    spot.target.position.set(x * 2, 0, -60);
    group.add(spot, spot.target);
    beams.push(spot);
  }
  // 빛 원뿔(안개 속 빛줄기 느낌). 전조등이 켜졌을 때만 보임
  const coneMat = new THREE.MeshBasicMaterial({ color: 0xfff2cc, transparent: true, opacity: 0, depthWrite: false,
    blending: THREE.AdditiveBlending, side: THREE.DoubleSide });
  const cones = [-0.62, 0.62].map((x) => {
    const cone = new THREE.Mesh(new THREE.ConeGeometry(2.6, 16, 24, 1, true), coneMat);
    cone.rotation.x = -Math.PI / 2;
    cone.position.set(x, 0.5, -10.3);
    group.add(cone);
    return cone;
  });

  // 후미등: 평소 은은하게, 브레이크(입력)면 밝게. 후진등은 기어 R(입력)
  const tailMat = new THREE.MeshStandardMaterial({ color: 0x5a0000, emissive: 0xff1a1a, emissiveIntensity: 0.25 });
  const revMat = new THREE.MeshStandardMaterial({ color: 0x777777, emissive: 0xffffff, emissiveIntensity: 0 });
  for (const x of [-0.66, 0.66]) {
    const t = box(0.4, 0.14, 0.05, tailMat); t.position.set(x, 0.76, 2.26); group.add(t);
    const r = box(0.14, 0.1, 0.05, revMat); r.position.set(x * 0.55, 0.76, 2.26); group.add(r);
  }

  // 와이퍼 2개 (출력 wiper). 앞유리(실내 박스 앞면) 아래쪽 축에서 회전
  const wipers = [-0.62, 0.08].map((x) => {
    const pivot = new THREE.Group();
    pivot.position.set(x, 0.96, -0.92);
    const blade = box(0.62, 0.04, 0.04, bladeMat);
    blade.position.x = 0.31;
    pivot.add(blade);
    group.add(pivot);
    return pivot;
  });

  // 도어 4개 (입력 doors). 앞쪽 모서리를 축으로 바깥으로 열림. 순서 FL FR RL RR
  const doors = [[-1, -0.95], [1, -0.95], [-1, 0.25], [1, 0.25]].map(([side, zFront]) => {
    const hinge = new THREE.Group();
    hinge.position.set(side * 0.93, 0.72, zFront);
    const panel = box(0.05, 0.6, 1.15, paint);
    panel.position.z = 0.575;
    const handle = box(0.03, 0.04, 0.18, chrome);
    handle.position.set(side * 0.03, 0.12, 0.85);
    hinge.add(panel, handle);
    hinge.userData.side = side;
    group.add(hinge);
    return hinge;
  });

  // 바퀴: 회전이 보이게 휠 디스크에 살 2개
  const wheels = [[-0.86, -1.45], [0.86, -1.45], [-0.86, 1.45], [0.86, 1.45]].map(([x, z]) => {
    const w = new THREE.Group();
    w.position.set(x, WHEEL_R, z);
    const tire = new THREE.Mesh(new THREE.CylinderGeometry(WHEEL_R, WHEEL_R, 0.26, 28), black);
    tire.rotation.z = Math.PI / 2;
    const spoke1 = box(0.27, 0.44, 0.06, chrome);
    const spoke2 = box(0.27, 0.06, 0.44, chrome);
    w.add(tire, spoke1, spoke2);
    group.add(w);
    return w;
  });

  group.traverse((o) => { if (o.isMesh) { o.castShadow = true; o.receiveShadow = true; } });
  cones.forEach((c) => { c.castShadow = false; c.receiveShadow = false; });

  // ---- 와이퍼 애니메이션 (주기는 연출값. 사양서는 단계만 정한다) ----
  let phase = 0;   // 0이면 쉬는 중, (0,1) 한 번 왕복 중
  let pause = 0;   // 간헐 모드에서 다음 왕복까지 남은 시간
  function wiperAmount(level, dt) {
    if (phase === 0) {
      if (level === 'stop') return 0;
      if (pause > 0) { pause -= dt; return 0; }
      phase = 1e-6;
    }
    const cycle = level === 'high' ? 0.5 : 1.0;   // 고속 0.5초, 저속·간헐 1초에 한 번 왕복
    phase += dt / cycle;
    if (phase >= 1) { phase = 0; if (level === 'int') pause = 2.0; return 0; }   // 간헐: 3초마다 한 번
    return Math.sin(phase * Math.PI);
  }

  function update(s, dt, speedMs, dir) {
    const on = !!s.out.headlight;
    headMat.emissiveIntensity = on ? 3 : 0;
    beams.forEach((b) => { b.intensity = on ? 1400 : 0; });
    coneMat.opacity = on ? 0.05 : 0;

    tailMat.emissiveIntensity = s.in.brake ? 2.5 : 0.25;
    revMat.emissiveIntensity = s.in.gear === 'R' ? 2 : 0;

    const a = wiperAmount(s.out.wiper, dt) * WIPER_SWEEP_RAD;
    wipers.forEach((p) => { p.rotation.z = a; });

    doors.forEach((d, i) => {
      // 패널은 경첩에서 +z로 뻗어 있어 y축 +회전이면 뒤끝이 +x로 간다 → 왼쪽(side -1)은 음수, 오른쪽은 양수가 바깥
      const target = s.in.doors[i] ? d.userData.side * DOOR_OPEN_RAD : 0;
      d.rotation.y += (target - d.rotation.y) * Math.min(1, dt * 8);
    });

    const spin = (speedMs * dt * dir) / WHEEL_R;
    wheels.forEach((w) => { w.rotation.x -= spin; });
  }

  // 빛 원뿔은 뒤·위에서 볼 때만 (정면 카메라에서는 카메라를 덮어 버림)
  const showBeams = (v) => cones.forEach((c) => { c.visible = v; });

  return { group, update, showBeams };
}
