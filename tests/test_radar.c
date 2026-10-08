/*
 * test_radar.c - 레이더 센서 모델(XH)과 전방 맵(XC1~XC8, XM1~XM10)
 * 작성: exp/radar-stop-and-go. 기대값은 요구사항 문서(전방 레이더 맵·Stop & Go 요구사항) 7.1의 경계값에서 계산
 */

#include <math.h>
#include <string.h>

#include "adas_map.h"
#include "env_radar.h"
#include "test_util.h"

static RadarFrame g_f;
static AdasMap g_m;

static VbcContext ctx_at(int kmh)
{
    VbcContext x;
    memset(&x, 0, sizeof x);
    x.ignition_on = true;
    x.gear = GEAR_D;
    x.speed_valid = true;
    x.speed_kmh = kmh;
    return x;
}

/* 장애물 하나를 정면 x m, 높이 z m에 둔 FR 프레임 (점 세 개, 정지 물체). v_ego로 도플러를 맞춘다 */
static void fr_frame(RadarFrame *f, uint8_t counter, float x, float y, float z, float v_ego)
{
    const RadarMount *m = &RADAR_MOUNTS[RADAR_FR];
    memset(f, 0, sizeof *f);
    f->id = RADAR_FR;
    f->counter = counter;
    f->status = RADAR_ST_OK;
    for (int i = 0; i < 3; i++) {
        float py = y + 0.3f * (float)(i - 1), dz = z - m->z;
        float r = sqrtf(x * x + py * py + dz * dz);
        RadarDetection *d = &f->det[f->count++];
        d->range_m = r;
        d->az_deg = atan2f(py, x) * 57.29578f;
        d->el_deg = asinf(dz / r) * 57.29578f;
        d->vr_mps = -v_ego * x / r;
        d->snr_db = 30.0f;
    }
}

static void corner_frame(RadarFrame *f, RadarId id, uint8_t counter)
{
    memset(f, 0, sizeof *f);
    f->id = id;
    f->counter = counter;
    f->status = RADAR_ST_OK;
}

/* 세 레이더에 t ms 프레임을 보내고 맵을 한 주기 돌린다 */
static void cycle(AdasMap *m, vbc_ms_t t, uint8_t counter, float x, float y, float z, const VbcContext *c)
{
    fr_frame(&g_f, counter, x, y, z, c->speed_kmh / 3.6f);
    map_rx_frame(m, t, &g_f, c);
    for (int id = RADAR_FCL; id <= RADAR_FCR; id++) {
        corner_frame(&g_f, (RadarId)id, counter);
        map_rx_frame(m, t, &g_f, c);
    }
    map_step(m, t, c);
}

static const Track *only_track(const AdasMap *m)
{
    for (int i = 0; i < MAP_MAX_TRACKS; i++)
        if (m->tracks[i].used)
            return &m->tracks[i];
    return NULL;
}

/* ---- 센서 모델 (XH) ---- */

TEST(xh1_object_outside_fov_gives_no_points)
{
    static World w;
    static RadarSim rs;
    world_init(&w);
    radar_sim_init(&rs, 1);
    world_add(&w, OBJ_CAR, 30.0, 0.0, 0.0);
    CHECK(radar_sim_frame(&rs, RADAR_FR, &w, 0.0, &g_f));
    int ahead = 0;
    for (int i = 0; i < g_f.count; i++)
        ahead += fabsf(g_f.det[i].az_deg) < 3.0f && fabsf(g_f.det[i].range_m - 30.0f) < 1.0f;
    CHECK(ahead > 0); /* 정면 30 m 차는 보인다 */

    world_clear(&w);
    world_add(&w, OBJ_CAR, 260.0, 0.0, 0.0); /* FR 거리 200 m 밖 */
    radar_sim_frame(&rs, RADAR_FR, &w, 0.0, &g_f);
    for (int i = 0; i < g_f.count; i++)
        CHECK(g_f.det[i].range_m <= 200.0f);
}

