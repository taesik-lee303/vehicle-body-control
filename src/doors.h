/*
 * doors.h - 도어·잠금 모듈 (D1~D3)
 * 담당: D (김의현)
 * 요구사항: D1~D3
 * 교차 검증: E (김주형)
 */

#ifndef DOORS_H
#define DOORS_H

#include "common.h"

/*
 * doors_update - 도어 잠금 상태 업데이트
 *
 * D1: 수신 중단 → 잠금 상태 유지
 * D2: 시동 OFF 또는 기어 P → 해제
 * D3: 유효한 속도 >= 15 km/h → 잠금
 */
void doors_update(VehicleInput *input, VehicleOutput *output, TimerState *timer);

#endif // DOORS_H
