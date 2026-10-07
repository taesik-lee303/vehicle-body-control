# 시연용 시뮬레이션 구현 계획 (F 입력/시연)

담당: 이태식 (F)   검증: C (이지윤)   브랜치: `feature/sim` (PR #6 `feature/input`이 develop에 머지된 뒤 develop에서 분기)
상태: 초안 v2, 2026-10-07. M0·M1 완료(PR #7). 팀 검토 전. 12절의 결정 사항을 정한 뒤 M0부터 시작한다.

## 0. 요약

- **그래픽은 브라우저(Three.js)로 만든다.** 사양서 1.2는 "구현 언어, 구조, 화면, 입력 방법은 개발팀이 정한다"고 했으므로 화면까지 C일 필요는 없다. 현실감(비, 야간 조명, 반사, glTF 차 모델)과 툴박스(슬라이더·토글)는 브라우저가 가장 싸게 낸다.
- **판단은 전부 기존 C 코어가 한다.** 화면은 C의 `console_exec()`에 문자열 명령을 넣고, `console_advance()`로 시간을 흘리고, C가 만들어 주는 상태 JSON을 그리기만 한다. 화면 코드에는 `if (lux < 1000)` 같은 요구사항 판단이 한 줄도 없다.
- **C를 브라우저에 붙이는 방법은 둘 중 하나.** 1안 WebAssembly(Emscripten)로 C를 브라우저 안에 넣는다. 2안 C 바이너리를 WSL에서 그대로 실행하고 WebSocket으로 잇는다. JS 쪽은 전송 계층 파일 하나만 다르다. 1안이 발표용으로 좋고(폴더 하나로 어느 노트북에서나 열림), 2안은 설치가 없어 하루 안에 확실히 된다.
- **운전은 방향키(↑ 가속, ↓ 브레이크, ←→ 차선 이동), Shift는 강한 가속.** 운전 조작은 새 C 파일 `drive.c`(R1~R5)가 속도로 바꿔 코어 입력(패널)에 넣는다. 그래서 주행(R)은 선택 요구사항으로 테스트·시나리오와 함께 제출된다.
- **터미널 시나리오(`scenarios/*.txt`)를 화면에서 그대로 재생한다.** 증명(터미널)과 시연(화면)이 같은 입력·같은 코어를 쓴다.

## 1. 목표

사양서 7장 "입력을 바꿔 가며 출력이 어떻게 바뀌는지 직접 보여 줄 수 있어야 한다"와 8장 인수 조건 1 "결과물 시연"을 그래픽 화면으로 만족한다.

- 운전자는 키보드로 차를 몬다 → 속도·기어·시동이 만들어져 5장의 입력이 된다 (사양서 6.1).
- 좌측 상단 직사각형 툴박스로 환경(강우량, 조도), 도어 덜 닫힘, 스위치, 센서 고장을 조절한다.
- 화면의 차가 전조등·와이퍼·도어 잠금·계기판 경고로 반응한다. 반응은 전부 `vbc_step()`의 출력이다.
- 2.9초/3.0초 같은 경계가 화면과 로그에 그대로 보인다 (일시정지, 50ms 한 틱 진행, 배속).

그래픽은 요구사항이 아니라 표현 수단이다. 평가는 C·L·W·D 확인 결과와 R 시연으로 받는다.

## 2. 기술 선택

### 2.1 화면: 브라우저 + Three.js

| 항목 | 브라우저 + Three.js (선택) | raylib (C) | Python(pygame 등) |
|---|---|---|---|
| 현실감 | PBR 재질, HDR 환경광, 그림자, 톤매핑, 블룸, glTF 모델이 기본 제공 | 게임풍. 같은 효과는 셰이더 직접 작성 | 2D 수준 |
| 툴박스(슬라이더·토글) | HTML `<input type=range>` 등으로 바로 | raygui 추가, 거침 | tkinter 혼용 필요 |
| 실행 환경 | Windows 쪽 브라우저가 GPU를 씀. WSL GPU 설정 불필요 | WSLg 창 (GPU 가속 불확실) | 느림 |
| 발표 | `sim/` 폴더를 복사하면 어느 노트북에서든 열림 | WSL 있는 노트북만 | Python 설치 필요 |

### 2.2 C 코어 연결: 1안 WASM, 2안 네이티브 + WebSocket

| 항목 | 1안 WebAssembly | 2안 네이티브 프로세스 + WebSocket |
|---|---|---|
| 방법 | `emcc`로 `src/*.c`(`sim_api.c` 포함)를 `vbc.js`(wasm 내장) 하나로 컴파일. 브라우저가 직접 호출 | `make`로 `build/vbc-json`(stdin 명령 → stdout JSON) 빌드. `sim/bridge/bridge.py`(python `websockets`)가 브라우저와 프로세스를 잇는다 |
| 설치 | WSL에 emcc 없음(확인함). `sudo apt install emscripten`(3.1.6, nodejs 18 포함 수백 MB) 또는 emsdk 최신(약 1.5GB) | 추가 설치 `pip install websockets`뿐. gcc 13.3, python 3.12 이미 있음 |
| 시연 | 서버 없이 `index.html` 열면 끝(단, 모듈 로딩 때문에 `python3 -m http.server` 권장) | WSL에서 프로세스 2개를 띄워 둬야 함 |
| 개발 | C 쪽 printf 디버깅은 브라우저 콘솔로 | `gdb`, 터미널 그대로. C를 고치면 `make`만 |
| 코어 동일성 | 같은 소스를 clang(emcc)으로 컴파일 | `make test`가 검사한 gcc 바이너리와 같은 오브젝트 |

**결정:** 1안을 목표로 하되, M1 첫날 오전에 `apt install emscripten` 후 Hello 빌드가 안 되면 그 자리에서 2안으로 간다. JS는 `core-wasm.js` / `core-ws.js` 중 하나를 고르는 것 외에 같다. 2안은 1안이 되더라도 C 디버깅용으로 계속 쓸 수 있다.

## 3. 구조

```
┌────────────────────────── 브라우저 (sim/) ──────────────────────────┐
│ 키보드(input.js) ─┐                                                 │
│ 툴박스(toolbox.js)┼─ 명령 문자열 ──► core.exec("rain 70")            │
│ 시나리오(scenario.js)┘                                              │
│ 메인 루프(main.js): 벽시계 50ms × 배속마다 ─► core.advance(50)       │
│ 3D 장면(scene/*) ◄── 매 프레임 ── core.state() ── 상태 JSON          │
│ 계기판(hud.js)   ◄┘                                                 │
│ 이벤트 로그      ◄── core.onLog ── console.c의 변화 출력 한 줄씩      │
└───────────────┬────────────────────────────────────────────────────┘
                │ core-wasm.js (1안)  또는  core-ws.js ⇄ bridge.py ⇄ vbc-json (2안)
                ▼
      src/sim_api.c ── sim_init() / sim_exec(line) / sim_advance(ms) / sim_state()
                │
      src/console.c ── 명령 해석, 패널 값 50ms마다 재송신(사양서 3.1), 변화 출력
                │
      src/drive.c (신규) ── 페달·기어·시동 요청 → 속도 (R1~R5) → 패널 speed
                │
      src/vbc.c ── vbc_step(): common.c 판정(C2~C4) → 와이퍼 → 조명 → 도어 → 계기판
```

### 3.1 C 코어가 값을 받는 통로 (바뀌지 않는 약속)

| 방향 | 통로 | 내용 |
|---|---|---|
| 화면 → C | `console_exec(&c, "rain 70")` | 툴박스·키보드·시나리오 전부 **문자열 명령**. 터미널과 같은 통로라서 화면에서 한 조작은 그대로 시나리오 파일에 적을 수 있다 |
| 화면 → C | `console_advance(&c, 50)` | 시간은 시뮬레이션 시각. 벽시계 50ms마다 한 번(배속 2×면 두 번, 일시정지면 0번). 일시정지 중엔 시각이 안 흐르니 C4 수신 중단도 안 생긴다 |
| C → 화면 | `sim_state()` → JSON 한 줄 | `Console.now`, 패널, `sys.in`(도어·스위치), `sys.ctx`(판정: rx_lost, speed_valid, lux_fault…), `sys.out`(출력 6개), drive 상태. C가 `snprintf`로 만든다. JS는 `JSON.parse`만 |
| C → 화면 | `console.c`의 `change()` 출력 | `[  2.900s] 전조등: 꺼짐 → 켜짐`, `✔ headlight on`. 1안은 Emscripten `print` 콜백, 2안은 stdout. 이벤트 로그 패널에 그대로 쌓는다 |

건드리지 않는 것: `src/common.h`, `src/vbc.h`, B~E 모듈. 바뀌는 것: `src/console.c/.h`(F 범위, 명령 추가), 신규 `src/drive.c/.h`, 신규 `sim/`, `Makefile` 타깃 추가.

### 3.2 새 파일

| 파일 | 내용 |
|---|---|
| `src/drive.h/c` | 주행 모델 R1~R5. 입력: 스로틀 단계(0/1/2), 브레이크(0/1), 기어 요청, 시동 요청. 출력: 속력 km/h(float). 순수 C, 시간은 인자로 받음 |
| `src/console.c/.h` (수정) | `drive on\|off`, `pedal <스로틀> <브레이크>` 명령. `drive on`이면 매 tick `drive_tick()` 결과를 `panel.speed`에 넣고, `ign`/`gear`는 drive 규칙(R1·R2)을 통과해야 반영. `expect speed <n>`, `expect gear p\|r\|n\|d` 항목 추가 |
| `src/sim_api.h/c` | 1안·2안 공용. `sim_init`, `sim_exec(const char*)`, `sim_advance(ms)`, `const char *sim_state(void)`. 전역 `Console` 하나. src에 두어 gcc로도 빌드되고 `tests/test_sim_api.c`가 JSON·시간 누적을 검증 |
| `sim/core/stdio_main.c` | 2안용 main. stdin 한 줄 = 명령(`advance 50`, `state`는 특수 처리) → stdout JSON/로그 |
| `sim/bridge/bridge.py` | 2안용. WebSocket(8765) ⇄ 프로세스 stdin/stdout 중계. 60줄 안팎 |
| `sim/index.html`, `sim/css/style.css` | 레이아웃: 전체 화면 3D 캔버스, 좌상단 툴박스, 하단 계기판, 우측 이벤트 로그, 도움말 오버레이. importmap으로 `three` 지정 |
| `sim/js/main.js` | 부팅, 고정 50ms 시뮬 틱 누적기, 배속·일시정지·한 틱 진행, 렌더 루프 |
| `sim/js/core-wasm.js`, `sim/js/core-ws.js` | 전송 계층. 같은 인터페이스 `{init, exec, advance, state, onLog}` |
| `sim/js/input.js` | 키보드 → 명령. 키가 바뀔 때만 `pedal` 전송, 창 포커스 잃으면 `pedal 0 0` |
| `sim/js/toolbox.js`, `sim/js/hud.js`, `sim/js/scenario.js` | 툴박스, 계기판, 시나리오 재생 |
| `sim/js/scene/world.js, car.js, weather.js, lights.js, camera.js` | 도로·하늘·환경광, 차와 부품 애니메이션, 비·안개·젖은 노면, 전조등·블룸, 카메라 3종 |
| `sim/vendor/three/` | three.module.js + addons(GLTFLoader, Sky, EffectComposer, UnrealBloomPass, Reflector) 로컬 복사. 발표장 인터넷 불필요 |
| `sim/assets/` | 차 glTF, HDRI 2장(낮·밤), 노면 텍스처. 출처·라이선스는 `sim/assets/LICENSES.md` |
| `tests/test_drive.c`, `scenarios/R1_*.txt … R5_*.txt` | R1~R5 증명 |
| `Makefile` | `make wasm`(emcc), `make sim`(정적 서버), `make bridge`(2안). `make test`는 emcc 없이 그대로 |

## 4. 화면 구성

```
┌──────────────────────────────────────────────────────────────────────┐
│ ┌─ 툴박스 (Tab으로 접기) ─────┐                                        │
│ │ 주행 모드  [●켜짐]  (끄면 속도 입력칸) │     3D: 도로 · 차 · 하늘 · 비     │
│ │ 강우량   [=====-----] 50 %   │                                        │
│ │ 조도     [==--------] 300 lx │                        ┌─ 이벤트 로그 ──┐│
│ │   낮 │ 흐림 │ 황혼 │ 밤 │ 칠흑 │                        │ [2.000s] …     ││
│ │ 문 덜 닫힘 □FL □FR □RL □RR   │                        │ [3.000s] 전조등 ││
│ │ 조명 SW   OFF │ AUTO │ ON    │                        │  꺼짐 → 켜짐    ││
│ │ 와이퍼 SW OFF │ AUTO │ LOW │ HIGH │                     │ ✔ headlight on ││
│ │ 고장 주입                    │                        └────────────────┘│
│ │  □조도 끊김  □강우 끊김       │                                        │
│ │  □차량 상태 끊김              │                                        │
│ │  □조도 범위 밖  □강우 범위 밖  │                                        │
│ │ 시간  ▶∥  ·한 틱  0.25 0.5 1 2× │                                        │
│ │ 시나리오 [L2_dark_turns_on ▾] ▶ │                                        │
│ │ 리셋(C5)   화질 [높음▾]  도움말 │                                        │
│ └──────────────────────────────┘                                        │
│ ┌─ 계기판 ──────────────────────────────────────────────────────────┐ │
│ │ 시동 ●  기어 D   [  37 ] km/h   전조등 ●  와이퍼 간헐  잠금 🔒          │ │
│ │ ⚠ 주행 중 도어 열림     조도 고장 ○  강우 고장 ○  수신 중단 ○         │ │
│ │ 시뮬 시각 12.350 s   배속 1×                                         │ │
│ └───────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────┘
```

### 4.1 툴박스 항목 ↔ 콘솔 명령 ↔ 보여 주는 요구사항

| 툴박스 | 보내는 명령 | 요구사항 |
|---|---|---|
| 주행 모드 토글 | `drive on` / `drive off` | 6.1 "구현하지 않은 경우 직접 입력": 끄면 속도 입력칸·기어·시동 버튼이 직접 입력(`speed N`)으로 바뀐다. C·L·W·D 시나리오는 이 모드로 돈다 |
| 강우량 슬라이더 0~100 | `rain N` (놓을 때와 드래그 중 100ms마다) | W2 단계, W3 정차 시 한 단계 낮춤, L6 10초 |
| 조도 슬라이더 0~60000(로그 눈금) + 프리셋 낮 30000 / 흐림 5000 / 황혼 1200 / 밤 300 / 칠흑 0 | `lux N` | L2 1초, L3 3초. 황혼 1200은 1000~1500 "직전 상태 유지" 구간 시연용 |
| 문 덜 닫힘 체크 4개 | `door fl open` / `door fl close` | D4 주행 중 도어 열림, D5 우선순위 |
| 조명 스위치 | `light off\|auto\|on` | L4, L2·L3·L6 |
| 와이퍼 스위치 | `wiper off\|auto\|low\|high` | W4, W5, W2·W3 |
| 조도/강우 끊김 토글 | `cut lux` / `resume lux`, `cut rain` / `resume rain` | C2, L5, W5 |
| 차량 상태 끊김 토글 | `cut vehicle` / `resume vehicle` | C4, D1, L1·W1(시동 꺼짐 간주) |
| 범위 밖 주입 토글 | 켜면 `lux 70000`(또는 `rain 101`), 끄면 슬라이더 값으로 복귀 | C2 범위 밖 즉시 고장 |
| 일시정지 · 한 틱 · 배속 | 명령 없음. `advance` 호출 횟수만 바뀜 | 경계 시각(2.95 → 3.00초)을 멈춰 가며 보여 줄 때 |
| 시나리오 재생 | 파일의 명령을 차례로 실행. `wait N`은 실시간(배속 적용)으로 흘림, `expect` 결과 ✔/✘를 로그에 강조 | 증명 시나리오를 화면으로 시연 |
| 리셋 | `reset` | C5 |
| 화질 | 명령 없음. 블룸·그림자·입자 수 단계 | 느린 노트북 대비 |

### 4.2 계기판(HUD)

속도(바늘 + 숫자), 기어, 시동, 전조등 램프, 와이퍼 단계, 자물쇠, 계기판 경고 문구 하나(D5, 도어 빨강 / 센서 노랑), 조도·강우 센서 고장 램프(L5·W5 "시동과 관계없이"), 수신 중단 램프, 시뮬 시각, 배속. 전부 `sim_state()` 값을 그대로 쓴다. 시뮬 시각은 항상 보이게 해서 벽시계와 혼동하지 않게 한다.

## 5. 조작 (키보드)

| 키 | 동작 | 코어로 가는 명령 |
|---|---|---|
| ↑ | 보통 가속 | `pedal 1 0` (키 상태가 바뀔 때만 전송, drive.c가 상태를 기억) |
| Shift (단독 또는 ↑와 함께) | 강한 가속(풀 스로틀) | `pedal 2 0` |
| ↓ | 브레이크. 가속과 함께 누르면 브레이크 우선(R4) | `pedal 0 1` / `pedal 1 1` |
| 아무것도 안 누름 | 타력 감속(R4) | `pedal 0 0` |
| ← → | 차선 안에서 좌우 이동. 화면 연출만, 사양서에 조향 입력이 없어 코어로 안 보냄 | 없음 |
| I | 시동 켜기/끄기. 속도 0일 때만 받음(R1), 아니면 계기판에 "정지 후 조작" 안내 | `ign on\|off` |
| P R N D | 기어. 속도 0일 때만(R2) | `gear p\|r\|n\|d` |
| L | 조명 스위치 순환 OFF → AUTO → ON | `light …` |
| W | 와이퍼 스위치 순환 OFF → AUTO → LOW → HIGH | `wiper …` |
| 1 2 3 4 | 도어 FL FR RL RR 열림/닫힘 토글 | `door fl open` 등 |
| Space | 일시정지/재개 | 없음 |
| . (마침표) | 일시정지 중 한 틱(50ms) 진행 | `advance 50` 한 번 |
| [ ] | 배속 내리기/올리기 (0.25 0.5 1 2) | 없음 |
| C | 카메라 전환: 추적(뒤) → 정면(전조등·와이퍼 잘 보임) → 실내(와이퍼·계기판) | 없음 |
| Tab | 툴박스 접기/펴기 | 없음 |
| H | 도움말 오버레이 | 없음 |

후진은 기어 R에서 ↑로 가속한다(R3). 속도는 사양서 3.1대로 방향 없는 속력이고 후진은 기어 R로 구분한다. 브레이크 등(↓)과 후진등(기어 R)은 입력을 그대로 비추는 연출이지 코어 출력이 아니다.

## 6. 주행 모델 (`drive.c`, R1~R5)

사양서 6.1 원문 기준. R5 "속도가 어떻게 변하는지(가속 정도)는 팀이 정해 발표한다" → 아래는 제안값이며 발표 때 밝힌다.

| 상태 | 변화율 | 50ms tick당 |
|---|---|---|
| 보통 가속 (↑) | +10 km/h/s | +0.5 |
| 풀 스로틀 (Shift) | +25 km/h/s | +1.25 |
| 브레이크 (↓) | −40 km/h/s, 0에서 멈춤 | −2.0 |
| 타력 (입력 없음) | −5 km/h/s, 0에서 멈춤 | −0.25 |
| 최고 속력 | 전진 250, 후진 20 (R5) | |

규칙
- R1: 시동 OFF면 속력 0 유지, 가속 무시. 시동 전환 요청은 속력 0일 때만 받고, 아니면 거부 메시지(로그).
- R2: 기어 전환 요청은 속력 0일 때만. 아니면 거부.
- R3: D에서 가속 → 증가, R에서 가속 → 후진(속력은 양수, 상한 20). P·N에서는 가속해도 0.
- R4: 브레이크가 있으면 감속 우선. 둘 다 없으면 타력 감속. 0 아래로 안 내려감.
- 내부는 float, 패널에는 반올림한 정수(`Signal.value`가 int32).
- `drive on`일 때 `speed` 명령은 무시하고 안내한다. `drive off`면 지금처럼 직접 입력.
- 시동 OFF 상태에서 속력이 0이 아닌 경우는 R1 때문에 생기지 않는다. `cut vehicle`로 송신을 끊어도 drive.c는 계속 계산하고 패널 값이 안 보내질 뿐이다(C4 시험은 그대로 됨).

테스트: `tests/test_drive.c`에 R1~R5 각 조건, 250·20 상한, 브레이크 우선, 타력 0 정지. 시나리오: `R1_no_move_when_off.txt`, `R2_gear_only_at_stop.txt`, `R3_reverse_in_R.txt`, `R4_brake_priority.txt`, `R5_top_speed.txt`. `expect speed <n>`로 확인한다.

## 7. 시각 효과 ↔ 코어 값 매핑

| 코어 값 | 종류 | 화면 |
|---|---|---|
| 조도 (입력) | 환경 | 태양 고도·세기, 하늘(`Sky` addon), 노출(ACES 톤매핑), 낮/밤 HDRI 혼합. 로그 눈금: 60000 한낮, 5000 흐림, 1200 황혼, 300 가로등 밤, 0 칠흑 |
| 강우량 (입력) | 환경 | 빗줄기 인스턴스 수 = 강우량 × 40(상한 4000), 안개 밀도, 노면 거칠기 감소 + 어두워짐 + 반사(`Reflector`), 유리 물방울 텍스처 |
| `headlight_on` | 출력 | 스폿라이트 2개(그림자), 발광 재질, 블룸, 반투명 빛 원뿔, 노면 빛 웅덩이 |
| `wiper` | 출력 | 블레이드 회전 애니메이션. 정지 / 간헐: 3초마다 한 번 왕복 / 저속: 1.0초 주기 / 고속: 0.5초 주기 (주기는 연출값, 사양서는 단계만 정함) |
| `door_locked` | 출력 | 계기판 자물쇠, 잠길 때 '철컥' 효과음(첫 클릭 뒤 활성), 도어 잠금 노브 |
| 도어 열림 (입력) | 입력 | 도어 노드 12° 회전(덜 닫힘), 문틈 실내등 빛, 계기판 도어 아이콘 |
| `warning` | 출력 | 계기판 문구 하나 + 색(도어 빨강, 센서 노랑) |
| `lux_fault_lamp`, `rain_fault_lamp` | 출력 | 계기판 램프. 시동과 무관하게 켜짐 |
| `rx_lost` (판정) | 판정 | 계기판 램프 + 화면 가장자리 노이즈 |
| 속력·기어 (입력) | 입력 | 노면 텍스처 스크롤, 바퀴 회전(R이면 역방향), 속도계 바늘, 고속에서 FOV 약간 확대. ↓ 누르면 브레이크등, 기어 R이면 후진등 |

화면이 "판단"하는 코드를 두지 않는다. 위 표의 왼쪽 열은 전부 `sim_state()`에서 읽은 값이다. 리뷰 체크리스트에 "scene/*.js에 요구사항 수치(1000, 1500, 15, 0.5초 등)가 없는가"를 넣는다.

### 7.1 현실감 단계 (화질 메뉴와 같음)

| 단계 | 내용 | 비용 |
|---|---|---|
| 기본 | 박스로 만든 차, 평면 도로, 단색 하늘, 기본 조명 | 어떤 노트북에서도 60fps |
| 표준 | glTF 차(PBR), HDRI 환경광, 그림자(PCFSoft), `Sky`, 톤매핑, 빗줄기 2000, 안개 | 내장 GPU에서 가능 |
| 높음 | 블룸, 노면 반사(`Reflector`), 빗줄기 4000, 유리 물방울, 실내 카메라 | 외장 GPU 권장 |

### 7.2 차 모델 조건

- 형식 glTF(.glb), 20MB 이하, 라이선스 CC0 또는 CC-BY(출처 표기).
- 노드가 나뉘어 있어야 함: 도어 4개, 바퀴 4개, 가능하면 와이퍼 2개, 전조등·후미등 재질 분리.
- 후보: Sketchfab 무료 CC 모델(도어 분리된 것 검색), Kenney Car Kit(CC0, 저폴리라 현실감은 낮음), Poly Pizza(CC0). 분리가 안 된 모델은 Blender에서 도어·와이퍼를 분리하고 노드 이름을 붙인 뒤 .glb로 내보낸다. 와이퍼가 없으면 얇은 박스 2개를 유리 위에 붙인다.
- HDRI는 Poly Haven(CC0) 낮·밤 각 1장, 2K.

## 8. 단계별 계획

| 단계 | 내용 | 완료 기준 | 기간 |
|---|---|---|---|
| M0 주행 모델 ✅ | `drive.c`, 콘솔 `drive`/`pedal`/`expect speed` 명령, `test_drive.c`, R 시나리오 5개, `docs/console.md` 갱신 | `make test` 통과, `make scenarios`에서 R 시나리오 PASS. 그래픽 없음 | 0.5일 |
| M1 코어 연결 ✅ (1안, emsdk 6.0.11) | `sim_api.c`. 1안: `apt install emscripten` → `make wasm`. 안 되면 2안: `stdio_main.c` + `bridge.py`. 숫자만 있는 임시 페이지 | 브라우저에서 ↑를 누르면 속도 숫자가 오르고, 툴박스 없이 콘솔 입력창으로 `lux 300` 넣고 3초 뒤 로그에 "전조등: 꺼짐 → 켜짐"(조명 모듈 머지 후) | 1일 |
| M2 장면·툴박스 | 박스 차·도로·추적 카메라, 툴박스 전부, 계기판, 이벤트 로그, 일시정지·한 틱·배속, 리셋 | 툴박스 조작 결과가 터미널(`make run`)과 같은 시각에 같은 로그. `L2_dark_turns_on` 재생 시 2.900s ✔ / 3.000s ✔ | 1일 |
| M3 현실감 | glTF 차, HDRI·`Sky`·조도 연동, 비·안개·젖은 노면, 전조등·블룸, 와이퍼·도어 애니, 카메라 3종, 화질 단계 | 밤 + 비 + 와이퍼 + 전조등이 한 화면에 보임. 표준 화질 내장 GPU 30fps 이상 | 1~2일 |
| M4 시연 준비 | 시나리오 재생 다듬기, 도움말, 효과음, 발표 대본(어떤 툴박스로 어떤 요구사항을 보일지), 리허설, `docs/verification.md`에 R 행 추가 | 발표 대본대로 10분 리허설 완주. `sim/` 폴더를 다른 노트북에 복사해 열림 | 0.5일 |

B~E 모듈이 아직 임시 구현이어도 M0~M2는 진행된다(출력이 안 바뀔 뿐). 모듈이 머지될 때마다 화면이 살아난다. M3는 B(조명)·C(와이퍼) 머지 후가 보기 좋다.

## 9. 빌드·실행

```bash
# 코어·테스트 (팀원 전원, emcc 불필요)
make test
make scenarios

# three.js 로컬 복사 (한 번, 인터넷 필요. node 없이 curl로)
sim/tools/fetch-vendor.sh        # jsdelivr에서 three.module.js + addons → sim/vendor/three/

# 1안 WASM
sudo apt install emscripten      # 또는 emsdk: git clone https://github.com/emscripten-core/emsdk ~/emsdk && ~/emsdk/emsdk install latest && ~/emsdk/emsdk activate latest
make wasm                        # sim/build/vbc.js (wasm 내장, -sSINGLE_FILE -sMODULARIZE -sEXPORT_ES6)
make sim                         # python3 -m http.server 8080 (저장소 루트에서. scenarios/ 도 같이 서빙. 포트는 SIM_PORT=...)
# Windows 브라우저에서 http://localhost:8080/sim/   (WSL2 localhost 포워딩)

# 2안 네이티브 + WebSocket
pip install websockets
make bridge                      # build/vbc-json 빌드 후 python3 sim/bridge/bridge.py 실행 (ws://localhost:8765)
make sim                         # 위와 같음. index.html 이 ws 연결을 자동 감지
```

emcc 플래그(참고): `-O2 -sMODULARIZE=1 -sEXPORT_ES6=1 -sSINGLE_FILE=1 -sEXPORTED_FUNCTIONS=_sim_init,_sim_exec,_sim_advance,_sim_state,_malloc,_free -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8`. `console.c`가 `stdout`에 쓰는 한 줄은 모듈 생성 시 `print` 콜백으로 받는다(UTF-8 한글 그대로 옴).

발표용: `sim/` 폴더(vendor, assets, build 포함)와 `scenarios/`를 복사하면 어느 노트북에서든 열린다. 1안이면 WSL이 없어도 된다.

## 10. 위험과 대응

| 위험 | 대응 |
|---|---|
| emcc 설치 실패, apt 3.1.6이 오래됨 | M1 첫 반나절 안에 판단해 2안으로. 2안은 추가 설치가 `websockets`뿐. JS는 전송 계층 파일만 교체 |
| 도어·와이퍼가 분리된 무료 차 모델을 못 찾음 | Blender에서 분리·이름 붙이기(반나절). 그래도 안 되면 문틈 빛 + 계기판 아이콘으로 도어, 박스 와이퍼 |
| 느린 노트북에서 프레임 저하 | 화질 3단계, 입자 상한, 블룸 끄기. 시뮬 틱은 렌더와 분리돼 있어 판정 시각은 안 흔들림 |
| 발표장 인터넷 없음 | three.js·모델·HDRI 전부 로컬, wasm 단일 파일 |
| 모델·HDRI 라이선스 | CC0 우선, CC-BY면 `sim/assets/LICENSES.md`와 README에 출처. 저장소 용량은 모델 20MB 이하로 |
| 화면이 "판단"하게 되는 실수 | scene/*.js, hud.js에 요구사항 수치 금지. PR 체크리스트 항목으로 |
| 시뮬 시각과 벽시계 혼동 | 계기판에 시뮬 시각·배속 상시 표시. 일시정지 중 C4가 안 생기는 이유를 도움말에 적음 |
| `console.c` 변경이 PR #6과 충돌 | `feature/sim`은 PR #6 머지 후 develop에서 분기. M0은 그 전에 로컬에서 준비만 |
| 효과음 자동재생 차단 | 첫 클릭·키 입력 뒤 오디오 활성, 없어도 시연에 지장 없음 |

## 11. 팀에 미치는 영향과 발표 연결

- B~E: 변경 없음. 자기 모듈이 머지되면 화면에서 바로 보인다. 자기 요구사항 시나리오를 `scenarios/`에 넣으면 화면에서도 재생된다.
- `console.c/.h`에 `drive`, `pedal`, `expect speed/gear` 추가(F 범위). `docs/console.md` 갱신.
- 리뷰: C(이지윤). M0은 R1~R5 테스트를 함께 본다. 발표에서 F는 R1~R5 "어떻게 확인했는지"를 `test_drive.c`와 R 시나리오로 답한다.
- 인수 조건 1(시연): 툴박스·키보드로 입력을 바꿔 가며 보인다. 인수 조건 2: R은 테스트+시나리오, 화면은 같은 시나리오 재생으로 증빙. 인수 조건 4·5(Claude Code 활용, Agent 한계): 화면 계층을 코어와 분리한 이유, 같은 시나리오로 증명과 시연을 한 점, 초안에서 바로잡은 것들을 `docs/verification.md` "AI가 틀린 곳"에 계속 기록.

## 12. 결정이 필요한 것 (팀 회의)

1. 코어 연결: 1안 WASM(목표) + 2안 대비 — 이대로 갈지.
2. 키 배치(5절): Shift를 "강한 가속"으로 두는 것. 대안은 Shift = 가속, ↑ = 없음.
3. 가속 수치(6절): 발표에서 팀이 정한 값으로 설명해야 하므로 숫자를 확정.
4. 차 모델: 현실감(Sketchfab CC-BY) vs 라이선스 단순함(Kenney CC0).
5. ←→를 나중에 X1 "방향지시등"(사양서 6.2 예시)으로 확장할지. 지금 범위에는 넣지 않는다.
