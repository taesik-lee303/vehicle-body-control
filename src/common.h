/*
 * common.h - 공통 타입, 입력 수신, 공통 규칙(C1~C5)
 * 담당: A (이태식)
 *
 * 모든 모듈이 이 파일에 의존한다. 바꿀 때는 팀 합의 후 feature/core에서 PR.
 *
 * 사용 흐름
 *   1. 입력이 들어올 때마다 vbc_rx_*() / vbc_set_*()  (수신 시각을 함께 넘긴다)
 *   2. 주기마다 vbc_build_context()로 C2~C4 판정 결과(VbcContext)를 만든다
 *   3. 각 모듈은 VbcContext만 보고 판단한다 (센서 원본 값과 수신 시각을 직접 보지 않는다)
 * 2~3은 vbc.c의 vbc_step()이 대신 해 준다.
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdint.h>

/* 시간: 시작 시점(0)부터의 밀리초. 실제 시계 대신 호출자가 넣는다 (2.9초/3초 경계 테스트용). */
typedef uint32_t vbc_ms_t;

/* 사양서 수치 (3.1, C2~C4) */
#define VBC_RX_TIMEOUT_MS 500u /* 0.5초 */
#define VBC_SPEED_MIN 0
#define VBC_SPEED_MAX 250
#define VBC_LUX_MIN 0
#define VBC_LUX_MAX 60000
#define VBC_RAIN_MIN 0
#define VBC_RAIN_MAX 100

typedef enum { GEAR_P, GEAR_R, GEAR_N, GEAR_D } Gear;
typedef enum { LIGHT_SW_OFF, LIGHT_SW_AUTO, LIGHT_SW_ON } LightSwitch;
typedef enum { WIPER_SW_OFF, WIPER_SW_AUTO, WIPER_SW_LOW, WIPER_SW_HIGH } WiperSwitch;
typedef enum { WIPER_STOP, WIPER_INTERMITTENT, WIPER_LOW, WIPER_HIGH } WiperLevel;
typedef enum {
    WARN_NONE,
    WARN_DOOR_OPEN_WHILE_DRIVING, /* 주행 중 도어 열림 */
    WARN_LUX_SENSOR_FAULT,        /* 조도 센서 고장 */
    WARN_RAIN_SENSOR_FAULT        /* 강우 센서 고장 */
} Warning;
typedef enum { DOOR_FL, DOOR_FR, DOOR_RL, DOOR_RR, DOOR_COUNT } DoorId;

/* 0.5초보다 짧은 간격으로 계속 들어와야 하는 입력 하나 (속도·기어·시동·조도·강우량) */
typedef struct {
    int32_t value;       /* 들어온 값 그대로. 범위 밖 값도 저장한다 (판정은 vbc_build_context) */
    bool received;       /* 한 번이라도 들어왔는가 */
    vbc_ms_t last_rx_ms; /* 마지막 수신 시각 */
} Signal;

/* 수신한 입력 원본 */
typedef struct {
    Signal speed;    /* km/h */
    Signal gear;     /* Gear */
    Signal ignition; /* 0 = 꺼짐, 1 = 켜짐 */
    Signal lux;      /* lux */
    Signal rain;     /* % */
    bool door_open[DOOR_COUNT];
    LightSwitch light_sw;
    WiperSwitch wiper_sw;
} VbcInputs;

/*
 * 이번 주기의 판정 결과. 모듈은 이것만 본다.
 * 필드별로 어떤 요구사항을 반영했는지 적어 둔다.
 */