TEST(xh4_occluded_car_is_hidden)
{
    static World w;
    static RadarSim rs;
    world_init(&w);
    radar_sim_init(&rs, 2);
    world_add(&w, OBJ_TRUCK, 20.0, 0.0, 0.0); /* 앞 트럭이 */
    world_add(&w, OBJ_CAR, 40.0, 0.0, 0.0);   /* 뒤 차를 가린다 */
    int behind = 0;
    for (int k = 0; k < 20; k++) {
        radar_sim_frame(&rs, RADAR_FR, &w, 0.0, &g_f);
        for (int i = 0; i < g_f.count; i++)
            behind += g_f.det[i].range_m > 38.0f && g_f.det[i].range_m < 45.0f && fabsf(g_f.det[i].az_deg) < 2.0f;
    }
    CHECK_EQ(behind, 0);
}

TEST(xh5_doppler_matches_relative_speed)
{
    static World w;
    static RadarSim rs;
    world_init(&w);
    radar_sim_init(&rs, 3);
    world_add(&w, OBJ_CAR, 50.0, 0.0, 0.0);              /* 서 있는 차 */
    radar_sim_frame(&rs, RADAR_FR, &w, 10.0, &g_f);       /* 자차 10 m/s */
    int n = 0;
    for (int i = 0; i < g_f.count; i++) {
        if (fabsf(g_f.det[i].range_m - 50.0f) < 1.0f && fabsf(g_f.det[i].az_deg) < 3.0f) {
            CHECK(fabsf(g_f.det[i].vr_mps + 10.0f) < 0.3f); /* 다가오므로 −10 m/s */
            n++;
        }
    }
    CHECK(n > 0);
}

TEST(xh6_same_seed_same_frames)
{
    static World w;
    static RadarSim a, b;
    static RadarFrame fa, fb;
    world_init(&w);
    world_add(&w, OBJ_CAR, 30.0, 0.0, 0.0);
    world_add(&w, OBJ_PED, 10.0, 3.0, 0.0);
    radar_sim_init(&a, 77);
    radar_sim_init(&b, 77);
    for (int k = 0; k < 5; k++) {
        radar_sim_frame(&a, RADAR_FR, &w, 5.0, &fa);
        radar_sim_frame(&b, RADAR_FR, &w, 5.0, &fb);
        CHECK_EQ(fa.count, fb.count);
        CHECK(memcmp(fa.det, fb.det, sizeof fa.det[0] * fa.count) == 0);
    }
}

TEST(xh8_fault_modes)
{
    static World w;
    static RadarSim rs;
    static RadarFrame prev;
    world_init(&w);
    radar_sim_init(&rs, 4);
    world_add(&w, OBJ_CAR, 30.0, 0.0, 0.0);
    radar_sim_frame(&rs, RADAR_FR, &w, 5.0, &prev);

    radar_sim_set_fault(&rs, RADAR_FR, RADAR_FAULT_CUT);
    CHECK(!radar_sim_frame(&rs, RADAR_FR, &w, 5.0, &g_f)); /* 끊김: 프레임 없음 */

    radar_sim_set_fault(&rs, RADAR_FR, RADAR_FAULT_REPEAT);
    CHECK(radar_sim_frame(&rs, RADAR_FR, &w, 5.0, &g_f));
    CHECK_EQ(g_f.counter, prev.counter); /* 반복: 카운터째 같음 */

    radar_sim_set_fault(&rs, RADAR_FR, RADAR_FAULT_STUCK);
    CHECK(radar_sim_frame(&rs, RADAR_FR, &w, 5.0, &g_f));
    CHECK_EQ(g_f.counter, (uint8_t)(prev.counter + 1)); /* 고착: 카운터는 늘고 */
    CHECK(g_f.count == prev.count && memcmp(g_f.det, prev.det, sizeof g_f.det[0] * g_f.count) == 0);

    radar_sim_set_fault(&rs, RADAR_FR, RADAR_FAULT_BLOCK);
    CHECK(radar_sim_frame(&rs, RADAR_FR, &w, 5.0, &g_f));
    CHECK_EQ(g_f.status, RADAR_ST_BLOCKED);
    CHECK_EQ(g_f.count, 0);
}

/* ---- XC 레이더 상태 ---- */

