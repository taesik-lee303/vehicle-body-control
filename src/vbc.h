/*
 * vbc.h - 시스템 통합: 입력 → 판정(C2~C4) → 모듈 → 출력
 * 담당: A (이태식)
 */

#ifndef VBC_H
#define VBC_H

#include "common.h"
#include "dashboard.h"
#include "doors.h"
#include "lighting.h"
#include "wipers.h"

typedef struct {
    VbcInputs in;   /* 입력은 vbc_rx_*(&sys.in, ...) 으로 넣는다 */
    VbcContext ctx; /* 마지막 vbc_step()의 판정 결과 (표시·디버그용) */
    VbcOutputs out; /* 마지막 vbc_step()의 출력 */
    WiperState wipers;
    LightingState lighting;
    DoorState doors;
    DashboardState dashboard;
} VbcSystem;

/* C5 초기 상태로 만든다 */
void vbc_init(VbcSystem *sys);

/*
 * 한 주기 실행. now 시점의 입력으로 판정하고 모든 출력을 갱신한다.
 * C1 시간 판정이 정확하려면 입력이 바뀔 때마다, 또는 50ms 이하 간격으로 호출한다.
 * 호출 순서: 와이퍼 → 조명(L6이 이번 주기 와이퍼 단계를 씀) → 도어·잠금 → 계기판
 */
void vbc_step(VbcSystem *sys, vbc_ms_t now);

#endif /* VBC_H */
