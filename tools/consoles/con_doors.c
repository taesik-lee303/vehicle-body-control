/*
 * con_doors.c - 도어·잠금(D1~D3) 대화형 테스트 콘솔. 실행: make con-doors
 * doors_step()만 호출한다.
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
        .about = "D1 수신 중단이면 그대로 → D2 시동 OFF 또는 기어 P면 해제 → D3 유효한 속도 15 km/h 이상이면 잠금. "
                 "예) 시동 ON, 기어 D, 속도 14 → 15 / 잠긴 뒤 속도·기어·시동 송신 끊고 t 0.5",
        .inputs = IN_IGN | IN_GEAR | IN_SPEED | IN_CUT_VEHICLE,
        .outputs = OUT_LOCK,
        .mod_init = mod_init,
        .mod_step = mod_step,
    };
    return modcon_run(&m);
}
