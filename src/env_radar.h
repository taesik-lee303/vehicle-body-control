/*
 * env_radar.h - 레이더 센서 모델: 시뮬레이터 세계를 보고 검출점 프레임을 만든다 (요구사항 8.1, 8.2 XH1~XH8)
 *
 * 실제 레이더 하드웨어를 대신하는 시뮬레이터 부품이다. 제품 소프트웨어가 아니다.
 * 같은 시드와 같은 세계면 항상 같은 프레임을 만든다 (XH6).
 */

#ifndef ENV_RADAR_H
#define ENV_RADAR_H

#include "env_world.h"
#include "radar_frame.h"

/* XH8 고장 주입 */
typedef enum {
    RADAR_FAULT_NONE,   /* 정상 */
    RADAR_FAULT_CUT,    /* 끊김: 프레임 없음 */
    RADAR_FAULT_REPEAT, /* 프레임 반복: 직전 프레임을 카운터째 다시 보냄 */
    RADAR_FAULT_STUCK,  /* 값 고착: 카운터만 늘고 검출점은 고정 */
    RADAR_FAULT_BLOCK,  /* 가림: 상태 '가림', 검출점 없음 */
    RADAR_FAULT_COUNT
} RadarFaultMode;

typedef struct {
    uint8_t counter;
    RadarFaultMode fault;
    bool have_last;
    RadarFrame last; /* 마지막으로 보낸 프레임 (반복·고착용) */
} RadarSimSensor;

typedef struct {
    RadarSimSensor s[RADAR_COUNT];
    uint64_t rng;
} RadarSim;

void radar_sim_init(RadarSim *rs, uint64_t seed);
void radar_sim_set_fault(RadarSim *rs, RadarId id, RadarFaultMode mode);
const char *radar_fault_name(RadarFaultMode mode);

/* 한 주기 프레임을 만든다. 끊김이면 false (보낼 프레임 없음) */
bool radar_sim_frame(RadarSim *rs, RadarId id, const World *w, double ego_v_mps, RadarFrame *out);

#endif /* ENV_RADAR_H */
