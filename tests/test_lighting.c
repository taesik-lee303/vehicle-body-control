/*
 * test_lighting.c - 조명 L1~L6
 * 작성: B (김성진)
 *
 * L1·L2·L3·L4·L5는 Sim으로 전체 시스템을 돌린다.
 * L6는 와이퍼 모듈이 구현되기 전에도 확인할 수 있도록 lighting_step에 와이퍼 단계를 직접 넣는다.
 * 시각은 sim 주기(50ms)의 배수이며, 조건이 성립한 첫 주기의 시각부터 경과를 센다.
 */

#include "sim.h"
#include "test_util.h"

/* ---- L1 시동 꺼짐 → 소등 ---- */

TEST(l1_ignition_off_headlight_off)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_ON);
    s.feed.ignition = false;
    sim_run_until(&s, 1000);
    CHECK_EQ(s.sys.out.headlight_on, false);
    s.feed.ignition = true;
    sim_run_until(&s, 1050);
    CHECK_EQ(s.sys.out.headlight_on, true);
    s.feed.ignition = false;
    sim_run_until(&s, 1100);
    CHECK_EQ(s.sys.out.headlight_on, false);
}

TEST(l1_beats_l5_and_l2)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
    s.feed.ignition = false;
    s.feed.lux = 60001; /* 조도 센서 고장: L5는 켜라고 하지만 L1이 우선 */
    sim_run_until(&s, 500);
    CHECK_EQ(s.sys.out.headlight_on, false);

    s.feed.lux = 300; /* 어두움 1초 이상: L2도 L1에 막힘 */
    sim_run_until(&s, 5000);
    CHECK_EQ(s.sys.out.headlight_on, false);
}

TEST(l1_rx_lost_counts_as_ignition_off)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_ON);
    sim_run_until(&s, 1000);
    CHECK_EQ(s.sys.out.headlight_on, true);

    s.feed.mute = SIM_GEAR; /* 기어만 끊김. 마지막 수신 1000ms */
    sim_run_until(&s, 1450);
    CHECK_EQ(s.sys.out.headlight_on, true); /* 450ms: 아직 정상 */
    sim_run_until(&s, 1500);
    CHECK_EQ(s.sys.out.headlight_on, false); /* 500ms: 수신 중단 → 시동 꺼짐으로 봄 (C4) */

    s.feed.mute = 0;
    sim_run_until(&s, 1550);
    CHECK_EQ(s.sys.out.headlight_on, true);
}

TEST(l1_ignition_cycle_clears_previous_state)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
    s.feed.lux = 300;
    sim_run_until(&s, 1000);
    CHECK_EQ(s.sys.out.headlight_on, true); /* L2 */

    s.feed.lux = 1200; /* 어느 규칙도 정하지 않는 조도 */
    s.feed.ignition = false;
    sim_run_until(&s, 1100);
    CHECK_EQ(s.sys.out.headlight_on, false);
    s.feed.ignition = true;
    sim_run_until(&s, 2000);
    CHECK_EQ(s.sys.out.headlight_on, false); /* 직전 상태 = 소등 */
}

/* ---- L4 스위치 ON/OFF ---- */

TEST(l4_on_stays_on_even_when_bright)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_ON);
    s.feed.lux = 30000;
    sim_run_until(&s, 10000);
    CHECK_EQ(s.sys.out.headlight_on, true);
}

TEST(l4_off_stays_off_even_when_dark)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_OFF);
    s.feed.lux = 100;
    sim_run_until(&s, 10000);
    CHECK_EQ(s.sys.out.headlight_on, false);
}

TEST(l4_switch_change_applies_next_cycle)
{
    Sim s;
    sim_init(&s);
    s.feed.lux = 30000;
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_ON);
    sim_run_until(&s, 500);
    CHECK_EQ(s.sys.out.headlight_on, true);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_OFF);
    sim_run_until(&s, 550);
    CHECK_EQ(s.sys.out.headlight_on, false);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_ON);
    sim_run_until(&s, 600);
    CHECK_EQ(s.sys.out.headlight_on, true);
}

