/*
 * adas_stopgo.c - Stop & Go (XS1~XS13, XD1~XD4)
 */

#include "adas_stopgo.h"

#include <math.h>
#include <string.h>

#define CRUISE_K 0.4f   /* XS3: 속도 오차 → 가속도 */
#define CRUISE_A_MIN (-2.0f)
#define FOLLOW_KD 0.45f /* XS4: 거리 오차 → 가속도 (임계 감쇠에 가깝게, 시정수 약 2초) */
#define FOLLOW_KV 0.7f  /* XS4: 상대속도 → 가속도 */
#define STOP_LEAD_V 0.5f   /* 앞차 지면 속도가 이보다 작으면 정지 모드 */
#define STOP_MODE_V 3.0f   /* 자차 m/s */
#define STOP_ROOM_MIN 0.3f
#define STOP_ROOM_FIRM 0.7f
#define STOP_FIRM_A (-0.8f)
#define STOP_FINAL_A (-1.5f)

void sg_init(StopGo *sg)
{
    memset(sg, 0, sizeof *sg);
    sg->gap_level = 2;
    sg->state = SG_OFF;
    hold_timer_reset(&sg->collision_clear);
}

void sg_switch(StopGo *sg, bool on) { sg->sw_on = on; }
void sg_press_set(StopGo *sg) { sg->ev_set = true; }
void sg_press_res(StopGo *sg) { sg->ev_res = true; }
void sg_adjust(StopGo *sg, int dir) { sg->ev_adjust += dir > 0 ? 1 : -1; }

void sg_set_gap(StopGo *sg, int level)
{
    if (level >= 1 && level <= 3)
        sg->gap_level = level;
}

float sg_gap_s(const StopGo *sg)
{
    return 0.5f + 0.5f * (float)sg->gap_level; /* 1.0 / 1.5 / 2.0 s */
}

static void enter(StopGo *sg, SgState s, vbc_ms_t now)
{
    if (sg->state != s) {
        sg->state = s;
        sg->since_ms = now;
    }
}

/* XS2: 10 km/h 단위로 올리고 30~60 */
static int set_from_speed(int kmh)
{
    int v = ((kmh + 9) / 10) * 10;
    return v < SG_SET_MIN ? SG_SET_MIN : v > SG_SET_MAX ? SG_SET_MAX : v;
}

static void takeover(StopGo *sg, vbc_ms_t now)
{
    sg->takeover_until = now + SG_TAKEOVER_MS;
    sg->takeover_timed = true;
}

/* XS11: 주행 중이면 가속도 요청을 1초에 걸쳐 0으로, 서 있었으면 정지 유지를 이어 간다 */
static void release(StopGo *sg, vbc_ms_t now, bool moving)
{
    if (moving) {
        sg->ramp = true;
        sg->ramp_a0 = sg->a_prev;
        sg->ramp_start = now;
    } else {
        sg->post_hold = true;
    }
    takeover(sg, now);
    enter(sg, SG_STANDBY, now);
}

/* XS3, XS4, XS12 */
static float control(StopGo *sg, vbc_ms_t now, float v, const Track *lead, float dt)
{
    float a = CRUISE_K * ((float)sg->set_kmh / 3.6f - v);
    if (a < CRUISE_A_MIN) a = CRUISE_A_MIN;
    if (a > SG_A_MAX) a = SG_A_MAX;

    bool risk = false;
    float ttc = INFINITY;
    if (lead) {
        float d = lead->x, vrel = lead->vx;
        float d_star = SG_STOP_GAP_M + sg_gap_s(sg) * v;
        float af = FOLLOW_KD * (d - d_star) + FOLLOW_KV * vrel;
        if (af < a)
            a = af;
        /* XS5 정지: 앞차가 서 있고 저속이면 4 m에서 정확히 서는 감속도 */
        if (lead->vx + v < STOP_LEAD_V && v < STOP_MODE_V) {
            float room = d - SG_STOP_GAP_M;
            float as = room > STOP_ROOM_MIN ? -(v * v) / (2.0f * room) : STOP_FINAL_A;
            if (room < STOP_ROOM_FIRM && as > STOP_FIRM_A)
                as = STOP_FIRM_A; /* 마지막 0.7 m는 확실히 선다 */
            if (as < a)
                a = as;
        }
        if (vrel < -0.1f)
            ttc = d / -vrel;
        float a_req = vrel < 0.0f ? vrel * vrel / (2.0f * fmaxf(d - 2.0f, 0.1f)) : 0.0f;
        risk = ttc < SG_TTC_RISK || a_req > -SG_A_MIN;
    }
    if (risk) {
        sg->collision = true;
        hold_timer_reset(&sg->collision_clear);
    } else if (sg->collision && hold_timer_update(&sg->collision_clear, now, ttc >= SG_TTC_CLEAR, 1000)) {
        sg->collision = false;
    }

    sg->a_des = a;
    if (sg->collision) {
        a = SG_A_MIN; /* XS12: 변화율 제한 없이 최대 감속 */
    } else {
        if (a < SG_A_MIN) a = SG_A_MIN;
        if (a > SG_A_MAX) a = SG_A_MAX;
        float da = SG_JERK * dt;
        if (a > sg->a_prev + da) a = sg->a_prev + da;
        if (a < sg->a_prev - da) a = sg->a_prev - da;
    }
    sg->a_prev = a;
    return a;
}

