/*
 * env_radar.c - 레이더 센서 모델 (요구사항 XH1~XH8)
 *
 * 한 프레임을 만드는 순서
 *   1. 물체마다 센서 쪽 면에서 산란점을 뽑는다 (XH2)
 *   2. 물체마다 검출 확률을 굴리고, 검출되면 산란점마다 0.7 확률로 남긴다 (XH3)
 *   3. 시야·거리 밖, 다른 물체에 가린 점을 뺀다 (XH1, XH4)
 *   4. 가드레일·노면 클러터를 더한다 (XH7)
 *   5. 잡음을 더하고, 분해능 칸이 같은 점은 하나로 합친다 (XH5)
 */

#include "env_radar.h"

#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846
#define DEG (PI / 180.0)
#define MAX_SCATTER 64     /* 물체당 센서당 산란점 (XH2) */
#define POINT_KEEP 0.7     /* 검출된 물체의 산란점이 남을 확률 (XH3) */
#define MIN_SPACING 0.4    /* 산란점 최소 간격 m (XH2) */
#define RAIL_STEP 1.0      /* 가드레일 점 간격 m (XH7) */
#define RAIL_Z 0.62
#define RAIL_RCS (-10.0)
#define GROUND_RCS (-15.0)

typedef struct {
    double x, y, z;
} V3;

/* ---- 의사난수 (XH6) ---- */

static double uniform(uint64_t *s)
{
    *s ^= *s >> 12;
    *s ^= *s << 25;
    *s ^= *s >> 27;
    return (double)((*s * 2685821657736338717ULL) >> 11) / 9007199254740992.0;
}

static double gauss(uint64_t *s)
{
    double u1 = uniform(s), u2 = uniform(s);
    if (u1 < 1e-12)
        u1 = 1e-12;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
}

void radar_sim_init(RadarSim *rs, uint64_t seed)
{
    memset(rs, 0, sizeof *rs);
    rs->rng = seed ? seed : 0x9E3779B97F4A7C15ULL;
}

void radar_sim_set_fault(RadarSim *rs, RadarId id, RadarFaultMode mode)
{
    rs->s[id].fault = mode;
}

const char *radar_fault_name(RadarFaultMode mode)
{
    static const char *const N[RADAR_FAULT_COUNT] = {"ok", "cut", "repeat", "stuck", "block"};
    return mode < RADAR_FAULT_COUNT ? N[mode] : "?";
}

/* ---- 기하 ---- */

/* 선분 a→b가 상자를 t∈(0.001, 0.99)에서 지나면 true (XH4) */
static bool segment_hits_box(V3 a, V3 b, const Box3 *bx)
{
    double t0 = 0.001, t1 = 0.99;
    const double o[3] = {a.x, a.y, a.z}, d[3] = {b.x - a.x, b.y - a.y, b.z - a.z};
    const double lo[3] = {bx->x0, bx->y0, bx->z0}, hi[3] = {bx->x1, bx->y1, bx->z1};

    for (int k = 0; k < 3; k++) {
        if (fabs(d[k]) < 1e-12) {
            if (o[k] < lo[k] || o[k] > hi[k])
                return false;
            continue;
        }
        double ta = (lo[k] - o[k]) / d[k], tb = (hi[k] - o[k]) / d[k];
        if (ta > tb) {
            double t = ta;
            ta = tb;
            tb = t;
        }
        if (ta > t0) t0 = ta;
        if (tb < t1) t1 = tb;
        if (t0 > t1)
            return false;
    }
    return true;
}

/* 한 면의 격자 점 (u, v 두 축). 인덱스 n번째 점을 돌려준다 */
typedef struct {
    int axis;     /* 0: x가 고정, 1: y 고정, 2: z 고정 */
    double fixed; /* 고정 축의 값 */
    double u0, u1, v0, v1;
    int nu, nv;
} Face;

static int grid_n(double len, double spacing)
{
    int n = (int)ceil(len / spacing);
    return n < 1 ? 1 : n;
}

