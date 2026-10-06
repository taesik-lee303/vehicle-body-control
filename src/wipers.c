/*
 * wipers.c - 와이퍼 (W1~W5)
 * 담당: C (이지윤)
 */

#include "wipers.h"

/* TODO(C 이지윤): W1~W5 구현. 지금은 통합 빌드용 임시 구현으로 C5 초기 상태(정지)만 유지한다 */

void wipers_init(WiperState *st)
{
    st->level = WIPER_STOP;
}

void wipers_step(WiperState *st, const VbcContext *ctx, VbcOutputs *out)
{
    (void)ctx;
    out->wiper = st->level;
    out->rain_fault_lamp = false;
}
