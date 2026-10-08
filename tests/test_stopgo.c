/*
 * test_stopgo.c - Stop & Go (XS1~XS13), 계기판 알림 (XD1~XD3), 주행 모델 연결 (XR1, XR2)
 * 작성: exp/radar-stop-and-go. 맵은 트랙을 직접 넣어 만든다 (레이더 없이 판단만 시험)
 */

#include <math.h>
#include <string.h>

#include "adas_stopgo.h"
#include "drive.h"
#include "test_util.h"

static AdasMap g_m;
static StopGo g_s;
static VbcContext g_c;
static const SgPedals NO_PEDAL = {false, false};

static void setup(int kmh)
{
    map_init(&g_m);
    for (int i = 0; i < RADAR_COUNT; i++)
        g_m.rx[i].health = RHEALTH_OK;
    g_m.status = MAP_OK;
    sg_init(&g_s);
    memset(&g_c, 0, sizeof g_c);
    g_c.ignition_on = true;
    g_c.gear = GEAR_D;
    g_c.speed_valid = true;
    g_c.speed_kmh = kmh;
}

/* 선행 차량 트랙: 앞 x m, 상대속도 vx m/s */
static Track *lead(float x, float vx)
{
    Track *t = &g_m.tracks[0];
    memset(t, 0, sizeof *t);
    t->used = t->confirmed = t->obstacle = t->in_path = true;
    t->id = 1;
    t->x = x;
    t->vx = vx;
    t->half_l = t->half_r = 0.9f;
    g_m.lead_id = 1;
    return t;
}

static void step(vbc_ms_t t) { sg_step(&g_s, t, &g_c, &g_m, NO_PEDAL); }

static void engage(vbc_ms_t t)
{
    sg_switch(&g_s, true);
    step(t);
    sg_press_set(&g_s);
    step(t);
}

/* XS1: 0~60 km/h에서만 켜진다 (59 / 60 / 61) */
TEST(xs1_engage_speed_boundary)
{
    int kmh[3] = {59, 60, 61};
    SgState want[3] = {SG_ACTIVE, SG_ACTIVE, SG_STANDBY};
    for (int k = 0; k < 3; k++) {
        setup(kmh[k]);
        engage(100);
        CHECK_EQ(g_s.state, want[k]);
    }
}

/* XS1: 정지 중에는 선행 차량이 20 m 안에 있어야 켜지고, 정지 유지로 시작 (19.9 / 20.0 / 20.1) */
TEST(xs1_engage_at_standstill_needs_lead_within_20m)
{
    float x[3] = {19.9f, 20.0f, 20.1f};
    SgState want[3] = {SG_HOLD, SG_HOLD, SG_STANDBY};
    for (int k = 0; k < 3; k++) {
        setup(0);
        lead(x[k], 0);
        engage(100);
        CHECK_EQ(g_s.state, want[k]);
    }
}

TEST(xs1_no_engage_with_door_open_or_brake_or_radar_unknown)
{
    setup(30);
    g_c.any_door_open = true;
    engage(100);
    CHECK_EQ(g_s.state, SG_STANDBY);

    setup(30);
    sg_switch(&g_s, true);
    sg_press_set(&g_s);
    sg_step(&g_s, 100, &g_c, &g_m, (SgPedals){true, false});
    CHECK_EQ(g_s.state, SG_STANDBY);

    setup(30);
    g_m.rx[RADAR_FCL].health = g_m.rx[RADAR_FCR].health = RHEALTH_UNKNOWN; /* XC6 확인 중 */
    engage(100);
    CHECK_EQ(g_s.state, SG_STANDBY);
}

/* XS2: 10 km/h 단위로 올리고 30~60. +/−는 10씩 */
TEST(xs2_set_speed)
{
    setup(23);
    engage(100);
    CHECK_EQ(g_s.set_kmh, 30);
    setup(41);
    engage(100);
    CHECK_EQ(g_s.set_kmh, 50);
    sg_adjust(&g_s, +1);
    step(150);
    CHECK_EQ(g_s.set_kmh, 60);
    sg_adjust(&g_s, +1);
    step(200);
    CHECK_EQ(g_s.set_kmh, 60);
    for (int i = 0; i < 5; i++) {
        sg_adjust(&g_s, -1);
        step(250 + 50u * (unsigned)i);
    }
    CHECK_EQ(g_s.set_kmh, 30);
}

