/*
 * con_lighting.c - 조명(L1~L6) 대화형 테스트 콘솔. 실행: make con-lighting
 * lighting_step()만 호출한다. 와이퍼 모듈은 실행하지 않고, L6에 쓰일 와이퍼 단계를 메뉴에서 직접 정한다.
 */

#include "lighting.h"
#include "modcon.h"

static LightingState st;

static void mod_init(ModCon *m) { (void)m; lighting_init(&st); }
static void mod_step(ModCon *m) { lighting_step(&st, &m->ctx, m->wiper_level, &m->out); }

int main(void)
{
    ModCon m = {
        .title = "조명 (L1~L6)",
        .about = "우선순위 L1 시동 OFF 소등 > L5 조도 고장 점등·표시 > L4 스위치 ON/OFF > L6 와이퍼 10초 작동 > "
                 "L2 1000 lux 미만 1초 점등 · L3 1500 lux 초과 3초 소등(그 사이는 직전 유지). "
                 "예) 시동 ON, 스위치 AUTO, 조도 300, t 0.95 → t 0.05",
        .inputs = IN_IGN | IN_LIGHT_SW | IN_LIGHT_SW_CODE | IN_LUX | IN_WIPER_LEVEL | IN_CUT_LUX | IN_CUT_VEHICLE,
        .outputs = OUT_HEADLIGHT | OUT_LUX_LAMP,
        .mod_init = mod_init,
        .mod_step = mod_step,
    };
    return modcon_run(&m);
}
