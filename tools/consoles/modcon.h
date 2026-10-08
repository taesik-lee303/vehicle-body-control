/*
 * modcon.h - 모듈별 테스트 콘솔 (신호 주입기)
 *
 * 한 모듈의 step 함수만 호출해서 그 모듈 코드를 직접 확인한다 (다른 모듈은 실행하지 않는다).
 *
 * 원칙: 테스트 도구는 입력을 거르지 않는다.
 *   - 모든 신호를 정수 코드 그대로 넣는다 (기어 7, 시동 2, 도어 5, 스위치 -1 같은 정의 안 된 코드도).
 *     정의 안 된 값을 어떻게 다룰지는 제품 코드(common.c, 각 모듈)가 정할 일이다.
 *   - 신호마다 따로 끊고(cut) 잇고(resume), 한 번만 보낼(send) 수 있다. 자동 재송신도 끌 수 있다.
 *   - 시간은 1ms 단위로 진행한다. 판단(step) 주기와 송신 주기를 바꿀 수 있다.
 * 판정(C2~C4, 코드 유효성)은 실제와 같은 common.c의 vbc_build_context()가 한다.
 *
 * 입력 방법: 메뉴 번호, 또는 명령 한 줄 (docs/test/README.md, help 명령).
 * 파일로 넣으면 '#' 줄은 설명, tc/e/obs 명령으로 테스트 케이스 결과를 자동 판정·기록한다.
 */

#ifndef MODCON_H
#define MODCON_H

#include "common.h"

/* 메뉴에 보여 줄 신호 (모듈이 쓰는 입력). 명령으로는 모든 신호를 넣을 수 있다 */
enum {
    SIG_IGN = 1 << 0,
    SIG_GEAR = 1 << 1,
    SIG_SPEED = 1 << 2,
    SIG_LUX = 1 << 3,
    SIG_RAIN = 1 << 4,
    SIG_DOORS = 1 << 5,
    SIG_LIGHT_SW = 1 << 6,
    SIG_WIPER_SW = 1 << 7,
    SIG_WIPER_LEVEL = 1 << 8 /* 조명 콘솔 전용: L6에 넘길 와이퍼 단계 */
};

/* 바뀌면 알려 줄 출력 */
enum {
    OUT_HEADLIGHT = 1 << 0,
    OUT_LUX_LAMP = 1 << 1,
    OUT_WIPER = 1 << 2,
    OUT_RAIN_LAMP = 1 << 3,
    OUT_LOCK = 1 << 4,
    OUT_WARNING = 1 << 5
};

typedef struct ModCon ModCon;

struct ModCon {
    /* 콘솔이 정하는 것 */
    const char *title;
    const char *about;
    unsigned show;     /* SIG_* */
    unsigned outputs;  /* OUT_* */
    void (*mod_init)(ModCon *m);
    void (*mod_step)(ModCon *m);
    int32_t start_ign;  /* 시작값 (계기판·와이퍼는 유효한 속도 판정을 위해 시동·기어를 정해 둔다) */
    int32_t start_gear;

    /* 지금 보내고 있는 원래 값 (코드 그대로) */
    int32_t ign, gear, speed, lux, rain;
    int32_t wiper_level;
    unsigned cut;            /* 송신을 끊은 주기 신호 */
    bool auto_send;          /* 주기 신호 자동 재송신 */
    vbc_ms_t send_period_ms; /* 기본 50 (사양서 3.1: 0.5초보다 짧게) */
    vbc_ms_t step_period_ms; /* 기본 1 (1ms마다 판단) */

    VbcInputs in;
    VbcContext ctx;
    VbcOutputs out;
    vbc_ms_t now;

    /* 테스트 케이스 기록 */
    char tc[48];
    int tc_count, tc_failed, checks, failures, observations;
    bool tc_has_fail;
};

/* 시작 상태로 두고 0초에 첫 입력을 보낸 뒤 대화형으로 돈다. expect가 하나라도 틀리면 1을 돌려준다 */
int modcon_run(ModCon *m);

#endif /* MODCON_H */
