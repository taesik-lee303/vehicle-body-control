/*
 * adas_map.c - 전방 집중형 맵 (XC1~XC8, XM1~XM10)
 */

#include "adas_map.h"

#include <math.h>
#include <string.h>

#define PI_F 3.14159265f
#define DEG_F (PI_F / 180.0f)

/* XM3 군집화 */
#define CL_DX 1.5f
#define CL_DY 1.0f    /* 또는 방위 분해능 1.5칸 중 큰 값 (멀수록 넓게) */
#define CL_DY_CELLS 1.5f
#define CL_DV 1.0f
#define CL_DZ 1.0f    /* FR만: 높이 차 (육교 아래 차와 육교를 나눈다) */
/* XM4 연관 게이트 */
#define GATE_X 2.5f
#define GATE_Y 0.75f /* 가로 범위 양쪽 여유 (합쳐 1.5 m) */
#define GATE_X_NEW 1.0f  /* 확정 전 트랙 (잡음이 우연히 이어지지 않게) */
#define GATE_Y_NEW 0.3f
/* XM7 정지·이동 */
#define STILL_V 0.5f
#define STILL_VY 1.0f /* 가로 속도는 위치 변화로만 추정해 잡음이 커서 기준을 넓게 */
#define MOVING_V 1.5f
#define MOVING_VY 1.0f
#define WIDE_M 3.0f
#define MOVING_CYCLES 5 /* 0.25초 연속 움직여야 '움직였음' */
/* XM6 높이 */
#define HIGH_Z 2.5f
#define LOW_Z 0.2f
#define EPS 1e-3f       /* 경계 비교 여유 1 mm (부동소수 오차) */
#define GROUND_Z 0.15f  /* XM2 노면 점 (+ 거리별 고도 잡음 3σ) */
#define MAX_STEP_X 1.0f /* 한 주기 위치 갱신 한도 (튀는 측정 방지) */

typedef struct {
    uint8_t sensor;
    int n;
    float x_min, y_min, y_max, y_sum;
    float z_lo, z_hi;
    bool has_z;
    float vr_sum, ux_sum, uy_sum; /* 도플러와 시선 방향 (vr = vx·ux + vy·uy) */
    int still_n;
} Cluster;

#define MAX_CLUSTERS 256

void map_init(AdasMap *m)
{
    memset(m, 0, sizeof *m);
    m->next_id = 1;
    m->status = MAP_NONE;
    for (int i = 0; i < RADAR_COUNT; i++) {
        m->rx[i].health = RHEALTH_UNKNOWN;
        hold_timer_reset(&m->rx[i].recover);
    }
    hold_timer_reset(&m->cutin);
    hold_timer_reset(&m->lead_out);
}

static float ego_speed(const VbcContext *ctx)
{
    if (!ctx->speed_valid)
        return 0.0f;
    float v = (float)ctx->speed_kmh / 3.6f;
    return ctx->gear == GEAR_R ? -v : v;
}

/* FNV-1a: 검출점 목록 지문 (XC7) */
static uint32_t frame_hash(const RadarFrame *f)
{
    uint32_t h = 2166136261u;
    const unsigned char *p = (const unsigned char *)f->det;
    size_t n = (size_t)f->count * sizeof f->det[0];
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h ^ f->count;
}

