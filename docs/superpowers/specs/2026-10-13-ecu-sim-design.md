# ECU 테스트 시뮬레이터 (BCU system 구성도) 설계

브랜치: `test/ecu-sim` (`test/module-consoles` `2f2d6f2`에서 분기)
상태: 설계 합의 완료, 2026-10-13. 구현 계획은 다음 단계.

## 1. 목표

모듈별 테스트 콘솔(TEST MODULE, `tools/consoles/modcon.c`)을 엔진으로 삼아, BCU 전체를 구성도로 보여 주는 브라우저 시뮬레이터를 만든다.

- 구성도: 입력 신호 → **BCU system**(VBC 상위 제어기 → SWC 4개) → 출력 ECU 4개. 화살표마다 지금 흐르는 실제 값을 표시한다.
- 시나리오(TC 파일)를 골라 재생하면 터미널 `make tc`와 **같은 엔진, 같은 파일**로 돌아서 판정(PASS/FAIL)과 판정 시각이 터미널과 같다.
- 사용감은 기존 그래픽 시뮬레이터(REAL SIM, `sim/index.html`)와 비슷하게: 시나리오 드롭다운, ▶/■, 배속, 시각 표시, 이벤트 로그.

하지 않는 것: `src/` 제품 코드와 `src/common.h`·`src/vbc.h` 변경, 사양서에 없는 동작 추가, 기존 REAL SIM(`sim/index.html`, `sim/js/*`) 수정, 터미널 세션 미러링(WebSocket, 이번 범위 밖).

## 2. 구성도

```
 입력 신호            ┌──────────────────── BCU system ────────────────────┐         출력 ECU
 (VbcInputs)          │                                                    │
 ignition ──►         │                ctx ──► [Lighting SWC] ─────────────┼──► 조명 ECU
 gear     ──►         │                           ▲ wiper level (L6)       │
 speed    ──►         │  [VBC 상위 제어기] ctx ──► [Wiper SWC] ─────────────┼──► 와이퍼 ECU
 lux      ──►         │   C2~C4 판정                                        │
 rain     ──►         │   → VbcContext   ctx ──► [Doors SWC] ──────────────┼──► 도어 ECU
 door ×4  ──►         │                                                    │
 light_sw ──►         │                ctx ──► [Dashboard SWC] ────────────┼──► 계기판 ECU
 wiper_sw ──►         └────────────────────────────────────────────────────┘
```

- VBC 상위 제어기 = `vbc_build_context()` (C2~C4). SWC = 각 모듈의 `*_step()`.
- SWC는 수직 병렬로 배치한다. 실행 순서는 `vbc_step()`과 같다: 와이퍼 → 조명 → 도어 → 계기판.
- Wiper SWC → Lighting SWC 링크는 L6(이번 주기 와이퍼 단계)다.

### 2.1 VBC → SWC 화살표에 싣는 필드

현재 코드의 `*_step()`이 실제로 읽는 `VbcContext` 필드. 화면 표시용 상수이며 판단에 쓰지 않는다. 모듈 코드가 읽는 필드가 바뀌면 이 표와 `sim/js/ecu/` 의 매핑을 함께 고친다.

| SWC | ctx 필드 |
|---|---|
| Lighting | ignition_on, light_sw, lux_valid, lux, lux_fault (+ 와이퍼 단계 링크) |
| Wiper | ignition_on, wiper_sw, rain_valid, rain, rain_fault, speed_valid, speed_kmh |
| Doors | rx_lost, ignition_on, gear, speed_valid, speed_kmh |
| Dashboard | speed_valid, speed_kmh, any_door_open, lux_fault, rain_fault |

### 2.2 SWC → 출력 ECU

| ECU | 받는 값 (`VbcOutputs`) | 표시 |
|---|---|---|
| 조명 | headlight_on, lux_fault_lamp | headlight_on이면 밝게, 모드 ON/OFF. 조도 고장등은 블록 안 램프 |
| 와이퍼 | wiper, rain_fault_lamp | wiper ≠ STOP이면 밝게, 모드 INT/LOW/HIGH. 강우 고장등은 블록 안 램프 |
| 도어 | door_locked | 잠김/풀림을 서로 다른 색으로 (밝기는 같게). 모드 LOCKED/UNLOCKED |
| 계기판 | warning | warning ≠ NONE이면 밝게, 모드 = 경고 이름 |

