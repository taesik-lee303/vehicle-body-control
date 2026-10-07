/*
 * test_drive.c - 주행 모델 R1~R5
 * 작성: F (이태식)
 *
 * 기대값은 사양서와 docs/sim-plan.md의 수치(보통 +10, 풀 +25, 브레이크 -40, 타력 -5 km/h/s)에서 계산한다.
 */

#include "console.h"
#include "drive.h"
#include "test_util.h"

/* 1초 = 50ms × 20 tick */
static void run_ms(Drive *d, vbc_ms_t ms)
{
    for (vbc_ms_t t = 0; t < ms; t += 50)
        drive_step(d, 50);
}

static void start(Drive *d, Gear gear)
{
    drive_init(d);
    drive_request_ignition(d, true);
    drive_request_gear(d, gear);
}

/* ---- R1 ---- */

TEST(r1_ignition_off_car_does_not_move)
{
    Drive d;
    drive_init(&d);
    drive_request_gear(&d, GEAR_D);
    drive_set_pedals(&d, THROTTLE_FULL, false);
    run_ms(&d, 5000);
    CHECK_EQ(drive_speed_kmh(&d), 0);
}

TEST(r1_ignition_only_changes_at_standstill)
{
    Drive d;
    start(&d, GEAR_D);
    CHECK_EQ(drive_request_ignition(&d, false), true); /* 정지 상태에서 끄기 */
    CHECK_EQ(drive_request_ignition(&d, true), true);
    drive_set_pedals(&d, THROTTLE_NORMAL, false);
    run_ms(&d, 50); /* 0.5 km/h: 아직 정지가 아님 */
    CHECK_EQ(d.speed_mkmh, 500);
    CHECK_EQ(drive_request_ignition(&d, false), false);
    CHECK_EQ(d.ignition, true);
    CHECK_EQ(drive_request_ignition(&d, true), true); /* 같은 값 요청은 항상 OK */
}

/* ---- R2 ---- */

TEST(r2_gear_only_changes_at_speed_zero)
{
    Drive d;
    start(&d, GEAR_D);
    drive_set_pedals(&d, THROTTLE_NORMAL, false);
    run_ms(&d, 50);
    CHECK_EQ(drive_request_gear(&d, GEAR_P), false);
    CHECK_EQ(drive_request_gear(&d, GEAR_R), false);
    CHECK_EQ(d.gear, GEAR_D);

    drive_set_pedals(&d, THROTTLE_NONE, true); /* 0.5 km/h를 브레이크로 0까지 */
    run_ms(&d, 50);
    CHECK_EQ(d.speed_mkmh, 0);
    CHECK_EQ(drive_request_gear(&d, GEAR_P), true);
    CHECK_EQ(d.gear, GEAR_P);
}

/* ---- R3 ---- */

TEST(r3_drive_accelerates_forward)
{
    Drive d;
    start(&d, GEAR_D);
    drive_set_pedals(&d, THROTTLE_NORMAL, false);
    run_ms(&d, 1000);
    CHECK_EQ(drive_speed_kmh(&d), 10);
    drive_set_pedals(&d, THROTTLE_FULL, false);
    run_ms(&d, 1000);
    CHECK_EQ(drive_speed_kmh(&d), 35); /* 10 + 25 */
}

TEST(r3_reverse_accelerates_as_speed_magnitude)
{
    Drive d;
    start(&d, GEAR_R);
    drive_set_pedals(&d, THROTTLE_NORMAL, false);
    run_ms(&d, 1000);
    CHECK_EQ(drive_speed_kmh(&d), 10); /* 속도는 방향 없는 속력, 후진은 기어 R로 구분 */
    CHECK_EQ(d.gear, GEAR_R);
}

TEST(r3_park_and_neutral_do_not_move)
{
    Drive d;
    start(&d, GEAR_P);
    drive_set_pedals(&d, THROTTLE_FULL, false);
    run_ms(&d, 5000);
    CHECK_EQ(d.speed_mkmh, 0);

    drive_set_pedals(&d, THROTTLE_NONE, false);
    CHECK_EQ(drive_request_gear(&d, GEAR_N), true);
    drive_set_pedals(&d, THROTTLE_FULL, false);
    run_ms(&d, 5000);
    CHECK_EQ(d.speed_mkmh, 0);
}

/* ---- R4 ---- */

TEST(r4_brake_stops_at_zero)
{
    Drive d;
    start(&d, GEAR_D);
    drive_set_pedals(&d, THROTTLE_FULL, false);
    run_ms(&d, 4000); /* 100 km/h */
    CHECK_EQ(drive_speed_kmh(&d), 100);
    drive_set_pedals(&d, THROTTLE_NONE, true);
    run_ms(&d, 2450); /* 100 - 40*2.45 = 2 */
    CHECK_EQ(drive_speed_kmh(&d), 2);
    run_ms(&d, 50); /* 0.05초 더: 0 (음수로 내려가지 않음) */
    CHECK_EQ(d.speed_mkmh, 0);
    run_ms(&d, 1000);
    CHECK_EQ(d.speed_mkmh, 0);
}

TEST(r4_brake_has_priority_over_throttle)
{
    Drive d;
    start(&d, GEAR_D);
    drive_set_pedals(&d, THROTTLE_FULL, false);
    run_ms(&d, 4000);
    drive_set_pedals(&d, THROTTLE_FULL, true); /* 가속 + 브레이크 */
    run_ms(&d, 1000);
    CHECK_EQ(drive_speed_kmh(&d), 60); /* 감속 40 */
}

