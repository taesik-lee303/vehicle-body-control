/*
 * con_wipers.c - 와이퍼(W1~W5) 대화형 테스트 콘솔. 실행: make con-wipers
 * wipers_step()만 호출한다.
 */

#include "modcon.h"
#include "wipers.h"

static WiperState st;

static void mod_init(ModCon *m) { (void)m; wipers_init(&st); }
static void mod_step(ModCon *m) { wipers_step(&st, &m->ctx, &m->out); }

int main(void)
{
    ModCon m = {
        .title = "와이퍼 (W1~W5)",
        .about = "우선순위 W1 시동 OFF 정지 > W4 스위치 OFF/LOW/HIGH > W5 AUTO+강우 고장 간헐 > "
                 "W2 AUTO 강우량(<10 정지, <40 간헐, <70 저속, 70 이상 고속) · W3 유효한 속도 0이면 한 단계 낮춤. "
                 "예) 시동 ON, 스위치 AUTO, 속도 30, 강우 9 → 10 → 40 → 70, 그다음 속도 0",
        .inputs = IN_IGN | IN_WIPER_SW | IN_WIPER_SW_CODE | IN_RAIN | IN_SPEED | IN_CUT_RAIN | IN_CUT_VEHICLE,
        .outputs = OUT_WIPER | OUT_RAIN_LAMP,
        .mod_init = mod_init,
        .mod_step = mod_step,
    };
    m.gear = GEAR_D;   /* 와이퍼 판단에 기어는 쓰이지 않음. 유효한 속도(C3)를 위해 계속 보낸다 */
    return modcon_run(&m);
}
