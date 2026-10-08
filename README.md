# vehicle-body-control 시뮬레이터

가상 차량의 차체 편의 기능(조명, 와이퍼, 도어 잠금, 계기판 경고)을 브라우저에서 운전하며 확인하는 시뮬레이터입니다.
판단은 전부 C 코어(사양서 VBC-SRS-001 구현)가 하고, 화면은 그 결과를 그리기만 합니다.

![시뮬레이터 화면: 밤, 비 75%, 50 km/h, 뒷문 열림](docs/images/simulator.png)

## 빠른 실행 (준비가 끝난 경우)
```bash
source ~/emsdk/emsdk_env.sh   # 새 터미널마다 한 번
make sim                      # C 코어를 브라우저용으로 빌드(make wasm)하고 서버 실행
```
Windows 브라우저에서 **http://localhost:8080/sim/** 을 엽니다. 종료는 터미널에서 `Ctrl+C`.

## 처음 한 번 준비
Windows는 **WSL Ubuntu 안에서** 진행합니다 (저장소도 WSL 안, 예: `~/work/...`).

1. 기본 도구: `gcc`, `make`, `python3`
   ```bash
   sudo apt update && sudo apt install -y build-essential python3 git
   ```
2. 저장소 받기
   ```bash
   gh repo clone taesik-lee303/vehicle-body-control
   cd vehicle-body-control
   git checkout develop
   ```
3. Emscripten(emsdk) 설치: C 코어를 브라우저에서 돌리기 위한 컴파일러입니다. 약 1GB, 몇 분 걸립니다.
   ```bash
   git clone --depth 1 https://github.com/emscripten-core/emsdk.git ~/emsdk
   ~/emsdk/emsdk install latest
   ~/emsdk/emsdk activate latest
   ```
4. 확인
   ```bash
   make test                     # ALL TESTS PASSED 가 나오면 정상 (emsdk 없이도 됨)
   source ~/emsdk/emsdk_env.sh
   make sim
   ```

three.js는 `sim/vendor/three/`에 들어 있어 따로 받지 않아도 됩니다. 인터넷이 없어도 실행됩니다.
`sim/build/vbc.js`는 빌드 결과물이라 저장소에 없습니다. C 코드를 바꾸면 `make sim`(또는 `make wasm`)으로 다시 만들고 브라우저를 새로고침합니다.

## 조작
| 키 | 동작 |
|---|---|
| `I` | 시동 켜기/끄기 (정지 상태에서만, R1) |
| `P` `R` `N` `D` | 기어 (정지 상태에서만, R2) |
| `↑` / `Shift` | 가속 / 강한 가속 |
| `↓` | 브레이크 (가속과 같이 누르면 브레이크 우선, R4) |
| `←` `→` | 핸들 (화면에서만 씀, 코어로 보내지 않음). 손을 떼면 가운데로 돌아오고, 빠를수록 덜 꺾임 |
| `L` / `W` | 조명 스위치 / 와이퍼 스위치 순환 |
| `1` ~ `4` | 도어 FL FR RL RR 열림/닫힘 |
| `Space` / `.` | 일시정지 / 일시정지 중 0.05초씩 진행 |
| `[` `]` | 배속 0.25 · 0.5 · 1 · 2 |
| `C` | 카메라: 추적 → 정면 → 위 → 측면 |
| `Tab` / `H` | 툴박스 접기 / 도움말 |

처음에는 `I` → `D` → `↑` 로 출발해 보세요.

