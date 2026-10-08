/*
 * test_common.c - 공통 규칙 C1~C5
 * 작성: A (이태식)
 */

#include "sim.h"
#include "test_util.h"

/* ---- C5 시작 상태 ---- */

TEST(c5_initial_outputs)
{
    VbcSystem sys;
    vbc_init(&sys);
    vbc_step(&sys, 0);
    CHECK_EQ(sys.out.headlight_on, false);
    CHECK_EQ(sys.out.lux_fault_lamp, false);
    CHECK_EQ(sys.out.wiper, WIPER_STOP);
    CHECK_EQ(sys.out.rain_fault_lamp, false);
    CHECK_EQ(sys.out.door_locked, false);
    CHECK_EQ(sys.out.warning, WARN_NONE);
}

/* ---- C2 센서 고장 (조건마다: 첫 수신 전, 범위 밖, 끊김, 정상 복귀) ---- */

TEST(c2_before_first_rx_counts_from_start)
{
    VbcInputs in;
    VbcContext ctx;
    vbc_inputs_init(&in);

    vbc_build_context(&in, 450, &ctx);
    CHECK_EQ(ctx.lux_fault, false);
    CHECK_EQ(ctx.lux_valid, false); /* 고장은 아니지만 쓸 값도 없음 */
    vbc_build_context(&in, 499, &ctx);
    CHECK_EQ(ctx.lux_fault, false);
    vbc_build_context(&in, 500, &ctx);
    CHECK_EQ(ctx.lux_fault, true);
    CHECK_EQ(ctx.rain_fault, true);
}

TEST(c2_out_of_range_is_fault_immediately)
{
    VbcInputs in;
    VbcContext ctx;
    vbc_inputs_init(&in);

    vbc_rx_lux(&in, 100, 60000);
    vbc_build_context(&in, 100, &ctx);
    CHECK_EQ(ctx.lux_fault, false);
    CHECK_EQ(ctx.lux, 60000);

    vbc_rx_lux(&in, 150, 60001);
    vbc_build_context(&in, 150, &ctx);
    CHECK_EQ(ctx.lux_fault, true);
    CHECK_EQ(ctx.lux_valid, false);

    vbc_rx_lux(&in, 200, -1);
    vbc_build_context(&in, 200, &ctx);
    CHECK_EQ(ctx.lux_fault, true);

    vbc_rx_rain(&in, 200, 100);
    vbc_build_context(&in, 200, &ctx);
    CHECK_EQ(ctx.rain_fault, false);
    vbc_rx_rain(&in, 250, 101);
    vbc_build_context(&in, 250, &ctx);
    CHECK_EQ(ctx.rain_fault, true);
}

TEST(c2_interruption_and_recovery)
{
    VbcInputs in;
    VbcContext ctx;
    vbc_inputs_init(&in);

    vbc_rx_lux(&in, 1000, 800);
    vbc_build_context(&in, 1499, &ctx);
    CHECK_EQ(ctx.lux_fault, false);
    CHECK_EQ(ctx.lux, 800);
    vbc_build_context(&in, 1500, &ctx);
    CHECK_EQ(ctx.lux_fault, true);

    vbc_rx_lux(&in, 2000, 800); /* 정상 값이 다시 들어오면 풀림 */
    vbc_build_context(&in, 2000, &ctx);
    CHECK_EQ(ctx.lux_fault, false);
    CHECK_EQ(ctx.lux_valid, true);
}

/* ---- C3 유효한 속도 ---- */

static void rx_vehicle(VbcInputs *in, vbc_ms_t t, int32_t speed)
{
    vbc_rx_speed(in, t, speed);
    vbc_rx_gear(in, t, GEAR_D);
    vbc_rx_ignition(in, t, true);
}

TEST(c3_speed_range)
{
    VbcInputs in;
    VbcContext ctx;
    vbc_inputs_init(&in);

    rx_vehicle(&in, 0, 0);
    vbc_build_context(&in, 0, &ctx);
    CHECK_EQ(ctx.speed_valid, true);
    CHECK_EQ(ctx.speed_kmh, 0);

    rx_vehicle(&in, 50, 250);
    vbc_build_context(&in, 50, &ctx);
    CHECK_EQ(ctx.speed_valid, true);

    rx_vehicle(&in, 100, 251);
    vbc_build_context(&in, 100, &ctx);
    CHECK_EQ(ctx.speed_valid, false);
    CHECK_EQ(ctx.rx_lost, false); /* 범위 밖은 수신 중단이 아님 */

    rx_vehicle(&in, 150, -1);
    vbc_build_context(&in, 150, &ctx);
    CHECK_EQ(ctx.speed_valid, false);
}