void map_rx_frame(AdasMap *m, vbc_ms_t now, const RadarFrame *f, const VbcContext *ctx)
{
    if (f->id >= RADAR_COUNT)
        return;
    RadarRx *r = &m->rx[f->id];
    const RadarMount *mt = &RADAR_MOUNTS[f->id];

    /* XC1: 카운터가 같으면 새 프레임이 아니다 (프레임 반복) */
    if (r->any_rx && f->counter == r->last_counter)
        return;
    r->any_rx = true;
    r->last_counter = f->counter;
    r->last_new_ms = now;
    r->frames++;
    r->status_bad = f->status != RADAR_ST_OK; /* XC2 */

    /* XC7: 자차가 움직이는데 검출점 목록이 1초 동안 그대로 */
    uint32_t h = frame_hash(f);
    bool moving = ctx->speed_valid && ctx->speed_kmh >= MAP_STUCK_MIN_KMH;
    if (r->status_bad || f->count == 0 || !moving || !r->hash_valid || h != r->last_hash) {
        r->same_since_ms = now;
        r->stuck = false;
    } else if (now - r->same_since_ms >= MAP_STUCK_MS) {
        r->stuck = true;
    }
    r->last_hash = h;
    r->hash_valid = true;

    m->n_pts[f->id] = 0;
    m->fresh[f->id] = false;
    if (r->status_bad || r->stuck || r->faulted)
        return; /* 고장인 레이더의 점은 쓰지 않는다 (XC2, XC8) */

    /* XM2: 차량 좌표로 바꾸고 정지 점을 가린다 */
    float v_ego = ego_speed(ctx);
    int n = 0;
    for (int i = 0; i < f->count && i < mt->max_det && i < RADAR_MAX_DET; i++) {
        const RadarDetection *d = &f->det[i];
        if (!radar_detection_valid(mt, d)) /* XC3 */
            continue;
        float az = (d->az_deg + mt->yaw_deg) * DEG_F;
        float el = mt->has_elevation ? d->el_deg * DEG_F : 0.0f;
        float ce = cosf(el);
        MapPoint p;
        p.x = mt->x + d->range_m * ce * cosf(az);
        p.y = mt->y + d->range_m * ce * sinf(az);
        p.z = mt->z + d->range_m * sinf(el);
        if (p.x < 0.0f || p.x > MAP_X_MAX || fabsf(p.y) > MAP_Y_MAX) /* XM1 */
            continue;
        if (f->id != RADAR_FR && p.x > MAP_CORNER_X_MAX)
            continue;
        p.range = d->range_m;
        p.ux = ce * cosf(az);
        p.uy = ce * sinf(az);
        p.vr = d->vr_mps;
        p.ground_v = d->vr_mps + v_ego * p.ux;
        p.stationary = fabsf(p.ground_v) < STILL_V;
        /* 노면 점: 기준 + 고도 잡음 3σ (멀수록 높이 오차가 커진다) */
        float z_noise = 3.0f * d->range_m * mt->sd_el_deg * DEG_F;
        p.ground = mt->has_elevation && p.stationary && p.z < GROUND_Z + z_noise;
        p.sensor = (uint8_t)f->id;
        m->pts[f->id][n++] = p;
    }
    m->n_pts[f->id] = n;
    m->fresh[f->id] = true;
}

/* ---- XC1, XC2, XC7, XC8: 레이더 상태 ---- */

static void update_health(AdasMap *m, vbc_ms_t now)
{
    for (int i = 0; i < RADAR_COUNT; i++) {
        RadarRx *r = &m->rx[i];
        bool timeout = r->any_rx ? now - r->last_new_ms >= MAP_TIMEOUT_MS : now >= MAP_TIMEOUT_MS;
        unsigned why = (timeout ? RFAULT_TIMEOUT : 0) | (r->status_bad ? RFAULT_STATUS : 0) |
                       (r->stuck ? RFAULT_STUCK : 0);
        if (why) {
            r->faulted = true;
            r->reason = why;
            hold_timer_reset(&r->recover);
        } else if (r->faulted && hold_timer_update(&r->recover, now, true, MAP_RECOVER_MS)) {
            r->faulted = false;
            r->reason = 0;
        }
        r->health = r->faulted ? RHEALTH_FAULT : r->any_rx ? RHEALTH_OK : RHEALTH_UNKNOWN;
    }
}

/* ---- XM3 군집화 ---- */

