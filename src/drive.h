/*
 * drive.h - 주행 모델 (R1~R5, 선택)
 * 담당: F (이태식)    교차 검증: C (이지윤)
 *
 * 운전 조작(페달, 기어, 시동)으로 속도를 만든다. 여기서 만든 속도·기어·시동이 5장 입력으로 쓰인다.
 * 시간은 호출자가 넘기는 dt_ms만 쓴다. 코어(common.h)와 독립이라 단독 테스트할 수 있다.
 *
 *   R1 시동이 꺼져 있으면 차는 움직이지 않는다. 시동은 정지 상태에서만 켜고 끌 수 있다.
 *   R2 기어 변경은 정지 상태(속도 0)에서만 된다.
 *   R3 D에서 가속하면 속도가 오르고, R에서 가속하면 후진한다. P·N에서는 움직이지 않는다.
 *   R4 브레이크를 밟으면 속도가 줄어 0에서 멈추고, 가속도 브레이크도 없으면 천천히 줄어든다.
 *      가속과 브레이크가 함께 입력되면 브레이크가 우선한다.
 *   R5 최고 속도는 전진 250 km/h, 후진 20 km/h. 가속 정도는 팀이 정해 발표한다 (아래 수치).
 *
 * 속도는 방향 없는 속력이다 (사양서 3.1). 후진은 기어 R로 구분한다.
 */

#ifndef DRIVE_H
#define DRIVE_H

#include "common.h"

/* R5: 가속 정도 (팀이 정한 값, 발표 때 밝힌다). 단위 km/h per second */
#define DRIVE_ACCEL_NORMAL 10 /* 보통 가속 */
#define DRIVE_ACCEL_FULL 25   /* 풀 스로틀 */
#define DRIVE_BRAKE 40        /* 브레이크 */
#define DRIVE_COAST 5         /* 가속도 브레이크도 없을 때 (R4) */
#define DRIVE_MAX_FORWARD 250 /* km/h */
#define DRIVE_MAX_REVERSE 20  /* km/h */

typedef enum { THROTTLE_NONE, THROTTLE_NORMAL, THROTTLE_FULL } Throttle;

typedef struct {
    int32_t speed_mkmh; /* 속도, 1/1000 km/h 단위 정수 (실수 오차 없이 경계를 정확히) */
    Gear gear;
    bool ignition;
    Throttle throttle;
    bool brake;
} Drive;

/* 정지, 기어 P, 시동 꺼짐 (C5와 같은 시작 상태) */
void drive_init(Drive *d);

/* 페달 상태. 누르고 있는 동안 유지된다 */
void drive_set_pedals(Drive *d, Throttle throttle, bool brake);

/* R1: 정지 상태에서만 바꾼다. 받아들이면 true. 이미 같은 값이면 true */
bool drive_request_ignition(Drive *d, bool on);

/* R2: 정지 상태에서만 바꾼다. 받아들이면 true. 이미 같은 기어면 true */
bool drive_request_gear(Drive *d, Gear gear);

/* dt_ms 동안 진행 (R1, R3, R4, R5) */
void drive_step(Drive *d, vbc_ms_t dt_ms);

/* 코어에 보낼 속도: 반올림한 km/h */
int32_t drive_speed_kmh(const Drive *d);

/* 속도를 직접 정한다 (drive on 시점에 패널 값을 이어받을 때). 0~최고 속도로 제한 */
void drive_set_speed_kmh(Drive *d, int32_t kmh);

#endif /* DRIVE_H */