/* XS3: 선행 차량이 없으면 설정 속도로, 가속도는 −2.0~+1.5 */
TEST(xs3_cruise_limits)
{
    setup(30);
    engage(100);
    sg_adjust(&g_s, +1);
    sg_adjust(&g_s, +1);
    sg_adjust(&g_s, +1); /* 60 */
    for (vbc_ms_t t = 150; t <= 3000; t += 50)
        step(t);
    CHECK(g_s.accel_valid && fabsf(g_s.accel - 1.5f) < 1e-3f);
    g_c.speed_kmh = 60;
    for (vbc_ms_t t = 3050; t <= 6000; t += 50)
        step(t);
    CHECK(fabsf(g_s.accel) < 0.05f);
}

/* XS4: 변화율 2.5 m/s³ (50 ms에 0.125) */
TEST(xs4_jerk_limit)
{
    setup(30);
    lead(8.0f, -5.0f); /* 가까이 빠르게 다가감 → 큰 감속 필요 (충돌 위험 전) */
    g_m.tracks[0].vx = -2.0f;
    engage(100);
    step(150);
    CHECK(g_s.accel >= -0.126f);
    step(200);
    CHECK(g_s.accel >= -0.251f);
}

/* XS5: 속도 0이 되면 정지 유지, 가속도 요청 없음 */
TEST(xs5_stop_enters_hold)
{
    setup(10);
    lead(6.0f, -2.0f);
    engage(100);
    for (vbc_ms_t t = 150; t <= 600; t += 50)
        step(t);
    g_c.speed_kmh = 0;
    g_m.tracks[0].vx = 0.0f;
    g_m.tracks[0].x = 4.2f;
    for (vbc_ms_t t = 650; t <= 800; t += 50)
        step(t);
    CHECK_EQ(g_s.state, SG_HOLD);
    CHECK(g_s.hold);
    CHECK(!g_s.accel_valid);
}

static void stopped_behind_lead(vbc_ms_t t0)
{
    setup(0);
    lead(4.5f, 0.0f);
    engage(t0); /* 정지 중 SET → 정지 유지 */
}

/* XS6, XS7: 정지 3초 안에 앞차가 출발하면 스스로 재출발, 3초면 재출발 대기 (2999 / 3000 / 3001) */
TEST(xs6_xs7_auto_go_boundary)
{
    vbc_ms_t dt[3] = {2999, 3000, 3001};
    SgState want[3] = {SG_ACTIVE, SG_WAIT, SG_WAIT};
    for (int k = 0; k < 3; k++) {
        stopped_behind_lead(1000);
        CHECK_EQ(g_s.state, SG_HOLD);
        step(1000 + dt[k] - 1);
        g_m.tracks[0].x = 5.6f; /* 1.0 m 넘게 멀어짐 */
        step(1000 + dt[k]);
        CHECK_EQ(g_s.state, want[k]);
    }
}

/* XS7: 재출발 대기에서는 RES 또는 가속 페달로만. 앞차가 떠나면 출발 가능 알림 (XD3) */
TEST(xs7_wait_res_and_go_alert)
{
    stopped_behind_lead(1000);
    step(4000);
    CHECK_EQ(g_s.state, SG_WAIT);
    g_m.tracks[0].x = 6.0f;
    step(4050);
    CHECK_EQ(g_s.state, SG_WAIT);
    CHECK_EQ(g_s.alert, SG_ALERT_GO);
    step(9050); /* 5초 뒤 알림 끔 */
    CHECK_EQ(g_s.alert, SG_ALERT_NONE);
    sg_press_res(&g_s);
    step(9100);
    CHECK_EQ(g_s.state, SG_ACTIVE);
}

/* XS8: 재출발 대기 180초 → 대기, 정지 유지는 P 또는 브레이크까지 계속 */
TEST(xs8_long_stop)
{
    stopped_behind_lead(1000);
    step(4000); /* 재출발 대기 */
    step(4000 + 179999);
    CHECK_EQ(g_s.state, SG_WAIT);
    step(4000 + 180000);
    CHECK_EQ(g_s.state, SG_STANDBY);
    CHECK(g_s.hold);
    CHECK_EQ(g_s.alert, SG_ALERT_TAKEOVER);
    g_c.gear = GEAR_P;
    step(4000 + 180050);
    CHECK(!g_s.hold);
}

