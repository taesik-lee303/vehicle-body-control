/*
 * main.c - 임시 데모
 * 담당: F (이태식)
 *
 * 입력 방법과 화면은 feature/input에서 교체한다.
 * 지금은 정해진 시나리오를 돌려 0.5초마다 출력을 찍는다.
 */

#include <stdio.h>

#include "vbc.h"

#define PERIOD_MS 50u

static void print_header(void)
{
    printf("%6s | %5s %4s %6s %5s | %4s %4s %4s %4s | %-17s\n",
           "t(ms)", "speed", "gear", "lux", "rain",
           "head", "wipe", "lock", "C4", "warning");
}

static void print_row(const VbcSystem *s, vbc_ms_t now)
{
    printf("%6u | %5d %4s %6d %5d | %4s %4s %4s %4s | %-17s\n",
           (unsigned)now, (int)s->in.speed.value, vbc_gear_name(s->ctx.gear),
           (int)s->in.lux.value, (int)s->in.rain.value,
           s->out.headlight_on ? "ON" : "off", vbc_wiper_name(s->out.wiper),
           s->out.door_locked ? "잠김" : "풀림", s->ctx.rx_lost ? "중단" : "-",
           vbc_warning_name(s->out.warning));
}

int main(void)
{
    VbcSystem sys;
    vbc_init(&sys);
    vbc_set_light_switch(&sys.in, LIGHT_SW_AUTO);
    vbc_set_wiper_switch(&sys.in, WIPER_SW_AUTO);

    print_header();
    for (vbc_ms_t t = 0; t <= 6000; t += PERIOD_MS) {
        int32_t lux = t < 2000 ? 3000 : 300;  /* 2초에 어두워짐 */
        int32_t speed = t < 3000 ? 0 : 30;    /* 3초에 출발 */
        Gear gear = t < 2500 ? GEAR_P : GEAR_D;

        vbc_rx_ignition(&sys.in, t, true);
        vbc_rx_gear(&sys.in, t, gear);
        vbc_rx_speed(&sys.in, t, speed);
        vbc_rx_lux(&sys.in, t, lux);
        vbc_rx_rain(&sys.in, t, 50);
        vbc_set_door(&sys.in, DOOR_FL, t >= 4000 && t < 5000);

        vbc_step(&sys, t);
        if (t % 500 == 0)
            print_row(&sys, t);
    }
    return 0;
}
