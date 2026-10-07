/*
 * doors.h - 도어·잠금 (D1~D3)
 * 담당: D (김의현)    교차 검증: E (김주형)
 *
 * 위에서부터 차례로 판단:
 *   D1 수신 중단(ctx->rx_lost) → 잠금 상태 그대로
 *   D2 그 외 시동 꺼짐 또는 기어 P → 해제
 *   D3 그 외 유효한 속도(ctx->speed_valid) >= 15 km/h → 잠금. 해당 없으면 직전 유지
 */

#ifndef DOORS_H
#define DOORS_H

#include "common.h"

/* 잠금 상태가 바뀐 주기에만 NONE이 아니다 (도어 잠금 구동부로 보낼 명령) */
typedef enum { DOOR_CMD_NONE, DOOR_CMD_LOCK, DOOR_CMD_UNLOCK } DoorCmd;

typedef struct {
    bool locked; /* 직전 잠금 상태 */
    DoorCmd cmd; /* 이번 주기에 낸 명령 */
} DoorState;

void doors_init(DoorState *st);
/* out->door_locked 를 정하고, 바뀌었으면 st->cmd 에 명령을 둔다 */
void doors_step(DoorState *st, const VbcContext *ctx, VbcOutputs *out);

#endif /* DOORS_H */