static int cluster_sensor(const AdasMap *m, int s, Cluster *out, int max)
{
    const MapPoint *p = m->pts[s];
    int n = m->n_pts[s];
    static unsigned char seen[RADAR_MAX_DET];
    static int queue[RADAR_MAX_DET];
    int nc = 0;
    float cell = RADAR_MOUNTS[s].res_az_deg * DEG_F * CL_DY_CELLS;

    memset(seen, 0, (size_t)n);
    for (int i = 0; i < n; i++)
        if (p[i].ground)
            seen[i] = 1; /* 노면 점은 군집에 넣지 않는다 */
    for (int i = 0; i < n && nc < max; i++) {
        if (seen[i])
            continue;
        Cluster c = {(uint8_t)s, 0, 1e9f, 1e9f, -1e9f, 0, 1e9f, -1e9f, s == RADAR_FR, 0, 0, 0, 0};
        int qh = 0, qt = 0;
        queue[qt++] = i;
        seen[i] = 1;
        while (qh < qt) {
            const MapPoint *a = &p[queue[qh++]];
            c.n++;
            if (a->x < c.x_min) c.x_min = a->x;
            if (a->y < c.y_min) c.y_min = a->y;
            if (a->y > c.y_max) c.y_max = a->y;
            c.y_sum += a->y;
            if (a->z < c.z_lo) c.z_lo = a->z;
            if (a->z > c.z_hi) c.z_hi = a->z;
            c.vr_sum += a->vr;
            c.ux_sum += a->ux;
            c.uy_sum += a->uy;
            if (a->stationary)
                c.still_n++;
            for (int j = 0; j < n; j++) {
                if (seen[j])
                    continue;
                const MapPoint *b = &p[j];
                float dy_max = fmaxf(CL_DY, cell * fmaxf(a->range, b->range));
                if (fabsf(a->x - b->x) <= CL_DX && fabsf(a->y - b->y) <= dy_max && fabsf(a->vr - b->vr) <= CL_DV &&
                    (s != RADAR_FR || fabsf(a->z - b->z) <= CL_DZ)) {
                    seen[j] = 1;
                    queue[qt++] = j;
                }
            }
        }
        out[nc++] = c;
    }
    return nc;
}

/* ---- XM4, XM5 추적 ---- */

static Track *new_track(AdasMap *m, vbc_ms_t now)
{
    for (int i = 0; i < MAP_MAX_TRACKS; i++) {
        Track *t = &m->tracks[i];
        if (t->used)
            continue;
        memset(t, 0, sizeof *t);
        t->used = true;
        t->id = m->next_id++;
        if (m->next_id == 0)
            m->next_id = 1;
        t->last_hit_ms = now;
        return t;
    }
    return NULL;
}

static float cl_y(const Cluster *c) { return c->y_sum / (float)c->n; }

static void track_init(Track *t, const Cluster *c, float v_ego)
{
    t->x = c->x_min;
    t->y = cl_y(c);
    t->half_l = fmaxf(c->y_max - t->y, 0.1f);
    t->half_r = fmaxf(t->y - c->y_min, 0.1f);
    float ux = c->ux_sum / (float)c->n;
    t->vx = fabsf(ux) > 0.5f ? (c->vr_sum / (float)c->n) / ux : -v_ego;
    t->vy = 0.0f;
    if (c->has_z) {
        t->z_lo = c->z_lo;
        t->z_hi = c->z_hi;
        t->has_z = true;
    }
}

static void track_update(Track *t, const Cluster *c, float dt)
{
    float my = cl_y(c);
    float rx = c->x_min - t->x, ry = my - t->y;
    if (rx > MAX_STEP_X) rx = MAX_STEP_X;
    if (rx < -MAX_STEP_X) rx = -MAX_STEP_X;
    t->x += 0.5f * rx;
    t->y += 0.4f * ry;
    /* 도플러 vr = vx·ux + vy·uy. 가로 속도(위치 변화로 추정) 몫을 빼고 x 속도를 구한다 */
    float n = (float)c->n, ux = c->ux_sum / n, uy = c->uy_sum / n;
    if (fabsf(ux) > 0.5f)
        t->vx += 0.5f * ((c->vr_sum / n - t->vy * uy) / ux - t->vx);
    if (dt > 0.0f) { /* α-β 필터 (α 0.4~0.5, β 0.05~0.1) */
        t->vx += 0.05f * rx / dt;
        t->vy += 0.1f * ry / dt;
    }
    if (t->vy > 4.0f) t->vy = 4.0f;
    if (t->vy < -4.0f) t->vy = -4.0f;
    t->half_l = fmaxf(0.6f * t->half_l + 0.4f * (c->y_max - my), 0.1f);
    t->half_r = fmaxf(0.6f * t->half_r + 0.4f * (my - c->y_min), 0.1f);
    if (c->has_z) {
        if (!t->has_z) {
            t->z_lo = c->z_lo;
            t->z_hi = c->z_hi;
            t->has_z = true;
        } else {
            t->z_lo += 0.3f * (c->z_lo - t->z_lo);
            t->z_hi += 0.3f * (c->z_hi - t->z_hi);
        }
    }
}

