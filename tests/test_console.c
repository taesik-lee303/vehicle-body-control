/*
 * test_console.c - 터미널 명령 입력
 * 작성: F (이태식)
 */

#include "console.h"
#include "test_util.h"

static void setup(Console *c)
{
    console_init(c, NULL); /* 출력 없이 */
}

TEST(init_is_c5_and_signals_sent_at_0)
{
    Console c;
    setup(&c);
    CHECK_EQ(c.now, 0);
    CHECK_EQ(c.sys.in.speed.received, true);
    CHECK_EQ(c.sys.in.lux.received, true);
    CHECK_EQ(c.sys.ctx.rx_lost, false);
    CHECK_EQ(c.sys.ctx.ignition_on, false);
    CHECK_EQ(c.sys.out.headlight_on, false);
    CHECK_EQ(c.sys.out.wiper, WIPER_STOP);
    CHECK_EQ(c.sys.out.door_locked, false);
    CHECK_EQ(c.sys.out.warning, WARN_NONE);
}

/* 사양서 3.1: 값이 그대로여도 계속 보내야 함 → wait 동안 수신 중단·고장이 생기지 않아야 한다 */
TEST(wait_keeps_resending_periodic_signals)
{
    Console c;
    setup(&c);
    CHECK_EQ(console_exec(&c, "ign on"), CONSOLE_OK);
    CHECK_EQ(console_exec(&c, "wait 10"), CONSOLE_OK);
    CHECK_EQ(c.now, 10000);
    CHECK_EQ(c.sys.ctx.rx_lost, false);
    CHECK_EQ(c.sys.ctx.ignition_on, true);
    CHECK_EQ(c.sys.ctx.lux_fault, false);
    CHECK_EQ(c.sys.ctx.rain_fault, false);
    CHECK_EQ(c.sys.in.speed.last_rx_ms, 10000);
}

/* 값을 바꾸면 그 시각에 바로 보낸다 (2.0초에 조도 변경 → 조건 시작 2.0초) */
TEST(value_change_is_sent_at_current_time)
{
    Console c;
    setup(&c);
    console_exec(&c, "wait 2");
    console_exec(&c, "lux 300");
    CHECK_EQ(c.sys.in.lux.value, 300);
    CHECK_EQ(c.sys.in.lux.last_rx_ms, 2000);
    CHECK_EQ(c.sys.ctx.lux, 300);
}

TEST(cut_and_resume_drive_c4)
{
    Console c;
    setup(&c);
    console_exec(&c, "ign on");
    console_exec(&c, "wait 1");
    console_exec(&c, "cut gear"); /* 마지막 기어 수신 1.000초 */
    console_exec(&c, "wait 0.45");
    CHECK_EQ(c.sys.ctx.rx_lost, false);
    console_exec(&c, "wait 0.05");
    CHECK_EQ(c.sys.ctx.rx_lost, true);
    CHECK_EQ(c.sys.ctx.ignition_on, false);
    console_exec(&c, "resume gear");
    CHECK_EQ(c.now, 1500);
    CHECK_EQ(c.sys.ctx.rx_lost, false);
    CHECK_EQ(c.sys.ctx.ignition_on, true);
}

TEST(cut_lux_drives_c2_and_value_kept_for_resume)
{
    Console c;
    setup(&c);
    console_exec(&c, "cut lux");
    console_exec(&c, "lux 800"); /* 송신 중단 중에도 값은 바꿔 둔다 */
    console_exec(&c, "wait 0.5");
    CHECK_EQ(c.sys.ctx.lux_fault, true);
    console_exec(&c, "resume lux");
    CHECK_EQ(c.sys.ctx.lux_fault, false);
    CHECK_EQ(c.sys.ctx.lux, 800);
}

TEST(out_of_range_values_are_sent_as_is)
{
    Console c;
    setup(&c);
    console_exec(&c, "lux 70000");
    CHECK_EQ(c.sys.ctx.lux_fault, true);
    console_exec(&c, "rain -1");
    CHECK_EQ(c.sys.ctx.rain_fault, true);
    console_exec(&c, "ign on");
    console_exec(&c, "speed 300");
    CHECK_EQ(c.sys.ctx.speed_valid, false);
    console_exec(&c, "speed 250");
    CHECK_EQ(c.sys.ctx.speed_valid, true);
}

