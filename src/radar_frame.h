/*
 * radar_frame.h - 레이더 하드웨어 ↔ 제품 소프트웨어 사이의 프레임 형식 (요구사항 2.2, 8.1)
 *
 * 실제 차량에서는 레이더가 CAN-FD/이더넷으로 이 프레임을 보낸다. 시뮬레이터에서는 센서 모델(env_radar)이 만든다.
 * 제품 쪽(adas_map, adas_stopgo)은 이 파일의 형식과 장착 정보만 알고, 센서 모델의 내부는 보지 않는다.
 *
 * 좌표: 차량 좌표 원점 = 앞 범퍼 가운데 노면. x 전방(+), y 왼쪽(+), z 위(+), 단위 m.
 * 각도: 센서 정면 기준, 방위각은 왼쪽(+), 고도각은 위(+), 단위 도.
 */

#ifndef RADAR_FRAME_H
#define RADAR_FRAME_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { RADAR_FR, RADAR_FCL, RADAR_FCR, RADAR_COUNT } RadarId;

/* 프레임 머리의 센서 자체 상태 (XC2) */
typedef enum { RADAR_ST_OK, RADAR_ST_FAULT, RADAR_ST_BLOCKED } RadarStatus;

#define RADAR_PERIOD_MS 50u
#define RADAR_MAX_DET 512 /* FR 최대. 측면은 128 (RadarMount.max_det) */

/* 검출점 하나 */
typedef struct {
    float range_m; /* 거리 */
    float az_deg;  /* 방위각 (센서 정면 기준, 왼쪽 +) */
    float el_deg;  /* 고도각. 고도가 없는 센서는 0 */
    float vr_mps;  /* 도플러 속도 = 상대속도의 시선 성분. 다가오면 음수 */
    float snr_db;  /* 신호 세기 */
} RadarDetection;

typedef struct {
    RadarId id;
    uint8_t counter; /* 새 프레임마다 +1, 255 다음 0 (XC1) */
    RadarStatus status;
    uint16_t count;
    RadarDetection det[RADAR_MAX_DET];
} RadarFrame;

/* 장착 정보와 제원 (요구사항 8.1). 제품과 센서 모델이 같은 값을 쓴다 */
typedef struct {
    const char *name;
    float x, y, z;     /* 장착 위치 (차량 좌표) */
    float yaw_deg;     /* 정면 방향 (왼쪽 +) */
    float az_fov_deg;  /* 방위 시야 ±값 */
    float el_fov_deg;  /* 고도 시야 ±값 */
    float range_min_m, range_max_m;
    bool has_elevation;
    uint16_t max_det;
    /* 분해능 (같은 칸의 점은 하나로 합쳐짐) */
    float res_range_m, res_az_deg, res_el_deg, res_vr_mps;
    /* 잡음 σ */
    float sd_range_m, sd_az_deg, sd_el_deg, sd_vr_mps;
    float r_ref_m; /* RCS 10 dBsm 물체를 Pd 0.88로 보는 거리 (XH3) */
} RadarMount;

extern const RadarMount RADAR_MOUNTS[RADAR_COUNT];

/* 유효 범위 (2.2). XC3: 범위 밖 검출점은 버린다 */
bool radar_detection_valid(const RadarMount *m, const RadarDetection *d);

#endif /* RADAR_FRAME_H */