/* XC1: 새 프레임 0.2초 없음 → 고장 (199 / 200 / 201 ms) */
TEST(xc1_timeout_boundary)
{
    VbcContext c = ctx_at(30);
    vbc_ms_t t0 = 1000;
    map_init(&g_m);
    fr_frame(&g_f, 1, 30, 0, 0.8f, 0);
    map_rx_frame(&g_m, t0, &g_f, &c);
    map_step(&g_m, t0 + 199, &c);
    CHECK_EQ(g_m.rx[RADAR_FR].health, RHEALTH_OK);
    map_step(&g_m, t0 + 200, &c);
    CHECK_EQ(g_m.rx[RADAR_FR].health, RHEALTH_FAULT);
    map_step(&g_m, t0 + 201, &c);
    CHECK_EQ(g_m.rx[RADAR_FR].health, RHEALTH_FAULT);
}

/* XC1: 같은 카운터 = 프레임 반복 → 새 프레임이 아니다 */
TEST(xc1_repeated_counter_is_not_new)
{
    VbcContext c = ctx_at(30);
    map_init(&g_m);
    fr_frame(&g_f, 9, 30, 0, 0.8f, 0);
    map_rx_frame(&g_m, 1000, &g_f, &c);
    for (vbc_ms_t t = 1050; t <= 1200; t += 50)
        map_rx_frame(&g_m, t, &g_f, &c); /* 카운터 9 그대로 */
    map_step(&g_m, 1200, &c);
    CHECK_EQ(g_m.rx[RADAR_FR].health, RHEALTH_FAULT);
    CHECK(g_m.rx[RADAR_FR].reason & RFAULT_TIMEOUT);
}

/* XC1, XC6: 처음 프레임 전에는 확인 중, 시작부터 0.2초면 고장 */
TEST(xc6_unknown_then_fault_without_frames)
{
    VbcContext c = ctx_at(0);
    map_init(&g_m);
    map_step(&g_m, 199, &c);
    CHECK_EQ(g_m.rx[RADAR_FCL].health, RHEALTH_UNKNOWN);
    CHECK_EQ(g_m.status, MAP_NONE);
    map_step(&g_m, 200, &c);
    CHECK_EQ(g_m.rx[RADAR_FCL].health, RHEALTH_FAULT);
}

TEST(xc2_blocked_status_is_fault)
{
    VbcContext c = ctx_at(30);
    map_init(&g_m);
    corner_frame(&g_f, RADAR_FCR, 1);
    g_f.status = RADAR_ST_BLOCKED;
    map_rx_frame(&g_m, 50, &g_f, &c);
    map_step(&g_m, 50, &c);
    CHECK_EQ(g_m.rx[RADAR_FCR].health, RHEALTH_FAULT);
    CHECK(g_m.rx[RADAR_FCR].reason & RFAULT_STATUS);
}

TEST(xc3_out_of_range_points_dropped_radar_ok)
{
    VbcContext c = ctx_at(0);
    map_init(&g_m);
    fr_frame(&g_f, 1, 30, 0, 0.8f, 0);
    g_f.det[0].range_m = 250.0f; /* 200 m 초과 */
    g_f.det[1].vr_mps = 80.0f;   /* 70 m/s 초과 */
    map_rx_frame(&g_m, 50, &g_f, &c);
    map_step(&g_m, 50, &c);
    CHECK_EQ(g_m.n_pts[RADAR_FR], 1);
    CHECK_EQ(g_m.rx[RADAR_FR].health, RHEALTH_OK);
}

/* XC7: 움직이는 중 검출점이 1초 그대로 → 고장 (999 / 1000 / 1001 ms) */
TEST(xc7_stuck_boundary)
{
    VbcContext c = ctx_at(30);
    vbc_ms_t t0 = 2000;
    vbc_ms_t probe[3] = {999, 1000, 1001};
    RadarHealth want[3] = {RHEALTH_OK, RHEALTH_FAULT, RHEALTH_FAULT};
    for (int k = 0; k < 3; k++) {
        map_init(&g_m);
        uint8_t cnt = 1;
        fr_frame(&g_f, cnt, 30, 0, 0.8f, 8.3f);
        map_rx_frame(&g_m, t0, &g_f, &c);
        g_f.counter = ++cnt; /* 카운터만 늘고 점은 그대로 */
        map_rx_frame(&g_m, t0 + 500, &g_f, &c);
        g_f.counter = ++cnt;
        map_rx_frame(&g_m, t0 + probe[k], &g_f, &c);
        map_step(&g_m, t0 + probe[k], &c);
        CHECK_EQ(g_m.rx[RADAR_FR].health, want[k]);
    }
}