TEST(xs8_door_open_while_stopped_keeps_hold)
{
    stopped_behind_lead(1000);
    g_c.any_door_open = true;
    step(1100);
    CHECK_EQ(g_s.state, SG_STANDBY);
    CHECK(g_s.hold);
}

/* XS9 (1): 경로 내 5 m 안에 정지할 때의 차가 아닌 장애물이 있으면 재출발 금지 (4.9 / 5.0 / 5.1) */
TEST(xs9_obstacle_within_5m_blocks)
{
    float x[3] = {4.9f, 5.0f, 5.1f};
    SgState want[3] = {SG_HOLD, SG_HOLD, SG_ACTIVE};
    for (int k = 0; k < 3; k++) {
        stopped_behind_lead(1000);
        Track *o = &g_m.tracks[1];
        memset(o, 0, sizeof *o);
        o->used = o->confirmed = o->obstacle = o->in_path = true;
        o->id = 2;
        o->x = x[k];
        o->half_l = o->half_r = 0.25f;
        g_m.tracks[0].x = 8.0f; /* 앞차는 떠남 */
        step(1500);
        CHECK_EQ(g_s.state, want[k]);
    }
}

/* XS9 (2): 측면 레이더가 본, 경로 쪽으로 다가오는 물체 */
TEST(xs9_crossing_seen_by_corner_blocks)
{
    stopped_behind_lead(1000);
    Track *o = &g_m.tracks[1];
    memset(o, 0, sizeof *o);
    o->used = o->confirmed = o->obstacle = true;
    o->id = 2;
    o->x = 3.0f;
    o->y = 3.0f;
    o->vy = -1.2f;
    o->half_l = o->half_r = 0.25f;
    o->corner_seen = true;
    o->corner_seen_ms = 1400;
    g_m.tracks[0].x = 8.0f;
    step(1500);
    CHECK_EQ(g_s.state, SG_HOLD);
    o->vy = +1.2f; /* 멀어지면 */
    step(1550);
    CHECK_EQ(g_s.state, SG_ACTIVE);
}

/* XS10: 브레이크 → 그 주기에 대기, 요청 없음. 가속 페달 → 요청 없음 (운전자 우선) */
TEST(xs10_driver_intervention)
{
    setup(30);
    lead(30.0f, 0.0f);
    engage(100);
    sg_step(&g_s, 150, &g_c, &g_m, (SgPedals){false, true});
    CHECK_EQ(g_s.state, SG_ACTIVE);
    CHECK(g_s.override && !g_s.accel_valid);
    sg_step(&g_s, 200, &g_c, &g_m, (SgPedals){true, false});
    CHECK_EQ(g_s.state, SG_STANDBY);
    CHECK(!g_s.accel_valid && !g_s.hold);
}

/* XS11: 70 km/h 초과면 해제 (69 / 70 / 71), 가속도 요청은 1초에 걸쳐 0으로, 개입 요청 5초 */
TEST(xs11_overspeed_release_boundary)
{
    int kmh[3] = {69, 70, 71};
    SgState want[3] = {SG_ACTIVE, SG_ACTIVE, SG_STANDBY};
    for (int k = 0; k < 3; k++) {
        setup(50);
        engage(100);
        for (vbc_ms_t t = 150; t <= 1000; t += 50)
            step(t);
        g_c.speed_kmh = kmh[k];
        step(1050);
        CHECK_EQ(g_s.state, want[k]);
    }
    CHECK_EQ(g_s.alert, SG_ALERT_TAKEOVER);
    CHECK(g_s.accel_valid); /* 램프 중 */
    step(2049);
    CHECK(g_s.accel_valid);
    step(2050);
    CHECK(!g_s.accel_valid);
    step(6049);
    CHECK_EQ(g_s.alert, SG_ALERT_TAKEOVER);
    step(6050);
    CHECK_EQ(g_s.alert, SG_ALERT_NONE);
}

TEST(xs11_fr_fault_releases)
{
    setup(30);
    engage(100);
    g_m.rx[RADAR_FR].health = RHEALTH_FAULT;
    g_m.status = MAP_NONE;
    step(150);
    CHECK_EQ(g_s.state, SG_STANDBY);
    CHECK_EQ(g_s.alert, SG_ALERT_TAKEOVER); /* 개입 요청 > 레이더 고장 (XD1) */
    step(5150);
    CHECK_EQ(g_s.alert, SG_ALERT_RADAR); /* XD2 */
}

