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

typedef struct {
    bool locked; /* 직전 잠금 상태 */
} DoorState;

void doors_init(DoorState *st);
/* out->door_locked 를 정한다 */
void doors_step(DoorState *st, const VbcContext *ctx, VbcOutputs *out);

#endif /* DOORS_H */
