/*
 * con_wipers.c - 와이퍼(W1~W5) 테스트 콘솔. 실행: make con-wipers
 * wipers_step()만 호출한다. 기어 D로 시작한다 (유효한 속도 C3에는 세 신호가 모두 필요).
 */

#include "consoles.h"
#include "modcon.h"
#include "wipers.h"

static WiperState st;

static void mod_init(ModCon *m) { (void)m; wipers_init(&st); }
static void mod_step(ModCon *m) { wipers_step(&st, &m->ctx, &m->out); }

void con_wipers_define(ModCon *m)
{
    *m = (ModCon){
        .name = "wipers",
        .swc = SWC_WIPER,
        .title = "와이퍼 (W1~W5)",
        .about = "우선순위 W1 시동 OFF 정지 > W4 스위치 OFF/LOW/HIGH > W5 AUTO+강우 고장 간헐 > "
                 "W2 AUTO 강우량(<10 정지, <40 간헐, <70 저속, 70 이상 고속) · W3 유효한 속도 0이면 한 단계 낮춤",
        .show = SIG_IGN | SIG_SPEED | SIG_RAIN | SIG_WIPER_SW | SIG_GEAR,
        .outputs = OUT_WIPER | OUT_RAIN_LAMP,
        .mod_init = mod_init,
        .mod_step = mod_step,
        .start_ign = 0,
        .start_gear = GEAR_D,
    };
}

#ifndef MODCON_NO_MAIN
int main(void)
{
    ModCon m;
    con_wipers_define(&m);
    return modcon_run(&m);
}
#endif