static void merge_cluster(Cluster *a, const Cluster *b)
{
    a->n += b->n;
    a->x_min = fminf(a->x_min, b->x_min);
    a->y_min = fminf(a->y_min, b->y_min);
    a->y_max = fmaxf(a->y_max, b->y_max);
    a->y_sum += b->y_sum;
    a->z_lo = fminf(a->z_lo, b->z_lo);
    a->z_hi = fmaxf(a->z_hi, b->z_hi);
    a->vr_sum += b->vr_sum;
    a->ux_sum += b->ux_sum;
    a->uy_sum += b->uy_sum;
    a->still_n += b->still_n;
}

static float t_ymin(const Track *t) { return t->y - t->half_r; }
static float t_ymax(const Track *t) { return t->y + t->half_l; }

static void track_all(AdasMap *m, vbc_ms_t now, float dt, float v_ego)
{
    static Cluster cl[MAX_CLUSTERS];
    static Cluster meas_fr[MAP_MAX_TRACKS], meas_cn[MAP_MAX_TRACKS];
    bool born[MAP_MAX_TRACKS];
    int nc = 0;

    /* FR 군집을 먼저 (같은 주기에 FR 값이 있으면 FR 값을 쓴다, XM3) */
    for (int s = 0; s < RADAR_COUNT; s++)
        if (m->fresh[s])
            nc += cluster_sensor(m, s, cl + nc, MAX_CLUSTERS - nc);

    for (int i = 0; i < MAP_MAX_TRACKS; i++) {
        Track *t = &m->tracks[i];
        meas_fr[i].n = meas_cn[i].n = 0;
        born[i] = false;
        if (!t->used)
            continue;
        t->x += t->vx * dt; /* 예측 */
        t->y += t->vy * dt;
        t->seen_by = 0;
    }

    for (int k = 0; k < nc; k++) {
        const Cluster *c = &cl[k];
        unsigned bit = 1u << c->sensor;
        int best = -1;
        float best_cost = 1e9f;
        for (int i = 0; i < MAP_MAX_TRACKS; i++) {
            const Track *t = &m->tracks[i];
            if (!t->used)
                continue;
            float dx = fabsf(c->x_min - t->x);
            float gx = t->confirmed ? GATE_X : GATE_X_NEW, gy = t->confirmed ? GATE_Y : GATE_Y_NEW;
            if (dx > gx || c->y_min - gy > t_ymax(t) || c->y_max + gy < t_ymin(t))
                continue;
            if (c->has_z && t->has_z && (c->z_lo > t->z_hi + CL_DZ || c->z_hi < t->z_lo - CL_DZ))
                continue; /* 높이가 다르면 다른 물체 (육교와 그 아래 차) */
            float cost = dx + fabsf(cl_y(c) - t->y);
            if (cost < best_cost) {
                best_cost = cost;
                best = i;
            }
        }
        if (best < 0) {
            Track *t = new_track(m, now);
            if (!t)
                continue;
            best = (int)(t - m->tracks);
            track_init(t, c, v_ego);
            born[best] = true;
        }
        Track *t = &m->tracks[best];
        t->seen_by |= (uint8_t)bit;
        if (c->sensor != RADAR_FR) {
            t->corner_seen = true;
            t->corner_seen_ms = now;
        }
        /* 레이더별로 이번 주기 측정을 모은다. 같은 물체가 여러 군집으로 보이면 합친다 */
        Cluster *agg = c->sensor == RADAR_FR ? &meas_fr[best] : &meas_cn[best];
        if (agg->n == 0)
            *agg = *c;
        else
            merge_cluster(agg, c);
    }

    for (int i = 0; i < MAP_MAX_TRACKS; i++) {
        Track *t = &m->tracks[i];
        if (!t->used)
            continue;
        const Cluster *z = meas_fr[i].n ? &meas_fr[i] : meas_cn[i].n ? &meas_cn[i] : NULL; /* FR 우선 (XM3) */
        if (z) {
            if (!born[i])
                track_update(t, z, dt);
            if (t->hits < 255)
                t->hits++;
            if (t->hits >= MAP_CONFIRM_HITS)
                t->confirmed = true;
            t->last_hit_ms = now;
        } else {
            t->hits = 0;
            if (!t->confirmed || now - t->last_hit_ms > MAP_COAST_MS) /* XM5 */
                t->used = false;
        }
    }
}