/* XS12: TTC 1.5초 미만이면 개입 요청과 −3.5 (1.49 / 1.50 / 1.51 s). 필요 감속 > 3.5 조건은 따로 */
TEST(xs12_ttc_boundary)
{
    float ttc[3] = {1.49f, 1.50f, 1.51f};
    bool risk[3] = {true, false, false};
    for (int k = 0; k < 3; k++) {
        setup(30);
        lead(30.0f, 0.0f);
        engage(100);
        g_m.tracks[0].x = 6.0f; /* 6 m에서는 필요 감속(약 2 m/s²)이 3.5보다 작아 TTC만 본다 */
        g_m.tracks[0].vx = -6.0f / ttc[k];
        step(150);
        CHECK_EQ(g_s.collision, risk[k]);
        if (risk[k])
            CHECK(fabsf(g_s.accel + 3.5f) < 1e-4f && g_s.alert == SG_ALERT_TAKEOVER);
    }
}

/* XS13: 측면 하나 고장 → 멈춘 뒤 바로 재출발 대기 (자동 출발 없음). 둘 다 → 켜지지 않음 */
TEST(xs13_corner_faults)
{
    stopped_behind_lead(1000);
    g_m.rx[RADAR_FCL].health = RHEALTH_FAULT;
    g_m.status = MAP_REDUCED;
    step(1050);
    CHECK_EQ(g_s.state, SG_WAIT);
    g_m.tracks[0].x = 8.0f;
    step(1100);
    CHECK_EQ(g_s.state, SG_WAIT);

    setup(30);
    g_m.rx[RADAR_FCL].health = g_m.rx[RADAR_FCR].health = RHEALTH_FAULT;
    g_m.status = MAP_REDUCED;
    engage(100);
    CHECK_EQ(g_s.state, SG_STANDBY);
}

/* XR1, XR2: 주행 모델 우선순위 = 브레이크 > 가속 페달 > 정지 유지 > 가속도 요청 > 자연 감속 */
TEST(xr_drive_request_priority)
{
    Drive d;
    drive_init(&d);
    d.ignition = true;
    d.gear = GEAR_D;
    drive_set_request(&d, true, 1000, false); /* +1.0 m/s² */
    drive_step(&d, 1000);
    CHECK_EQ(d.speed_mkmh, 3600); /* 1 s × 1.0 m/s² = 3.6 km/h */
    drive_set_request(&d, true, -500, false);
    drive_step(&d, 1000);
    CHECK_EQ(d.speed_mkmh, 1800);
    drive_set_pedals(&d, THROTTLE_NORMAL, false);
    drive_step(&d, 100);
    CHECK_EQ(d.speed_mkmh, 1800 + DRIVE_ACCEL_NORMAL * 100); /* 가속 페달이 앞섬 */
    drive_set_pedals(&d, THROTTLE_NONE, false);
    drive_set_request(&d, true, 1000, true); /* 정지 유지 */
    drive_step(&d, 1000);
    CHECK_EQ(d.speed_mkmh, 0);
    drive_set_request(&d, true, 1000, false);
    drive_set_pedals(&d, THROTTLE_NONE, true); /* 브레이크 */
    drive_step(&d, 100);
    CHECK_EQ(d.speed_mkmh, 0);
}

int main(void)
{
    printf("test_stopgo\n");
    RUN(xs1_engage_speed_boundary);
    RUN(xs1_engage_at_standstill_needs_lead_within_20m);
    RUN(xs1_no_engage_with_door_open_or_brake_or_radar_unknown);
    RUN(xs2_set_speed);
    RUN(xs3_cruise_limits);
    RUN(xs4_jerk_limit);
    RUN(xs5_stop_enters_hold);
    RUN(xs6_xs7_auto_go_boundary);
    RUN(xs7_wait_res_and_go_alert);
    RUN(xs8_long_stop);
    RUN(xs8_door_open_while_stopped_keeps_hold);
    RUN(xs9_obstacle_within_5m_blocks);
    RUN(xs9_crossing_seen_by_corner_blocks);
    RUN(xs10_driver_intervention);
    RUN(xs11_overspeed_release_boundary);
    RUN(xs11_fr_fault_releases);
    RUN(xs12_ttc_boundary);
    RUN(xs13_corner_faults);
    RUN(xr_drive_request_priority);
    return test_summary();
}
