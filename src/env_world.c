/*
 * env_world.c - 시뮬레이터 세계 (요구사항 8.3)
 */

#include "env_world.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846
#define PROJECT_WINDOW 80.0 /* 자차를 경로에 투영할 때 직전 위치에서 앞뒤로 찾는 거리 m */

const ObjSpec OBJ_SPECS[OBJ_KIND_COUNT] = {
    [OBJ_CAR] = {"car", 4.6f, 1.9f, 0.0f, 1.5f, 10.0f, true},
    [OBJ_TRUCK] = {"truck", 10.0f, 2.5f, 0.0f, 3.5f, 20.0f, true},
    [OBJ_PED] = {"ped", 0.5f, 0.5f, 0.0f, 1.75f, -5.0f, true},
    [OBJ_BRIDGE] = {"bridge", 10.0f, 30.0f, 4.5f, 5.5f, 25.0f, false},
    [OBJ_SIGN] = {"sign", 0.3f, 16.0f, 5.0f, 6.5f, 15.0f, false},
    [OBJ_MANHOLE] = {"manhole", 0.8f, 0.8f, 0.0f, 0.03f, 5.0f, false},
};

/* 직선 도로 (sim/js/scene/road.js): 편도 2차로, 내 차로는 가운데선 옆 차로, 오른쪽 차로 하나, 반대 방향 2차로 */
static const LaneInfo STRAIGHT_LANES = {-5.4f, 1.8f, 9.0f, 3.6f, true};

struct RoadPath {
    int n;
    double length;
    double x[PATH_MAX_PTS], y[PATH_MAX_PTS];
    double s[PATH_MAX_PTS + 1]; /* 점 i까지 누적 거리 (s[n] = 한 바퀴) */
    double h[PATH_MAX_PTS];     /* 점 i의 접선 방향 */
    LaneInfo lane[PATH_MAX_PTS];
};

static RoadPath g_path;

static double wrap(double a)
{
    return atan2(sin(a), cos(a));
}

/* ---- 경로 ---- */

void world_path_set(const float *xy, const float *lanes, int n)
{
    if (n > PATH_MAX_PTS)
        n = PATH_MAX_PTS;
    g_path.n = n < 3 ? 0 : n;
    if (!g_path.n)
        return;
    g_path.s[0] = 0;
    for (int i = 0; i < n; i++) {
        g_path.x[i] = xy[i * 2];
        g_path.y[i] = xy[i * 2 + 1];
    }
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        g_path.s[i + 1] = g_path.s[i] + hypot(g_path.x[j] - g_path.x[i], g_path.y[j] - g_path.y[i]);
    }
    g_path.length = g_path.s[n];
    for (int i = 0; i < n; i++) {
        int a = (i - 1 + n) % n, b = (i + 1) % n;
        g_path.h[i] = atan2(g_path.y[b] - g_path.y[a], g_path.x[b] - g_path.x[a]);
        if (lanes) {
            LaneInfo L = {lanes[i * 4], lanes[i * 4 + 1], lanes[i * 4 + 2], lanes[i * 4 + 3], false};
            L.has_opp = L.opp_l > L.same_l + 0.5f;
            if (L.lw < 2.0f) L.lw = 3.0f;
            if (L.same_r > -0.5f * L.lw) L.same_r = -0.5f * L.lw; /* 내 차로(y 0)는 언제나 쓸 수 있다 */
            if (L.same_l < 0.5f * L.lw) L.same_l = 0.5f * L.lw;
            g_path.lane[i] = L;
        } else {
            g_path.lane[i] = STRAIGHT_LANES;
        }
    }
}

void world_path_attach(World *w)
{
    w->path = g_path.n ? &g_path : NULL;
}

double world_path_length(const World *w)
{
    return w->path ? w->path->length : 0.0;
}

/* s를 한 바퀴 안으로, 그 구간의 앞 점 번호 */
static int path_index(const RoadPath *p, double *s)
{
    double L = p->length;
    *s = fmod(fmod(*s, L) + L, L);
    int lo = 0, hi = p->n;
    while (hi - lo > 1) {
        int m = (lo + hi) / 2;
        if (p->s[m] <= *s) lo = m;
        else hi = m;
    }
    return lo;
}

