# C 구현 구조와 작업 방법

## 빌드와 테스트
```bash
make          # build/vbc (임시 데모)
make run      # 터미널 입력 실행 (명령: docs/console.md)
make scenarios # scenarios/*.txt 실행 (expect 확인)
make test     # tests/test_*.c 전부 빌드·실행. 새 test_*.c 파일은 자동 포함
make clean
```

## 구조
```
입력(vbc_rx_*, vbc_set_*) ─► common.c: C2~C4 판정 → VbcContext
                                   │
               vbc_step() 호출 순서 ▼
   wipers_step ──(최종 와이퍼 단계)──► lighting_step   doors_step   dashboard_step
        │                                  │               │              │
   out.wiper, rain_fault_lamp   out.headlight_on, lux_fault_lamp  out.door_locked  out.warning
```

| 파일 | 담당 | 내용 |
|---|---|---|
| `src/common.h/c` | A 이태식 | 타입, 입력 수신, C1 타이머(HoldTimer), C2~C4 판정, C5 초기값 |
| `src/vbc.h/c` | A 이태식 | 시스템 상태(VbcSystem), 모듈 호출 순서 |
| `src/console.h/c` | F 이태식 | 터미널 명령 해석, 주기 입력 재송신, 시간 진행, expect |
| `src/drive.h/c` | F 이태식 | 주행 모델 R1~R5 (페달·기어·시동 → 속도). 코어와 독립 |
| `src/main.c` | F 이태식 | 대화형 / 시나리오 파일 실행 |
| `scenarios/*.txt` | 각 담당 | 요구사항별 시연·증명 시나리오 |
| `src/wipers.h/c` | C 이지윤 | W1~W5 |
| `src/lighting.h/c` | B 김성진 | L1~L6 |
| `src/doors.h/c` | D 김의현 | D1~D3 (잠금) |
| `src/dashboard.h/c` | E 김주형 | D4~D5 (계기판 경고) |
| `tests/test_util.h` | 공용 | CHECK, CHECK_EQ, TEST, RUN |
| `tests/sim.h` | 공용 | 주기 입력 자동 공급 + 시간 진행 |
| `tests/test_<모듈>.c` | 각 담당 | 요구사항별 테스트 |

## 모듈 작성 규칙
- 모듈은 `const VbcContext *ctx`만 보고 판단한다. 수신 시각이나 원본 값(`VbcInputs`)을 직접 보지 않는다.
  C2~C4 판단은 이미 `ctx`에 반영돼 있다.
  - 시동: `ctx->ignition_on` (C4 수신 중단이면 이미 false)
  - 유효한 속도: `ctx->speed_valid`가 true일 때만 `ctx->speed_kmh` 사용 (W3, D3, D4)
  - 센서: `ctx->lux_fault`, `ctx->lux_valid`, `ctx->lux` / `ctx->rain_*`
  - D1 수신 중단: `ctx->rx_lost`
- 시간은 `ctx->now`(ms)만 쓴다. `time()`, `clock()`, `sleep()` 금지 → 테스트에서 2.9초/3초를 정확히 확인할 수 있다.
- "N초 연속"(C1)은 `HoldTimer`를 상태 구조체에 넣고 `hold_timer_update()`를 매 주기 호출한다.
- 주기 사이에 기억할 값은 모듈 상태 구조체(`LightingState` 등)에 필드를 추가한다. 전역 변수는 쓰지 않는다.
- `common.h`, `vbc.h`의 시그니처를 바꿔야 하면 A와 먼저 합의한다 (모든 모듈에 영향).

## 테스트 작성 예 (tests/test_lighting.c)
```c
#include "sim.h"
#include "test_util.h"

/* L2: 시동 ON, AUTO, 2초에 조도 3000→300 → 2.9초 꺼짐, 3.0초 점등 */
TEST(l2_dark_1s_turns_on)
{
    Sim s;
    sim_init(&s);                                   /* 시동 ON, P, 속도 0, 50ms마다 입력 */
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
    s.feed.lux = 3000;
    sim_run_until(&s, 1950);
    s.feed.lux = 300;                               /* 2.0초부터 */
    sim_run_until(&s, 2900);
    CHECK_EQ(s.sys.out.headlight_on, false);
    sim_run_until(&s, 3000);
    CHECK_EQ(s.sys.out.headlight_on, true);
}

int main(void)
{
    printf("== L 조명\n");
    RUN(l2_dark_1s_turns_on);
    return test_summary();
}
```
- 끊김·고장 시험: `s.feed.mute = SIM_LUX;` (그 입력만 안 보냄), 범위 밖: `s.feed.lux = 60001;`
- 도어·스위치: `vbc_set_door(&s.sys.in, DOOR_FL, true);`, `vbc_set_wiper_switch(...)`
- 테스트 이름에 요구사항 ID를 붙이고, 결과(PASS/FAIL 출력)를 PR과 `docs/verification.md`에 붙인다.

## 브랜치와 합치기
```
feature/core, feature/lighting, feature/wipers, feature/doors, feature/dashboard, feature/input
        │ PR (리뷰어 = 교차 검증 담당, make test 결과 첨부)
        ▼
     develop ── 통합 테스트 ──► main (발표 기준본)
```
1. `git checkout feature/<모듈>` 후 `git merge origin/develop`으로 최신 인터페이스를 받는다.
2. 자기 모듈 `.c`/`.h`와 `tests/test_<모듈>.c`만 수정한다.
3. `make test` 통과 확인 → `develop`으로 PR. 리뷰어는 roles.md의 교차 검증 담당.
4. 머지 후 다른 사람 브랜치는 `git merge origin/develop`으로 따라간다. 충돌을 합친 뒤에는 `make test`를 다시 돌린다.

## 코어가 정한 해석 (질의 후보)
사양서에 명시가 없어 코어가 임시로 정한 부분. 강사 질의(사양서 9장) 후 답에 맞춰 고친다.

| # | 항목 | 현재 구현 | 근거 |
|---|---|---|---|
| Q1 | 0.5초 경계 | 마지막 수신 후 499ms까지 정상, 500ms부터 미수신(고장·중단) | C2 "0.5초 동안 들어오지 않으면", C3 "0.5초가 지나지 않았고" |
| Q2 | 첫 수신 전(0~0.5초)의 시동·기어 | 시동 꺼짐, 기어 P로 간주 | C5 시작 상태와 일치하도록 |
| Q3 | 첫 수신 전의 조도·강우 | 고장 아님, 값 없음(`lux_valid=false`) → L2·L3·W2 판단 안 함 | C2 "시작 시점부터 0.5초를 셉니다" |
| Q4 | 범위 밖 값 | 들어온 즉시 고장 (0.5초 기다리지 않음), 정상 값이 오면 즉시 해제 | C2 "또는 유효 범위 밖" |
| Q5 | 범위 밖 속도 | 유효한 속도 아님(C3), 수신 중단(C4)은 아님 | C4는 "들어오지 않으면"만 해당 |
| Q6 | 스위치 초기 위치 | 조명 OFF, 와이퍼 OFF | 사양서에 없음. 시동이 꺼져 있어 C5 출력에는 영향 없음 |
| Q7 | 시간 판정 간격 | `vbc_step()`을 50ms 이하 간격 또는 입력 변화마다 호출 | 조건 변화는 step 시점에만 관측됨 |