/* ---- L5 조도 센서 고장 → 점등 + 표시 (표시는 시동과 무관) ---- */

TEST(l5_before_first_rx_counts_from_start)
{
    Sim s;
    sim_init(&s);
    s.feed.mute = SIM_LUX; /* 조도가 한 번도 안 들어옴, 스위치 OFF */
    sim_run_until(&s, 450);
    CHECK_EQ(s.sys.out.lux_fault_lamp, false);
    CHECK_EQ(s.sys.out.headlight_on, false);
    sim_run_until(&s, 500);
    CHECK_EQ(s.sys.out.lux_fault_lamp, true);
    CHECK_EQ(s.sys.out.headlight_on, true); /* L5 > L4 */
}

TEST(l5_out_of_range_is_fault_immediately)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
    sim_run_until(&s, 100);
    CHECK_EQ(s.sys.out.headlight_on, false);
    CHECK_EQ(s.sys.out.lux_fault_lamp, false);

    s.feed.lux = 60001;
    sim_run_until(&s, 150);
    CHECK_EQ(s.sys.out.lux_fault_lamp, true);
    CHECK_EQ(s.sys.out.headlight_on, true);

    s.feed.lux = -1;
    sim_run_until(&s, 200);
    CHECK_EQ(s.sys.out.lux_fault_lamp, true);
    CHECK_EQ(s.sys.out.headlight_on, true);

    s.feed.lux = 60000; /* 유효 범위 끝값: 정상 복귀 */
    sim_run_until(&s, 250);
    CHECK_EQ(s.sys.out.lux_fault_lamp, false);
    CHECK_EQ(s.sys.out.headlight_on, true); /* 직전 상태 유지 */
}

TEST(l5_beats_l4_off)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_OFF);
    s.feed.lux = 60001;
    sim_run_until(&s, 100);
    CHECK_EQ(s.sys.out.headlight_on, true);
    CHECK_EQ(s.sys.out.lux_fault_lamp, true);
}

TEST(l5_lamp_shows_even_when_ignition_off)
{
    Sim s;
    sim_init(&s);
    s.feed.ignition = false;
    s.feed.lux = 60001;
    sim_run_until(&s, 100);
    CHECK_EQ(s.sys.out.lux_fault_lamp, true);
    CHECK_EQ(s.sys.out.headlight_on, false); /* L1 > L5 */
}

TEST(l5_interruption_recovery_and_timer_restart)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
    sim_run_until(&s, 1000);
    CHECK_EQ(s.sys.out.headlight_on, false);

    s.feed.mute = SIM_LUX; /* 조도 마지막 수신 1000ms */
    sim_run_until(&s, 1450);
    CHECK_EQ(s.sys.out.lux_fault_lamp, false);
    CHECK_EQ(s.sys.out.headlight_on, false);
    sim_run_until(&s, 1500);
    CHECK_EQ(s.sys.out.lux_fault_lamp, true);
    CHECK_EQ(s.sys.out.headlight_on, true);

    s.feed.mute = 0; /* 1550ms에 정상 값(30000) 복귀 */
    sim_run_until(&s, 1550);
    CHECK_EQ(s.sys.out.lux_fault_lamp, false);
    CHECK_EQ(s.sys.out.headlight_on, true); /* 직전 상태 유지 */
    /* 고장 중에는 L3 시간을 세지 않는다 (C1): 복귀한 1550ms부터 3초 */
    sim_run_until(&s, 4500);
    CHECK_EQ(s.sys.out.headlight_on, true);
    sim_run_until(&s, 4550);
    CHECK_EQ(s.sys.out.headlight_on, false);
}

/* ---- L2 AUTO: 조도 < 1000 lux 1초 연속 → 점등 ---- */