계기판 FAULT는 **사양서대로** 한다 (D5): 계기판으로 나가는 경고는 한 번에 하나 (주행 중 도어 열림 > 조도 센서 고장 > 강우 센서 고장). 동시에 성립한 후보는 VBC → Dashboard SWC 화살표의 값(lux_fault, rain_fault, speed_valid·speed_kmh·any_door_open)으로 보이고, 계기판 ECU에는 선택된 하나만 보인다.

화면은 `out` 값을 그대로 그린다. 작동 여부·색 결정은 값 → 표시 매핑일 뿐 요구사항 판단이 아니다.

## 3. 구성요소와 파일

```
tools/consoles/
  modcon.c/h      엔진 / 터미널 REPL 분리 (3.1)
  con_doors.c     정의 함수 con_doors_define(ModCon*) + main (#ifndef MODCON_NO_MAIN)
  con_dashboard.c 〃
  con_lighting.c  〃
  con_wipers.c    〃
  con_ecu.c       [신규] BCU 전체 콘솔. make con-ecu
  ecu_wasm.c      [신규] WASM 진입점 (3.3)
  tc/ecu.txt      [신규] ECU 단위 TC (5장)
  tc/run_tc.py    MODULES에 ecu 추가

sim/
  ecu.html        [신규] 구성도 페이지
  js/ecu/         [신규] engine.js(WASM 래퍼), diagram.js(SVG), runner.js(시나리오 재생), panel.js(입력 패널), main.js
  css/ecu.css     [신규]

Makefile          con-ecu (기존 con-% 규칙), wasm-ecu → sim/build/ecu.js, sim-ecu
```

### 3.1 엔진 API (`modcon.h`)

```c
void modcon_start(ModCon *m);                  /* C5 초기 상태, 0초 첫 송신 (지금 modcon_run 앞부분) */
int  modcon_exec(ModCon *m, const char *line); /* 명령 한 줄. 0 OK, 1 오류, 2 quit */
void modcon_advance(ModCon *m, long ms);       /* 1ms 단위 진행 (t 명령과 같은 함수) */
const char *modcon_state_json(const ModCon *m);/* 다음 호출 전까지 유효 */
void modcon_finish(ModCon *m);                 /* 진행 중 TC 마무리 + 결과 요약 출력 */
int  modcon_run(ModCon *m);                    /* 터미널 REPL: 위 함수들을 감싼다. 기존 동작·출력 그대로 */
```

- 변화 로그와 `e`/`obs` 판정 줄은 지금처럼 `printf`. WASM에서는 Emscripten `print` 훅으로 화면 이벤트 로그에 간다.
- 엔진 모드(`modcon_exec`)에서는 인자 없는 명령이 값을 되묻지 않고 오류(1)를 낸다. 터미널 REPL은 지금처럼 되묻는다.
- `ModCon` 정의부에 `unsigned swc` (활성 SWC 비트: lighting, wiper, doors, dashboard)를 추가한다. 모듈 콘솔은 자기 SWC 하나, ECU 콘솔은 넷 다.

### 3.2 ECU 콘솔 (`con_ecu.c`)

- `mod_step`: `vbc_step()`과 같은 순서로 wipers → lighting(와이퍼 단계는 이번 주기 와이퍼 SWC 출력) → doors → dashboard.
- `show`: 모든 신호 (와이퍼 단계 주입 `SIG_WIPER_LEVEL` 제외). `outputs`: 모든 출력.
- 시작값: 시동 OFF, 기어 P (C5).
- 직전 상태 주입(`state_cmd`)은 두지 않는다.

