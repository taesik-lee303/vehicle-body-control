/*
 * wipers.h - 와이퍼 모듈 (W1~W5)
 * 담당: C (이지윤)
 * 요구사항: W1~W5
 * 교차 검증: D (김의현)
 */

#ifndef WIPERS_H
#define WIPERS_H

#include "common.h"

/*
 * wipers_update - 와이퍼 상태 업데이트
 *
 * 우선순위: W1 > W4 > W5 > W2·W3
 * W1: 시동 OFF → 정지
 * W4: OFF 정지 / LOW 저속 / HIGH 고속
 * W5: AUTO + 강우 센서 고장 → 간헐
 * W2: AUTO + 강우량: <10% 정지, 10~40% 간헐, 40~70% 저속, >=70% 고속
 * W3: AUTO + 유효한 속도 0 → W2 결과를 한 단계 낮춤
 */
void wipers_update(VehicleInput *input, VehicleOutput *output, TimerState *timer);

#endif // WIPERS_H