TEST(l2_dark_1s_turns_on)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
    s.feed.lux = 3000;
    sim_run_until(&s, 1950);
    s.feed.lux = 300; /* 2.0초부터 */
    sim_run_until(&s, 2900);
    CHECK_EQ(s.sys.out.headlight_on, false);
    sim_run_until(&s, 2950);
    CHECK_EQ(s.sys.out.headlight_on, false); /* 950ms */
    sim_run_until(&s, 3000);
    CHECK_EQ(s.sys.out.headlight_on, true); /* 1000ms */
}

TEST(l2_lux_boundary_1000_is_not_dark)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
    s.feed.lux = 1000;
    sim_run_until(&s, 5000);
    CHECK_EQ(s.sys.out.headlight_on, false);
    s.feed.lux = 999;
    sim_run_until(&s, 5950);
    CHECK_EQ(s.sys.out.headlight_on, false);
    sim_run_until(&s, 6000); /* 5050부터 센다: 999lux 첫 주기 5050 */
    CHECK_EQ(s.sys.out.headlight_on, false);
    sim_run_until(&s, 6050);
    CHECK_EQ(s.sys.out.headlight_on, true);
}

TEST(l2_interruption_restarts_count)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
    s.feed.lux = 300;
    sim_run_until(&s, 900);
    CHECK_EQ(s.sys.out.headlight_on, false);
    s.feed.lux = 1200; /* 조건이 깨짐 */
    sim_run_until(&s, 1000);
    CHECK_EQ(s.sys.out.headlight_on, false);
    s.feed.lux = 300; /* 1050부터 처음부터 다시 */
    sim_run_until(&s, 2000);
    CHECK_EQ(s.sys.out.headlight_on, false);
    sim_run_until(&s, 2050);
    CHECK_EQ(s.sys.out.headlight_on, true);
}

TEST(l2_counts_only_while_switch_is_auto)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_OFF);
    s.feed.lux = 300;
    sim_run_until(&s, 5000);
    CHECK_EQ(s.sys.out.headlight_on, false);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO); /* AUTO가 된 5050부터 센다 */
    sim_run_until(&s, 6000);
    CHECK_EQ(s.sys.out.headlight_on, false);
    sim_run_until(&s, 6050);
    CHECK_EQ(s.sys.out.headlight_on, true);
}

TEST(l2_counts_only_while_ignition_on)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
    s.feed.lux = 300;
    s.feed.ignition = false;
    sim_run_until(&s, 3000);
    CHECK_EQ(s.sys.out.headlight_on, false);
    s.feed.ignition = true; /* 3050부터 센다 */
    sim_run_until(&s, 3050);
    CHECK_EQ(s.sys.out.headlight_on, false);
    sim_run_until(&s, 4000);
    CHECK_EQ(s.sys.out.headlight_on, false);
    sim_run_until(&s, 4050);
    CHECK_EQ(s.sys.out.headlight_on, true);
}

/* ---- L3 AUTO: 조도 > 1500 lux 3초 연속 → 소등, 아니면 직전 상태 유지 ---- */

TEST(l3_bright_3s_turns_off)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
    s.feed.lux = 300;
    sim_run_until(&s, 1000);
    CHECK_EQ(s.sys.out.headlight_on, true);
    s.feed.lux = 2000; /* 1050부터 */
    sim_run_until(&s, 4000);
    CHECK_EQ(s.sys.out.headlight_on, true); /* 2950ms */
    sim_run_until(&s, 4050);
    CHECK_EQ(s.sys.out.headlight_on, false); /* 3000ms */
}

TEST(l3_lux_boundary_1500_is_not_bright)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
    s.feed.lux = 300;
    sim_run_until(&s, 1000);
    CHECK_EQ(s.sys.out.headlight_on, true);
    s.feed.lux = 1500;
    sim_run_until(&s, 10000);
    CHECK_EQ(s.sys.out.headlight_on, true);
    s.feed.lux = 1501; /* 10050부터 */
    sim_run_until(&s, 13000);
    CHECK_EQ(s.sys.out.headlight_on, true);
    sim_run_until(&s, 13050);
    CHECK_EQ(s.sys.out.headlight_on, false);
}

