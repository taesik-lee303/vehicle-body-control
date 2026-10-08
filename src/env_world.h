/*
 * env_world.h - 시뮬레이터 세계: 교통 참여자와 고정 물체 (요구사항 8.3)
 *
 * 제품 소프트웨어가 아니다. 레이더 센서 모델(env_radar)이 이 세계를 "보고", 화면이 이 세계를 그린다.
 * 제품(adas_*)은 이 파일을 쓰지 않는다 (레이더 프레임으로만 세계를 안다).
 *
 * 도로 좌표: s = 도로를 따라 잰 거리(m, 자차 진행 방향 +), y = 내 차로 가운데 기준 왼쪽(+).
 * 자차의 s는 앞 범퍼 위치다. 차량 좌표로 바꾸면 x = s - ego_s.
 */

#ifndef ENV_WORLD_H
#define ENV_WORLD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { OBJ_CAR, OBJ_TRUCK, OBJ_PED, OBJ_BRIDGE, OBJ_SIGN, OBJ_MANHOLE, OBJ_KIND_COUNT } ObjKind;

typedef struct {
    const char *name;
    float length, width; /* s 방향 길이, y 방향 폭 */
    float z0, z1;        /* 높이 범위 */
    float rcs_dbsm;
    bool movable;
} ObjSpec;

extern const ObjSpec OBJ_SPECS[OBJ_KIND_COUNT];

#define WORLD_MAX_OBJ 32
#define WORLD_LANE_W 3.6f
#define WORLD_RAIL_Y (-7.45f) /* 오른쪽 가드레일 (XH7) */

typedef struct {
    bool used;
    int id;
    ObjKind kind;
    double s, y;      /* 가운데 위치 */
    double v;         /* s 방향 속도 m/s (반대 방향 차는 음수) */
    double v_target;  /* 목표 속도 */
    double accel;     /* 목표 속도로 갈 때 가감속 크기 m/s² */
    double y_target;  /* 목표 가로 위치 */
    double vy_max;    /* 가로 이동 속도 크기 m/s */
    double vy;        /* 지금 가로 속도 (화면·도플러용) */
} WorldObj;

/* 차량 좌표의 상자 */
typedef struct {
    float x0, x1, y0, y1, z0, z1;
} Box3;

typedef struct {
    WorldObj obj[WORLD_MAX_OBJ];
    int next_id;
    double ego_s; /* 자차 앞 범퍼 */
} World;

void world_init(World *w);

/* ahead_m: 자차 앞 범퍼에서 물체의 가까운 면까지 거리. 반환: 번호(1부터), 자리가 없으면 0 */
int world_add(World *w, ObjKind kind, double ahead_m, double y, double v_mps);
WorldObj *world_find(World *w, int id);
bool world_remove(World *w, int id);
void world_clear(World *w);

/* dt_s 동안 진행. ego_v_mps: 자차 속도 (후진이면 음수) */
void world_step(World *w, double dt_s, double ego_v_mps);

/* 물체 상자를 차량 좌표로 */
Box3 world_box(const World *w, const WorldObj *o);

/* 이름 → 종류. 없으면 -1 */
int world_kind_from_name(const char *name);

/* 시험용 정답: 내 차로(|y| ≤ 1.3 m와 겹침)에서 앞에 있는 가장 가까운 물체의 가까운 면까지 거리. 없으면 -1 */
double world_truth_gap(const World *w);

#endif /* ENV_WORLD_H */
