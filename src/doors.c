/*
 * doors.c - 도어·잠금 (D1~D3)
 * 담당: D (김의현)
 */

#include "doors.h"

/* TODO(D 김의현): D1~D3 구현. 지금은 통합 빌드용 임시 구현으로 C5 초기 상태(잠금 해제)만 유지한다 */

void doors_init(DoorState *st)
{
    st->locked = false;
}

void doors_step(DoorState *st, const VbcContext *ctx, VbcOutputs *out)
{
    (void)ctx;
    out->door_locked = st->locked;
}
