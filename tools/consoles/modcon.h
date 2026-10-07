/*
 * modcon.h - 모듈별 대화형 테스트 콘솔의 공통 틀
 *
 * 한 모듈의 step 함수만 호출해서 그 모듈 코드를 직접 확인한다 (다른 모듈은 끼어들지 않는다).
 * 사용자가 메뉴에서 입력값을 바꾸면 그 시각에 바로 반영하고, 바뀐 출력을 보여 준다.
 * 판정(C2 센서 고장, C3 유효한 속도, C4 수신 중단)은 실제와 같게 common.c의 vbc_build_context()가 한다.
 * 속도·기어·시동·조도·강우량은 사양서 3.1대로 시간이 흐르는 동안 0.05초마다 자동으로 다시 보낸다.
 */

#ifndef MODCON_H
#define MODCON_H

#include "common.h"

#define MODCON_TICK_MS 50u

/* 이 콘솔에서 바꿀 수 있는 입력 (메뉴에 나오는 항목) */
enum {
    IN_IGN = 1 << 0,         /* 시동 (켜기/끄기 전환) */
    IN_GEAR = 1 << 1,        /* 기어 */
    IN_SPEED = 1 << 2,       /* 속도 */
    IN_LUX = 1 << 3,         /* 조도 */
    IN_RAIN = 1 << 4,        /* 강우량 */
    IN_DOORS = 1 << 5,       /* 도어 4개 */
    IN_LIGHT_SW = 1 << 6,    /* 조명 스위치 */
    IN_WIPER_SW = 1 << 7,    /* 와이퍼 스위치 */
    IN_WIPER_LEVEL = 1 << 8, /* 와이퍼 단계 (조명 L6 확인용으로 직접 정함) */
    IN_CUT_VEHICLE = 1 << 9, /* 속도·기어·시동 송신 끊기/잇기 (C4) */
    IN_CUT_LUX = 1 << 10,    /* 조도 송신 끊기/잇기 (C2) */
    IN_CUT_RAIN = 1 << 11    /* 강우량 송신 끊기/잇기 (C2) */
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
    const char *title;    /* 예: "도어·잠금 (D1~D3)" */
    const char *about;    /* 시작할 때 보여 줄 한두 줄 설명 */
    unsigned inputs;      /* IN_* 조합 */
    unsigned outputs;     /* OUT_* 조합 */
    void *mod;            /* 모듈 상태 (DoorState 등) */
    void (*mod_init)(ModCon *m);   /* 모듈 상태 초기화 */
    void (*mod_step)(ModCon *m);   /* m->ctx로 모듈 step을 불러 m->out을 채운다 */

    /* 지금 보내고 있는 값 (사용자가 바꾼다) */
    bool ign;
    Gear gear;
    int32_t speed;
    int32_t lux;
    int32_t rain;
    unsigned cut;            /* 송신 중단 중인 신호 (CUT_*) */
    WiperLevel wiper_level;  /* IN_WIPER_LEVEL: 조명에 넘길 와이퍼 단계 */

    VbcInputs in;
    VbcContext ctx;
    VbcOutputs out;
    vbc_ms_t now;
};

/* 모든 값을 시작 상태로 두고 0초에 첫 입력을 보낸 뒤 대화형 메뉴를 돈다. 종료 코드를 돌려준다 */
int modcon_run(ModCon *m);

#endif /* MODCON_H */
