/*
 * env_world.c - 시뮬레이터 세계 (요구사항 8.3)
 */

#include "env_world.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

const ObjSpec OBJ_SPECS[OBJ_KIND_COUNT] = {
    [OBJ_CAR] = {"car", 4.6f, 1.9f, 0.0f, 1.5f, 10.0f, true},
    [OBJ_TRUCK] = {"truck", 10.0f, 2.5f, 0.0f, 3.5f, 20.0f, true},
    [OBJ_PED] = {"ped", 0.5f, 0.5f, 0.0f, 1.75f, -5.0f, true},
    [OBJ_BRIDGE] = {"bridge", 10.0f, 30.0f, 4.5f, 5.5f, 25.0f, false},
    [OBJ_SIGN] = {"sign", 0.3f, 16.0f, 5.0f, 6.5f, 15.0f, false},
    [OBJ_MANHOLE] = {"manhole", 0.8f, 0.8f, 0.0f, 0.03f, 5.0f, false},
};

void world_init(World *w)
{
    memset(w, 0, sizeof *w);
    w->next_id = 1;
    w->rail = true;
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

static double approach(double cur, double target, double step)
{
    if (cur < target)
        return cur + step > target ? target : cur + step;
    return cur - step < target ? target : cur - step;
}

void world_step(World *w, double dt_s, double ego_v_mps)
{
    w->ego_s += ego_v_mps * dt_s;
    for (int i = 0; i < WORLD_MAX_OBJ; i++) {
        WorldObj *o = &w->obj[i];
        if (!o->used || !OBJ_SPECS[o->kind].movable)
            continue;
        double v0 = o->v;
        o->v = approach(o->v, o->v_target, o->accel * dt_s);
        o->s += (v0 + o->v) / 2.0 * dt_s;
        double y0 = o->y;
        o->y = approach(o->y, o->y_target, o->vy_max * dt_s);
        o->vy = dt_s > 0 ? (o->y - y0) / dt_s : 0.0;
    }
}

Box3 world_box(const World *w, const WorldObj *o)
{
    const ObjSpec *sp = &OBJ_SPECS[o->kind];
    double x = o->s - w->ego_s;
    Box3 b = {(float)(x - sp->length / 2.0), (float)(x + sp->length / 2.0), (float)(o->y - sp->width / 2.0),
              (float)(o->y + sp->width / 2.0), sp->z0, sp->z1};
    return b;
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
