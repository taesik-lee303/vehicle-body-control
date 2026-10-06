#include "sim.h"
#include "test_util.h"

/* 주행 중(시동 ON, D단, 20km/h) 잠긴 상태까지 진행 */
static void drive_locked(Sim *s)
{
    s->feed.gear = GEAR_D;
    s->feed.speed = 20;
    sim_run_until(s, 500);
}

/* C5: 시작 상태는 잠금 해제 */
TEST(d_initial_unlocked)
{
    Sim s;
    sim_init(&s);
    sim_run_until(&s, 100);
    CHECK_EQ(s.sys.out.door_locked, false);
}

/* D3: 속도 14 → 해제 유지, 15 → 잠금 (경계) */
TEST(d3_lock_at_15kmh)
{
    Sim s;
    sim_init(&s);
    s.feed.gear = GEAR_D;
    s.feed.speed = 14;
    sim_run_until(&s, 500);
    CHECK_EQ(s.sys.out.door_locked, false);
    s.feed.speed = 15;
    sim_run_until(&s, 600);
    CHECK_EQ(s.sys.out.door_locked, true);
}

/* D3: 속도가 15 아래로 내려가도 직전(잠금) 유지 */
TEST(d3_keeps_lock_when_slow)
{
    Sim s;
    sim_init(&s);
    drive_locked(&s);
    CHECK_EQ(s.sys.out.door_locked, true);
    s.feed.speed = 5;
    sim_run_until(&s, 1000);
    CHECK_EQ(s.sys.out.door_locked, true);
}

/* D3: 범위 밖 속도는 유효하지 않으므로 잠그지 않는다 */
TEST(d3_out_of_range_speed_does_not_lock)
{
    Sim s;
    sim_init(&s);
    s.feed.gear = GEAR_D;
    s.feed.speed = 1000; /* 범위 밖 */
    sim_run_until(&s, 500);
    CHECK_EQ(s.sys.out.door_locked, false);
}

/* D3: 속도 끊김(그 입력만) 중에는 새로 잠그지 않는다. 첫 수신 전에도 마찬가지 */
TEST(d3_no_speed_no_lock)
{
    Sim s;
    sim_init(&s);
    s.feed.gear = GEAR_D;
    s.feed.speed = 20;
    s.feed.mute = SIM_SPEED;
    sim_run_until(&s, 1000);
    CHECK_EQ(s.sys.out.door_locked, false);
}

/* D2: 기어 P → 해제 */
TEST(d2_gear_p_unlocks)
{
    Sim s;
    sim_init(&s);
    drive_locked(&s);
    s.feed.gear = GEAR_P;
    sim_run_until(&s, 600);
    CHECK_EQ(s.sys.out.door_locked, false);
}

/* D2: 시동 OFF → 해제 */
TEST(d2_ignition_off_unlocks)
{
    Sim s;
    sim_init(&s);
    drive_locked(&s);
    s.feed.ignition = false;
    sim_run_until(&s, 600);
    CHECK_EQ(s.sys.out.door_locked, false);
}

/* D2: 기어 P이면 속도가 15 이상이어도 해제 (D2가 D3보다 우선) */
TEST(d2_beats_d3)
{
    Sim s;
    sim_init(&s);
    s.feed.gear = GEAR_P;
    s.feed.speed = 30;
    sim_run_until(&s, 500);
    CHECK_EQ(s.sys.out.door_locked, false);
}

/* D1: 수신 중단(C4) → 잠금 그대로. 0.5초 경계(Q1): 마지막 수신 후 499ms까지 정상 */
TEST(d1_rx_lost_keeps_locked)
{
    Sim s;
    sim_init(&s);
    drive_locked(&s);
    s.feed.mute = SIM_VEHICLE; /* 마지막 수신 t=500 */
    sim_run_until(&s, 1000);
    CHECK_EQ(s.sys.out.door_locked, true);
    sim_run_until(&s, 3000);
    CHECK_EQ(s.sys.out.door_locked, true);
}

/* D1: 수신 중단에서도 해제 상태는 그대로 (잠기지 않음) */
TEST(d1_rx_lost_keeps_unlocked)
{
    Sim s;
    sim_init(&s);
    s.feed.gear = GEAR_D;
    s.feed.speed = 10;
    sim_run_until(&s, 500);
    s.feed.mute = SIM_VEHICLE;
    sim_run_until(&s, 2000);
    CHECK_EQ(s.sys.out.door_locked, false);
}

/* D1 → 정상 복귀: 수신이 돌아오면 D2/D3 판단 재개 */
TEST(d1_recovery_resumes_d2)
{
    Sim s;
    sim_init(&s);
    drive_locked(&s);
    s.feed.mute = SIM_VEHICLE;
    sim_run_until(&s, 2000);
    CHECK_EQ(s.sys.out.door_locked, true);
    s.feed.mute = 0;
    s.feed.gear = GEAR_P;
    s.feed.speed = 0;
    sim_run_until(&s, 2100);
    CHECK_EQ(s.sys.out.door_locked, false);
}

int main(void)
{
    printf("== D 도어·잠금\n");
    RUN(d_initial_unlocked);
    RUN(d3_lock_at_15kmh);
    RUN(d3_keeps_lock_when_slow);
    RUN(d3_out_of_range_speed_does_not_lock);
    RUN(d3_no_speed_no_lock);
    RUN(d2_gear_p_unlocks);
    RUN(d2_ignition_off_unlocks);
    RUN(d2_beats_d3);
    RUN(d1_rx_lost_keeps_locked);
    RUN(d1_rx_lost_keeps_unlocked);
    RUN(d1_recovery_resumes_d2);
    return test_summary();
}