Pose2 world_path_pose(const World *w, double s, double lat)
{
    Pose2 q;
    if (!w->path) {
        q.x = s;
        q.y = lat;
        q.yaw = 0.0;
        return q;
    }
    const RoadPath *p = w->path;
    int i = path_index(p, &s), j = (i + 1) % p->n;
    double seg = p->s[i + 1] - p->s[i], t = seg > 1e-9 ? (s - p->s[i]) / seg : 0.0;
    double yaw = p->h[i] + wrap(p->h[j] - p->h[i]) * t;
    q.x = p->x[i] + (p->x[j] - p->x[i]) * t - sin(yaw) * lat;
    q.y = p->y[i] + (p->y[j] - p->y[i]) * t + cos(yaw) * lat;
    q.yaw = yaw;
    return q;
}

LaneInfo world_lanes(const World *w, double s)
{
    if (!w->path)
        return STRAIGHT_LANES;
    int i = path_index(w->path, &s);
    return w->path->lane[i];
}

/* 전역 점을 경로에 투영한 s. hint 근처 PROJECT_WINDOW 안에서 찾는다 (hint < 0이면 전체) */
static double path_project(const RoadPath *p, double x, double y, double hint)
{
    double best = 0, bd = INFINITY;
    for (int i = 0; i < p->n; i++) {
        if (hint >= 0) {
            double d = fabs(wrap((p->s[i] - hint) / p->length * 2 * PI)) / (2 * PI) * p->length;
            if (d > PROJECT_WINDOW)
                continue;
        }
        int j = (i + 1) % p->n;
        double ax = p->x[i], ay = p->y[i], dx = p->x[j] - ax, dy = p->y[j] - ay;
        double len2 = dx * dx + dy * dy, t = len2 > 1e-12 ? ((x - ax) * dx + (y - ay) * dy) / len2 : 0.0;
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        double qx = ax + dx * t - x, qy = ay + dy * t - y, d2 = qx * qx + qy * qy;
        if (d2 < bd) {
            bd = d2;
            best = p->s[i] + (p->s[i + 1] - p->s[i]) * t;
        }
    }
    return best;
}

/* ---- 세계 ---- */

void world_init(World *w)
{
    memset(w, 0, sizeof *w);
    w->next_id = 1;
    world_path_attach(w);
    w->rail = w->path == NULL; /* 가드레일은 직선 도로에만 */
    w->ego = world_path_pose(w, 0.0, 0.0);
}

int world_add(World *w, ObjKind kind, double ahead_m, double y, double v_mps)
{
    for (int i = 0; i < WORLD_MAX_OBJ; i++) {
        WorldObj *o = &w->obj[i];
        if (o->used)
            continue;
        const ObjSpec *sp = &OBJ_SPECS[kind];
        memset(o, 0, sizeof *o);
        o->used = true;
        o->id = w->next_id++;
        o->kind = kind;
        o->s = w->ego_s + ahead_m + sp->length / 2.0;
        o->y = y;
        o->v = sp->movable ? v_mps : 0.0;
        o->v_target = o->v;
        o->accel = 2.0;
        o->y_target = y;
        o->vy_max = 1.0;
        return o->id;
    }
    return 0;
}

WorldObj *world_find(World *w, int id)
{
    for (int i = 0; i < WORLD_MAX_OBJ; i++)
        if (w->obj[i].used && w->obj[i].id == id)
            return &w->obj[i];
    return NULL;
}

bool world_remove(World *w, int id)
{
    WorldObj *o = world_find(w, id);
    if (!o)
        return false;
    o->used = false;
    return true;
}

void world_clear(World *w)
{
    for (int i = 0; i < WORLD_MAX_OBJ; i++)
        w->obj[i].used = false;
}

void world_set_ego(World *w, double x, double y, double yaw, double yaw_rate)
{
    w->ego.x = x;
    w->ego.y = y;
    w->ego.yaw = yaw;
    w->ego_yaw_rate = yaw_rate;
    if (w->path)
        w->ego_s = path_project(w->path, x, y, w->ego_external ? w->ego_s : -1.0);
    w->ego_external = true;
}

static double approach(double cur, double target, double step)
{
    if (cur < target)
        return cur + step > target ? target : cur + step;
    return cur - step < target ? target : cur - step;
}

