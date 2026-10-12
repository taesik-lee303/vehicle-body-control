/*
 * doors.c - 도어·잠금 (D1~D3), AUTOSAR 스타일 SWC "DoorLock"
 * 담당: D (김의현)
 *
 * SWC 본체는 Rte_DoorLock.h의 포트 함수로만 읽고 쓴다.
 * doors_init/doors_step은 기존 인터페이스(vbc.c)를 위한 얇은 연결부다.
 */

#include <stddef.h>
#include "Rte_DoorLock.h"

#define LOCK_SPEED_KMH 15 /* D3 */

void DoorLock_Init(Rte_Instance self)
{
    *Rte_Pim_LockState(self) = FALSE; /* C5 초기 상태 */
}

void DoorLock_MainFunction(Rte_Instance self)
{
    boolean rx_lost;
    boolean ign_on;
    Gear gear;
    sint32 speed;
    boolean *locked = Rte_Pim_LockState(self);

    (void)Rte_Read_RxStatus_RxLost(self, &rx_lost);
    (void)Rte_Read_Ignition_On(self, &ign_on);
    (void)Rte_Read_Gear_Position(self, &gear);

    if (rx_lost) {
        /* D1: 수신 중단 → 잠금 상태 그대로 (C4로 시동이 꺼진 것으로 보이므로 D2보다 먼저) */
    } else if (!ign_on || gear == GEAR_P) {
        *locked = FALSE; /* D2 */
    } else if (Rte_Read_VehicleSpeed_Kmh(self, &speed) == E_OK && speed >= LOCK_SPEED_KMH) {
        *locked = TRUE; /* D3: 유효한 속도(C3)만 */
    }
    /* 해당 없으면 직전 유지 */
    (void)Rte_Write_DoorLock_Locked(self, *locked);
}

void doors_init(DoorState *st)
{
    const Rte_CDS_DoorLock inst = { NULL, NULL, st };

    DoorLock_Init(&inst);
}

void doors_step(DoorState *st, const VbcContext *ctx, VbcOutputs *out)
{
    const Rte_CDS_DoorLock inst = { ctx, out, st };

    DoorLock_MainFunction(&inst);
}
