/*
 * dashboard.h - 계기판 경고 모듈 (D4~D5)
 * 담당: E (김주형)
 * 요구사항: D4~D5
 * 교차 검증: B (김성진)
 */

#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "common.h"

/*
 * dashboard_update - 계기판 경고 상태 업데이트
 *
 * D4: 유효한 속도 > 0 + 문 열림 → "주행 중 도어 열림"
 * D5: 경고는 하나만 표시: 주행 중 도어 열림 > 조도 센서 고장 > 강우 센서 고장
 */
void dashboard_update(VehicleInput *input, VehicleOutput *output, TimerState *timer);

#endif // DASHBOARD_H