static V3 face_point(const Face *f, int i)
{
    int iu = i % f->nu, iv = i / f->nu;
    double u = f->u0 + (iu + 0.5) * (f->u1 - f->u0) / f->nu;
    double v = f->v0 + (iv + 0.5) * (f->v1 - f->v0) / f->nv;
    V3 p;
    if (f->axis == 0) { p.x = f->fixed; p.y = u; p.z = v; }
    else if (f->axis == 1) { p.x = u; p.y = f->fixed; p.z = v; }
    else { p.x = u; p.y = v; p.z = f->fixed; }
    return p;
}

/* 센서 쪽에서 보이는 면들 (XH2) */
static int visible_faces(const Box3 *b, V3 s, double spacing, Face *faces)
{
    int n = 0;
    if (s.x < b->x0 || s.x > b->x1) {
        Face f = {0, s.x < b->x0 ? b->x0 : b->x1, b->y0, b->y1, b->z0, b->z1, 0, 0};
        faces[n++] = f;
    }
    if (s.y < b->y0 || s.y > b->y1) {
        Face f = {1, s.y < b->y0 ? b->y0 : b->y1, b->x0, b->x1, b->z0, b->z1, 0, 0};
        faces[n++] = f;
    }
    if (s.z > b->z1) { /* 윗면 (맨홀 등). 아래에서 올려다보는 바닥면은 정반사로 거의 돌아오지 않아 넣지 않는다 */
        Face f = {2, b->z1, b->x0, b->x1, b->y0, b->y1, 0, 0};
        faces[n++] = f;
    }
    for (int i = 0; i < n; i++) {
        faces[i].nu = grid_n(faces[i].u1 - faces[i].u0, spacing);
        faces[i].nv = grid_n(faces[i].v1 - faces[i].v0, spacing);
    }
    return n;
}

/* ---- 측정 ---- */

typedef struct {
    double r, az, el; /* 참값 (센서 기준, 라디안) */
} Polar;

static Polar to_sensor(const RadarMount *m, V3 p)
{
    double dx = p.x - m->x, dy = p.y - m->y, dz = p.z - m->z;
    double c = cos(m->yaw_deg * DEG), sn = sin(m->yaw_deg * DEG);
    double xs = dx * c + dy * sn, ys = -dx * sn + dy * c;
    Polar q;
    q.r = sqrt(dx * dx + dy * dy + dz * dz);
    q.az = atan2(ys, xs);
    q.el = q.r > 1e-9 ? asin(dz / q.r) : 0.0;
    return q;
}

static bool in_fov(const RadarMount *m, Polar q)
{
    return q.r >= m->range_min_m && q.r <= m->range_max_m && fabs(q.az) <= m->az_fov_deg * DEG &&
           fabs(q.el) <= m->el_fov_deg * DEG;
}

/* 검출 여유 dB (XH3) */
static double margin_db(const RadarMount *m, double rcs_dbsm, double r)
{
    if (r < 0.1)
        r = 0.1;
    return rcs_dbsm - 10.0 - 40.0 * log10(r / m->r_ref_m);
}

static double pd_of(double margin)
{
    return 1.0 / (1.0 + exp(-(margin + 3.0) / 1.5));
}

typedef struct {
    RadarSim *rs;
    const RadarMount *m;
    RadarFrame *f;
    V3 s;
    const World *w;
    double ego_v;
    OBox box[WORLD_MAX_OBJ]; /* 이번 프레임의 물체 상자 (자차 기준) */
    bool used[WORLD_MAX_OBJ];
} Gen;

/* 자차 기준 점 → 상자 자기 좌표 (u 물체 앞, v 물체 왼쪽) */
static V3 to_local(const OBox *b, V3 p)
{
    double c = cos(b->yaw), sn = sin(b->yaw), dx = p.x - b->cx, dy = p.y - b->cy;
    V3 q = {dx * c + dy * sn, -dx * sn + dy * c, p.z};
    return q;
}

static V3 from_local(const OBox *b, V3 q)
{
    double c = cos(b->yaw), sn = sin(b->yaw);
    V3 p = {b->cx + q.x * c - q.y * sn, b->cy + q.x * sn + q.y * c, q.z};
    return p;
}

static Box3 local_box(const OBox *b)
{
    Box3 r = {-b->hl, b->hl, -b->hw, b->hw, b->z0, b->z1};
    return r;
}

