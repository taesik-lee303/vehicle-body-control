/*
 * con_lighting.c - 조명(L1~L6) 테스트 콘솔. 실행: make con-lighting
 * lighting_step()만 호출한다. 와이퍼 모듈은 실행하지 않고, L6에 넘길 와이퍼 단계(level)를 직접 넣는다.
 */

#include "lighting.h"
#include "modcon.h"

static LightingState st;

static void mod_init(ModCon *m) { (void)m; lighting_init(&st); }
static void mod_step(ModCon *m) { lighting_step(&st, &m->ctx, (WiperLevel)m->wiper_level, &m->out); }

int main(void)
{
    ModCon m = {
        .title = "조명 (L1~L6)",
        .about = "우선순위 L1 시동 OFF 소등 > L5 조도 고장 점등·표시 > L4 스위치 ON/OFF > L6 와이퍼 10초 작동 > "
                 "L2 1000 lux 미만 1초 점등 · L3 1500 lux 초과 3초 소등(그 사이는 직전 유지)",
        .show = SIG_IGN | SIG_LUX | SIG_LIGHT_SW | SIG_WIPER_LEVEL | SIG_GEAR | SIG_SPEED,
        .outputs = OUT_HEADLIGHT | OUT_LUX_LAMP,
        .mod_init = mod_init,
        .mod_step = mod_step,
        .start_ign = 0,
        .start_gear = GEAR_P,
    };
    return modcon_run(&m);
}
