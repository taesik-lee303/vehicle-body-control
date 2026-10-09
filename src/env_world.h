/*
 * env_world.h - 시뮬레이터 세계: 교통 참여자와 고정 물체 (요구사항 8.3)
 *
 * 제품 소프트웨어가 아니다. 레이더 센서 모델(env_radar)이 이 세계를 "보고", 화면이 이 세계를 그린다.
 * 제품(adas_*)은 이 파일을 쓰지 않는다 (레이더 프레임으로만 세계를 안다).
 *
 * 물체는 "도로 경로"를 따라 움직인다: s = 경로를 따라 잰 거리, y = 경로(내 차로 가운데) 기준 왼쪽(+).
 *  - 경로가 없으면(터미널, 직선 도로 화면) 경로 = 전역 x축 직선. 예전과 같다.
 *  - 실제 지도 화면은 경로 폴리라인과 구간별 차로 범위를 넘겨준다(world_path_set). 물체는 그 차로 안에 머문다.
 * 자차는 경로 위를 달리거나(내부), 화면이 알려 준 자세(world_set_ego)를 쓴다(직접 운전으로 경로를 벗어나도 됨).
 * 센서 모델은 물체를 자차 기준 좌표의 회전 상자(OBox)로 본다. 자차 기준 원점 = 앞 범퍼, x 앞, y 왼쪽.
 */

#ifndef ENV_WORLD_H
#define ENV_WORLD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { OBJ_CAR, OBJ_TRUCK, OBJ_PED, OBJ_BRIDGE, OBJ_SIGN, OBJ_MANHOLE, OBJ_KIND_COUNT } ObjKind;

typedef struct {
    const char *name;
    float length, width; /* 진행 방향 길이, 폭 */
    float z0, z1;        /* 높이 범위 */
    float rcs_dbsm;
    bool movable;
} ObjSpec;

extern const ObjSpec OBJ_SPECS[OBJ_KIND_COUNT];

#define WORLD_MAX_OBJ 32
#define WORLD_LANE_W 3.6f
#define WORLD_RAIL_Y (-7.45f) /* 직선 도로의 오른쪽 가드레일 (XH7) */
#define PATH_MAX_PTS 4096

/* 경로 위 한 점의 차로 범위 (경로 기준 y, 왼쪽 +) */
typedef struct {
    float same_r, same_l; /* 같은 방향 차로들이 차지하는 범위 (오른쪽 끝 ≤ 0 ≤ 왼쪽 끝) */
    float opp_l;          /* 반대 방향 차로의 왼쪽 끝 (반대 차로가 있으면 same_l < opp_l) */
    float lw;             /* 차로 폭 */
    bool has_opp;
} LaneInfo;

/* 전역 2D 좌표 (x, y), 방향 yaw (x축에서 반시계, 라디안) */
typedef struct {
    double x, y, yaw;
} Pose2;

/* 자차 기준 회전 상자와 물체의 지면 속도 (자차 축으로 나눈 값) */
typedef struct {
    float cx, cy;  /* 가운데 */
    float yaw;     /* 자차 방향 기준 물체 방향 */
    float hl, hw;  /* 절반 길이·폭 */
    float z0, z1;
    float vx, vy;  /* 지면 속도 */
} OBox;

typedef struct {
    bool used;
    int id;
    ObjKind kind;
    double s, y;      /* 경로 위 가운데 위치 */
    double v;         /* 경로 방향 속도 m/s (반대 방향 차는 음수) */
    double v_target;  /* 목표 속도 */
    double accel;     /* 목표 속도로 갈 때 가감속 크기 m/s² */
    double y_target;  /* 목표 가로 위치 (차로 범위로 제한된다) */
    double vy_max;    /* 가로 이동 속도 크기 m/s */
    double vy;        /* 지금 가로 속도 */
    /* 조건부 가로 이동 (교통 장면: 끼어들기·횡단). 가까운 면까지 거리가 trig_d 이하(1) / 이상(2)이 되면 시작 */
    int trig_mode;
    double trig_d, trig_y, trig_vy;
} WorldObj;

/* 자차 기준 축 정렬 상자 (OBox를 감싸는 상자) */
typedef struct {
    float x0, x1, y0, y1, z0, z1;
} Box3;

typedef struct RoadPath RoadPath;

typedef struct {
    WorldObj obj[WORLD_MAX_OBJ];
    int next_id;
    double ego_s;        /* 자차 앞 범퍼의 경로 위 거리 (외부 자세면 경로에 투영한 값) */
    Pose2 ego;           /* 자차 앞 범퍼의 전역 자세 */
    double ego_yaw_rate; /* rad/s, 왼쪽으로 돌면 + */
    double ego_v;        /* 마지막 world_step의 자차 속도 */
    bool ego_external;   /* 화면이 자세를 알려 준다 (직접 운전·지도) */
    bool rail;           /* 오른쪽 가드레일이 있음 (직선 도로). 실제 지도에는 없음 */
    const RoadPath *path; /* NULL이면 직선 */
} World;

void world_init(World *w);

/* ahead_m: 자차 앞 범퍼에서 경로를 따라 물체의 가까운 면까지 거리. 반환: 번호(1부터), 자리가 없으면 0 */
int world_add(World *w, ObjKind kind, double ahead_m, double y, double v_mps);
WorldObj *world_find(World *w, int id);
bool world_remove(World *w, int id);
void world_clear(World *w);

/* dt_s 동안 진행. ego_v_mps: 자차 속도 (후진이면 음수) */
void world_step(World *w, double dt_s, double ego_v_mps);

/* 화면이 알려 주는 자차 앞 범퍼 자세. 부르면 외부 자세 모드가 된다 */
void world_set_ego(World *w, double x, double y, double yaw, double yaw_rate);

/* 경로: 전역 좌표 점 n개 (닫힌 고리), 점마다 차로 정보 4개(same_r, same_l, opp_l, lw; opp_l이 same_l 이하면 반대 차로 없음).
 * lanes가 NULL이면 직선 도로 기본값. 한 번 넣으면 world_init 뒤에도 쓴다. n이 0이면 지운다 */
void world_path_set(const float *xy, const float *lanes, int n);
void world_path_attach(World *w); /* 넣어 둔 경로를 이 세계에 연결 (world_init이 자동으로 함) */
double world_path_length(const World *w); /* 직선이면 0 */

Pose2 world_path_pose(const World *w, double s, double lat);
LaneInfo world_lanes(const World *w, double s);

OBox world_obox(const World *w, const WorldObj *o);
Box3 world_box(const World *w, const WorldObj *o);

/* 차로 이름 → 그 자리(s)의 가로 위치. same(내 차로), right·left(같은 방향 옆 차로), side(오른쪽, 없으면 왼쪽),
 * opp(반대 차로), curbl·curbr(왼쪽·오른쪽 길가, 보행자용). 그 자리에 없으면 false */
bool world_lane_y(const World *w, double s, const char *name, double *y);

/* 조건부 가로 이동: 가까운 면까지 거리가 d가 되면(지금보다 멀면 다가와서, 가까우면 멀어져서) y로 vy m/s */
void world_trigger_lane(World *w, WorldObj *o, double d, double y, double vy);

/* 이름 → 종류. 없으면 -1 */
int world_kind_from_name(const char *name);

/* 시험용 정답: 내 차로(|y| ≤ 1.3 m와 겹침)에서 앞에 있는 가장 가까운 물체의 가까운 면까지 거리. 없으면 -1 */
double world_truth_gap(const World *w);

#endif /* ENV_WORLD_H */