TEST(l3_between_1000_and_1500_keeps_previous_state)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
    s.feed.lux = 1200; /* 켜진 적 없으니 꺼진 채 유지 */
    sim_run_until(&s, 10000);
    CHECK_EQ(s.sys.out.headlight_on, false);

    s.feed.lux = 300;
    sim_run_until(&s, 11050);
    CHECK_EQ(s.sys.out.headlight_on, true);
    s.feed.lux = 1200; /* 켜진 채 유지 */
    sim_run_until(&s, 20000);
    CHECK_EQ(s.sys.out.headlight_on, true);
}

TEST(l3_interruption_restarts_count)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
    s.feed.lux = 300;
    sim_run_until(&s, 1000);
    CHECK_EQ(s.sys.out.headlight_on, true);
    s.feed.lux = 2000; /* 1050부터 */
    sim_run_until(&s, 3000);
    s.feed.lux = 1400; /* 조건이 깨짐 */
    sim_run_until(&s, 3050);
    CHECK_EQ(s.sys.out.headlight_on, true);
    s.feed.lux = 2000; /* 3100부터 처음부터 다시 */
    sim_run_until(&s, 6050);
    CHECK_EQ(s.sys.out.headlight_on, true);
    sim_run_until(&s, 6100);
    CHECK_EQ(s.sys.out.headlight_on, false);
}

TEST(l3_not_applied_when_switch_is_on_or_off)
{
    Sim s;
    sim_init(&s);
    s.feed.lux = 30000;
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_ON);
    sim_run_until(&s, 5000);
    CHECK_EQ(s.sys.out.headlight_on, true);
    /* ON → AUTO: 시간은 AUTO가 된 시점(5050)부터 센다 */
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
    sim_run_until(&s, 8000);
    CHECK_EQ(s.sys.out.headlight_on, true);
    sim_run_until(&s, 8050);
    CHECK_EQ(s.sys.out.headlight_on, false);
}

/* ---- L6 AUTO: 와이퍼 10초 연속 작동 → 점등, 작동 중 유지 ----
 * 와이퍼 단계를 직접 넣어서 확인한다 (와이퍼 모듈과 독립). */

typedef struct {
    VbcInputs in;
    VbcContext ctx;
    VbcOutputs out;
    LightingState st;
    WiperLevel wiper;
    int32_t lux;
    bool ignition;
    vbc_ms_t now;
    bool started;
} LightRig;

static void rig_init(LightRig *r)
{
    vbc_inputs_init(&r->in);
    vbc_outputs_init(&r->out);
    lighting_init(&r->st);
    r->wiper = WIPER_STOP;
    r->lux = 1200; /* L2·L3 모두 정하지 않는 조도 */
    r->ignition = true;
    r->now = 0;
    r->started = false;
    vbc_set_light_switch(&r->in, LIGHT_SW_AUTO);
}

static void rig_run_until(LightRig *r, vbc_ms_t t_end)
{
    vbc_ms_t t = r->started ? r->now + 50 : 0;

    for (; t <= t_end; t += 50) {
        vbc_rx_speed(&r->in, t, 0);
        vbc_rx_gear(&r->in, t, GEAR_P);
        vbc_rx_ignition(&r->in, t, r->ignition);
        vbc_rx_lux(&r->in, t, r->lux);
        vbc_build_context(&r->in, t, &r->ctx);
        lighting_step(&r->st, &r->ctx, r->wiper, &r->out);
        r->now = t;
        r->started = true;
    }
}

TEST(l6_wiper_10s_turns_on)
{
    LightRig r;
    rig_init(&r);
    r.wiper = WIPER_LOW;
    rig_run_until(&r, 9950);
    CHECK_EQ(r.out.headlight_on, false);
    rig_run_until(&r, 10000);
    CHECK_EQ(r.out.headlight_on, true);
}