TEST(c3_each_of_speed_gear_ignition_must_be_fresh)
{
    VbcInputs in;
    VbcContext ctx;
    vbc_inputs_init(&in);

    rx_vehicle(&in, 1000, 30);
    vbc_rx_speed(&in, 1400, 30);
    vbc_rx_ignition(&in, 1400, true);
    /* 기어만 1000ms에 멈춤 */
    vbc_build_context(&in, 1499, &ctx);
    CHECK_EQ(ctx.speed_valid, true);
    vbc_build_context(&in, 1500, &ctx);
    CHECK_EQ(ctx.speed_valid, false);
}

/* ---- C4 차량 상태 수신 중단 ---- */

TEST(c4_before_first_rx_counts_from_start)
{
    VbcInputs in;
    VbcContext ctx;
    vbc_inputs_init(&in);

    vbc_build_context(&in, 499, &ctx);
    CHECK_EQ(ctx.rx_lost, false);
    CHECK_EQ(ctx.ignition_on, false); /* 아직 안 들어옴 → 꺼진 것으로 */
    vbc_build_context(&in, 500, &ctx);
    CHECK_EQ(ctx.rx_lost, true);
}

TEST(c4_any_vehicle_signal_lost_means_ignition_off)
{
    Sim s;
    sim_init(&s);
    sim_run_until(&s, 1000);
    CHECK_EQ(s.sys.ctx.rx_lost, false);
    CHECK_EQ(s.sys.ctx.ignition_on, true);

    s.feed.mute = SIM_GEAR; /* 1050ms부터 기어만 안 들어옴, 마지막 수신 1000ms */
    sim_run_until(&s, 1450);
    CHECK_EQ(s.sys.ctx.rx_lost, false);
    sim_run_until(&s, 1500);
    CHECK_EQ(s.sys.ctx.rx_lost, true);
    CHECK_EQ(s.sys.ctx.ignition_on, false); /* 시동 값은 켜짐이지만 꺼진 것으로 봄 */
    CHECK_EQ(s.sys.ctx.speed_valid, false);

    s.feed.mute = 0; /* 정상 복귀 */
    sim_run_until(&s, 1550);
    CHECK_EQ(s.sys.ctx.rx_lost, false);
    CHECK_EQ(s.sys.ctx.ignition_on, true);
}

TEST(c4_lux_or_rain_loss_is_not_rx_lost)
{
    Sim s;
    sim_init(&s);
    s.feed.mute = SIM_LUX | SIM_RAIN;
    sim_run_until(&s, 2000);
    CHECK_EQ(s.sys.ctx.rx_lost, false);
    CHECK_EQ(s.sys.ctx.lux_fault, true);
    CHECK_EQ(s.sys.ctx.rain_fault, true);
}

/* ---- C1 N초 연속 ---- */

TEST(c1_hold_timer_boundary_and_restart)
{
    HoldTimer t;
    hold_timer_reset(&t);

    CHECK_EQ(hold_timer_update(&t, 900, false, 1000), false);
    CHECK_EQ(hold_timer_update(&t, 1000, true, 1000), false); /* 1.0초부터 성립 */
    CHECK_EQ(hold_timer_update(&t, 1999, true, 1000), false);
    CHECK_EQ(hold_timer_update(&t, 2000, true, 1000), true);  /* 정확히 1초 */
    CHECK_EQ(hold_timer_update(&t, 2500, false, 1000), false); /* 깨짐 → 처음부터 */
    CHECK_EQ(hold_timer_update(&t, 2600, true, 1000), false);
    CHECK_EQ(hold_timer_update(&t, 3599, true, 1000), false);
    CHECK_EQ(hold_timer_update(&t, 3600, true, 1000), true);
}

/* ---- 정의된 코드 판정 (사양서 3.1 값 목록). 정의 안 된 코드일 때의 처리는 질의 Q8~Q11이라 판정만 확인 ---- */