/* 차·트럭은 그 자리의 차로 범위 안에서만 (같은 방향이면 같은 방향 차로, 반대 방향이면 반대 차로) */
static double lane_clamp(const World *w, const WorldObj *o, double y)
{
    if (o->kind != OBJ_CAR && o->kind != OBJ_TRUCK)
        return y;
    LaneInfo L = world_lanes(w, o->s);
    double half = OBJ_SPECS[o->kind].width / 2.0 + 0.2, lo, hi;
    if (o->v < -0.1 && L.has_opp) {
        lo = L.same_l + half;
        hi = L.opp_l - half;
    } else {
        lo = L.same_r + half;
        hi = L.same_l - half;
    }
    if (lo > hi)
        return (lo + hi) / 2.0;
    return y < lo ? lo : y > hi ? hi : y;
}

void world_step(World *w, double dt_s, double ego_v_mps)
{
    w->ego_v = ego_v_mps;
    if (!w->ego_external) {
        Pose2 before = w->ego;
        w->ego_s += ego_v_mps * dt_s;
        w->ego = world_path_pose(w, w->ego_s, 0.0);
        w->ego_yaw_rate = dt_s > 0 ? wrap(w->ego.yaw - before.yaw) / dt_s : 0.0;
    }
    for (int i = 0; i < WORLD_MAX_OBJ; i++) {
        WorldObj *o = &w->obj[i];
        if (!o->used || !OBJ_SPECS[o->kind].movable)
            continue;
        double v0 = o->v;
        o->v = approach(o->v, o->v_target, o->accel * dt_s);
        o->s += (v0 + o->v) / 2.0 * dt_s;
        double y0 = o->y, target = lane_clamp(w, o, o->y_target);
        double rate = o->vy_max;
        if (fabs(lane_clamp(w, o, o->y) - o->y) > 1e-6 && rate < 1.0)
            rate = 1.0; /* 차로가 없어지면 남은 차로로 합류 */
        o->y = approach(o->y, target, rate * dt_s);
        o->vy = dt_s > 0 ? (o->y - y0) / dt_s : 0.0;
    }
}

OBox world_obox(const World *w, const WorldObj *o)
{
    const ObjSpec *sp = &OBJ_SPECS[o->kind];
    Pose2 p = world_path_pose(w, o->s, o->y);
    bool flip = (o->kind == OBJ_CAR || o->kind == OBJ_TRUCK) && o->v < -0.1;
    double yaw = p.yaw + (flip ? PI : 0.0);
    double c = cos(w->ego.yaw), sn = sin(w->ego.yaw), dx = p.x - w->ego.x, dy = p.y - w->ego.y;
    /* 지면 속도: 경로 방향 v, 경로 왼쪽 vy */
    double gx = cos(p.yaw) * o->v - sin(p.yaw) * o->vy, gy = sin(p.yaw) * o->v + cos(p.yaw) * o->vy;
    OBox b;
    b.cx = (float)(dx * c + dy * sn);
    b.cy = (float)(-dx * sn + dy * c);
    b.yaw = (float)wrap(yaw - w->ego.yaw);
    b.hl = sp->length / 2.0f;
    b.hw = sp->width / 2.0f;
    b.z0 = sp->z0;
    b.z1 = sp->z1;
    b.vx = (float)(gx * c + gy * sn);
    b.vy = (float)(-gx * sn + gy * c);
    return b;
}

Box3 world_box(const World *w, const WorldObj *o)
{
    OBox b = world_obox(w, o);
    float c = fabsf(cosf(b.yaw)), sn = fabsf(sinf(b.yaw));
    float ex = b.hl * c + b.hw * sn, ey = b.hl * sn + b.hw * c;
    Box3 r = {b.cx - ex, b.cx + ex, b.cy - ey, b.cy + ey, b.z0, b.z1};
    return r;
}

int world_kind_from_name(const char *name)
{
    for (int k = 0; k < OBJ_KIND_COUNT; k++) {
        const char *a = OBJ_SPECS[k].name, *b = name;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
            a++;
            b++;
        }
        if (!*a && !*b)
            return k;
    }
    return -1;
}

double world_truth_gap(const World *w)
{
    double best = -1;
    for (int i = 0; i < WORLD_MAX_OBJ; i++) {
        const WorldObj *o = &w->obj[i];
        if (!o->used || o->kind == OBJ_BRIDGE || o->kind == OBJ_SIGN || o->kind == OBJ_MANHOLE)
            continue;
        Box3 b = world_box(w, o);
        if (b.y1 < -1.3f || b.y0 > 1.3f || b.x1 <= 0.0f)
            continue;
        double gap = b.x0 < 0 ? 0 : b.x0;
        if (best < 0 || gap < best)
            best = gap;
    }
    return best;
}
