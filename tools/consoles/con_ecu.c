/*
 * con_ecu.c - BCU 전체(ECU) 테스트 콘솔. 실행: make con-ecu
 * SWC 4개를 모두 실행해 모듈 TC가 볼 수 없는 SWC 간 상호작용(L6, D5, C4)을 시험한다.
 */

#include <stddef.h>
#include "wipers.h"
#include "lighting.h"
#include "doors.h"
#include "dashboard.h"
#include "consoles.h"
#include "modcon.h"

static WiperState wipers;
static LightingState lighting;
static DoorState doors;
static DashboardState dashboard;

static void mod_init(ModCon *m)
{
    (void)m;
    wipers_init(&wipers);
    lighting_init(&lighting);
    doors_init(&doors);
    dashboard_init(&dashboard);
}

static void mod_step(ModCon *m)
{
    /* vbc_step()과 같은 순서: 와이퍼 → 조명(L6이 이번 주기 와이퍼 단계를 씀) → 도어 → 계기판 */
    wipers_step(&wipers, &m->ctx, &m->out);
    lighting_step(&lighting, &m->ctx, m->out.wiper, &m->out);
    doors_step(&doors, &m->ctx, &m->out);
    dashboard_step(&dashboard, &m->ctx, &m->out);
}

void con_ecu_define(ModCon *m)
{
    *m = (ModCon){
        .name = "ecu",
        .swc = SWC_LIGHTING | SWC_WIPER | SWC_DOORS | SWC_DASHBOARD,
        .title = "BCU 전체 (ECU)",
        .about = "입력 → VBC 상위 제어기(C2~C4 판정) → SWC 4개(와이퍼 → 조명 → 도어 → 계기판, vbc_step과 같은 순서) → 출력. 모듈 TC가 볼 수 없는 SWC 간 상호작용을 시험한다",
        .show = SIG_IGN | SIG_GEAR | SIG_SPEED | SIG_LUX | SIG_RAIN | SIG_DOORS | SIG_LIGHT_SW | SIG_WIPER_SW,
        .outputs = OUT_HEADLIGHT | OUT_WIPER | OUT_LOCK | OUT_WARNING | OUT_LUX_LAMP | OUT_RAIN_LAMP,
        .mod_init = mod_init,
        .mod_step = mod_step,
        .start_ign = 0,
        .start_gear = GEAR_P,
        .state_cmd = NULL,
    };
}

#ifndef MODCON_NO_MAIN
int main(void)
{
    ModCon m;
    con_ecu_define(&m);
    return modcon_run(&m);
}
#endif