typedef struct {
    vbc_ms_t now;

    /* C4: 속도·기어·시동 중 하나라도 0.5초 동안 안 들어옴 (첫 수신 전에는 시작부터 셈) */
    bool rx_lost;
    /* 시동. C4 수신 중단이면 false (조명·와이퍼는 꺼진 것으로 봄). 첫 수신 전에도 false */
    bool ignition_on;
    /* 기어. 첫 수신 전에는 GEAR_P로 둔다 (가정, docs/c-structure.md 참고) */
    Gear gear;

    /* C3: 속도·기어·시동 모두 0.5초 안에 들어왔고 속도가 0~250 */
    bool speed_valid;
    int32_t speed_kmh; /* speed_valid일 때만 의미 있음 */

    /* C2: 0.5초 동안 안 들어옴(첫 수신 전에는 시작부터) 또는 범위 밖 */
    bool lux_fault;
    bool lux_valid; /* 쓸 수 있는 조도 값이 있음 = 수신됨 && 고장 아님 */
    int32_t lux;    /* lux_valid일 때만 의미 있음 */

    bool rain_fault; /* C2 */
    bool rain_valid;
    int32_t rain;

    bool door_open[DOOR_COUNT];
    bool any_door_open;
    LightSwitch light_sw;
    WiperSwitch wiper_sw;
} VbcContext;

/* 출력. C5 초기값은 vbc_outputs_init() */
typedef struct {
    bool headlight_on;    /* 조명 (B) */
    bool lux_fault_lamp;  /* 조명 (B), L5 */
    WiperLevel wiper;     /* 와이퍼 (C) */
    bool rain_fault_lamp; /* 와이퍼 (C), W5 */
    bool door_locked;     /* 도어·잠금 (D) */
    Warning warning;      /* 계기판 (E) */
} VbcOutputs;

/* C1 "N초 연속" 타이머 */
typedef struct {
    bool running;      /* 조건이 성립 중인가 */
    vbc_ms_t since_ms; /* 조건이 성립하기 시작한 시각 */
} HoldTimer;

/* ---- 초기화 (C5) ---- */
void vbc_inputs_init(VbcInputs *in);
void vbc_outputs_init(VbcOutputs *out);

/* ---- 입력 수신: now는 수신 시각 ---- */
void vbc_rx_speed(VbcInputs *in, vbc_ms_t now, int32_t kmh);
void vbc_rx_gear(VbcInputs *in, vbc_ms_t now, Gear gear);
void vbc_rx_ignition(VbcInputs *in, vbc_ms_t now, bool on);
void vbc_rx_lux(VbcInputs *in, vbc_ms_t now, int32_t lux);
void vbc_rx_rain(VbcInputs *in, vbc_ms_t now, int32_t percent);
/* 도어와 스위치는 바뀔 때만 들어와도 된다 (주기 수신 대상 아님) */
void vbc_set_door(VbcInputs *in, DoorId door, bool open);
void vbc_set_light_switch(VbcInputs *in, LightSwitch sw);
void vbc_set_wiper_switch(VbcInputs *in, WiperSwitch sw);

/* ---- C2~C4 판정 ---- */
/* 마지막 수신 후 0.5초가 지나지 않음 (C3) */
bool vbc_signal_fresh(const Signal *s, vbc_ms_t now);
/* 0.5초 동안 안 들어옴. 첫 수신 전에는 시작 시점부터 센다 (C2, C4) */
bool vbc_signal_timed_out(const Signal *s, vbc_ms_t now);
/* 입력 원본으로 이번 주기의 판정 결과를 만든다 */
void vbc_build_context(const VbcInputs *in, vbc_ms_t now, VbcContext *ctx);

/* ---- C1: N초 연속 ----
 * 매 주기 호출한다. condition이 끊김 없이 duration_ms 이상 성립했으면 true.
 * condition이 false가 되면 처음부터 다시 센다.
 * "언제 세는가"(예: L2는 시동 ON + AUTO + 조도 정상일 때만)는 condition에 포함해서 넘긴다.
 *   예) bool dark = ctx->ignition_on && ctx->light_sw == LIGHT_SW_AUTO
 *                   && ctx->lux_valid && ctx->lux < 1000;
 *       if (hold_timer_update(&st->dark, ctx->now, dark, 1000)) { 점등 }
 */
void hold_timer_reset(HoldTimer *t);
bool hold_timer_update(HoldTimer *t, vbc_ms_t now, bool condition, vbc_ms_t duration_ms);

/* ---- 로그·화면 표시용 이름 ---- */
const char *vbc_gear_name(Gear g);
const char *vbc_wiper_name(WiperLevel w);
const char *vbc_warning_name(Warning w);

#endif /* COMMON_H */
