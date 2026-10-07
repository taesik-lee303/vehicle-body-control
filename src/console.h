/*
 * console.h - 터미널 명령 입력 (입력/시연)
 * 담당: F (이태식)    교차 검증: C (이지윤)
 *
 * 한 줄 명령을 받아 차량 입력을 바꾸고 시간을 진행한다. 명령 목록은 docs/console.md.
 *
 * 패널(Panel)은 "지금 보내고 있는 값"이다. 사양서 3.1에 따라 속도·기어·시동·조도·강우량은
 * 값이 그대로여도 계속 보내야 하므로, 시간이 흐르는 동안 CONSOLE_TICK_MS마다 패널 값을 다시 보낸다.
 * 시간은 wait 명령(또는 console_advance)으로만 흐른다 → 결과가 매번 같아 경계 시각을 증명할 수 있다.
 *
 * 주행 모델(drive.h)을 켜면(drive on) 속도·기어·시동은 운전 조작(pedal, gear, ign)으로 만들어진다.
 *
 * 터미널 대신 다른 화면(그래픽 시뮬레이션 등)을 붙일 때도 이 모듈을 그대로 쓰고,
 * 화면 쪽에서 console_exec()에 명령을 넘기거나 console_advance()로 시간을 진행하면 된다.
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdio.h>

#include "drive.h"
#include "vbc.h"

#define CONSOLE_TICK_MS 50u /* 주기 입력 송신 간격. 0.5초보다 짧아야 한다 */

/* 송신을 끊거나 다시 이을 입력 (cut / resume) */
enum {
    CON_SPEED = 1 << 0,
    CON_GEAR = 1 << 1,
    CON_IGNITION = 1 << 2,
    CON_LUX = 1 << 3,
    CON_RAIN = 1 << 4,
    CON_VEHICLE = CON_SPEED | CON_GEAR | CON_IGNITION,
    CON_ALL = CON_VEHICLE | CON_LUX | CON_RAIN
};

/* 지금 보내고 있는 주기 입력 값. 범위 밖 값도 그대로 보낸다 (C2, C3 시험) */
typedef struct {
    bool ignition;
    Gear gear;
    int32_t speed;
    int32_t lux;
    int32_t rain;
    unsigned cut; /* 송신 중단 중인 입력 (CON_* 조합) */
} Panel;

typedef struct {
    VbcSystem sys;
    Panel panel;
    Drive drive;         /* 주행 모델 (R1~R5) */
    bool drive_on;       /* true면 속도·기어·시동을 drive가 만든다 */
    vbc_ms_t now;
    FILE *out;           /* 결과를 쓸 곳. NULL이면 아무것도 쓰지 않는다 (테스트용) */
    int expect_failures; /* expect 불일치 횟수 */
} Console;

typedef enum { CONSOLE_OK, CONSOLE_ERROR, CONSOLE_QUIT } ConsoleResult;

/* C5 초기 상태로 만들고 0ms에 첫 주기 입력을 보낸다 */
void console_init(Console *c, FILE *out);

/* 명령 한 줄 실행. '#' 뒤는 주석, 빈 줄은 무시 */
ConsoleResult console_exec(Console *c, const char *line);

/* 시간을 ms만큼 진행 (CONSOLE_TICK_MS 단위). 매 tick 패널 값을 보내고 vbc_step, 바뀐 출력을 표시 */
void console_advance(Console *c, vbc_ms_t ms);

void console_print_status(const Console *c);
void console_print_help(const Console *c);

#endif /* CONSOLE_H */