`vbc_step()`을 직접 부르지 않는 이유: modcon은 `VbcInputs`/`VbcContext`/`VbcOutputs`를 따로 들고 `vbc_build_context()` 후 `mod_step`을 부르는 구조이기 때문. 순서는 `vbc.c`와 같게 두고 주석에 근거를 적는다.

### 3.3 WASM (`ecu_wasm.c`)

5개 콘솔 정의(ecu, doors, dashboard, lighting, wipers)를 모두 담고, JS에 다음을 내보낸다.

```c
int  ecu_select(const char *name);   /* 콘솔 바꿈 + modcon_start. 0 OK, 1 없는 이름 */
int  ecu_exec(const char *line);
void ecu_advance(unsigned ms);
const char *ecu_state(void);
void ecu_finish(void);
```

빌드: `make wasm-ecu` → `sim/build/ecu.js` (emcc, `-DMODCON_NO_MAIN`, `src/*.c` 중 main 제외 + `tools/consoles/*.c`). 기존 `make wasm`과 같은 플래그 계열.

### 3.4 state JSON

```json
{
  "console": "ecu", "swc": ["wiper","lighting","doors","dashboard"], "t": 2900,
  "in": { "ign": {"code":1,"received":true,"age_ms":12,"cut":false}, "gear": {...}, "speed": {...},
          "lux": {...}, "rain": {...}, "doors": [0,0,1,0], "light_sw": 1, "wiper_sw": 1 },
  "ctx": { VbcContext 전체 필드 },
  "link": { "wiper_level": "low", "source": "swc" },
  "out": { "headlight": 1, "lux_lamp": 0, "wiper": "low", "rain_lamp": 0, "locked": 1, "warning": "none" },
  "tc": { "id": "TC-ECU-L6-01", "count": 3, "checks": 12, "failures": 0, "observations": 0 }
}
```

- `in`: 입력 → VBC 화살표. 원래 코드 그대로 (정의 안 된 코드 포함).
- `link.source`: 조명 모듈 콘솔에서는 `"inject"` (와이퍼 단계를 주입), ECU 콘솔에서는 `"swc"`.
- 정의 안 된 enum 코드는 이름 배열 밖을 읽지 않고 `"INVALID"`로 낸다 (`sim_api.c`의 `name_or`와 같은 방식).

## 4. 화면 (`sim/ecu.html`)

```
┌ 상단 바 ─────────────────────────────────────────────────────────────────────────┐
│ 콘솔 [ECU ▾]  시나리오 [ecu.txt ▾] ▶ ■ ⏭  배속 [1× ▾]   t = 2.900 s   TC-…  PASS n / FAIL n │
├────────────┬──────────────────────── BCU system ─────────────────────┬────────────┤
│ 입력 패널  │  입력 화살표 → VBC → ctx 화살표 → SWC ×4 → 출력 화살표     │ 출력 ECU ×4 │
├────────────┴──────────────────────────────────────────────────────────┴────────────┤
│ 이벤트 로그: 명령 / 변화 로그 / PASS·FAIL (FAIL 빨강)                                │
└───────────────────────────────────────────────────────────────────────────────────┘
```

- **화살표**: 현재 값을 라벨로. 값이 바뀐 순간 잠깐 깜박임. 끊긴 입력(cut)은 점선 회색, C2/C4 판정(lux_fault, rain_fault, rx_lost)이 참이면 해당 화살표 빨강.
- **SWC**: 활성은 진하게, 비활성(모듈 콘솔 선택 시 나머지 셋)은 흐리게. L6 링크는 조명 모듈 콘솔에서 "주입" 라벨.
- **출력 ECU**: 2.2 표대로.
- **콘솔 드롭다운**: ECU / 도어 / 계기판 / 조명 / 와이퍼. 바꾸면 `ecu_select()` 후 그 콘솔의 TC 파일(`tools/consoles/tc/<콘솔>.txt`)을 시나리오로 보여 준다.
- **재생**: 줄 단위로 `ecu_exec()`. `t <초>`만 JS가 가로채 벽시계 × 배속으로 나눠 `ecu_advance()`한다. 흘린 시간의 합은 `t` 값과 정확히 같아 판정 시각이 터미널과 같다 (REAL SIM `scenario.js`의 `wait`와 같은 방식). 끝나면 `ecu_finish()`.
- **⏭**: 일시정지 중 한 판단 주기(기본 1ms, `period step`) 진행. 시나리오 재생 중에는 다음 줄 실행.
- **입력 패널**: 값 변경·cut/resume을 명령 문자열(`speed 15`, `cut lux`)로 만들어 `ecu_exec()`에 넣는다. 수동 조작과 시나리오가 같은 경로.
- **URL 파라미터**: `?console=ecu&tc=ecu.txt&speed=2` (발표용 시작 장면).
- **그리기**: SVG + 바닐라 JS, 의존성 추가 없음. JS에 요구사항 판단 없음.
- **서빙**: `make sim-ecu` = `wasm-ecu` 후 저장소 루트에서 `python3 -m http.server` (TC 파일을 `fetch`로 읽기 위해). 주소 `http://localhost:8080/sim/ecu.html`.

