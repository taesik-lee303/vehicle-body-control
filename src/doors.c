/*
 * doors.c - 도어·잠금 (D1~D3)
 * 담당: D (김의현)
 */

#include "doors.h"

#define LOCK_SPEED_KMH 15 /* D3 */

void doors_init(DoorState *st)
{
    st->locked = false; /* C5 초기 상태 */
}

void doors_step(DoorState *st, const VbcContext *ctx, VbcOutputs *out)
{
    if (ctx->rx_lost) {
        /* D1: 수신 중단 → 잠금 상태 그대로 (C4로 시동이 꺼진 것으로 보이므로 D2보다 먼저) */
    } else if (!ctx->ignition_on || ctx->gear == GEAR_P) {
        st->locked = false; /* D2 */
    } else if (ctx->speed_valid && ctx->speed_kmh >= LOCK_SPEED_KMH) {
        st->locked = true; /* D3 */
    }
    /* 해당 없으면 직전 유지 */
    out->door_locked = st->locked;
}