TEST(r4_coasts_slowly_without_pedals)
{
    Drive d;
    start(&d, GEAR_D);
    drive_set_pedals(&d, THROTTLE_FULL, false);
    run_ms(&d, 4000);
    drive_set_pedals(&d, THROTTLE_NONE, false);
    run_ms(&d, 2000);
    CHECK_EQ(drive_speed_kmh(&d), 90); /* 5 km/h/s */
    run_ms(&d, 20000);
    CHECK_EQ(d.speed_mkmh, 0);
}

/* ---- R5 ---- */

TEST(r5_forward_max_250)
{
    Drive d;
    start(&d, GEAR_D);
    drive_set_pedals(&d, THROTTLE_FULL, false);
    run_ms(&d, 9950); /* 248.75 */
    CHECK_EQ(drive_speed_kmh(&d), 249);
    run_ms(&d, 50); /* 250.00 */
    CHECK_EQ(drive_speed_kmh(&d), 250);
    run_ms(&d, 50); /* 더 안 올라감 */
    CHECK_EQ(drive_speed_kmh(&d), 250);
    run_ms(&d, 10000);
    CHECK_EQ(d.speed_mkmh, 250000);
}

TEST(r5_reverse_max_20)
{
    Drive d;
    start(&d, GEAR_R);
    drive_set_pedals(&d, THROTTLE_FULL, false);
    run_ms(&d, 700); /* 17.5 */
    CHECK_EQ(d.speed_mkmh, 17500);
    run_ms(&d, 100); /* 20.0 이상으로 올라가지 않음 */
    CHECK_EQ(d.speed_mkmh, 20000);
    run_ms(&d, 10000);
    CHECK_EQ(d.speed_mkmh, 20000);
}

/* ---- 콘솔 통합 ---- */

TEST(console_drive_mode_sends_speed_to_core)
{
    Console c;
    console_init(&c, NULL);
    CHECK_EQ(console_exec(&c, "drive on"), CONSOLE_OK);
    console_exec(&c, "ign on");
    console_exec(&c, "gear d");
    console_exec(&c, "pedal 2 0");
    console_exec(&c, "wait 10");
    CHECK_EQ(c.panel.speed, 250);
    CHECK_EQ(c.sys.ctx.speed_valid, true);
    CHECK_EQ(c.sys.ctx.speed_kmh, 250);
    CHECK_EQ(c.sys.ctx.rx_lost, false); /* 주행 중에도 입력은 계속 송신됨 */
    CHECK_EQ(c.sys.in.speed.last_rx_ms, 10000);
}

TEST(console_rejects_gear_and_ignition_while_moving)
{
    Console c;
    console_init(&c, NULL);
    console_exec(&c, "drive on");
    console_exec(&c, "ign on");
    console_exec(&c, "gear d");
    console_exec(&c, "pedal 1 0");
    console_exec(&c, "wait 2"); /* 20 km/h */
    console_exec(&c, "gear p");
    CHECK_EQ(c.panel.gear, GEAR_D);
    console_exec(&c, "ign off");
    CHECK_EQ(c.panel.ignition, true);
    CHECK_EQ(c.sys.ctx.gear, GEAR_D);
}

TEST(console_speed_command_blocked_in_drive_mode)
{
    Console c;
    console_init(&c, NULL);
    CHECK_EQ(console_exec(&c, "pedal 1 0"), CONSOLE_ERROR); /* drive off에서는 pedal 불가 */
    console_exec(&c, "drive on");
    CHECK_EQ(console_exec(&c, "speed 100"), CONSOLE_ERROR);
    CHECK_EQ(c.panel.speed, 0);
    console_exec(&c, "drive off");
    CHECK_EQ(console_exec(&c, "speed 100"), CONSOLE_OK);
    CHECK_EQ(c.panel.speed, 100);
}

TEST(console_drive_on_takes_over_panel_state)
{
    Console c;
    console_init(&c, NULL);
    console_exec(&c, "ign on");
    console_exec(&c, "gear d");
    console_exec(&c, "speed 50");
    console_exec(&c, "drive on");
    CHECK_EQ(c.drive.ignition, true);
    CHECK_EQ(c.drive.gear, GEAR_D);
    CHECK_EQ(drive_speed_kmh(&c.drive), 50);
    console_exec(&c, "wait 1"); /* 아무 페달도 안 누르면 타력 감속 */
    CHECK_EQ(c.panel.speed, 45);
}

TEST(console_expect_speed_gear_ign)
{
    Console c;
    console_init(&c, NULL);
    console_exec(&c, "expect speed 0");
    console_exec(&c, "expect gear p");
    console_exec(&c, "expect ign off");
    CHECK_EQ(c.expect_failures, 0);
    console_exec(&c, "expect speed 1");
    console_exec(&c, "expect gear d");
    console_exec(&c, "expect ign on");
    CHECK_EQ(c.expect_failures, 3);
}

int main(void)
{
    printf("== R1~R5 주행 모델\n");
    RUN(r1_ignition_off_car_does_not_move);
    RUN(r1_ignition_only_changes_at_standstill);
    RUN(r2_gear_only_changes_at_speed_zero);
    RUN(r3_drive_accelerates_forward);
    RUN(r3_reverse_accelerates_as_speed_magnitude);
    RUN(r3_park_and_neutral_do_not_move);
    RUN(r4_brake_stops_at_zero);
    RUN(r4_brake_has_priority_over_throttle);
    RUN(r4_coasts_slowly_without_pedals);
    RUN(r5_forward_max_250);
    RUN(r5_reverse_max_20);
    RUN(console_drive_mode_sends_speed_to_core);
    RUN(console_rejects_gear_and_ignition_while_moving);
    RUN(console_speed_command_blocked_in_drive_mode);
    RUN(console_drive_on_takes_over_panel_state);
    RUN(console_expect_speed_gear_ign);
    return test_summary();
}