/* 같은 분해능 칸이면 하나로 합친다 (XH5). 센 쪽을 남긴다 */
static void emit(Gen *g, V3 p, V3 vel, double margin)
{
    const RadarMount *m = g->m;
    Polar q = to_sensor(m, p);
    if (!in_fov(m, q))
        return;

    double ux = (p.x - g->s.x) / q.r, uy = (p.y - g->s.y) / q.r, uz = (p.z - g->s.z) / q.r;
    double vr = vel.x * ux + vel.y * uy + vel.z * uz;

    RadarDetection d;
    uint64_t *rng = &g->rs->rng;
    d.range_m = (float)(q.r + gauss(rng) * m->sd_range_m);
    d.az_deg = (float)(q.az / DEG + gauss(rng) * m->sd_az_deg);
    d.el_deg = m->has_elevation ? (float)(q.el / DEG + gauss(rng) * m->sd_el_deg) : 0.0f;
    d.vr_mps = (float)(vr + gauss(rng) * m->sd_vr_mps);
    double snr = 20.0 + margin + gauss(rng) * 1.5;
    d.snr_db = (float)(snr < 1.0 ? 1.0 : snr > 60.0 ? 60.0 : snr);
    if (!radar_detection_valid(m, &d)) /* 잡음으로 시야 밖에 나간 점은 내지 않는다 (XH1) */
        return;

    long cr = lround(d.range_m / m->res_range_m), ca = lround(d.az_deg / m->res_az_deg);
    long ce = m->has_elevation ? lround(d.el_deg / m->res_el_deg) : 0;
    long cv = lround(d.vr_mps / m->res_vr_mps);
    for (int i = 0; i < g->f->count; i++) {
        const RadarDetection *e = &g->f->det[i];
        if (lround(e->range_m / m->res_range_m) == cr && lround(e->az_deg / m->res_az_deg) == ca &&
            (m->has_elevation ? lround(e->el_deg / m->res_el_deg) : 0) == ce &&
            lround(e->vr_mps / m->res_vr_mps) == cv) {
            if (d.snr_db > e->snr_db)
                g->f->det[i] = d;
            return;
        }
    }
    if (g->f->count < m->max_det)
        g->f->det[g->f->count++] = d;
}

static bool occluded(const Gen *g, V3 p, int skip)
{
    for (int j = 0; j < WORLD_MAX_OBJ; j++) {
        if (!g->used[j] || j == skip)
            continue;
        const OBox *b = &g->box[j];
        Box3 lb = local_box(b);
        if (segment_hits_box(to_local(b, g->s), to_local(b, p), &lb))
            return true;
    }
    return false;
}

static void gen_object(Gen *g, int idx)
{
    const WorldObj *o = &g->w->obj[idx];
    const ObjSpec *sp = &OBJ_SPECS[o->kind];
    const OBox *ob = &g->box[idx];
    Box3 b = local_box(ob);
    V3 c = {ob->cx, ob->cy, (ob->z0 + ob->z1) / 2.0};
    Polar qc = to_sensor(g->m, c);
    if (qc.r > g->m->range_max_m + 20.0)
        return;

    double margin = margin_db(g->m, sp->rcs_dbsm, qc.r);
    if (uniform(&g->rs->rng) >= pd_of(margin)) /* 이번 프레임에 못 봄 (XH3) */
        return;

    double spacing = qc.r * g->m->res_az_deg * DEG;
    if (spacing < MIN_SPACING)
        spacing = MIN_SPACING;
    /* 점이 많으면 건너뛰지 않고 간격을 넓혀 고르게 뽑는다 (XH2) */
    Face faces[3];
    int nf, total;
    V3 s_local = to_local(ob, g->s);
    for (;;) {
        nf = visible_faces(&b, s_local, spacing, faces);
        total = 0;
        for (int i = 0; i < nf; i++)
            total += faces[i].nu * faces[i].nv;
        if (total <= MAX_SCATTER)
            break;
        spacing *= 1.15;
    }

    V3 vel = {ob->vx - g->ego_v, ob->vy, 0.0}; /* 상대속도 (자차 축) */
    for (int i = 0; i < nf; i++) {
        int n = faces[i].nu * faces[i].nv;
        for (int j = 0; j < n; j++) {
            if (uniform(&g->rs->rng) >= POINT_KEEP)
                continue;
            V3 p = from_local(ob, face_point(&faces[i], j));
            if (occluded(g, p, idx))
                continue;
            emit(g, p, vel, margin);
        }
    }
}

