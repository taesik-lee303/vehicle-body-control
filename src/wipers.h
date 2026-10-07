/*
 * wipers.h - 와이퍼 (W1~W5)
 * 담당: C (이지윤)    교차 검증: D (김의현)
 *
 * 우선순위: W1 > W4 > W5 > W2·W3
 *   W1 시동 꺼짐(ctx->ignition_on == false, C4 수신 중단 포함) → 정지
 *   W4 OFF 정지 / LOW 저속 / HIGH 고속
 *   W5 AUTO + 강우 센서 고장 → 간헐. 고장 표시는 시동·스위치와 관계없이
 *   W2 AUTO: <10% 정지, 10~40% 간헐, 40~70% 저속, >=70% 고속
 *   W3 AUTO + 유효한 속도(ctx->speed_valid)가 0 → W2 결과를 한 단계 낮춤
 */

#ifndef WIPERS_H
#define WIPERS_H

#include "common.h"

/* 모듈이 주기 사이에 기억할 값. 필요한 필드는 담당자가 추가한다 */
typedef struct {
    WiperLevel level;
    WiperSwitch last_switch; /* 마지막으로 받은 정상 스위치 값 (SWR-W4-005) */
} WiperState;

void wipers_init(WiperState *st);
/* out->wiper, out->rain_fault_lamp 를 정한다. 조명(L6)보다 먼저 호출된다 */
void wipers_step(WiperState *st, const VbcContext *ctx, VbcOutputs *out);

#endif /* WIPERS_H */
