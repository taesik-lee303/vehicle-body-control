/*
 * main.c - 메인 루프
 * 담당: A (이태식)
 */

#include <stdio.h>
#include <time.h>
#include "common.h"
#include "lighting.h"
#include "wipers.h"
#include "doors.h"
#include "dashboard.h"

int main(void) {
    VehicleInput input;
    VehicleOutput output;
    TimerState timer;
    
    /* 초기화 */
    state_init(&input, &output, &timer);
    printf("Vehicle Body Control System Started\n");
    
    /* 메인 루프 (스켈레톤) */
    while (1) {
        /* 1. 입력 수신 (E 담당) */
        /* input_read(&input, &timer.current_time); */
        
        /* 2. 각 모듈 업데이트 */
        lighting_update(&input, &output, &timer);
        wipers_update(&input, &output, &timer);
        doors_update(&input, &output, &timer);
        dashboard_update(&input, &output, &timer);
        
        /* 3. 출력 (E 담당) */
        /* output_display(&output); */
        
        /* 시간 진행 (시뮬레이션용) */
        // timer.current_time += 0.05; // 50ms 간격
        
        // usleep(50000); // 50ms 슬립
    }
    
    return 0;
}