static void gen_clutter(Gen *g)
{
    V3 still = {-g->ego_v, 0.0, 0.0};
    /* 가드레일 (XH7) */
    for (double x = 1.0; g->w->rail && x <= g->m->range_max_m; x += RAIL_STEP) {
        V3 p = {x, WORLD_RAIL_Y, RAIL_Z};
        Polar q = to_sensor(g->m, p);
        if (!in_fov(g->m, q))
            continue;
        double margin = margin_db(g->m, RAIL_RCS, q.r);
        if (uniform(&g->rs->rng) >= pd_of(margin) || occluded(g, p, -1))
            continue;
        emit(g, p, still, margin);
    }
    /* 노면: 매 프레임 다른 곳 0~3점 */
    int n = (int)(uniform(&g->rs->rng) * 4.0);
    for (int i = 0; i < n; i++) {
        V3 p = {2.0 + uniform(&g->rs->rng) * 28.0, -6.0 + uniform(&g->rs->rng) * 12.0, 0.0};
        Polar q = to_sensor(g->m, p);
        if (!in_fov(g->m, q))
            continue;
        double margin = margin_db(g->m, GROUND_RCS, q.r);
        if (uniform(&g->rs->rng) >= pd_of(margin))
            continue;
        emit(g, p, still, margin);
    }
}

static void generate(RadarSim *rs, RadarId id, const World *w, double ego_v, RadarFrame *f)
{
    const RadarMount *m = &RADAR_MOUNTS[id];
    static Gen g;
    memset(&g, 0, sizeof g);
    g.rs = rs;
    g.m = m;
    g.f = f;
    g.s.x = m->x;
    g.s.y = m->y;
    g.s.z = m->z;
    g.w = w;
    g.ego_v = ego_v;
    int order[WORLD_MAX_OBJ], n = 0;
    double dist[WORLD_MAX_OBJ];

    /* 가까운 물체부터 (점 수 한도에 걸리면 먼 것부터 빠진다) */
    for (int i = 0; i < WORLD_MAX_OBJ; i++) {
        if (!w->obj[i].used)
            continue;
        g.box[i] = world_obox(w, &w->obj[i]);
        g.used[i] = true;
        double cx = g.box[i].cx, cy = g.box[i].cy;
        double d2 = cx * cx + cy * cy;
        int j = n++;
        while (j > 0 && dist[j - 1] > d2) {
            dist[j] = dist[j - 1];
            order[j] = order[j - 1];
            j--;
        }
        dist[j] = d2;
        order[j] = i;
    }
    for (int i = 0; i < n; i++)
        gen_object(&g, order[i]);
    gen_clutter(&g);
}

bool radar_sim_frame(RadarSim *rs, RadarId id, const World *w, double ego_v_mps, RadarFrame *out)
{
    RadarSimSensor *s = &rs->s[id];

    switch (s->fault) {
    case RADAR_FAULT_CUT:
        return false;
    case RADAR_FAULT_REPEAT:
        if (!s->have_last)
            return false;
        *out = s->last;
        return true;
    case RADAR_FAULT_STUCK:
        if (s->have_last) {
            *out = s->last;
            out->counter = ++s->counter;
            out->status = RADAR_ST_OK;
            s->last = *out;
            return true;
        }
        break; /* 고정할 프레임이 없으면 이번만 정상으로 만든다 */
    default:
        break;
    }

    out->id = id;
    out->counter = ++s->counter;
    out->count = 0;
    if (s->fault == RADAR_FAULT_BLOCK) {
        out->status = RADAR_ST_BLOCKED;
    } else {
        out->status = RADAR_ST_OK;
        generate(rs, id, w, ego_v_mps, out);
    }
    s->last = *out;
    s->have_last = true;
    return true;
}
