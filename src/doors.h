/*
 * doors.h - 도어 잠금 (DOORS-1 ~ DOORS-4)
 * 담당: D (김의현)    교차 검증: E (김주형)
 *
 * DOORS-1   속도·기어·시동 중 하나라도 0.5 s 이상 안 들어옴(ctx->rx_lost) → 명령 없음
 * DOORS-2.1 그 외 시동 OFF 또는 기어 P 가 1 s 연속 → 잠금 해제 명령 1회
 * DOORS-3   그 외 시동 ON, 기어 P 아님, 유효한 속도 >= 15 km/h 가 1 s 연속 → 잠금 명령 1회
 *           (2.1, 3 모두 조건이 깨지면 유지시간 0 s, 조건이 해제되기 전까지 재출력 없음)
 * DOORS-4   위 명령 외에는 명령 없음
 */

#ifndef DOORS_H
#define DOORS_H

#include "common.h"

/* 도어 클로저 액추에이터로 내는 명령. 명령이 있는 주기에만 NONE이 아니다 */
typedef enum { DOOR_CMD_NONE, DOOR_CMD_LOCK, DOOR_CMD_UNLOCK } DoorCmd;

typedef struct {
    HoldTimer unlock_hold; /* DOORS-2.1 조건 유지시간 */
    HoldTimer lock_hold;   /* DOORS-3 조건 유지시간 */
    bool unlock_sent;      /* 조건이 유지되는 동안 해제 명령을 이미 냄 */
    bool lock_sent;        /* 조건이 유지되는 동안 잠금 명령을 이미 냄 */
    DoorCmd cmd;           /* 이번 주기에 낸 명령 */
    bool locked;           /* 마지막 명령에 따른 잠금 상태 */
} DoorState;

void doors_init(DoorState *st);
/* st->cmd 에 이번 주기 명령을, out->door_locked 에 마지막 명령에 따른 잠금 상태를 둔다 */
void doors_step(DoorState *st, const VbcContext *ctx, VbcOutputs *out);

#endif /* DOORS_H */