/* ---- XM6~XM8 분류 ---- */

static void classify(AdasMap *m, float v_ego)
{
    for (int i = 0; i < MAP_MAX_TRACKS; i++) {
        Track *t = &m->tracks[i];
        if (!t->used)
            continue;
        float ground = t->vx + v_ego;
        /* 폭이 3 m를 넘는 트랙(육교·표지판·가드레일)은 가로 속도를 믿지 않는다: 점 구성이 바뀌면 가운데가 흔들린다 */
        float vy = t->half_l + t->half_r > WIDE_M ? 0.0f : t->vy;
        t->stationary = fabsf(ground) < STILL_V && fabsf(vy) < STILL_VY; /* XM7 */
        if (fabsf(ground) >= MOVING_V || fabsf(vy) >= MOVING_VY) {
            if (t->moving_count < 255)
                t->moving_count++;
        } else {
            t->moving_count = 0;
        }
        if (t->moving_count >= MOVING_CYCLES)
            t->ever_moving = true;
        /* XM6: 한 번도 움직이지 않은 정지 물체 중 높거나 낮은 것은 장애물이 아니다 (높이는 FR만) */
        /* XM6: 바닥이 2.5 m 이상이면 밑으로 지나갈 수 있다 (움직임과 관계없이) */
        bool high = t->has_z && t->z_lo >= HIGH_Z - EPS;
        /* XM6: 한 번도 움직이지 않은 정지 물체 중 낮은 것(맨홀), 또는
         * FR 시야 안인데 FR이 한 번도 못 본 측면 레이더 정지 물체(측면 오검출) */
        const RadarMount *fr = &RADAR_MOUNTS[RADAR_FR];
        float ang = atan2f(t->y - fr->y, t->x - fr->x) / DEG_F;
        bool fr_could_see = t->x - fr->x >= 2.0f && fabsf(ang) <= fr->az_fov_deg - 5.0f;
        bool low = t->has_z && t->z_hi < LOW_Z - EPS;
        bool corner_only_ghost = !t->has_z && fr_could_see;
        if (t->stationary && !t->ever_moving && (low || corner_only_ghost))
            t->ignored = true;
        if (t->ever_moving)
            t->ignored = false;
        t->obstacle = !(high || t->ignored);
        t->in_path = t_ymax(t) >= -MAP_PATH_HALF - EPS && t_ymin(t) <= MAP_PATH_HALF + EPS; /* XM8 */
    }
}

static Track *find_track(AdasMap *m, uint16_t id)
{
    if (!id)
        return NULL;
    for (int i = 0; i < MAP_MAX_TRACKS; i++)
        if (m->tracks[i].used && m->tracks[i].id == id)
            return &m->tracks[i];
    return NULL;
}

/* ---- XM9 선행 차량 ---- */