TEST(l6_every_non_stop_level_counts_as_working)
{
    WiperLevel levels[] = {WIPER_INTERMITTENT, WIPER_LOW, WIPER_HIGH};

    for (unsigned i = 0; i < sizeof(levels) / sizeof(levels[0]); i++) {
        LightRig r;
        rig_init(&r);
        r.wiper = levels[i];
        rig_run_until(&r, 9950);
        CHECK_EQ(r.out.headlight_on, false);
        rig_run_until(&r, 10000);
        CHECK_EQ(r.out.headlight_on, true);
    }
}

TEST(l6_stays_on_while_wiper_works_even_when_bright)
{
    LightRig r;
    rig_init(&r);
    r.wiper = WIPER_HIGH;
    r.lux = 30000; /* L3(3초 밝음)은 만족하지만 L6이 우선 */
    rig_run_until(&r, 9950);
    CHECK_EQ(r.out.headlight_on, false); /* 아직 L6 미성립, L3은 소등을 정함 */
    rig_run_until(&r, 10000);
    CHECK_EQ(r.out.headlight_on, true);
    rig_run_until(&r, 30000);
    CHECK_EQ(r.out.headlight_on, true);
}

TEST(l6_interruption_restarts_count)
{
    LightRig r;
    rig_init(&r);
    r.wiper = WIPER_LOW;
    rig_run_until(&r, 9900);
    r.wiper = WIPER_STOP; /* 10초가 차기 직전에 멈춤 */
    rig_run_until(&r, 9950);
    r.wiper = WIPER_LOW; /* 10000부터 처음부터 다시 */
    rig_run_until(&r, 19950);
    CHECK_EQ(r.out.headlight_on, false);
    rig_run_until(&r, 20000);
    CHECK_EQ(r.out.headlight_on, true);
}

TEST(l6_after_wiper_stops_keeps_previous_state)
{
    LightRig r;
    rig_init(&r);
    r.wiper = WIPER_LOW;
    rig_run_until(&r, 10000);
    CHECK_EQ(r.out.headlight_on, true);
    r.wiper = WIPER_STOP;
    rig_run_until(&r, 20000);
    CHECK_EQ(r.out.headlight_on, true); /* 조도 1200: 어느 규칙도 안 정함 → 직전 유지 */
}

TEST(l6_counting_continues_while_higher_rule_decides)
{
    LightRig r;
    rig_init(&r);
    vbc_set_light_switch(&r.in, LIGHT_SW_OFF); /* L4가 소등을 정하는 동안에도 L6 시간은 센다 */
    r.wiper = WIPER_LOW;
    rig_run_until(&r, 12000);
    CHECK_EQ(r.out.headlight_on, false);
    vbc_set_light_switch(&r.in, LIGHT_SW_AUTO);
    rig_run_until(&r, 12050);
    CHECK_EQ(r.out.headlight_on, true); /* 12초 작동 이력으로 즉시 점등 */
}

TEST(l6_counting_continues_during_lux_fault)
{
    LightRig r;
    rig_init(&r);
    r.wiper = WIPER_LOW;
    r.lux = 60001; /* L5가 점등을 정하는 동안 */
    rig_run_until(&r, 9950);
    CHECK_EQ(r.out.headlight_on, true);
    r.lux = 30000; /* 고장 해제 후 L3은 3초 필요하지만 L6이 10초 채움 */
    rig_run_until(&r, 10050);
    CHECK_EQ(r.out.headlight_on, true);
    rig_run_until(&r, 20000);
    CHECK_EQ(r.out.headlight_on, true);
}

TEST(l6_c1_counts_in_higher_rule_then_l3_already_satisfied)
{
    LightRig r;
    rig_init(&r);
    r.wiper = WIPER_LOW;
    r.lux = 30000; /* L3 시간도 0초부터 계속 센다 */
    rig_run_until(&r, 12000);
    CHECK_EQ(r.out.headlight_on, true);
    r.wiper = WIPER_STOP;
    rig_run_until(&r, 12050);
    CHECK_EQ(r.out.headlight_on, false); /* L6 해제 즉시, 이미 찬 L3이 소등을 정함 */
}

