/*
 * lighting.c - 조명 (L1~L6)
 * 담당: B (김성진)
 */

#include "lighting.h"

/* TODO(B 김성진): L1~L6 구현. 지금은 통합 빌드용 임시 구현으로 C5 초기 상태(소등)만 유지한다 */

void lighting_init(LightingState *st)
{
    st->headlight_on = false;
}

void lighting_step(LightingState *st, const VbcContext *ctx, WiperLevel wiper, VbcOutputs *out)
{
    (void)ctx;
    (void)wiper;
    out->headlight_on = st->headlight_on;
    out->lux_fault_lamp = false;
}
