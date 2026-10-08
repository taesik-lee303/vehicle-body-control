/*
 * con_doors.c - 도어·잠금(D1~D3) 테스트 콘솔. 실행: make con-doors
 * doors_step()만 호출한다. 입력은 걸러지지 않고 코드 그대로 들어간다 (modcon.h).
 * lock on|off: 직전 잠금 상태를 바로 넣는다 (잠금은 사양서 입력이 아니라 모듈 상태라 따로 둔다).
 */

#include "doors.h"
#include "modcon.h"

static DoorState st;

static void mod_init(ModCon *m) { (void)m; doors_init(&st); }
static void mod_step(ModCon *m) { doors_step(&st, &m->ctx, &m->out); }
static bool get_lock(ModCon *m) { (void)m; return st.locked; }
static void set_lock(ModCon *m, bool v) { st.locked = v; m->out.door_locked = v; }

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
        .state_cmd = "lock",
        .state_label = "직전 잠금 상태",
        .state_words = {"풀림", "잠김"},
        .mod_get_state = get_lock,
        .mod_set_state = set_lock,
    };
    return modcon_run(&m);
}
