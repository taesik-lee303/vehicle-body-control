/*
 * sim_api.c - 그래픽 시뮬레이션(브라우저)용 API
 * 담당: F (이태식)
 */

#include "sim_api.h"

#include "console.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define API EMSCRIPTEN_KEEPALIVE
#else
#define API
#endif

static Console g_con;
static unsigned g_carry_ms; /* 50ms에 못 미쳐 남은 시간 */
static char g_json[1024];

static const char *const LIGHT_SW[] = {"OFF", "AUTO", "ON"};
static const char *const WIPER_SW[] = {"OFF", "AUTO", "LOW", "HIGH"};
static const char *const WIPER_LV[] = {"stop", "int", "low", "high"};
static const char *const WARN[] = {"none", "door", "lux", "rain"};

API void sim_init(int quiet)
{
    console_init(&g_con, quiet ? NULL : stdout);
    g_carry_ms = 0;
}

API int sim_exec(const char *line)
{
    int r = (int)console_exec(&g_con, line);

    if (g_con.out)
        fflush(g_con.out);
    return r;
}

API void sim_advance(unsigned ms)
{
    unsigned total = g_carry_ms + ms;
    unsigned run = total - total % CONSOLE_TICK_MS;

    g_carry_ms = total - run;
    if (run)
        console_advance(&g_con, run);
    if (g_con.out)
        fflush(g_con.out);
}

API const char *sim_state(void)
{
    const Console *c = &g_con;
    const Panel *p = &c->panel;
    const VbcContext *x = &c->sys.ctx;
    const VbcOutputs *o = &c->sys.out;
    const bool *d = c->sys.in.door_open;

    snprintf(g_json, sizeof g_json,
             "{\"t\":%u,"
             "\"in\":{\"ign\":%d,\"gear\":\"%s\",\"speed\":%d,\"speed_mkmh\":%d,\"lux\":%d,\"rain\":%d,"
             "\"doors\":[%d,%d,%d,%d],\"light\":\"%s\",\"wiper\":\"%s\",\"cut\":%u,\"drive\":%d,"
             "\"throttle\":%d,\"brake\":%d},"
             "\"ctx\":{\"rx_lost\":%d,\"ign\":%d,\"speed_valid\":%d,\"lux_fault\":%d,\"rain_fault\":%d},"
             "\"out\":{\"headlight\":%d,\"lux_lamp\":%d,\"wiper\":\"%s\",\"rain_lamp\":%d,"
             "\"locked\":%d,\"warning\":\"%s\"},"
             "\"expect_failures\":%d}",
             (unsigned)c->now,
             p->ignition, vbc_gear_name(p->gear), (int)p->speed,
             (int)(c->drive_on ? c->drive.speed_mkmh : p->speed * 1000), (int)p->lux, (int)p->rain,
             d[DOOR_FL], d[DOOR_FR], d[DOOR_RL], d[DOOR_RR],
             LIGHT_SW[c->sys.in.light_sw], WIPER_SW[c->sys.in.wiper_sw], p->cut, c->drive_on,
             (int)c->drive.throttle, c->drive.brake,
             x->rx_lost, x->ignition_on, x->speed_valid, x->lux_fault, x->rain_fault,
             o->headlight_on, o->lux_fault_lamp, WIPER_LV[o->wiper], o->rain_fault_lamp,
             o->door_locked, WARN[o->warning],
             c->expect_failures);
    return g_json;
}