## 화면 구성
- **좌상단 툴박스**: 강우량, 조도(낮·흐림·황혼·밤·칠흑), 문 덜 닫힘, 조명·와이퍼 스위치, 고장 주입(조도·강우·차량 상태 끊김, 범위 밖 값), 시간(일시정지·배속), 시나리오 재생, 리셋(C5)
- **하단 계기판**: 속도, 기어, 시동·전조등·와이퍼·잠금 상태, 센서 고장·수신 중단 램프, 계기판 경고, 시뮬레이션 시각
- **우상단 이벤트 로그**: 출력이 바뀐 순간이 시각과 함께 찍힙니다. 예: `[  1.000s] 전조등: 꺼짐 → 켜짐`
- **3D 장면**: 세종대학교 주변 실제 도로·지형(OpenStreetMap, 고도 자료)입니다.
  - **직접 운전**(주행 모드): `←` `→`로 핸들을 돌려야 커브·교차로를 돕니다. 도로 밖으로도 나갈 수 있고, 건물(또는 지도 끝)에 부딪히면 멈출 때까지 브레이크가 걸립니다. 후진(`R`)으로 빠져나옵니다.
  - **자동 주행**(속도 직접 입력, 시나리오 재생): 능동로 → 광나루로 → 군자로 → 천호대로 순환로(약 2.9 km)를 따라갑니다. `←` `→`는 차로 안 좌우 이동.
  - 차선·횡단보도·보도·건물·공원·가로수·가로등, 실사 하늘(낮·흐림·밤 HDRI), 곡면 차(도장 반사, 유리 너머 실내), 비와 젖은 노면, 전조등 블룸.
  - 지형은 공개 고도 자료(약 30 m 해상도)라 높이 오차가 수 m 있습니다. 세종대 건물 층수는 `sim/tools/map-overrides.json`에 적어 보정합니다.
  하늘 밝기는 조도, 흐림·비·젖은 노면은 강우량을 연속으로 비출 뿐 요구사항 판단에는 쓰지 않습니다. 도로 모양도 판단과 관계없습니다.
- **우측 미니맵**: 경로와 차 위치, 지금 달리는 도로 이름. 지도 데이터 © OpenStreetMap contributors (ODbL).
- **화질**(툴박스): 기본(그림자·블룸 없음) / 표준 / 높음. 느리면 기본으로 바꾸세요. URL `?q=low`도 같습니다.
- **실사 차 모델**(선택): `sim/assets/car.glb`를 넣으면 그 모델로 바뀝니다. 노드 이름 규칙은 `sim/assets/README.md`.

시간은 시뮬레이션 시각입니다. 일시정지 중에는 시각이 흐르지 않아 수신 중단(C4)도 생기지 않습니다.
툴박스를 바꾸지 않아도 속도·기어·시동·조도·강우량은 0.05초마다 자동으로 다시 보내집니다 (사양서 3.1).

## 요구사항별로 확인해 보기
| 해 볼 것 | 기대 결과 | 요구사항 |
|---|---|---|
| 시동 ON, 조명 AUTO, 조도 **밤** | 1초 뒤 전조등 켜짐. **낮**으로 바꾸면 3초 뒤 꺼짐 | L2, L3 |
| 조명 AUTO, 와이퍼 **LOW**, 낮 | 10초 뒤 전조등 켜짐, 와이퍼 OFF면 바로 꺼짐 | L6 |
| 와이퍼 AUTO, 강우 9 → 10 → 40 → 70% (주행 중) | 정지 → 간헐 → 저속 → 고속. 정차하면 한 단계 낮음 | W2, W3 |
| 고장 주입 **조도 범위 밖** | 전조등 켜짐 + 조도 고장 램프 + 경고 "조도 센서 고장" (시동 OFF여도 램프 유지) | L5, D5 |
| 고장 주입 **강우 끊김** | 0.5초 뒤 강우 고장, 와이퍼 AUTO면 간헐 | C2, W5 |
| 15 km/h 이상으로 주행 | 그 순간 도어 잠김. 정지 후 `P`면 풀림 | D3, D2 |
| 주행 중 **차량 상태 끊김** | 0.5초 뒤 수신 중단 램프, 전조등·와이퍼 꺼짐, 잠금은 유지 | C4, D1 |
| 주행 중 문 열기 (`1`~`4`) | 빨간 "주행 중 도어 열림". 속도 0이면 사라짐 | D4, D5 |

