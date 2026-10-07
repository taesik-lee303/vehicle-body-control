/*
 * wipers.c - 와이퍼 (W1~W5)
 * 담당: C (이지윤)
 */

#include "wipers.h"

/* W2 강우량 구간 경계 (%) */
#define WIPER_RAIN_INTERMITTENT 10
#define WIPER_RAIN_LOW 40
#define WIPER_RAIN_HIGH 70

/* W2: 강우량 → 기본 단계 */
static WiperLevel level_from_rain(int32_t rain)
{
    if (rain >= WIPER_RAIN_HIGH) {
        return WIPER_HIGH;
    }
    if (rain >= WIPER_RAIN_LOW) {
        return WIPER_LOW;
    }
    if (rain >= WIPER_RAIN_INTERMITTENT) {
        return WIPER_INTERMITTENT;
    }
    return WIPER_STOP;
}

/* W3: 한 단계 낮춤 (고속→저속→간헐→정지, 정지는 그대로) */
static WiperLevel level_down(WiperLevel level)
{
    return (level == WIPER_STOP) ? WIPER_STOP : (WiperLevel)(level - 1);
}

static bool switch_is_valid(WiperSwitch sw)
{
    return sw >= WIPER_SW_OFF && sw <= WIPER_SW_HIGH;
}

void wipers_init(WiperState *st)
{
    /* C5: 정지로 시작. 스위치는 정상 값을 받기 전까지 OFF (SWR-W4-005) */
    st->level = WIPER_STOP;
    st->last_switch = WIPER_SW_OFF;
}

void wipers_step(WiperState *st, const VbcContext *ctx, VbcOutputs *out)
{
    WiperLevel level;

    /* W5: 강우 센서 고장 표시는 시동·스위치와 무관 */
    out->rain_fault_lamp = ctx->rain_fault;

    /* SWR-W4-005: 잘못된 스위치 값은 마지막 정상 값을 계속 쓴다 */
    if (switch_is_valid(ctx->wiper_sw)) {
        st->last_switch = ctx->wiper_sw;
    }

    if (!ctx->ignition_on) {
        /* W1: 시동 꺼짐 (C4 수신 중단 포함) → 정지. 최우선 */
        level = WIPER_STOP;
    } else {
        switch (st->last_switch) {
        case WIPER_SW_LOW: /* W4 */
            level = WIPER_LOW;
            break;
        case WIPER_SW_HIGH: /* W4 */
            level = WIPER_HIGH;
            break;
        case WIPER_SW_AUTO:
            if (ctx->rain_fault) {
                /* W5: 고장이면 간헐. W3로 낮추지 않는다 */
                level = WIPER_INTERMITTENT;
            } else if (!ctx->rain_valid) {
                /* 첫 수신 전(0~0.5초): 고장 아님, 값 없음 → W2 판단 안 함, 직전 단계 유지
                 * (docs/c-structure.md Q3) */
                level = st->level;
            } else {
                level = level_from_rain(ctx->rain); /* W2 */
                /* W3: 유효한 속도가 0이면 한 단계 낮춤 */
                if (ctx->speed_valid && ctx->speed_kmh == 0) {
                    level = level_down(level);
                }
            }
            break;
        case WIPER_SW_OFF: /* W4 */
        default:
            level = WIPER_STOP;
            break;
        }
    }

    st->level = level;
    out->wiper = level;
}
