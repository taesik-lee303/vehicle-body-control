/*
 * dashboard.h - 계기판 경고 (D4~D5)
 * 담당: E (김주형)    교차 검증: B (김성진)
 *
 *   D4 유효한 속도(ctx->speed_valid) > 0 이고 문이 하나라도 열림 → "주행 중 도어 열림"
 *   D5 경고는 하나만: 주행 중 도어 열림 > 조도 센서 고장 > 강우 센서 고장. 없으면 표시 안 함
 *      센서 고장은 ctx->lux_fault, ctx->rain_fault 를 쓴다
 */

#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "common.h"

typedef struct {
    Warning warning; /* 직전 표시 (필요 없으면 그대로 둬도 됨) */
} DashboardState;

void dashboard_init(DashboardState *st);
/* out->warning 을 정한다 */
void dashboard_step(DashboardState *st, const VbcContext *ctx, VbcOutputs *out);

#endif /* DASHBOARD_H */
