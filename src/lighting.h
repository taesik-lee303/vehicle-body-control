/*
 * lighting.h - 조명 모듈 (L1~L6)
 * 담당: B (김성진)
 * 요구사항: L1~L6
 * 교차 검증: C (이지윤)
 */

#ifndef LIGHTING_H
#define LIGHTING_H

#include "common.h"

/*
 * lighting_update - 조명 상태 업데이트
 * 
 * 우선순위: L1 > L5 > L4 > L6 > L2·L3
 * L1: 시동 OFF → 소등
 * L5: 조도 센서 고장 → 점등 + 고장 표시
 * L4: 스위치 ON → 점등, OFF → 소등
 * L6: AUTO + 와이퍼 10초 연속 작동 → 점등
 * L2: AUTO + 조도 < 1000 lux 1초 연속 → 점등
 * L3: AUTO + 조도 > 1500 lux 3초 연속 → 소등
 */
void lighting_update(VehicleInput *input, VehicleOutput *output, TimerState *timer);

#endif // LIGHTING_H