### 시작 장면을 URL로 지정 (시연용)
`cmd=`에 콘솔 명령을 여러 개 넣을 수 있습니다 (`docs/console.md`). `cam`은 0 추적, 1 정면, 2 위, 3 측면. `q`는 화질(low·std·high), `car=code`는 코드 차 모델.
`at=1200`은 경로의 1.2 km 지점(군자로)에서 출발, `map=straight`는 예전 직선 도로입니다.
```
http://localhost:8080/sim/?cmd=drive%20off&cmd=ign%20on&cmd=gear%20d&cmd=speed%2050&cmd=light%20auto&cmd=wiper%20auto&cmd=lux%20300&cmd=rain%2075&cmd=door%20rl%20open
http://localhost:8080/sim/?scenario=L2_dark_turns_on.txt
```

### 시나리오 재생
툴박스 **시나리오**에서 `scenarios/*.txt`를 골라 ▶를 누르면 명령이 차례로 실행되고, 로그에 `expect` 결과가 ✔/✘로 찍힙니다.
판정 시각과 결과는 터미널 실행(`./build/vbc scenarios/<파일>`)과 같습니다.

## 터미널로 실행·테스트 (emsdk 불필요)
```bash
make test        # 단위 테스트 전체
make scenarios   # scenarios/*.txt 전체 (PASS/FAIL)
make run         # 터미널에서 명령으로 조작 (help 로 명령 목록)
```

## 문제 해결
| 증상 | 확인 |
|---|---|
| `emcc가 없습니다` | 그 터미널에서 `source ~/emsdk/emsdk_env.sh`를 했는지 |
| 화면에 "C 코어를 불러오지 못했습니다" | `sim/build/vbc.js`가 있는지 → `make wasm` |
| `Address already in use` | 포트를 바꿔 실행: `SIM_PORT=8081 make sim` → `http://localhost:8081/sim/` |
| 파일을 더블클릭해 열었더니 빈 화면 | `file://`로는 모듈을 못 읽습니다. 반드시 `make sim`의 주소로 엽니다 |
| Windows 브라우저에서 접속 안 됨 | WSL 터미널에 서버가 떠 있는지, 주소가 `localhost`인지 |
| 키가 안 먹힘 | 화면(3D 영역)을 한 번 클릭. 숫자 입력칸에 커서가 있으면 단축키가 꺼집니다 |
| C를 고쳤는데 그대로 | `make wasm` 후 브라우저 강력 새로고침(`Ctrl+Shift+R`) |

## 구조
```
브라우저 (sim/)  ── 키보드·툴박스·시나리오 → 명령 문자열 ──►  C 코어 (WebAssembly, src/)
   3D 장면·계기판·로그  ◄── 상태 JSON (sim_state) ──────────   console.c → vbc_step()
                                                              common.c(C1~C5) → 와이퍼 → 조명 → 도어 → 계기판
```
- 화면 코드(`sim/js`)에는 요구사항 판단(1000 lx, 15 km/h 등)이 없습니다. 모두 C 코어의 출력을 그립니다.
- 같은 C 코드를 `make test`(gcc)와 브라우저(emcc)가 함께 씁니다.

## 문서
| 문서 | 내용 |
|---|---|
| `docs/spec/VBC-SRS-001_v1.0.pdf`, `docs/requirements.md` | 사양서와 요구사항 요약 |
| `docs/roles.md` | 역할, 담당, 검증자 |
| `docs/c-structure.md` | C 구조, 모듈 작성 규칙, 질의 후보 |
| `docs/console.md` | 콘솔 명령 (시나리오·URL `cmd=`에 같은 명령) |
| `docs/sim-plan.md` | 시뮬레이터 설계와 단계 계획 |
| `docs/verification.md` | 요구사항별 증명 기록, AI가 틀린 곳 |

협업 규칙: `main`에 직접 push하지 않습니다. 기능 브랜치 → `develop`으로 PR → 동료 리뷰 → 머지. 자세한 내용은 `CLAUDE.md`.