TEST(l6_does_not_apply_for_switch_off_or_on)
{
    LightRig r;
    rig_init(&r);
    vbc_set_light_switch(&r.in, LIGHT_SW_OFF);
    r.wiper = WIPER_HIGH;
    rig_run_until(&r, 20000);
    CHECK_EQ(r.out.headlight_on, false); /* L4 OFF */

    vbc_set_light_switch(&r.in, LIGHT_SW_ON);
    r.wiper = WIPER_STOP;
    rig_run_until(&r, 20050);
    CHECK_EQ(r.out.headlight_on, true); /* L4 ON */
}

TEST(l6_counts_only_while_ignition_on)
{
    LightRig r;
    rig_init(&r);
    r.wiper = WIPER_LOW;
    r.ignition = false;
    rig_run_until(&r, 12000);
    CHECK_EQ(r.out.headlight_on, false);
    r.ignition = true; /* 12050부터 센다 */
    rig_run_until(&r, 22000);
    CHECK_EQ(r.out.headlight_on, false);
    rig_run_until(&r, 22050);
    CHECK_EQ(r.out.headlight_on, true);
}

/* ---- 스위치 입력: OFF·AUTO·ON 외의 값은 받지 않는다 ---- */

TEST(light_switch_ignores_values_other_than_off_auto_on)
{
    Sim s;
    sim_init(&s);
    vbc_set_light_switch(&s.sys.in, LIGHT_SW_ON);
    vbc_set_light_switch(&s.sys.in, (LightSwitch)99);
    vbc_set_light_switch(&s.sys.in, (LightSwitch)-1);
    CHECK_EQ(s.sys.in.light_sw, LIGHT_SW_ON);
    sim_run_until(&s, 1000);
    CHECK_EQ(s.sys.out.headlight_on, true);
}

int main(void)
{
    printf("== L 조명\n");
    RUN(l1_ignition_off_headlight_off);
    RUN(l1_beats_l5_and_l2);
    RUN(l1_rx_lost_counts_as_ignition_off);
    RUN(l1_ignition_cycle_clears_previous_state);
    RUN(l4_on_stays_on_even_when_bright);
    RUN(l4_off_stays_off_even_when_dark);
    RUN(l4_switch_change_applies_next_cycle);
    RUN(l5_before_first_rx_counts_from_start);
    RUN(l5_out_of_range_is_fault_immediately);
    RUN(l5_beats_l4_off);
    RUN(l5_lamp_shows_even_when_ignition_off);
    RUN(l5_interruption_recovery_and_timer_restart);
    RUN(l2_dark_1s_turns_on);
    RUN(l2_lux_boundary_1000_is_not_dark);
    RUN(l2_interruption_restarts_count);
    RUN(l2_counts_only_while_switch_is_auto);
    RUN(l2_counts_only_while_ignition_on);
    RUN(l3_bright_3s_turns_off);
    RUN(l3_lux_boundary_1500_is_not_bright);
    RUN(l3_between_1000_and_1500_keeps_previous_state);
    RUN(l3_interruption_restarts_count);
    RUN(l3_not_applied_when_switch_is_on_or_off);
    RUN(l6_wiper_10s_turns_on);
    RUN(l6_every_non_stop_level_counts_as_working);
    RUN(l6_stays_on_while_wiper_works_even_when_bright);
    RUN(l6_interruption_restarts_count);
    RUN(l6_after_wiper_stops_keeps_previous_state);
    RUN(l6_counting_continues_while_higher_rule_decides);
    RUN(l6_counting_continues_during_lux_fault);
    RUN(l6_c1_counts_in_higher_rule_then_l3_already_satisfied);
    RUN(l6_does_not_apply_for_switch_off_or_on);
    RUN(l6_counts_only_while_ignition_on);
    RUN(light_switch_ignores_values_other_than_off_auto_on);
    return test_summary();
}