TEST(driver_controls)
{
    Console c;
    setup(&c);
    CHECK_EQ(console_exec(&c, "gear d"), CONSOLE_OK);
    CHECK_EQ(c.sys.ctx.gear, GEAR_D);
    CHECK_EQ(console_exec(&c, "door rr open"), CONSOLE_OK);
    CHECK_EQ(c.sys.ctx.door_open[DOOR_RR], true);
    CHECK_EQ(c.sys.ctx.any_door_open, true);
    CHECK_EQ(console_exec(&c, "door all close"), CONSOLE_OK);
    CHECK_EQ(c.sys.ctx.any_door_open, false);
    CHECK_EQ(console_exec(&c, "LIGHT Auto"), CONSOLE_OK); /* 대소문자 무관 */
    CHECK_EQ(c.sys.ctx.light_sw, LIGHT_SW_AUTO);
    CHECK_EQ(console_exec(&c, "wiper high"), CONSOLE_OK);
    CHECK_EQ(c.sys.ctx.wiper_sw, WIPER_SW_HIGH);
}

TEST(invalid_commands_are_rejected_without_side_effects)
{
    Console c;
    setup(&c);
    CHECK_EQ(console_exec(&c, "wait 0.03"), CONSOLE_ERROR); /* 0.05초 단위 아님 */
    CHECK_EQ(console_exec(&c, "wait -1"), CONSOLE_ERROR);
    CHECK_EQ(console_exec(&c, "wait abc"), CONSOLE_ERROR);
    CHECK_EQ(c.now, 0);
    CHECK_EQ(console_exec(&c, "speed fast"), CONSOLE_ERROR);
    CHECK_EQ(c.panel.speed, 0);
    CHECK_EQ(console_exec(&c, "gear X"), CONSOLE_ERROR);
    CHECK_EQ(console_exec(&c, "door xx open"), CONSOLE_ERROR);
    CHECK_EQ(console_exec(&c, "cut nothing"), CONSOLE_ERROR);
    CHECK_EQ(console_exec(&c, "foo"), CONSOLE_ERROR);
    CHECK_EQ(console_exec(&c, "ign on off extra"), CONSOLE_ERROR);
    CHECK_EQ(console_exec(&c, ""), CONSOLE_OK);
    CHECK_EQ(console_exec(&c, "   # 주석만"), CONSOLE_OK);
    CHECK_EQ(console_exec(&c, "quit"), CONSOLE_QUIT);
}

TEST(expect_counts_mismatches)
{
    Console c;
    setup(&c);
    /* 시동 OFF이므로 구현과 관계없이 L1로 소등 */
    CHECK_EQ(console_exec(&c, "expect headlight off"), CONSOLE_OK);
    CHECK_EQ(c.expect_failures, 0);
    CHECK_EQ(console_exec(&c, "expect headlight on"), CONSOLE_OK);
    CHECK_EQ(c.expect_failures, 1);
    CHECK_EQ(console_exec(&c, "expect warning none"), CONSOLE_OK);
    CHECK_EQ(console_exec(&c, "expect rxlost off"), CONSOLE_OK);
    CHECK_EQ(c.expect_failures, 1);
    CHECK_EQ(console_exec(&c, "expect wiper fast"), CONSOLE_ERROR);
    CHECK_EQ(console_exec(&c, "expect nothing on"), CONSOLE_ERROR);
}

TEST(reset_returns_to_c5)
{
    Console c;
    setup(&c);
    console_exec(&c, "ign on");
    console_exec(&c, "door fl open");
    console_exec(&c, "wait 3");
    console_exec(&c, "reset");
    CHECK_EQ(c.now, 0);
    CHECK_EQ(c.panel.ignition, false);
    CHECK_EQ(c.sys.ctx.any_door_open, false);
    CHECK_EQ(c.sys.out.door_locked, false);
}

int main(void)
{
    printf("== F 터미널 입력\n");
    RUN(init_is_c5_and_signals_sent_at_0);
    RUN(wait_keeps_resending_periodic_signals);
    RUN(value_change_is_sent_at_current_time);
    RUN(cut_and_resume_drive_c4);
    RUN(cut_lux_drives_c2_and_value_kept_for_resume);
    RUN(out_of_range_values_are_sent_as_is);
    RUN(driver_controls);
    RUN(invalid_commands_are_rejected_without_side_effects);
    RUN(expect_counts_mismatches);
    RUN(reset_returns_to_c5);
    return test_summary();
}
