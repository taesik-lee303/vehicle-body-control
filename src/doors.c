/*
 * doors.c - 도어 잠금 (DOORS-1 ~ DOORS-4)
 * 담당: D (김의현)
 *
 * 시간은 ctx->now만 쓰고 경과시간은 모두 부호 없는 뺄셈(now - 시각)으로 구한다.
 * 시각 카운터가 넘쳐 0으로 돌아가도 경과시간이 맞다 (DOORS-FS-1). 동적 할당 없음 (DOORS-FS-2).
 */

#include "doors.h"

#define LOCK_SPEED_KMH 15     /* DOORS-3.1 */
#define CMD_HOLD_MS 1000u     /* DOORS-2.1, 3.1: 연속 1 s 이상 */
#define RX_ENTER_MS 5000u     /* DOORS-1.2: 경과 5 s 이상 → 제한 모드 */
#define RX_RELEASE_MS 1000u   /* DOORS-1.6: 경과 1 s 미만 → 해제 */

/* DOORS-1.2, 1.3: 진입하면 다시 수신해야 풀린다. DOORS-1.5: 유지시간 0 s */
static void enter_restricted(DoorState *st)
{
    st->restricted = true;
    st->rx_since_entry = false;
    hold_timer_reset(&st->unlock_hold);
    hold_timer_reset(&st->lock_hold);
}

void doors_init(DoorState *st)
{
    st->rx_ms = 0;
    st->unlock_sent = false;
    st->lock_sent = false;
    st->cmd = DOOR_CMD_NONE;
    st->locked = false;   /* C5 초기 상태 */
    enter_restricted(st); /* DOORS-1.3: 초기화 이후의 수신만 센다 */
}

/*
 * DOORS-1.1 (근사): ctx에는 신호별 수신 시각이 없어 세 신호 중 가장 오래된 시각 하나만 추정한다.
 *   ctx->speed_valid: 세 신호 모두 0.5 s 안에 정상 값으로 들어옴(C3) → now를 기록
 *   ctx->rx_lost: 하나라도 0.5 s 이상 안 들어옴(C4) → 경과시간을 0.5 s 이상으로 바로잡는다
 * 그 외(속도 범위 밖 등)에는 갱신하지 않는다 → 이전 값 재전달로 시각이 늘지 않는다.
 * 입력이 주기마다 들어오면 정확하고, 그렇지 않으면 입력 간격만큼 오차가 날 수 있다.
 */
static void track_rx(DoorState *st, const VbcContext *ctx)
{
    if (ctx->speed_valid) {
        st->rx_ms = ctx->now;
        if (st->restricted)
            st->rx_since_entry = true;
    } else if (ctx->rx_lost && ctx->now - st->rx_ms < VBC_RX_TIMEOUT_MS) {
        st->rx_ms = ctx->now - VBC_RX_TIMEOUT_MS;
    }
}

/*
 * 조건이 연속 1 s 이상이면 한 번만 true (DOORS-2.1, 3.1).
 * 조건이 깨지면 유지시간을 0으로 (DOORS-2.2, 3.2, hold_timer_update) 하고 다시 낼 수 있게 한다.
 * 다음 명령은 조건이 다시 1 s 연속 성립해야 나가므로 해제 후 1 s 이상 지난 뒤다 (DOORS-2.3, 3.3).
 */
static bool hold_once(HoldTimer *t, bool *sent, vbc_ms_t now, bool cond)
{
    bool held = hold_timer_update(t, now, cond, CMD_HOLD_MS);

    if (!cond) {
        *sent = false;
        return false;
    }
    if (!held || *sent)
        return false;
    *sent = true;
    return true;
}

void doors_step(DoorState *st, const VbcContext *ctx, VbcOutputs *out)
{
    st->cmd = DOOR_CMD_NONE;
    track_rx(st, ctx);

    if (!st->restricted && ctx->now - st->rx_ms >= RX_ENTER_MS)
        enter_restricted(st); /* DOORS-1.2 */
    if (st->restricted && st->rx_since_entry && ctx->now - st->rx_ms < RX_RELEASE_MS)
        st->restricted = false; /* DOORS-1.6. 1.7: 이번 주기부터 0 s부터 센다 */

    if (!st->restricted) {
        /* DOORS-2.1: 시동 OFF 또는 기어 P */
        bool unlock_cond = !ctx->ignition_on || ctx->gear == GEAR_P;
        /* DOORS-3.1: 2.1 아님(= 시동 ON, 기어 P 아님) + 유효한 속도 >= 15 */
        bool lock_cond = !unlock_cond && ctx->speed_valid && ctx->speed_kmh >= LOCK_SPEED_KMH;
        /* 두 조건은 동시에 성립하지 않는다. 유지시간은 둘 다 매 주기 갱신한다 */
        bool unlock = hold_once(&st->unlock_hold, &st->unlock_sent, ctx->now, unlock_cond);
        bool lock = hold_once(&st->lock_hold, &st->lock_sent, ctx->now, lock_cond);

        if (unlock) {
            st->cmd = DOOR_CMD_UNLOCK;
            st->locked = false;
        } else if (lock) {
            st->cmd = DOOR_CMD_LOCK;
            st->locked = true;
        }
    }
    /* DOORS-1.4, DOORS-4: 위 두 경우 외에는 명령 없음, 잠금 상태 그대로 */
    out->door_locked = st->locked;
}
