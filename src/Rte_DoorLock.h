/*
 * Rte_DoorLock.h - DoorLock SWC의 RTE 계약 (D1~D3)
 * 담당: D (김의현)
 *
 * 실제 AUTOSAR에서는 RTE 생성기가 만드는 파일이다. 여기서는 손으로 써서 VbcContext/VbcOutputs에 연결한다.
 * SWC 본체(doors.c)는 이 파일의 Rte_* 함수로만 입력을 읽고 출력을 쓴다.
 *
 *   R-Port  RxStatus.RxLost      ← ctx->rx_lost      (C4)
 *           Ignition.On          ← ctx->ignition_on
 *           Gear.Position        ← ctx->gear
 *           VehicleSpeed.Kmh     ← ctx->speed_kmh    (C3: speed_valid가 아니면 RTE_E_INVALID)
 *   P-Port  DoorLock.Locked      → out->door_locked
 *   PIM     LockState            = DoorState.locked  (직전 잠금 상태)
 *
 * 테스트가 Sim을 여러 개 만들므로 전역 상태를 쓰지 않고, 다중 인스턴스 방식(Rte_Instance)으로 상태를 넘긴다.
 */

#ifndef RTE_DOORLOCK_H
#define RTE_DOORLOCK_H

#include "Std_Types.h"
#include "doors.h"

typedef struct {
    const VbcContext *ctx; /* R-Port 데이터 출처 */
    VbcOutputs *out;       /* P-Port 쓰기 대상 */
    DoorState *pim;        /* Per-Instance Memory */
} Rte_CDS_DoorLock;

typedef const Rte_CDS_DoorLock *Rte_Instance;

static inline Std_ReturnType Rte_Read_RxStatus_RxLost(Rte_Instance self, boolean *v)
{
    *v = self->ctx->rx_lost;
    return E_OK;
}

static inline Std_ReturnType Rte_Read_Ignition_On(Rte_Instance self, boolean *v)
{
    *v = self->ctx->ignition_on;
    return E_OK;
}

static inline Std_ReturnType Rte_Read_Gear_Position(Rte_Instance self, Gear *v)
{
    *v = self->ctx->gear;
    return E_OK;
}

/* C3 유효한 속도가 아니면 값을 주지 않고 RTE_E_INVALID */
static inline Std_ReturnType Rte_Read_VehicleSpeed_Kmh(Rte_Instance self, sint32 *v)
{
    if (!self->ctx->speed_valid)
        return RTE_E_INVALID;
    *v = self->ctx->speed_kmh;
    return E_OK;
}

static inline Std_ReturnType Rte_Write_DoorLock_Locked(Rte_Instance self, boolean v)
{
    self->out->door_locked = v;
    return E_OK;
}

#define Rte_Pim_LockState(self) (&(self)->pim->locked)

/* Runnable */
void DoorLock_Init(Rte_Instance self);          /* InitEvent */
void DoorLock_MainFunction(Rte_Instance self);  /* TimingEvent 50ms */

#endif /* RTE_DOORLOCK_H */
