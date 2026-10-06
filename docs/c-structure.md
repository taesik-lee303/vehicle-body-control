# C 구현 구조와 브랜치 전략

## 파일 구조 (제안)
```
vehicle-body-control/
├── src/
│   ├── common.h              (A 담당) 공통 구조체, 상태, C1~C5 타이머
│   ├── common.c              (A 담당)
│   ├── lighting.h            (B 담당) L1~L6 로직
│   ├── lighting.c            (B 담당)
│   ├── wipers.h              (C 담당) W1~W5 로직
│   ├── wipers.c              (C 담당)
│   ├── doors.h               (D 담당) D1~D5 로직
│   ├── doors.c               (D 담당)
│   ├── input.h               (E 담당) 입력 처리 (파일/터미널/시뮬레이션)
│   ├── input.c               (E 담당)
│   ├── output.h              (E 담당) 출력 표시
│   ├── output.c              (E 담당)
│   └── main.c                (A 담당) 메인 루프, 각 모듈 호출
├── tests/
│   ├── test_common.c         (테스트)
│   ├── test_lighting.c       (B가 작성, D가 리뷰)
│   ├── test_wipers.c         (C가 작성, B가 리뷰)
│   ├── test_doors.c          (D가 작성, C가 리뷰)
│   └── test_integration.c    (모든 모듈이 함께 동작하는지)
├── Makefile
└── docs/
    ├── requirements.md
    ├── roles.md
    └── verification.md
```

## 핵심 인터페이스 (common.h)
```c
/* 상태 구조체 */
typedef struct {
  /* 입력 */
  uint16_t speed;           // 0~250 km/h
  char gear;                // 'P', 'R', 'N', 'D'
  uint8_t engine_on;        // 0/1
  uint16_t illuminance;     // 0~60000 lux (65535 = 센서 고장)
  uint8_t rainfall;         // 0~100 % (255 = 센서 고장)
  uint8_t door_fl, door_fr, door_rl, door_rr;  // 0=닫음, 1=열림
  char light_switch;        // 'A'(AUTO), 'O'(OFF), 'N'(ON)
  char wiper_switch;        // 'O'(OFF), 'A'(AUTO), 'L'(LOW), 'H'(HIGH)
  
  /* 타임스탬프 (C2~C4용) */
  time_t speed_time, gear_time, engine_time;
  time_t illuminance_time, rainfall_time;
  
  /* 출력 */
  uint8_t headlight;        // 0=꺼짐, 1=켜짐
  uint8_t light_fault;      // 0=정상, 1=고장
  uint8_t wiper_step;       // 0=정지, 1=간헐, 2=저속, 3=고속
  uint8_t rainfall_fault;   // 0=정상, 1=고장
  uint8_t door_lock;        // 0=풀림, 1=잠김
  uint8_t warning;          // 0=없음, 1=주행중도어, 2=조도고장, 3=강우고장
} VehicleState;

/* C1~C5 상태 (코어에서 관리) */
typedef struct {
  time_t last_valid_speed_time;
  time_t condition_start_time;  // 특정 조건이 시작된 시각
  // ... 각 조건별 타이머
} TimerState;

/* 함수 인터페이스 */
void state_init(VehicleState *state);
uint8_t is_valid_speed(const VehicleState *s, const TimerState *t);
uint8_t is_receiving_stopped(const VehicleState *s, const TimerState *t);
uint8_t check_sensor_fault(const VehicleState *s, const TimerState *t, char sensor);
// ...

/* 각 모듈의 함수 */
void lighting_update(VehicleState *s, const TimerState *t);
void wipers_update(VehicleState *s, const TimerState *t);
void doors_update(VehicleState *s, const TimerState *t);
```

## 브랜치 전략

### 주요 브랜치
- `main`: 안정적인 릴리스 버전
- `develop`: 통합 개발 브랜치

### 기능 브랜치 (각자 담당 모듈)
```
feature/core          → A (팀장) 담당
feature/lighting      → B 담당
feature/wipers        → C 담당
feature/doors         → D 담당
feature/input-output  → E 담당
```

### 통합 흐름
```
feature/core
  ↓ PR + 리뷰 (develop으로, 리뷰어: B)
develop ← merge
  ↓
feature/lighting
  ↓ PR + 리뷰 (develop으로, 리뷰어: C)
develop ← merge
  ↓
feature/wipers
  ↓ PR + 리뷰 (develop으로, 리뷰어: D)
develop ← merge
  ↓
feature/doors
  ↓ PR + 리뷰 (develop으로, 리뷰어: B)
develop ← merge
  ↓
feature/input-output
  ↓ PR + 리뷰 (develop으로, 리뷰어: A)
develop ← merge
  ↓
develop → main (최종 통합, 전체 테스트 후)
```

## 각 단계별 작업

### 1단계: 코어 구조 정의 (A 담당, develop에 merge)
- [ ] `common.h`: VehicleState, TimerState 구조체 정의
- [ ] `common.c`: C1~C5 타이머 로직 구현
- [ ] `main.c`: 메인 루프 스켈레톤 (각 모듈 함수 호출)
- 브랜치: `feature/core`
- PR: develop으로 (리뷰어: B)

### 2단계: 각 기능 모듈 구현 (병렬)
- B: `feature/lighting` → lighting.h/c + test_lighting.c
  - PR: develop으로 (리뷰어: C)
- C: `feature/wipers` → wipers.h/c + test_wipers.c
  - PR: develop으로 (리뷰어: D)
- D: `feature/doors` → doors.h/c + test_doors.c
  - PR: develop으로 (리뷰어: B)

### 3단계: 입력/출력 (E 담당, develop에 merge)
- [ ] `input.c`: 입력 처리 (파일/시나리오/터미널)
- [ ] `output.c`: 결과 표시
- 브랜치: `feature/input-output`
- PR: develop으로 (리뷰어: A)

### 4단계: 통합 테스트 (모두)
- develop에서 전체 모듈이 함께 동작하는지 확인
- `test_integration.c` 작성 (각자 자기 요구사항 테스트 추가)

### 5단계: 최종 릴리스 (develop → main)
- develop에서 회귀 테스트 완료 후
- main으로 merge (태그: v1.0.0)

## PR 체크리스트
각 PR에는 다음을 포함하세요:

```markdown
## 요구사항 ID
[예: C1, C2, L1~L6]

## 변경 내용
- [파일 목록]
- [구현한 내용]

## 테스트 결과
| 입력 | 기대 | 실제 | 결과 |
|---|---|---|---|

## 리뷰 체크리스트 (리뷰어용)
- [ ] 코드가 요구사항을 구현했는가
- [ ] 경계값(예: 2.9초, 3초)을 테스트했는가
- [ ] 다른 모듈과의 인터페이스가 올바른가
- [ ] 타이머/센서 고장 처리가 올바른가
```

## 충돌 해결
여러 사람이 `common.h`를 건드릴 수 있으니, A가 interface를 먼저 정의하고 다른 사람이 건드리지 않도록 합시다.

만약 충돌이 나면:
1. 양쪽 변경을 코드 리뷰로 확인
2. 합친 결과를 테스트
3. 전체 통합 테스트 실행