TEST(xc7_not_counted_when_standing_still)
{
    VbcContext c = ctx_at(0);
    map_init(&g_m);
    fr_frame(&g_f, 1, 30, 0, 0.8f, 0);
    for (int k = 0; k <= 30; k++) {
        g_f.counter = (uint8_t)(k + 1);
        map_rx_frame(&g_m, 50u * (unsigned)k, &g_f, &c);
        map_step(&g_m, 50u * (unsigned)k, &c);
    }
    CHECK_EQ(g_m.rx[RADAR_FR].health, RHEALTH_OK); /* 서 있으면 장면이 그대로인 게 정상 */
}

/* XC8: 고장 뒤 정상 새 프레임이 1초 연속이어야 복귀 (999 / 1000 / 1001 ms) */
TEST(xc8_recovery_boundary)
{
    VbcContext c = ctx_at(0);
    vbc_ms_t probe[3] = {999, 1000, 1001};
    RadarHealth want[3] = {RHEALTH_FAULT, RHEALTH_OK, RHEALTH_OK};
    for (int k = 0; k < 3; k++) {
        map_init(&g_m);
        map_step(&g_m, 300, &c); /* 프레임 없이 0.3초 → 고장 */
        CHECK_EQ(g_m.rx[RADAR_FR].health, RHEALTH_FAULT);
        vbc_ms_t t1 = 400;
        uint8_t cnt = 1;
        for (vbc_ms_t t = t1; t <= t1 + probe[k]; t += 1) {
            if ((t - t1) % 50 == 0 || t == t1 + probe[k]) {
                fr_frame(&g_f, cnt++, 30, 0, 0.8f, 0);
                g_f.det[0].snr_db = (float)(cnt % 7) + 20.0f;
                map_rx_frame(&g_m, t, &g_f, &c);
                map_step(&g_m, t, &c);
            }
        }
        CHECK_EQ(g_m.rx[RADAR_FR].health, want[k]);
    }
}

/* ---- XM 맵 ---- */

/* XM4: 3주기 연속 연관되어야 확정 */
TEST(xm4_confirmed_on_third_cycle)
{
    VbcContext c = ctx_at(0);
    map_init(&g_m);
    cycle(&g_m, 50, 1, 20, 0, 0.8f, &c);
    cycle(&g_m, 100, 2, 20, 0, 0.8f, &c);
    CHECK(only_track(&g_m) && !only_track(&g_m)->confirmed);
    cycle(&g_m, 150, 3, 20, 0, 0.8f, &c);
    CHECK(only_track(&g_m) && only_track(&g_m)->confirmed);
    CHECK_EQ(g_m.lead_id, only_track(&g_m)->id); /* 경로 내 장애물 → 선행 차량 */
}

/* XM5: 확정 트랙은 보이지 않아도 0.3초까지 유지 (300 / 301 ms) */
TEST(xm5_coast_boundary)
{
    VbcContext c = ctx_at(0);
    for (int k = 0; k < 2; k++) {
        map_init(&g_m);
        for (int i = 1; i <= 3; i++)
            cycle(&g_m, 50u * (unsigned)i, (uint8_t)i, 20, 0, 0.8f, &c);
        /* 150 ms 이후 점 없는 프레임 */
        vbc_ms_t end = 150 + (k == 0 ? 300 : 301);
        uint8_t cnt = 4;
        for (vbc_ms_t t = 200; t <= end; t += 50) {
            for (int id = 0; id < RADAR_COUNT; id++) {
                corner_frame(&g_f, (RadarId)id, cnt);
                map_rx_frame(&g_m, t, &g_f, &c);
            }
            cnt++;
            map_step(&g_m, t, &c);
        }
        if (end % 50)
            map_step(&g_m, end, &c);
        CHECK_EQ(only_track(&g_m) != NULL, k == 0);
    }
}