static void select_lead(AdasMap *m, vbc_ms_t now)
{
    Track *nearest = NULL;
    for (int i = 0; i < MAP_MAX_TRACKS; i++) {
        Track *t = &m->tracks[i];
        if (t->used && t->confirmed && t->obstacle && t->in_path && t->x > -0.5f && (!nearest || t->x < nearest->x))
            nearest = t;
    }
    if (m->status == MAP_NONE) { /* XM10 */
        m->lead_id = 0;
        return;
    }

    Track *lead = find_track(m, m->lead_id);
    if (lead && (!lead->confirmed || !lead->obstacle))
        lead = NULL;
    if (lead && hold_timer_update(&m->lead_out, now, fabsf(lead->y) > 1.8f, 500))
        lead = NULL; /* 0.5초 연속 |y| > 1.8 m */

    if (!lead) {
        lead = nearest;
        hold_timer_reset(&m->lead_out);
        hold_timer_reset(&m->cutin);
        m->cutin_id = 0;
    } else if (nearest && nearest != lead && nearest->x < lead->x) {
        if (m->cutin_id != nearest->id) {
            m->cutin_id = nearest->id;
            hold_timer_reset(&m->cutin);
        }
        if (hold_timer_update(&m->cutin, now, true, 300)) { /* 끼어들기 0.3초 연속 */
            lead = nearest;
            hold_timer_reset(&m->lead_out);
            hold_timer_reset(&m->cutin);
            m->cutin_id = 0;
        }
    } else {
        hold_timer_reset(&m->cutin);
        m->cutin_id = 0;
    }
    m->lead_id = lead ? lead->id : 0;
}

void map_step(AdasMap *m, vbc_ms_t now, const VbcContext *ctx)
{
    float dt = m->stepped ? (float)(now - m->last_step_ms) / 1000.0f : 0.0f;
    float v_ego = ego_speed(ctx);
    m->last_step_ms = now;
    m->stepped = true;
    m->ego_v = v_ego;

    update_health(m, now);
    track_all(m, now, dt, v_ego);
    for (int s = 0; s < RADAR_COUNT; s++)
        m->fresh[s] = false;
    classify(m, v_ego);

    /* XM10 맵 상태 */
    if (m->rx[RADAR_FR].health != RHEALTH_OK)
        m->status = MAP_NONE;
    else if (m->rx[RADAR_FCL].health != RHEALTH_OK || m->rx[RADAR_FCR].health != RHEALTH_OK)
        m->status = MAP_REDUCED;
    else
        m->status = MAP_OK;

    select_lead(m, now);
}

const Track *map_lead(const AdasMap *m)
{
    return map_track(m, m->lead_id);
}

const Track *map_track(const AdasMap *m, uint16_t id)
{
    return find_track((AdasMap *)m, id);
}

unsigned map_restart_block(const AdasMap *m, vbc_ms_t now, uint16_t except_id)
{
    unsigned why = 0;
    for (int i = 0; i < MAP_MAX_TRACKS; i++) {
        const Track *t = &m->tracks[i];
        if (!t->used || !t->confirmed || t->id == except_id)
            continue;
        if (t->obstacle && t->in_path && t->x <= 5.0f) /* (1) */
            why |= 1;
        bool by_corner = t->corner_seen && now - t->corner_seen_ms <= MAP_COAST_MS;
        bool toward = (t->y > 0.0f && t->vy <= -0.5f) || (t->y < 0.0f && t->vy >= 0.5f);
        if (by_corner && t->x <= 10.0f && fabsf(t->y) <= 5.0f && toward) /* (2) */
            why |= 2;
    }
    if (m->status == MAP_NONE) /* (3) */
        why |= 4;
    return why;
}

const char *map_status_name(MapStatus s)
{
    switch (s) {
    case MAP_NONE: return "none";
    case MAP_REDUCED: return "reduced";
    case MAP_OK: return "ok";
    }
    return "?";
}

const char *radar_health_name(RadarHealth h)
{
    switch (h) {
    case RHEALTH_UNKNOWN: return "unknown";
    case RHEALTH_OK: return "ok";
    case RHEALTH_FAULT: return "fault";
    }
    return "?";
}
