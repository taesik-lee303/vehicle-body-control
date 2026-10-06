/*
 * dashboard.c - 계기판 경고 (D4~D5)
 * 담당: E (김주형)
 */

#include "dashboard.h"

/* TODO(E 김주형): D4~D5 구현. 지금은 통합 빌드용 임시 구현으로 C5 초기 상태(경고 없음)만 유지한다 */

void dashboard_init(DashboardState *st)
{
    st->warning = WARN_NONE;
}

void dashboard_step(DashboardState *st, const VbcContext *ctx, VbcOutputs *out)
{
    (void)ctx;
    out->warning = st->warning;
}
