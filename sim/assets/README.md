# 시뮬레이터 자원 (sim/assets)

## 들어 있는 것 (모두 CC0, 출처 표기 의무 없음)
| 파일 | 원본 (Poly Haven) | 쓰는 곳 |
|---|---|---|
| `sky/day.hdr` | [kloofendal_48d_partly_cloudy_puresky](https://polyhaven.com/a/kloofendal_48d_partly_cloudy_puresky) 2K | 낮 하늘, 반사 환경, 해 방향 |
| `sky/overcast.hdr` | [overcast_soil_puresky](https://polyhaven.com/a/overcast_soil_puresky) 2K | 비 올 때 흐린 하늘 |
| `sky/night.hdr` | [rogland_clear_night](https://polyhaven.com/a/rogland_clear_night) 1K | 밤하늘 |
| `tex/asphalt_*.jpg` | [asphalt_04](https://polyhaven.com/a/asphalt_04) 1K (색, 법선, 거칠기) | 노면 |
| `tex/grass_*.jpg` | [leafy_grass](https://polyhaven.com/a/leafy_grass) 1K (색, 법선) | 풀밭 |

다시 받기: `sim/tools/fetch-assets.sh`. 차, 나무, 가로등, 가드레일은 코드로 만듭니다 (`sim/js/scene/car.js`, `road.js`).

## 지도 (map/)
| 파일 | 원본 | 쓰는 곳 |
|---|---|---|
| `map/sejong.json` | [OpenStreetMap](https://www.openstreetmap.org/copyright) 세종대학교 주변 도로·건물·녹지 (© OpenStreetMap contributors, **ODbL 1.0**) | 실제 도로 장면 (`sim/js/scene/city.js`), 미니맵 |
| (같은 파일의 `terrain`) | [AWS Terrain Tiles](https://registry.opendata.aws/terrain-tiles/) Terrarium z15 (SRTM 기반 공개 고도 자료) | 지형 높이 |

- **출처 표시 의무**가 있습니다. 화면 미니맵 아래에 `© OpenStreetMap contributors`가 항상 표시되며, 이 줄을 지우지 않습니다.
- 주행 경로(자동 주행용): 능동로(세종대 앞) → 광나루로 → 군자로 → 천호대로(군자역) → 능동로, 한 바퀴 약 2.9 km. 일방통행을 지키는 순환로를 도구가 자동으로 찾습니다.
- 다시 만들기 (인터넷 필요, Overpass API + 고도 타일): `python3 sim/tools/fetch-map.py`. 서버가 바쁘면 504가 나므로 도구가 다른 서버로 몇 번 다시 시도합니다.
  `--cache DIR`을 주면 받은 원본을 저장해 두고 다음에는 다시 받지 않습니다.
- **지형**: 고도 자료가 건물·나무 꼭대기까지 잰 표면 높이라, 반경 50 m보다 작은 솟은 부분을 깎고(열림 연산) 25 m로 평활화했습니다. 해상도가 약 30 m라 높이 오차가 수 m 있습니다. 높이는 출발점 = 0 m 기준입니다.
- **건물 높이**: 우선순위는 `sim/tools/map-overrides.json`(팀 보정) > OSM `height` > OSM `building:levels` > 건물 종류별 기본값입니다.
  세종대 건물은 OSM에 층수가 있는 것이 4개(광개토관 15층, 동천관 10층, 김원관 6층, 이당관 2층)뿐입니다. 나머지는 실제 지상 층수를 보정 파일의 `levels`에 적고 도구를 다시 실행하세요.
  `python3 sim/tools/fetch-map.py --campus`로 캠퍼스 건물과 층수 출처를 볼 수 있습니다. 외벽은 실제 모양이 아니라 공통 창 무늬입니다.
- 지하차도·터널은 뺐습니다. 나무·가로등 위치는 실제 위치가 아닙니다.
- 직접 운전의 충돌 판정은 건물 바닥 외곽선과 지도 끝만 봅니다 (나무·가로등·연석은 통과).
- URL `?map=straight`면 예전 직선 도로, `?at=1200`이면 경로의 1.2 km 지점에서 출발합니다.

## 실사 차 모델 넣기 (선택)
`sim/assets/car.glb`가 있으면 코드로 만든 차 대신 그 모델을 씁니다. 없으면 코드 모델이 그대로 나옵니다.
URL `?car=code`는 항상 코드 모델, `?car=glb`는 glb가 없을 때 이벤트 로그에 알려 줍니다.
불러오면 이벤트 로그에 `차 모델: car.glb (도어 4/4, 바퀴 4/4, ...)`처럼 찾은 부품 수가 찍힙니다.

**모델 조건**
- 형식 `.glb` (DRACO 압축 가능), 20MB 이하. 라이선스는 CC0 또는 CC-BY. CC-BY면 아래 "출처" 표에 작성자·링크를 꼭 적습니다.
- 크기와 위치는 자동으로 맞춥니다 (길이 4.6m, 바닥 y=0, 가운데). 앞이 -z가 아니면 `car.json`의 `rotationY`로 돌립니다.

**노드 이름 규칙** (대소문자, `_`·`-`·공백 구분 상관없음)
| 부품 | 이름 예 | 없으면 |
|---|---|---|
| 도어 4개 | `door_fl`, `door_fr`, `door_rl`, `door_rr`, `door_lf`(좌우 먼저) 또는 `Door Front Left` | 그 도어는 열림이 보이지 않음 (계기판에는 표시됨) |
| 바퀴 4개 | `wheel_fl` … `wheel_rr` | 합쳐진 타이어·휠 메시(`tyres`, `tires`, `rims`, `wheels`)가 있으면 차 가운데 기준 네 구역으로 나눠 돌림. 그것도 없으면 돌지 않음 |
| 전조등 | 노드나 재질 이름에 `headlight`, `headlamp`, `frontlight`, `projector` | 빛(스포트라이트)만 나감 |
| 후미등·브레이크등 | `taillight`, `taillamp`, `rearlight`, `brakelight`, `stoplamp` | 표시 안 됨 |
| 후진등 | `reverse`, `backup` | 표시 안 됨 |
| 와이퍼 | 이름에 `wiper` | 모델의 와이퍼는 숨기고 같은 규칙의 와이퍼를 붙임 (위치는 `car.json`) |

도어는 경계 상자의 바깥쪽 앞 모서리, 바퀴는 경계 상자 가운데를 회전축으로 잡으므로 모델의 원점이 어디든 상관없습니다.
도어가 한 덩어리로 붙어 있는 모델은 Blender에서 도어를 떼어 이름을 붙인 뒤 `.glb`로 내보냅니다.

**car.json** (선택, `car.glb` 옆)
```json
{
  "file": "car.glb",
  "rotationY": 180,
  "length": 4.6,
  "wiper": { "x": [-0.6, 0.06], "y": 0.98, "z": -0.77, "slopeDeg": 28 }
}
```
`rotationY`는 도 단위, `wiper`는 차 좌표(앞 -z, 단위 m)의 와이퍼 축 위치와 앞유리 기울기입니다. 화면에서 와이퍼가 유리에서 뜨거나 묻히면 이 값을 고칩니다.

## 출처 (CC-BY 모델을 넣으면 여기에 추가)
| 파일 | 작성자 | 링크 | 라이선스 |
|---|---|---|---|
| `car.glb` | CCamo ([nicocarmo05](https://sketchfab.com/nicocarmo05)) | [(Game Ready / PBR) Porsche Cayenne Turbo 2005](https://sketchfab.com/3d-models/game-ready-pbr-porsche-cayenne-turbo-2005-0c21727cc75e4ecbb39c4eb5c78991ad) | CC-BY 4.0 |