/* XS6: 정지할 때보다 1.0 m 이상 멀어지거나 지면 속도 1.0 m/s 이상. 선행 차량을 잃어도 출발로 본다 */
static bool lead_departed(const StopGo *sg, const Track *lead, float v)
{
    if (!lead)
        return true;
    return lead->x >= sg->stop_lead_x + 1.0f || lead->vx + v >= 1.0f;
}

static void begin_hold(StopGo *sg, vbc_ms_t now, const Track *lead)
{
    enter(sg, SG_HOLD, now);
    sg->stop_lead_id = lead ? lead->id : 0;
    sg->stop_lead_x = lead ? lead->x : 0.0f;
    sg->go_alert = false;
    sg->a_prev = 0.0f;
}

static void begin_active(StopGo *sg, vbc_ms_t now)
{
    enter(sg, SG_ACTIVE, now);
    sg->a_prev = 0.0f;
    sg->collision = false;
    sg->go_alert = false;
}

void sg_step(StopGo *sg, vbc_ms_t now, const VbcContext *ctx, const AdasMap *map, SgPedals pd)
{
    float dt = sg->started ? (float)(now - sg->last_ms) / 1000.0f : 0.0f;
    sg->last_ms = now;
    sg->started = true;

    bool moving = ctx->speed_valid && ctx->speed_kmh > 0;
    float v = ctx->speed_valid ? (float)ctx->speed_kmh / 3.6f : 0.0f;
    const Track *lead = map_lead(map);
    int corners_bad = (map->rx[RADAR_FCL].health != RHEALTH_OK) + (map->rx[RADAR_FCR].health != RHEALTH_OK);
    bool fr_ok = map->rx[RADAR_FR].health == RHEALTH_OK;
    bool sys_release = !ctx->ignition_on || ctx->gear != GEAR_D || ctx->rx_lost || !ctx->speed_valid ||
                       ctx->speed_kmh > SG_RELEASE_KMH || map->status == MAP_NONE || ctx->any_door_open;
    bool engage_ok = ctx->gear == GEAR_D && ctx->speed_valid && !ctx->rx_lost && ctx->speed_kmh <= SG_MAX_ENGAGE_KMH &&
                     !pd.brake && !ctx->any_door_open && fr_ok && map->status != MAP_NONE && corners_bad < 2;

    sg->block = map_restart_block(map, now, sg->stop_lead_id);
    if (corners_bad >= 2)
        sg->block |= 8; /* XS13: 측면 둘 다 고장이면 재출발 금지 */

    bool ev_set = sg->ev_set, ev_res = sg->ev_res;
    int adj = sg->ev_adjust;
    sg->ev_set = sg->ev_res = false;
    sg->ev_adjust = 0;
    if (adj && sg->set_kmh) { /* XS2 */
        int s = sg->set_kmh + 10 * adj;
        sg->set_kmh = s < SG_SET_MIN ? SG_SET_MIN : s > SG_SET_MAX ? SG_SET_MAX : s;
    }

    if (sg->post_hold && (ctx->gear == GEAR_P || pd.brake)) /* XS8: P 또는 브레이크까지 */
        sg->post_hold = false;

    bool stopped_state = sg->state == SG_HOLD || sg->state == SG_WAIT;
    bool a_valid = false;
    float a_out = 0.0f;
    sg->override = false;

    if (!sg->sw_on || !ctx->ignition_on) {
        if (stopped_state && !pd.brake && ctx->gear != GEAR_P) /* XS10: 정지 중 OFF면 정지 유지는 계속 */
            sg->post_hold = true;
        enter(sg, SG_OFF, now);
        sg->ramp = false;
    } else {
        if (sg->state == SG_OFF)
            enter(sg, SG_STANDBY, now);

        switch (sg->state) {
        case SG_STANDBY:
            if ((ev_set || (ev_res && sg->set_kmh)) && engage_ok) { /* XS1, XS2 */
                if (ev_set)
                    sg->set_kmh = set_from_speed((int)ctx->speed_kmh);
                sg->ramp = false;
                if (ctx->speed_kmh == 0) {
                    if (lead && lead->x <= SG_ENGAGE_STOP_M)
                        begin_hold(sg, now, lead);
                } else {
                    begin_active(sg, now);
                }
            }
            break;

        case SG_ACTIVE: {
            if (pd.brake) { /* XS10 */
                enter(sg, SG_STANDBY, now);
                break;
            }
            if (sys_release) { /* XS11 */
                release(sg, now, moving);
                break;
            }
            if (ev_set && moving)
                sg->set_kmh = set_from_speed((int)ctx->speed_kmh);
            float a = control(sg, now, v, lead, dt);
            /* XS5: 멈췄다. 출발 직후 0.5초는 보지 않는다 (변화율 제한 때문에 요청이 아직 0 근처) */
            if (!moving && sg->a_des <= 0.0f && lead && now - sg->since_ms >= 500) {
                if (corners_bad >= 2) { /* XS13 */
                    sg->post_hold = true;
                    takeover(sg, now);
                    enter(sg, SG_STANDBY, now);
                } else {
                    begin_hold(sg, now, lead);
                }
                break;
            }
            if (pd.accel) { /* XS10: 운전자 가속이 앞선다 */
                sg->override = true;
                sg->a_prev = 0.0f;
            } else {
                a_valid = true;
                a_out = a;
            }
            break;
        }

        case SG_HOLD:
            if (pd.brake) { /* XS10 */
                enter(sg, SG_STANDBY, now);
                break;
            }
            if (sys_release) { /* XS8, XS11 */
                sg->post_hold = true;
                takeover(sg, now);
                enter(sg, SG_STANDBY, now);
                break;
            }
            if (corners_bad >= 1 || now - sg->since_ms >= SG_AUTO_GO_MS) /* XS13, XS7 */
                enter(sg, SG_WAIT, now);
            else if (lead_departed(sg, lead, v) && !sg->block) /* XS6, XS9 */
                begin_active(sg, now);
            break;

        case SG_WAIT:
            if (pd.brake) {
                enter(sg, SG_STANDBY, now);
                break;
            }
            if (sys_release || now - sg->since_ms >= SG_LONG_STOP_MS) { /* XS8, XS11 */
                sg->post_hold = true;
                takeover(sg, now);
                enter(sg, SG_STANDBY, now);
                break;
            }
            if (!sg->go_alert && lead_departed(sg, lead, v) && !sg->go_since) { /* XD3 */
                sg->go_alert = true;
                sg->go_since = now;
            }
            if (pd.accel) /* XS7: 가속 페달 */
                begin_active(sg, now);
            else if (ev_res && !sg->block) /* XS7: RES, XS9 */
                begin_active(sg, now);
            break;

        case SG_OFF:
            break;
        }
    }

    if (sg->state != SG_WAIT) {
        sg->go_alert = false;
        sg->go_since = 0;
    } else if (sg->go_alert && (now - sg->go_since >= SG_GO_ALERT_MS || !lead)) {
        sg->go_alert = false; /* XD3: 5초 또는 선행 차량을 잃음 (go_since는 남겨 다시 켜지지 않게) */
    }

    if (sg->ramp && sg->state == SG_STANDBY) { /* XS11 */
        vbc_ms_t t = now - sg->ramp_start;
        if (t < SG_RAMP_MS) {
            a_valid = true;
            a_out = sg->ramp_a0 * (1.0f - (float)t / (float)SG_RAMP_MS);
        } else {
            sg->ramp = false;
        }
    } else {
        sg->ramp = false;
    }

    sg->accel_valid = a_valid;
    sg->accel = a_valid ? a_out : 0.0f;
    sg->hold = sg->state == SG_HOLD || sg->state == SG_WAIT || sg->post_hold;
    if (!a_valid && sg->state != SG_ACTIVE)
        sg->a_prev = 0.0f;

    /* XD1, XD2 */
    bool radar_fault = false;
    for (int i = 0; i < RADAR_COUNT; i++)
        radar_fault |= map->rx[i].health == RHEALTH_FAULT;
    bool takeover_on = (sg->takeover_timed && now < sg->takeover_until) || sg->collision || sg->post_hold;
    if (sg->takeover_timed && now >= sg->takeover_until)
        sg->takeover_timed = false;
    sg->alert = takeover_on ? SG_ALERT_TAKEOVER
              : (ctx->ignition_on && radar_fault) ? SG_ALERT_RADAR
              : sg->go_alert ? SG_ALERT_GO
              : SG_ALERT_NONE;
}

const char *sg_state_name(SgState s)
{
    switch (s) {
    case SG_OFF: return "off";
    case SG_STANDBY: return "standby";
    case SG_ACTIVE: return "active";
    case SG_HOLD: return "hold";
    case SG_WAIT: return "wait";
    }
    return "?";
}

const char *sg_alert_name(SgAlert a)
{
    switch (a) {
    case SG_ALERT_NONE: return "none";
    case SG_ALERT_TAKEOVER: return "takeover";
    case SG_ALERT_RADAR: return "radar";
    case SG_ALERT_GO: return "go";
    }
    return "?";
}