TEST(code_validity_ignition_gear)
{
    VbcInputs in;
    VbcContext ctx;
    vbc_inputs_init(&in);

    vbc_build_context(&in, 0, &ctx);
    CHECK_EQ(ctx.ignition_valid, true); /* 아직 안 받음 */
    CHECK_EQ(ctx.gear_valid, true);

    int32_t ign_ok[] = {0, 1}, ign_bad[] = {-1, 2, 255};
    for (int i = 0; i < 2; i++) { vbc_rx_ignition(&in, 0, ign_ok[i]); vbc_build_context(&in, 0, &ctx); CHECK_EQ(ctx.ignition_valid, true); }
    for (int i = 0; i < 3; i++) { vbc_rx_ignition(&in, 0, ign_bad[i]); vbc_build_context(&in, 0, &ctx); CHECK_EQ(ctx.ignition_valid, false); }

    int32_t gear_ok[] = {GEAR_P, GEAR_R, GEAR_N, GEAR_D}, gear_bad[] = {-1, 4, 7};
    for (int i = 0; i < 4; i++) { vbc_rx_gear(&in, 0, gear_ok[i]); vbc_build_context(&in, 0, &ctx); CHECK_EQ(ctx.gear_valid, true); }
    for (int i = 0; i < 3; i++) { vbc_rx_gear(&in, 0, gear_bad[i]); vbc_build_context(&in, 0, &ctx); CHECK_EQ(ctx.gear_valid, false); }

    vbc_rx_gear(&in, 0, 7);
    vbc_build_context(&in, 0, &ctx);
    CHECK_EQ(ctx.gear, 7); /* 원래 코드가 바뀌지 않고 모듈까지 전달된다 */
}

TEST(code_validity_doors_switches)
{
    VbcInputs in;
    VbcContext ctx;
    vbc_inputs_init(&in);

    vbc_build_context(&in, 0, &ctx);
    CHECK_EQ(ctx.door_valid[DOOR_FL], true);
    CHECK_EQ(ctx.light_sw_valid, true);
    CHECK_EQ(ctx.wiper_sw_valid, true);

    vbc_set_door(&in, DOOR_RR, 1);
    vbc_build_context(&in, 0, &ctx);
    CHECK_EQ(ctx.door_valid[DOOR_RR], true);
    vbc_set_door(&in, DOOR_RR, 2);
    vbc_build_context(&in, 0, &ctx);
    CHECK_EQ(ctx.door_valid[DOOR_RR], false);
    CHECK_EQ(ctx.door_valid[DOOR_FL], true);

    vbc_set_light_switch(&in, LIGHT_SW_ON);
    vbc_build_context(&in, 0, &ctx);
    CHECK_EQ(ctx.light_sw_valid, true);
    vbc_set_light_switch(&in, 3);
    vbc_build_context(&in, 0, &ctx);
    CHECK_EQ(ctx.light_sw_valid, false);
    vbc_set_light_switch(&in, -1);
    vbc_build_context(&in, 0, &ctx);
    CHECK_EQ(ctx.light_sw_valid, false);
    CHECK_EQ(ctx.light_sw, -1); /* 원래 코드 그대로 */

    vbc_set_wiper_switch(&in, WIPER_SW_HIGH);
    vbc_build_context(&in, 0, &ctx);
    CHECK_EQ(ctx.wiper_sw_valid, true);
    vbc_set_wiper_switch(&in, 4);
    vbc_build_context(&in, 0, &ctx);
    CHECK_EQ(ctx.wiper_sw_valid, false);
}

int main(void)
{
    printf("== C1~C5 공통 규칙\n");
    RUN(c5_initial_outputs);
    RUN(c2_before_first_rx_counts_from_start);
    RUN(c2_out_of_range_is_fault_immediately);
    RUN(c2_interruption_and_recovery);
    RUN(c3_speed_range);
    RUN(c3_each_of_speed_gear_ignition_must_be_fresh);
    RUN(c4_before_first_rx_counts_from_start);
    RUN(c4_any_vehicle_signal_lost_means_ignition_off);
    RUN(c4_lux_or_rain_loss_is_not_rx_lost);
    RUN(c1_hold_timer_boundary_and_restart);
    RUN(code_validity_ignition_gear);
    RUN(code_validity_doors_switches);
    return test_summary();
}
