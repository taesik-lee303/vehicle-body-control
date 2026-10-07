/*
 * doors.c - 도어 잠금 (DOORS-1 ~ DOORS-4)
 * 담당: D (김의현)
 */

#include "doors.h"

#define LOCK_SPEED_KMH 15 /* DOORS-3 */
#define CMD_HOLD_MS 1000u /* DOORS-2.1, 3: 연속 1 s 이상 */

void doors_init(DoorState *st)
{
    hold_timer_reset(&st->unlock_hold);
    hold_timer_reset(&st->lock_hold);
    st->unlock_sent = false;
    st->lock_sent = false;
    st->cmd = DOOR_CMD_NONE;
    st->locked = false; /* C5 초기 상태 */
}

/*
 * 조건이 연속 1 s 이상이면 한 번만 true.
 * 조건이 깨지면 유지시간을 0으로 하고(hold_timer_update) 다시 낼 수 있게 한다.
 */
static bool hold_once(HoldTimer *t, bool *sent, vbc_ms_t now, bool cond)
{
    bool fire = hold_timer_update(t, now, cond, CMD_HOLD_MS) && !*sent;

    *sent = cond && (*sent || fire);
    return fire;
}

void doors_step(DoorState *st, const VbcContext *ctx, VbcOutputs *out)
{
    bool rx_ok = !ctx->rx_lost; /* DOORS-1 */
    bool unlock_cond = rx_ok && (!ctx->ignition_on || ctx->gear == GEAR_P); /* DOORS-2.1 */
    bool lock_cond = rx_ok && ctx->ignition_on && ctx->gear != GEAR_P
                  && ctx->speed_valid && ctx->speed_kmh >= LOCK_SPEED_KMH; /* DOORS-3 */
    bool unlock = hold_once(&st->unlock_hold, &st->unlock_sent, ctx->now, unlock_cond);
    bool lock = hold_once(&st->lock_hold, &st->lock_sent, ctx->now, lock_cond);

    /* DOORS-4: 두 조건은 동시에 성립하지 않으며, 이 외에는 명령 없음 */
    st->cmd = unlock ? DOOR_CMD_UNLOCK : lock ? DOOR_CMD_LOCK : DOOR_CMD_NONE;
    if (st->cmd != DOOR_CMD_NONE)
        st->locked = lock;
    out->door_locked = st->locked;
}
