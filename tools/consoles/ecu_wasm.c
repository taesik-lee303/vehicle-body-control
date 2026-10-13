/*
 * ecu_wasm.c - ECU 시뮬레이터(브라우저)용 WASM API (C1~C5)
 * 콘솔 5개 중 하나를 골라 modcon 엔진을 그대로 부른다. 판정은 모두 C 코드가 한다.
 */

#include <string.h>

#include "consoles.h"
#include "modcon.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define API EMSCRIPTEN_KEEPALIVE
#else
#define API
#endif

static ModCon g;

static const struct {
    const char *name;
    void (*define)(ModCon *m);
} CONSOLES[] = {
    {"ecu", con_ecu_define},
    {"doors", con_doors_define},
    {"dashboard", con_dashboard_define},
    {"lighting", con_lighting_define},
    {"wipers", con_wipers_define},
};

/* 콘솔을 고르고 처음 상태로 둔다. 0 정상, 1 없는 이름 */
API int ecu_select(const char *name)
{
    for (size_t i = 0; i < sizeof CONSOLES / sizeof CONSOLES[0]; i++)
        if (strcmp(name, CONSOLES[i].name) == 0) {
            CONSOLES[i].define(&g);
            g.interactive = false; /* 인자 없는 명령은 되묻지 않고 오류 1 */
            modcon_start(&g);
            return 0;
        }
    return 1;
}

API int ecu_exec(const char *line) { return modcon_exec(&g, line); } /* 0 정상, 1 오류, 2 quit */

API int ecu_advance(unsigned ms) { return modcon_advance(&g, (long)ms); }

API const char *ecu_state(void) { return modcon_state_json(&g); }

API int ecu_finish(void) { return modcon_finish(&g); }
