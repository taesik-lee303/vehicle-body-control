/*
 * lighting.c - 조명 (L1~L6)
 * 담당: B (김성진)
 */

#include "lighting.h"

#define L2_DARK_LUX 1000      /* 미만이면 어두움 */
#define L2_DARK_HOLD_MS 1000  /* 1초 연속 */
#define L3_BRIGHT_LUX 1500    /* 초과이면 밝음 */
#define L3_BRIGHT_HOLD_MS 3000 /* 3초 연속 */
#define L6_WIPER_HOLD_MS 10000 /* 10초 연속 */

void lighting_init(LightingState *st)
{
    st->headlight_on = false;
    hold_timer_reset(&st->dark);
    hold_timer_reset(&st->bright);
    hold_timer_reset(&st->wiper_run);
}

void lighting_step(LightingState *st, const VbcContext *ctx, WiperLevel wiper, VbcOutputs *out)
{
    /* C1: 시간은 우선순위 판단 전에 매 주기 갱신한다 (상위 규칙이 정하는 동안에도 계속 셈) */
    bool auto_counting = ctx->ignition_on && ctx->light_sw == LIGHT_SW_AUTO && ctx->lux_valid;
    bool l2_dark = hold_timer_update(&st->dark, ctx->now,
                                     auto_counting && ctx->lux < L2_DARK_LUX, L2_DARK_HOLD_MS);
    bool l3_bright = hold_timer_update(&st->bright, ctx->now,
                                       auto_counting && ctx->lux > L3_BRIGHT_LUX, L3_BRIGHT_HOLD_MS);
    bool l6_wiper = hold_timer_update(&st->wiper_run, ctx->now,
                                      ctx->ignition_on && wiper != WIPER_STOP, L6_WIPER_HOLD_MS);

    bool on;

    if (!ctx->ignition_on)
        on = false; /* L1 (C4 수신 중단 포함) */
    else if (ctx->lux_fault)
        on = true; /* L5 */
    else if (ctx->light_sw == LIGHT_SW_ON)
        on = true; /* L4 */
    else if (ctx->light_sw == LIGHT_SW_OFF)
        on = false; /* L4 */
    else if (l6_wiper)
        on = true; /* L6 */
    else if (l2_dark)
        on = true; /* L2 */
    else if (l3_bright)
        on = false; /* L3 */
    else
        on = st->headlight_on; /* L3: 어느 규칙도 정하지 않으면 직전 상태 유지 */

    st->headlight_on = on;
    out->headlight_on = on;
    out->lux_fault_lamp = ctx->lux_fault; /* L5: 표시는 시동과 관계없이 */
}
