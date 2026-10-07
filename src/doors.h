/*
 * doors.h - 도어 잠금 (DOORS-1 ~ DOORS-4, DOORS-NF-1/2, DOORS-FS-1/2)
 * 담당: D (김의현)    교차 검증: E (김주형)
 *
 * 제한 모드 (DOORS-1)
 *   1.3 초기화 시 진입
 *   1.2 속도·기어·시동 중 하나라도 마지막 정상 수신 후 5 s 이상 → 진입
 *   1.4 제한 모드 동안 명령 없음, 1.5 유지시간 0 s로 초기화·유지
 *   1.6 진입 후 세 신호를 다시 정상 수신하고 모두 1 s 미만 → 해제
 *   1.7 해제 시점부터 0 s부터 다시 셈
 * 잠금 해제 (DOORS-2): 시동 OFF 또는 기어 P 가 1 s 연속 → 해제 명령 1회
 * 잠금 (DOORS-3): 그 외 시동 ON, 기어 P 아님, 유효한 속도 >= 15 km/h 가 1 s 연속 → 잠금 명령 1회
 * 명령은 조건이 해제된 뒤 다시 1 s 연속 성립해야 재출력 (DOORS-2.3, 3.3). 그 외 명령 없음 (DOORS-4)
 *
 * 시동은 ctx->ignition_on (C4 적용)을 쓴다.
 * 신호별 수신 시각이 ctx에 없어 DOORS-1.1 수신 경과시간은 ctx->speed_valid로 근사한다 (doors.c 참고).
 */

#ifndef DOORS_H
#define DOORS_H

#include "common.h"

/* 도어 클로저 액추에이터로 내는 명령. 명령이 있는 주기에만 NONE이 아니다 */
typedef enum { DOOR_CMD_NONE, DOOR_CMD_LOCK, DOOR_CMD_UNLOCK } DoorCmd;

typedef struct {
    bool restricted;       /* 제한 모드 (DOORS-1.3) */
    bool rx_since_entry;   /* 제한 모드 진입 후 세 신호를 정상 수신했는가 (DOORS-1.6) */
    vbc_ms_t rx_ms;        /* 세 신호 중 가장 오래된 마지막 정상 수신 시각 (근사, DOORS-1.1) */
    HoldTimer unlock_hold; /* DOORS-2.1 조건 유지시간 */
    HoldTimer lock_hold;   /* DOORS-3.1 조건 유지시간 */
    bool unlock_sent;      /* 조건이 유지되는 동안 해제 명령을 이미 냄 (DOORS-2.3) */
    bool lock_sent;        /* 조건이 유지되는 동안 잠금 명령을 이미 냄 (DOORS-3.3) */
    DoorCmd cmd;           /* 이번 주기에 낸 명령 */
    bool locked;           /* 마지막 명령에 따른 잠금 상태 */
} DoorState;

void doors_init(DoorState *st);
/* st->cmd 에 이번 주기 명령을, out->door_locked 에 마지막 명령에 따른 잠금 상태를 둔다 */
void doors_step(DoorState *st, const VbcContext *ctx, VbcOutputs *out);

#endif /* DOORS_H */
