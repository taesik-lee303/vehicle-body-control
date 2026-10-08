/*
 * con_doors.c - 도어·잠금(D1~D3) 테스트 콘솔. 실행: make con-doors
 * doors_step()만 호출한다. 입력은 걸러지지 않고 코드 그대로 들어간다 (modcon.h).
 */

#include "doors.h"
#include "modcon.h"

static DoorState st;

static void mod_init(ModCon *m) { (void)m; doors_init(&st); }
static void mod_step(ModCon *m) { doors_step(&st, &m->ctx, &m->out); }

int main(void)
{
    ModCon m = {
        .title = "도어·잠금 (D1~D3)",
        .about = "D1 수신 중단이면 그대로 → D2 시동 OFF 또는 기어 P면 해제 → D3 유효한 속도 15 km/h 이상이면 잠금 → 그 외 직전 유지",
        .show = SIG_IGN | SIG_GEAR | SIG_SPEED,
        .outputs = OUT_LOCK,
        .mod_init = mod_init,
        .mod_step = mod_step,
        .start_ign = 0,
        .start_gear = GEAR_P,
    };
    return modcon_run(&m);
}
