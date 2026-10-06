/*
 * sim.h - 시간 진행 시뮬레이터 (테스트용)
 *
 * 속도·기어·시동·조도·강우량을 period_ms(기본 50ms)마다 자동으로 넣고 vbc_step()을 부른다.
 * 사양서 전제(0.5초보다 짧은 간격으로 계속 입력)를 테스트마다 손으로 쓰지 않아도 된다.
 *
 *   Sim s;
 *   sim_init(&s);                          // 시동 ON, P, 속도 0, 조도 30000, 강우 0
 *   vbc_set_light_switch(&s.sys.in, LIGHT_SW_AUTO);
 *   s.feed.lux = 3000;
 *   sim_run_until(&s, 1950);               // 0 ~ 1.95초: 조도 3000
 *   s.feed.lux = 300;                      // 2.0초부터 조도 300
 *   sim_run_until(&s, 2900);
 *   CHECK_EQ(s.sys.out.headlight_on, false); // 2.9초: 아직 꺼짐
 *   sim_run_until(&s, 3000);
 *   CHECK_EQ(s.sys.out.headlight_on, true);  // 3.0초: 점등 (L2)
 *
 * 고장·끊김 시험: s.feed.mute 에 SIM_LUX 등을 넣으면 그 입력만 보내지 않는다.
 * 시각은 period_ms의 배수로 지정한다.
 */

#ifndef SIM_H
#define SIM_H

#include "vbc.h"

enum {
    SIM_SPEED = 1 << 0,
    SIM_GEAR = 1 << 1,
    SIM_IGNITION = 1 << 2,
    SIM_LUX = 1 << 3,
    SIM_RAIN = 1 << 4,
    SIM_VEHICLE = SIM_SPEED | SIM_GEAR | SIM_IGNITION,
    SIM_ALL = SIM_VEHICLE | SIM_LUX | SIM_RAIN
};

typedef struct {
    int32_t speed;
    Gear gear;
    bool ignition;
    int32_t lux;
    int32_t rain;
    unsigned mute; /* 보내지 않을 입력 (SIM_* 조합) */
} SimFeed;

typedef struct {
    VbcSystem sys;
    SimFeed feed;
    vbc_ms_t now;       /* 마지막으로 실행한 시각 */
    vbc_ms_t period_ms;
    bool started;
} Sim;

static inline void sim_init(Sim *s)
{
    vbc_init(&s->sys);
    s->feed.speed = 0;
    s->feed.gear = GEAR_P;
    s->feed.ignition = true;
    s->feed.lux = 30000;
    s->feed.rain = 0;
    s->feed.mute = 0;
    s->now = 0;
    s->period_ms = 50;
    s->started = false;
}

/* t 시각에 (mute 아닌) 주기 입력을 보내고 한 주기 실행 */
static inline void sim_tick_at(Sim *s, vbc_ms_t t)
{
    VbcInputs *in = &s->sys.in;
    unsigned m = s->feed.mute;

    if (!(m & SIM_SPEED)) vbc_rx_speed(in, t, s->feed.speed);
    if (!(m & SIM_GEAR)) vbc_rx_gear(in, t, s->feed.gear);
    if (!(m & SIM_IGNITION)) vbc_rx_ignition(in, t, s->feed.ignition);
    if (!(m & SIM_LUX)) vbc_rx_lux(in, t, s->feed.lux);
    if (!(m & SIM_RAIN)) vbc_rx_rain(in, t, s->feed.rain);
    vbc_step(&s->sys, t);
    s->now = t;
}

/* 다음 주기부터 t_end(포함)까지 period_ms 간격으로 실행. 처음 호출이면 0부터 */
static inline void sim_run_until(Sim *s, vbc_ms_t t_end)
{
    vbc_ms_t t = s->started ? s->now + s->period_ms : 0;

    for (; t <= t_end; t += s->period_ms) {
        sim_tick_at(s, t);
        s->started = true;
    }
}

#endif /* SIM_H */
