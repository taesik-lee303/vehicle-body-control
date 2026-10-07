/*
 * con_dashboard.c - 계기판 경고(D4·D5) 대화형 테스트 콘솔. 실행: make con-dashboard
 * dashboard_step()만 호출한다. 시동·기어는 ON·D로 계속 보내진다 (유효한 속도 판정용, C3).
 */

#include "dashboard.h"
#include "modcon.h"

static DashboardState st;

static void mod_init(ModCon *m) { (void)m; dashboard_init(&st); }
static void mod_step(ModCon *m) { dashboard_step(&st, &m->ctx, &m->out); }

int main(void)
{
    ModCon m = {
        .title = "계기판 경고 (D4·D5)",
        .about = "D4 유효한 속도 > 0 이고 문 하나라도 열림 → 주행 중 도어 열림. D5 경고는 하나만: 도어 > 조도 고장 > 강우 고장. "
                 "예) 속도 0 → 1 에서 문 열기 / 조도 70000, 강우 101 넣고 하나씩 되돌리기",
        .inputs = IN_SPEED | IN_DOORS | IN_LUX | IN_RAIN | IN_CUT_VEHICLE | IN_CUT_LUX | IN_CUT_RAIN,
        .outputs = OUT_WARNING,
        .mod_init = mod_init,
        .mod_step = mod_step,
    };
    m.ign = true;   /* 계기판 판단에는 시동·기어가 쓰이지 않지만, 유효한 속도(C3)는 세 신호가 모두 들어와야 한다 */
    m.gear = GEAR_D;
    return modcon_run(&m);
}
