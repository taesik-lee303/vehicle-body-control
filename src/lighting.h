/*
 * lighting.h - 조명 (L1~L6)
 * 담당: B (김성진)    교차 검증: C (이지윤)
 *
 * 우선순위: L1 > L5 > L4 > L6 > L2·L3
 *   L1 시동 꺼짐(ctx->ignition_on == false, C4 수신 중단 포함) → 소등
 *   L5 조도 센서 고장 → 점등 + 고장 표시 (표시는 시동과 관계없이)
 *   L4 스위치 ON 점등 / OFF 소등
 *   L6 AUTO: 와이퍼 10초 연속 작동(정지 아님) → 점등, 작동 중 유지
 *   L2 AUTO: 조도 < 1000 lux 1초 연속 → 점등
 *   L3 AUTO: 조도 > 1500 lux 3초 연속 → 소등. 아무 규칙도 안 정하면 직전 유지
 * C1: L2·L3 시간은 시동 ON + AUTO + 조도 정상일 때만, L6 시간은 시동 ON + 와이퍼 작동 중에만 센다.
 *     상위 규칙이 전조등을 정하는 동안에도 시간은 계속 센다 → 타이머는 우선순위 판단 전에 매 주기 갱신.
 *     HoldTimer(common.h) 사용.
 */

#ifndef LIGHTING_H
#define LIGHTING_H

#include "common.h"

/* 모듈이 주기 사이에 기억할 값. 필요한 필드(HoldTimer 등)는 담당자가 추가한다 */
typedef struct {
    bool headlight_on; /* 직전 상태 (L3 "직전 상태 유지") */
} LightingState;

void lighting_init(LightingState *st);
/* wiper: 이번 주기의 최종 와이퍼 단계 (L6). out->headlight_on, out->lux_fault_lamp 를 정한다 */
void lighting_step(LightingState *st, const VbcContext *ctx, WiperLevel wiper, VbcOutputs *out);

#endif /* LIGHTING_H */
