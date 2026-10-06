/*
 * common.c - 입력 수신, 공통 규칙(C1~C5)
 * 담당: A (이태식)
 */

#include "common.h"

#include <string.h>

static void signal_rx(Signal *s, vbc_ms_t now, int32_t value)
{
    s->value = value;
    s->received = true;
    s->last_rx_ms = now;
}

static bool in_range(int32_t v, int32_t lo, int32_t hi)
{
    return v >= lo && v <= hi;
}

/* C5: 입력 쪽 초기 상태. 스위치 초기 위치는 사양서에 없어 OFF로 둔다 (가정) */
void vbc_inputs_init(VbcInputs *in)
{
    memset(in, 0, sizeof(*in));
    in->light_sw = LIGHT_SW_OFF;
    in->wiper_sw = WIPER_SW_OFF;
}

/* C5: 모두 꺼짐, 정지, 잠금 해제, 경고 없음 */
void vbc_outputs_init(VbcOutputs *out)
{
    out->headlight_on = false;
    out->lux_fault_lamp = false;
    out->wiper = WIPER_STOP;
    out->rain_fault_lamp = false;
    out->door_locked = false;
    out->warning = WARN_NONE;
}

void vbc_rx_speed(VbcInputs *in, vbc_ms_t now, int32_t kmh) { signal_rx(&in->speed, now, kmh); }
void vbc_rx_gear(VbcInputs *in, vbc_ms_t now, Gear gear) { signal_rx(&in->gear, now, (int32_t)gear); }
void vbc_rx_ignition(VbcInputs *in, vbc_ms_t now, bool on) { signal_rx(&in->ignition, now, on ? 1 : 0); }
void vbc_rx_lux(VbcInputs *in, vbc_ms_t now, int32_t lux) { signal_rx(&in->lux, now, lux); }
void vbc_rx_rain(VbcInputs *in, vbc_ms_t now, int32_t percent) { signal_rx(&in->rain, now, percent); }

void vbc_set_door(VbcInputs *in, DoorId door, bool open)
{
    if (door < DOOR_COUNT)
        in->door_open[door] = open;
}

void vbc_set_light_switch(VbcInputs *in, LightSwitch sw) { in->light_sw = sw; }
void vbc_set_wiper_switch(VbcInputs *in, WiperSwitch sw) { in->wiper_sw = sw; }

/* C3: "마지막으로 들어온 지 0.5초가 지나지 않았고" → 경과 < 500ms */
bool vbc_signal_fresh(const Signal *s, vbc_ms_t now)
{
    return s->received && now - s->last_rx_ms < VBC_RX_TIMEOUT_MS;
}

/* C2, C4: "0.5초 동안 들어오지 않으면" → 경과 >= 500ms. 첫 수신 전에는 시작(0)부터 센다 */
bool vbc_signal_timed_out(const Signal *s, vbc_ms_t now)
{
    if (!s->received)
        return now >= VBC_RX_TIMEOUT_MS;
    return now - s->last_rx_ms >= VBC_RX_TIMEOUT_MS;
}

/* C2: 0.5초 미수신 또는 범위 밖이면 고장. 정상 값이 다시 들어오면 풀린다 */
static void sensor_judge(const Signal *s, vbc_ms_t now, int32_t lo, int32_t hi,
                         bool *fault, bool *valid, int32_t *value)
{
    bool out_of_range = s->received && !in_range(s->value, lo, hi);

    *fault = vbc_signal_timed_out(s, now) || out_of_range;
    *valid = s->received && !*fault;
    *value = *valid ? s->value : 0;
}

void vbc_build_context(const VbcInputs *in, vbc_ms_t now, VbcContext *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->now = now;

    /* C4 */
    ctx->rx_lost = vbc_signal_timed_out(&in->speed, now)
                || vbc_signal_timed_out(&in->gear, now)
                || vbc_signal_timed_out(&in->ignition, now);

    ctx->ignition_on = !ctx->rx_lost && in->ignition.received && in->ignition.value != 0;
    ctx->gear = in->gear.received ? (Gear)in->gear.value : GEAR_P;

    /* C3 */
    ctx->speed_valid = vbc_signal_fresh(&in->speed, now)
                    && vbc_signal_fresh(&in->gear, now)
                    && vbc_signal_fresh(&in->ignition, now)
                    && in_range(in->speed.value, VBC_SPEED_MIN, VBC_SPEED_MAX);
    ctx->speed_kmh = ctx->speed_valid ? in->speed.value : 0;

    /* C2 */
    sensor_judge(&in->lux, now, VBC_LUX_MIN, VBC_LUX_MAX,
                 &ctx->lux_fault, &ctx->lux_valid, &ctx->lux);
    sensor_judge(&in->rain, now, VBC_RAIN_MIN, VBC_RAIN_MAX,
                 &ctx->rain_fault, &ctx->rain_valid, &ctx->rain);

    for (int i = 0; i < DOOR_COUNT; i++) {
        ctx->door_open[i] = in->door_open[i];
        if (in->door_open[i])
            ctx->any_door_open = true;
    }
    ctx->light_sw = in->light_sw;
    ctx->wiper_sw = in->wiper_sw;
}

/* C1 */
void hold_timer_reset(HoldTimer *t)
{
    t->running = false;
    t->since_ms = 0;
}

bool hold_timer_update(HoldTimer *t, vbc_ms_t now, bool condition, vbc_ms_t duration_ms)
{
    if (!condition) {
        hold_timer_reset(t);
        return false;
    }
    if (!t->running) {
        t->running = true;
        t->since_ms = now;
    }
    return now - t->since_ms >= duration_ms;
}

const char *vbc_gear_name(Gear g)
{
    switch (g) {
    case GEAR_P: return "P";
    case GEAR_R: return "R";
    case GEAR_N: return "N";
    case GEAR_D: return "D";
    }
    return "?";
}

const char *vbc_wiper_name(WiperLevel w)
{
    switch (w) {
    case WIPER_STOP: return "정지";
    case WIPER_INTERMITTENT: return "간헐";
    case WIPER_LOW: return "저속";
    case WIPER_HIGH: return "고속";
    }
    return "?";
}

const char *vbc_warning_name(Warning w)
{
    switch (w) {
    case WARN_NONE: return "없음";
    case WARN_DOOR_OPEN_WHILE_DRIVING: return "주행 중 도어 열림";
    case WARN_LUX_SENSOR_FAULT: return "조도 센서 고장";
    case WARN_RAIN_SENSOR_FAULT: return "강우 센서 고장";
    }
    return "?";
}
