// radar-view.js - 레이더가 "본 것"을 3D 장면에 겹쳐 그린다: 검출점, 시야, 맵의 트랙, 선행 차량.
// 검출점·트랙은 C 코어의 맵(adas_map)이 만든 값이다. 화면은 판단하지 않는다.
// 좌표는 도로를 편 좌표(x 앞, y 왼쪽, z 위)이고, 화면 위치는 world.js의 mapper가 정한다 (지도면 경로를 따라 휨).
import * as THREE from 'three';

export const SENSOR_COLORS = [0x4dd2ff, 0x9cff57, 0xffb347];   // FR, FCL, FCR
// 장착 정보 (요구사항 8.1, src/radar_frame.c와 같은 값): x, y, 정면 각도, 방위 시야, 거리
export const MOUNTS = [
  { name: 'FR', x: 0, y: 0, yaw: 0, fov: 60, range: 200 },
  { name: 'FCL', x: -0.2, y: 0.75, yaw: 45, fov: 75, range: 80 },
  { name: 'FCR', x: -0.2, y: -0.75, yaw: -45, fov: 75, range: 80 },
];
const MAX_POINTS = 512 + 128 + 128;
const MAX_TRACKS = 96;
const FOV_DRAW_MAX = 120;   // 화면에 그리는 시야 반지름 한도 (FR 200 m는 너무 길어서)

function fanGeometry(m) {
  const r = Math.min(m.range, FOV_DRAW_MAX), seg = 48;
  const pos = [0, 0, 0];
  for (let i = 0; i <= seg; i++) {
    const a = (m.yaw - m.fov + (2 * m.fov * i) / seg) * Math.PI / 180;
    pos.push(r * Math.cos(a), 0, r * Math.sin(a));   // 차량 좌표 (x, 0, y)로 만들고 나중에 변환
  }
  const idx = [];
  for (let i = 1; i <= seg; i++) idx.push(0, i, i + 1);
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.setIndex(idx);
  return g;
}

function labelSprite() {
  const canvas = document.createElement('canvas');
  canvas.width = 256; canvas.height = 64;
  const tex = new THREE.CanvasTexture(canvas);
  const sp = new THREE.Sprite(new THREE.SpriteMaterial({ map: tex, depthTest: false, transparent: true, sizeAttenuation: false }));
  sp.scale.set(0.14, 0.035, 1);   // 화면 크기 고정 (가까워도 커지지 않게)
  sp.renderOrder = 10;
  sp.userData = { canvas, tex, text: '' };
  return sp;
}

function setLabel(sp, text) {
  if (sp.userData.text === text) return;
  sp.userData.text = text;
  const { canvas, tex } = sp.userData, c = canvas.getContext('2d');
  c.clearRect(0, 0, canvas.width, canvas.height);
  c.fillStyle = 'rgba(14,17,23,0.82)';
  c.beginPath(); c.roundRect(8, 8, 240, 48, 10); c.fill();
  c.fillStyle = '#ffd34d';
  c.font = 'bold 30px system-ui, sans-serif';
  c.textAlign = 'center'; c.textBaseline = 'middle';
  c.fillText(text, 128, 33);
  tex.needsUpdate = true;
}