## 5. ECU 단위 TC (`tools/consoles/tc/ecu.txt`)

모듈 TC가 볼 수 없는 SWC 간 상호작용만 다룬다. 범위(합의함):

1. **L6 와이퍼 → 조명 연동**: 와이퍼 SWC 출력이 조명 SWC로 넘어가 10초 연속 판정 (9.999 / 10.000 / 10.001초, 중간 정지 후 재성립).
2. **D5 경고 우선순위**: 고장이 동시에 성립할 때 계기판에 하나만 (도어 열림 > 조도 고장 > 강우 고장), 상위 해소 시 다음 순위로.
3. **C4 수신 중단 시 SWC 4개 동시 반응**: 조명·와이퍼는 시동 OFF로 간주(L1, W1), 도어는 잠금 유지(D1), 수신 복귀 후 정상 동작.

- 기대값은 사양서 VBC-SRS-001 v1.0에서 계산하고 TC 줄에 근거 절을 적는다. 사양서에 없는 동작은 `obs`로 관찰만 하고 `docs/c-structure.md` 질의 후보에 올린다.
- 구체적인 TC 목록(ID, 입력, 기대)은 구현 계획 단계에서 사용자에게 먼저 보이고 승인받은 뒤 작성한다.

## 6. 검증

1. **회귀**: 리팩터 전 `make tc`의 `docs/test/logs/*.log`를 보관 → 리팩터 후 다시 실행해 diff. 차이는 생성 시각·커밋 줄만 허용. 기존 `tc/*.txt`는 고치지 않는다.
2. **터미널 = 화면 동일성**: 같은 TC 파일을 `build/con_<콘솔>`과 WASM 엔진(Node로 `sim/build/ecu.js` 로드)에 넣어 출력 로그를 비교한다.
3. **`make test`**: 변경 후 실행하고 결과를 남긴다 (CLAUDE.md).
4. **`make tc`**: ecu 포함 전체 실행, `docs/test/tc-results.md` 갱신.
5. **화면**: `make sim-ecu`로 띄워 ECU 시나리오와 모듈 시나리오 각 하나를 재생, 화살표 값·ECU 밝기/색·모드·PASS/FAIL 표시를 확인.

## 7. 위험과 대응

| 위험 | 대응 |
|---|---|
| modcon 리팩터가 기존 콘솔 출력을 바꿈 | 6.1 로그 diff로 바로 확인. REPL 경로는 출력 문자열을 그대로 둔다 |
| 콘솔별 모듈 상태가 `static`이라 WASM에서 콘솔 전환 시 섞임 | `ecu_select()`가 항상 `modcon_start()` → `mod_init`으로 상태를 초기화 |
| `t` 분할 재생이 판정 시각을 바꿈 | 분할 합 = `t` 값, 1ms 단위 진행이므로 같음. 6.2에서 확인 |
| SWC별 ctx 필드 표(2.1)가 코드와 어긋남 | 표를 `sim/js/ecu/`에 상수 하나로 두고 이 문서를 참조. 모듈 코드 변경 시 갱신 |
