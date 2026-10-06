/*
 * common.h - 공통 구조체, 상태, C1~C5 타이머 로직
 * 담당: A (이태식)
 * 요구사항: C1~C5
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <time.h>

/* 입력 상태 */
typedef struct {
    uint16_t speed;               // 0~250 km/h (65535 = 범위 밖, 센서 고장)
    char gear;                    // 'P', 'R', 'N', 'D'
    uint8_t engine_on;            // 0 = OFF, 1 = ON
    uint16_t illuminance;         // 0~60000 lux (65535 = 범위 밖, 센서 고장)
    uint8_t rainfall;             // 0~100 % (255 = 범위 밖, 센서 고장)
    uint8_t door_fl, door_fr;     // 0 = 닫힘, 1 = 열림 (front left/right)
    uint8_t door_rl, door_rr;     // 0 = 닫힘, 1 = 열림 (rear left/right)
    char light_switch;            // 'A' = AUTO, 'O' = OFF, 'N' = ON
    char wiper_switch;            // 'O' = OFF, 'A' = AUTO, 'L' = LOW, 'H' = HIGH
    
    /* 마지막 수신 시각 (C2, C3, C4용) */
    time_t speed_time;
    time_t gear_time;
    time_t engine_time;
    time_t illuminance_time;
    time_t rainfall_time;
} VehicleInput;

/* 출력 상태 */
typedef struct {
    uint8_t headlight;            // 0 = 꺼짐, 1 = 켜짐
    uint8_t illuminance_fault;    // 0 = 정상, 1 = 고장
    uint8_t wiper_step;           // 0 = 정지, 1 = 간헐, 2 = 저속, 3 = 고속
    uint8_t rainfall_fault;       // 0 = 정상, 1 = 고장
    uint8_t door_lock;            // 0 = 풀림, 1 = 잠김
    uint8_t warning;              // 0 = 없음, 1 = 주행 중 도어 열림, 2 = 조도 고장, 3 = 강우 고장
} VehicleOutput;

/* 타이머/상태 */
typedef struct {
    time_t current_time;          // 현재 시각 (주입 가능)
    time_t last_valid_speed_time; // 마지막 유효한 속도 수신
    
    /* C1용: 조건이 시작된 시각 (조건이 깨지면 0으로 리셋) */
    time_t condition_start_time;
    
    /* 이전 상태 (C5 초기화 등에 사용) */
    uint8_t prev_door_lock;
} TimerState;

/* 함수: 초기화 */
void state_init(VehicleInput *input, VehicleOutput *output, TimerState *timer);

/* 함수: C2 센서 고장 판정 (특정 센서의 고장 여부) */
uint8_t is_sensor_fault_illuminance(const VehicleInput *input, const TimerState *timer);
uint8_t is_sensor_fault_rainfall(const VehicleInput *input, const TimerState *timer);

/* 함수: C3 유효한 속도 판정 */
uint8_t is_valid_speed(const VehicleInput *input, const TimerState *timer);

/* 함수: C4 수신 중단 판정 */
uint8_t is_receiving_stopped(const VehicleInput *input, const TimerState *timer);

/* 함수: C1 N초 연속 조건 판정 */
uint8_t check_condition_duration(const TimerState *timer, time_t duration_sec, uint8_t condition);

#endif // COMMON_H