export function createRadarView(scene) {
  const root = new THREE.Group();
  scene.add(root);
  const tmp = new THREE.Vector3(), tmp2 = new THREE.Vector3();

  // ---- 검출점 (화면 좌표로 바로 넣는다) ----
  const pPos = new Float32Array(MAX_POINTS * 3), pCol = new Float32Array(MAX_POINTS * 3);
  const pGeo = new THREE.BufferGeometry();
  pGeo.setAttribute('position', new THREE.BufferAttribute(pPos, 3));
  pGeo.setAttribute('color', new THREE.BufferAttribute(pCol, 3));
  const points = new THREE.Points(pGeo, new THREE.PointsMaterial({ size: 0.32, vertexColors: true, transparent: true,
    opacity: 0.95, depthWrite: false, sizeAttenuation: true }));
  points.frustumCulled = false;
  points.renderOrder = 5;
  root.add(points);
  const cols = SENSOR_COLORS.map((c) => new THREE.Color(c));
  const gray = new THREE.Color(0x777777);

  // ---- 시야 부채꼴: 자차 앞 범퍼에 붙여 차 방향으로 (차 기준 좌표: 앞 -z, 왼쪽 -x) ----
  const fanGroup = new THREE.Group();
  root.add(fanGroup);
  const fans = MOUNTS.map((m, i) => {
    const g = fanGeometry(m);
    const p = g.attributes.position.array;
    for (let k = 0; k < p.length; k += 3) {   // (x, 0, y) 차량 좌표 → (−y, 높이, −x)
      const x = p[k] + m.x, y = p[k + 2] + m.y;
      p[k] = -y; p[k + 1] = 0.05; p[k + 2] = -x;
    }
    const mesh = new THREE.Mesh(g, new THREE.MeshBasicMaterial({ color: SENSOR_COLORS[i], transparent: true, opacity: 0.06,
      side: THREE.DoubleSide, depthWrite: false }));
    mesh.renderOrder = 2;
    fanGroup.add(mesh);
    return mesh;
  });

  // ---- 트랙 상자 (가까운 면에서 앞으로 0.6 m 얇은 상자, 그 자리 도로 방향으로 돌림) ----
  const boxGeo = new THREE.EdgesGeometry(new THREE.BoxGeometry(1, 1, 1));
  boxGeo.translate(0, 0.5, -0.5);   // 아래·가까운 면이 원점, 앞(-z)으로 뻗음
  const mats = {
    lead: new THREE.LineBasicMaterial({ color: 0xffd34d }),
    path: new THREE.LineBasicMaterial({ color: 0xff6b5c }),
    obstacle: new THREE.LineBasicMaterial({ color: 0xe8eaee }),
    ignored: new THREE.LineBasicMaterial({ color: 0x6f7682, transparent: true, opacity: 0.6 }),
    tentative: new THREE.LineBasicMaterial({ color: 0x4f5560, transparent: true, opacity: 0.35 }),
  };
  const boxes = [];
  for (let i = 0; i < MAX_TRACKS; i++) {
    const b = new THREE.LineSegments(boxGeo, mats.obstacle);
    b.visible = false;
    b.renderOrder = 6;
    root.add(b);
    boxes.push(b);
  }
  const leadLabel = labelSprite();
  root.add(leadLabel);
  const leadLine = new THREE.Line(new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(), new THREE.Vector3()]),
    new THREE.LineDashedMaterial({ color: 0xffd34d, dashSize: 0.6, gapSize: 0.4 }));
  leadLine.frustumCulled = false;
  leadLine.renderOrder = 6;
  root.add(leadLine);

  const show = { points: true, fov: true, tracks: true };

  function update(adas, pts, mapper) {
    const on = !!(adas && adas.on);
    root.visible = on;
    leadLabel.visible = false;
    if (!on) return;

    // 검출점: [센서, x, y, z, 도플러, 표시]
    const n = Math.min(MAX_POINTS, (pts ? pts.length : 0) / 6);
    for (let i = 0; i < n; i++) {
      const o = i * 6, s = pts[o], flags = pts[o + 5];
      mapper.point(pts[o + 1], pts[o + 2], Math.max(0.04, pts[o + 3]), tmp);
      pPos[i * 3] = tmp.x; pPos[i * 3 + 1] = tmp.y; pPos[i * 3 + 2] = tmp.z;
      const c = flags & 2 ? gray : cols[s] || gray, k = flags & 1 ? 0.55 : 1;   // 정지 점은 조금 어둡게
      pCol[i * 3] = c.r * k; pCol[i * 3 + 1] = c.g * k; pCol[i * 3 + 2] = c.b * k;
    }
    pGeo.setDrawRange(0, n);
    pGeo.attributes.position.needsUpdate = true;
    pGeo.attributes.color.needsUpdate = true;
    points.visible = show.points;

    const radars = adas.map.radars;
    mapper.point(0, 0, 0, fanGroup.position);
    fanGroup.rotation.set(0, mapper.egoHeading(), 0);
    fans.forEach((f, i) => {
      f.visible = show.fov;
      f.material.opacity = radars[i].h === 'ok' ? 0.06 : 0.0;
    });

    // 트랙: [id, x, y, ymin, ymax, vx, vy, zlo, zhi, 표시]
    let k = 0, lead = null;
    for (const t of adas.tracks) {
      if (k >= MAX_TRACKS) break;
      const [, x, , ymin, ymax, , , zlo, zhi, f] = t;
      if (x > 150) continue;
      const b = boxes[k++];
      const hasZ = f & 32;
      const z0 = hasZ ? Math.max(0, zlo) : 0, z1 = hasZ ? Math.max(z0 + 0.2, zhi) : 1.4;
      mapper.point(x, (ymin + ymax) / 2, z0, b.position);
      b.rotation.set(0, mapper.heading(x), 0);
      b.scale.set(Math.max(0.2, ymax - ymin), z1 - z0, 0.6);
      b.material = f & 16 ? mats.lead : !(f & 1) ? mats.tentative : !(f & 2) ? mats.ignored : f & 4 ? mats.path : mats.obstacle;
      b.visible = show.tracks && ((f & 1) || show.tentative);
      if (f & 16) lead = t;
    }
    for (; k < MAX_TRACKS; k++) boxes[k].visible = false;

    leadLine.visible = !!lead && show.tracks;
    if (lead) {
      const [, x, y, , , vx] = lead;
      const p = leadLine.geometry.attributes.position.array;
      mapper.point(0, 0, 0.3, tmp);
      mapper.point(x, y, 0.3, tmp2);
      p[0] = tmp.x; p[1] = tmp.y; p[2] = tmp.z; p[3] = tmp2.x; p[4] = tmp2.y; p[5] = tmp2.z;
      leadLine.geometry.attributes.position.needsUpdate = true;
      leadLine.computeLineDistances();
      leadLabel.visible = show.tracks;
      mapper.point(x, y, 2.6, leadLabel.position);
      setLabel(leadLabel, `${x.toFixed(1)} m  ${vx >= 0 ? '+' : ''}${(vx * 3.6).toFixed(0)} km/h`);
    }
  }

  return { update, show };
}