static const Track *settle(float x, float y, float z, int kmh)
{
    VbcContext c = ctx_at(kmh);
    map_init(&g_m);
    for (int i = 1; i <= 5; i++)
        cycle(&g_m, 50u * (unsigned)i, (uint8_t)i, x, y, z, &c);
    return only_track(&g_m);
}

/* XM6: 바닥이 2.5 m 이상이면 장애물이 아니다 (2.49 / 2.50 / 2.51) */
TEST(xm6_high_boundary)
{
    float z[3] = {2.49f, 2.50f, 2.51f};
    bool obstacle[3] = {true, false, false};
    for (int k = 0; k < 3; k++) {
        const Track *t = settle(30, 0, z[k], 0);
        CHECK(t != NULL);
        if (t)
            CHECK_EQ(t->obstacle, obstacle[k]);
    }
}

/* XM6: 한 번도 움직이지 않은 정지 물체의 꼭대기가 0.2 m 미만이면 장애물이 아니다 (0.19 / 0.20 / 0.21).
 * 30 m에서는 노면 점 제거(XM2)가 먼저 지우므로 노면 기준이 낮은 5 m에서 본다 */
TEST(xm6_low_boundary)
{
    float z[3] = {0.19f, 0.20f, 0.21f};
    bool obstacle[3] = {false, true, true};
    for (int k = 0; k < 3; k++) {
        const Track *t = settle(5, 0, z[k], 0);
        CHECK(t != NULL);
        if (t)
            CHECK_EQ(t->obstacle, obstacle[k]);
    }
}

/* XM8: 트랙 가로 범위가 |y| ≤ 1.3 m와 겹치면 경로 내 (1.29 / 1.30 / 1.31). 점 세 개는 y±0.3 */
TEST(xm8_path_boundary)
{
    float edge[3] = {1.29f, 1.30f, 1.31f};
    bool in[3] = {true, true, false};
    for (int k = 0; k < 3; k++) {
        const Track *t = settle(20, edge[k] + 0.3f, 0.8f, 0);
        CHECK(t != NULL);
        if (t)
            CHECK_EQ(t->in_path, in[k]);
    }
}

/* XM10: FR 고장이면 맵 없음, 측면 하나 고장이면 축소 */
TEST(xm10_map_status)
{
    VbcContext c = ctx_at(0);
    map_init(&g_m);
    cycle(&g_m, 50, 1, 20, 0, 0.8f, &c);
    CHECK_EQ(g_m.status, MAP_OK);
    corner_frame(&g_f, RADAR_FCL, 2);
    g_f.status = RADAR_ST_BLOCKED;
    map_rx_frame(&g_m, 100, &g_f, &c);
    fr_frame(&g_f, 2, 20, 0, 0.8f, 0);
    map_rx_frame(&g_m, 100, &g_f, &c);
    corner_frame(&g_f, RADAR_FCR, 2);
    map_rx_frame(&g_m, 100, &g_f, &c);
    map_step(&g_m, 100, &c);
    CHECK_EQ(g_m.status, MAP_REDUCED);
    map_step(&g_m, 400, &c); /* 모두 0.3초 끊김 */
    CHECK_EQ(g_m.status, MAP_NONE);
    CHECK_EQ(g_m.lead_id, 0);
}

int main(void)
{
    printf("test_radar\n");
    RUN(xh1_object_outside_fov_gives_no_points);
    RUN(xh4_occluded_car_is_hidden);
    RUN(xh5_doppler_matches_relative_speed);
    RUN(xh6_same_seed_same_frames);
    RUN(xh8_fault_modes);
    RUN(xc1_timeout_boundary);
    RUN(xc1_repeated_counter_is_not_new);
    RUN(xc6_unknown_then_fault_without_frames);
    RUN(xc2_blocked_status_is_fault);
    RUN(xc3_out_of_range_points_dropped_radar_ok);
    RUN(xc7_stuck_boundary);
    RUN(xc7_not_counted_when_standing_still);
    RUN(xc8_recovery_boundary);
    RUN(xm4_confirmed_on_third_cycle);
    RUN(xm5_coast_boundary);
    RUN(xm6_high_boundary);
    RUN(xm6_low_boundary);
    RUN(xm8_path_boundary);
    RUN(xm10_map_status);
    return test_summary();
}
