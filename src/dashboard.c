/*
 * dashboard.c - 계기판 경고 (D4~D5)
 * 담당: E (김주형)
 */

#include "dashboard.h"

/* D4~D5: 이번 주기에 표시할 경고 하나를 정한다 (우선순위 순서대로 검사) */
static Warning dashboard_decide(const VbcContext *ctx)
{
    /* D4: 유효한 속도 > 0 이고 문이 하나라도 열림 */
    if (ctx->speed_valid && ctx->speed_kmh > 0 && ctx->any_door_open) {
        return WARN_DOOR_OPEN_WHILE_DRIVING;
    }
    /* D5: 주행 중 도어 열림 > 조도 센서 고장 > 강우 센서 고장 */
    if (ctx->lux_fault) {
        return WARN_LUX_SENSOR_FAULT;
    }
    if (ctx->rain_fault) {
        return WARN_RAIN_SENSOR_FAULT;
    }
    return WARN_NONE;
}

void dashboard_init(DashboardState *st)
{
    st->warning = WARN_NONE;
}

void dashboard_step(DashboardState *st, const VbcContext *ctx, VbcOutputs *out)
{
    Warning w = dashboard_decide(ctx);

    st->warning = w;
    out->warning = w;
}
