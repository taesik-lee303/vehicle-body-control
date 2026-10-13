/*
 * con_dashboard.c - 계기판 경고(D4·D5) 테스트 콘솔. 실행: make con-dashboard
 * dashboard_step()만 호출한다. 시동 ON·기어 D로 시작한다 (유효한 속도 C3에는 세 신호가 모두 필요).
 */

#include "dashboard.h"
#include "consoles.h"
#include "modcon.h"

static DashboardState st;

static void mod_init(ModCon *m) { (void)m; dashboard_init(&st); }
static void mod_step(ModCon *m) { dashboard_step(&st, &m->ctx, &m->out); }

void con_dashboard_define(ModCon *m)
{
    *m = (ModCon){
        .name = "dashboard",
        .swc = SWC_DASHBOARD,
        .title = "계기판 경고 (D4·D5)",
        .about = "D4 유효한 속도 > 0 이고 문 하나라도 열림 → 주행 중 도어 열림. D5 경고는 하나만: 도어 > 조도 고장 > 강우 고장",
        .show = SIG_SPEED | SIG_LUX | SIG_RAIN | SIG_DOORS | SIG_IGN | SIG_GEAR,
        .outputs = OUT_WARNING,
        .mod_init = mod_init,
        .mod_step = mod_step,
        .start_ign = 1,
        .start_gear = GEAR_D,
    };
}

#ifndef MODCON_NO_MAIN
int main(void)
{
    ModCon m;
    con_dashboard_define(&m);
    return modcon_run(&m);
}
#endif
