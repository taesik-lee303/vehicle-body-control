/*
 * adas_map.h - 전방 집중형 맵 (요구사항 3장 XC1~XC8, 4장 XM1~XM10)
 *
 * 제품 소프트웨어다. 레이더 프레임(radar_frame.h)과 VbcContext(차량 상태 C3·C4)만 본다.
 *
 * 사용 흐름 (50 ms 주기)
 *   1. 레이더 프레임이 들어올 때마다 map_rx_frame()   (새 프레임 판정, 값 고착 판정, 좌표 변환)
 *   2. 주기마다 map_step()   (수신 중단·복귀 판정, 군집화, 추적·융합, 분류, 선행 차량, 맵 상태)
 */

#ifndef ADAS_MAP_H
#define ADAS_MAP_H

#include "common.h"
#include "radar_frame.h"

/* 레이더별 상태 (XC1~XC8). 처음 프레임 전에는 확인 중 (XC6) */
typedef enum { RHEALTH_UNKNOWN, RHEALTH_OK, RHEALTH_FAULT } RadarHealth;

/* 고장 이유 (화면·로그용) */
enum { RFAULT_TIMEOUT = 1, RFAULT_STATUS = 2, RFAULT_STUCK = 4 };

#define MAP_TIMEOUT_MS 200u   /* XC1: 새 프레임 없음 */
#define MAP_STUCK_MS 1000u    /* XC7: 값 고착 */
#define MAP_RECOVER_MS 1000u  /* XC8: 복귀 */
#define MAP_STUCK_MIN_KMH 4   /* XC7: 자차가 이 속도 이상일 때만 고착을 본다 */

typedef struct {
    bool any_rx;            /* 새 프레임을 한 번이라도 받음 */
    uint8_t last_counter;
    vbc_ms_t last_new_ms;   /* 마지막 새 프레임 시각 */
    bool status_bad;        /* 마지막 새 프레임의 상태가 정상이 아님 (XC2) */
    uint32_t last_hash;     /* 검출점 목록 지문 (XC7) */
    bool hash_valid;
    vbc_ms_t same_since_ms; /* 지금 목록이 처음 나온 시각 */
    bool stuck;             /* XC7 조건 성립 중 */
    bool faulted;           /* 고장 (XC8로 풀릴 때까지 유지) */
    unsigned reason;        /* RFAULT_* (마지막으로 고장을 만든 이유) */
    HoldTimer recover;      /* XC8 */
    RadarHealth health;
    unsigned frames;        /* 받은 새 프레임 수 (표시용) */
} RadarRx;

/* 차량 좌표로 바꾼 검출점 (XM2) */
typedef struct {
    float x, y, z;
    float range;    /* 센서에서 거리 */
    float vr;       /* 도플러 */
    float ground_v; /* 지면 속도의 시선 성분 = vr + 자차 속도 × cos */
    float ux, uy;   /* 시선 방향 단위 벡터의 x, y 성분 */
    uint8_t sensor;
    bool stationary;
    bool ground;    /* FR 노면 점: 정지 점이면서 z < 0.12 m. 군집에 넣지 않는다 (XM2) */
} MapPoint;

#define MAP_MAX_TRACKS 96
#define MAP_X_MAX 150.0f       /* XM1 */
#define MAP_Y_MAX 10.0f
#define MAP_CORNER_X_MAX 40.0f
#define MAP_PATH_HALF 1.3f     /* XM8 */
#define MAP_CONFIRM_HITS 3     /* XM4 */
#define MAP_COAST_MS 300u      /* XM5 */

typedef struct {
    bool used;
    uint16_t id;
    float x;          /* 가까운 면 */
    float y;          /* 가로 가운데 */
    float half_l, half_r; /* y 아래·위 폭 (y_min = y - half_r, y_max = y + half_l) */
    float vx, vy;     /* 상대속도 */
    float z_lo, z_hi; /* FR 점에서 얻은 높이 (XM3, XM6) */
    bool has_z;
    uint8_t hits;     /* 연속 연관 주기 수 */
    bool confirmed;
    vbc_ms_t last_hit_ms;
    vbc_ms_t corner_seen_ms; /* 측면 레이더가 마지막으로 본 시각 (XS9 (2)) */
    bool corner_seen;
    uint8_t seen_by;  /* 이번 주기에 연관된 레이더 (비트) */
    bool stationary, ever_moving;
    bool ignored;     /* 낮은 정지 물체·측면 오검출로 한 번 판정되면 유지 (가까이 와서 FR 시야를 벗어나도) */
    uint8_t moving_count;
    bool obstacle, in_path;
} Track;

typedef enum { MAP_NONE, MAP_REDUCED, MAP_OK } MapStatus; /* XM10 */

typedef struct {
    RadarRx rx[RADAR_COUNT];
    MapPoint pts[RADAR_COUNT][RADAR_MAX_DET];
    int n_pts[RADAR_COUNT];
    bool fresh[RADAR_COUNT]; /* 이번 주기에 처리할 새 프레임이 있음 */
    Track tracks[MAP_MAX_TRACKS];
    uint16_t next_id;
    uint16_t lead_id;        /* 0 = 없음 (XM9) */
    uint16_t cutin_id;
    HoldTimer cutin, lead_out;
    MapStatus status;
    vbc_ms_t last_step_ms;
    bool stepped;
    float ego_v;             /* 마지막 주기의 자차 속도 m/s (후진이면 음수) */
} AdasMap;

void map_init(AdasMap *m);
void map_rx_frame(AdasMap *m, vbc_ms_t now, const RadarFrame *f, const VbcContext *ctx);
void map_step(AdasMap *m, vbc_ms_t now, const VbcContext *ctx);

/* 선행 차량. 없으면 NULL */
const Track *map_lead(const AdasMap *m);
const Track *map_track(const AdasMap *m, uint16_t id);

/* XS9 재출발 금지: (1) 경로 내 x ≤ 5 m에 정지할 때 앞에 있던 차량(except_id)이 아닌 장애물,
 * (2) 측면 레이더가 본 다가오는 횡단 물체, (3) 맵 상태 없음. 해당하는 조건을 비트(1, 2, 4)로 돌려준다 */
unsigned map_restart_block(const AdasMap *m, vbc_ms_t now, uint16_t except_id);

const char *map_status_name(MapStatus s);
const char *radar_health_name(RadarHealth h);

#endif /* ADAS_MAP_H */
