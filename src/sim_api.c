/*
 * sim_api.c - 그래픽 시뮬레이션(브라우저)용 API
 * 담당: F (이태식)
 */

#include "sim_api.h"

#include <stdarg.h>

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
static char g_adas[24576];
static float g_pts[RADAR_COUNT * RADAR_MAX_DET * 6];
static int g_npts;

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

/* ---- 레이더·Stop & Go ---- */

typedef struct {
    char *p;
    size_t left;
} Buf;

static void put(Buf *b, const char *fmt, ...)
{
    va_list ap;
    int n;

    if (b->left <= 1)
        return;
    va_start(ap, fmt);
    n = vsnprintf(b->p, b->left, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if ((size_t)n >= b->left)
        n = (int)b->left - 1;
    b->p += n;
    b->left -= (size_t)n;
}

API const char *sim_adas(void)
{
    const ConsoleAdas *a = &g_con.adas;
    const StopGo *sg = &a->sg;
    const AdasMap *m = &a->map;
    Buf b = {g_adas, sizeof g_adas};

    put(&b, "{\"on\":%d", a->on);
    put(&b, ",\"sg\":{\"state\":\"%s\",\"sw\":%d,\"set\":%d,\"gap\":%d,\"gap_s\":%.1f,\"accel\":", sg_state_name(sg->state),
        sg->sw_on, sg->set_kmh, sg->gap_level, (double)sg_gap_s(sg));
    if (sg->accel_valid)
        put(&b, "%.3f", (double)sg->accel);
    else
        put(&b, "null");
    put(&b, ",\"hold\":%d,\"override\":%d,\"alert\":\"%s\",\"block\":%u}", sg->hold, sg->override, sg_alert_name(sg->alert),
        sg->block);

    put(&b, ",\"map\":{\"status\":\"%s\",\"radars\":[", map_status_name(m->status));
    for (int i = 0; i < RADAR_COUNT; i++)
        put(&b, "%s{\"name\":\"%s\",\"h\":\"%s\",\"why\":%u,\"n\":%d,\"inject\":\"%s\"}", i ? "," : "",
            RADAR_MOUNTS[i].name, radar_health_name(m->rx[i].health), m->rx[i].reason, m->n_pts[i],
            radar_fault_name(a->radar.s[i].fault));
    put(&b, "]}");

    const Track *ld = map_lead(m);
    if (ld)
        put(&b, ",\"lead\":{\"id\":%u,\"x\":%.2f,\"y\":%.2f,\"vx\":%.2f}", (unsigned)ld->id, (double)ld->x,
            (double)ld->y, (double)ld->vx);
    else
        put(&b, ",\"lead\":null");

    /* 트랙: [id, x, y, ymin, ymax, vx, vy, zlo, zhi, 표시]. 표시 1 확정 2 장애물 4 경로 내 8 정지 16 선행 32 높이 있음 64 측면 128 무시 */
    put(&b, ",\"tracks\":[");
    int nt = 0;
    for (int i = 0; i < MAP_MAX_TRACKS; i++) {
        const Track *t = &m->tracks[i];
        if (!t->used)
            continue;
        unsigned f = (t->confirmed ? 1u : 0) | (t->obstacle ? 2u : 0) | (t->in_path ? 4u : 0) | (t->stationary ? 8u : 0) |
                     (t->id == m->lead_id ? 16u : 0) | (t->has_z ? 32u : 0) | (t->corner_seen ? 64u : 0) |
                     (t->ignored ? 128u : 0);
        put(&b, "%s[%u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%u]", nt++ ? "," : "", (unsigned)t->id, (double)t->x,
            (double)t->y, (double)(t->y - t->half_r), (double)(t->y + t->half_l), (double)t->vx, (double)t->vy,
            (double)t->z_lo, (double)t->z_hi, f);
    }
    put(&b, "]");

    /* 세계 물체 (시뮬레이터 정답, 화면에 그리기용): [id, 종류, 가까운 면 x, y, 길이, 폭, z0, z1, 속도, 가로 속도] */
    put(&b, ",\"objs\":[");
    int no = 0;
    for (int i = 0; i < WORLD_MAX_OBJ; i++) {
        const WorldObj *o = &a->world.obj[i];
        if (!o->used)
            continue;
        const ObjSpec *sp = &OBJ_SPECS[o->kind];
        Box3 bx = world_box(&a->world, o);
        put(&b, "%s[%d,\"%s\",%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f]", no++ ? "," : "", o->id, sp->name,
            (double)bx.x0, o->y, (double)sp->length, (double)sp->width, (double)sp->z0, (double)sp->z1, o->v, o->vy);
    }
    put(&b, "],\"ego_s\":%.2f,\"gap\":%.2f}", a->world.ego_s, world_truth_gap(&a->world));
    return g_adas;
}

API const float *sim_points(void)
{
    const AdasMap *m = &g_con.adas.map;
    int n = 0;

    for (int s = 0; s < RADAR_COUNT; s++) {
        for (int i = 0; i < m->n_pts[s]; i++) {
            const MapPoint *p = &m->pts[s][i];
            float *o = &g_pts[n++ * 6];
            o[0] = (float)s;
            o[1] = p->x;
            o[2] = p->y;
            o[3] = p->z;
            o[4] = p->vr;
            o[5] = (float)((p->stationary ? 1 : 0) | (p->ground ? 2 : 0));
        }
    }
    g_npts = n;
    return g_pts;
}

API int sim_points_count(void)
{
    return g_npts;
}
