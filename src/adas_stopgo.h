/*
 * adas_stopgo.h - Stop & Go (요구사항 5장 XS1~XS13, 6.3 XD1~XD4)
 *
 * 제품 소프트웨어다. VbcContext(C3·C4), 맵(adas_map), 운전자 페달·버튼만 보고
 * 목표 가속도와 정지 유지를 "요청"한다. 실제 속도는 주행 모델(drive, XR1·XR2)이 바꾼다.
 *
 * 우선순위: XS10 > XS11 > XS8 > XS9 > XS12 > XS5·XS6·XS7 > XS4 > XS3
 */

#ifndef ADAS_STOPGO_H
#define ADAS_STOPGO_H

#include "adas_map.h"
#include "common.h"

typedef enum { SG_OFF, SG_STANDBY, SG_ACTIVE, SG_HOLD, SG_WAIT } SgState;
typedef enum { SG_ALERT_NONE, SG_ALERT_TAKEOVER, SG_ALERT_RADAR, SG_ALERT_GO } SgAlert; /* XD1 순서 */

#define SG_MAX_ENGAGE_KMH 60   /* XS1 */
#define SG_RELEASE_KMH 70      /* XS11 */
#define SG_SET_MIN 30          /* XS2 */
#define SG_SET_MAX 60
#define SG_STOP_GAP_M 4.0f     /* XS4, XS5 */
#define SG_ENGAGE_STOP_M 20.0f /* XS1: 정지 중 켜기 */
#define SG_AUTO_GO_MS 3000u    /* XS6, XS7 */
#define SG_LONG_STOP_MS 180000u /* XS8 */
#define SG_A_MAX 1.5f
#define SG_A_MIN (-3.5f)
#define SG_JERK 2.5f           /* m/s³ (XS4) */
#define SG_TTC_RISK 1.5f       /* XS12 (정상 추종 거리의 TTC는 1.5 + 4/v초) */
#define SG_TTC_CLEAR 2.5f
#define SG_TAKEOVER_MS 5000u   /* XS11 */
#define SG_RAMP_MS 1000u       /* XS11 */
#define SG_GO_ALERT_MS 5000u   /* XD3 */

typedef struct {
    bool brake;
    bool accel;
} SgPedals;

typedef struct {
    /* 운전자 입력 */
    bool sw_on;
    int gap_level; /* 1~3 → 1.0 / 1.5 / 2.0 s */
    int set_kmh;   /* 0 = 저장 없음 */
    bool ev_set, ev_res;
    int ev_adjust;

    SgState state;
    vbc_ms_t since_ms; /* 지금 상태가 된 시각 */
    vbc_ms_t last_ms;
    bool started;

    /* 출력 (XR1, XR2, XD) */
    bool accel_valid;
    float accel;     /* 목표 가속도 m/s² */
    bool hold;       /* 정지 유지 요청 */
    SgAlert alert;
    bool override;   /* 가속 페달로 운전자가 앞섬 (XS10) */

    float a_prev;
    float a_des;           /* 변화율 제한 전 목표 가속도 */
    uint16_t stop_lead_id; /* 정지할 때의 선행 차량 */
    float stop_lead_x;
    bool collision;        /* XS12 */
    HoldTimer collision_clear;
    bool ramp;             /* XS11 해제 뒤 1초 */
    float ramp_a0;
    vbc_ms_t ramp_start;
    vbc_ms_t takeover_until;
    bool takeover_timed;
    bool post_hold;        /* XS8: 대기로 돌아간 뒤에도 정지 유지 */
    bool go_alert;         /* XD3 */
    vbc_ms_t go_since;
    unsigned block;        /* XS9 이유 (map_restart_block) */
} StopGo;

void sg_init(StopGo *sg);
void sg_switch(StopGo *sg, bool on);
void sg_press_set(StopGo *sg);
void sg_press_res(StopGo *sg);
void sg_set_gap(StopGo *sg, int level);
void sg_adjust(StopGo *sg, int dir);

void sg_step(StopGo *sg, vbc_ms_t now, const VbcContext *ctx, const AdasMap *map, SgPedals pedals);

float sg_gap_s(const StopGo *sg);
const char *sg_state_name(SgState s);
const char *sg_alert_name(SgAlert a);

#endif /* ADAS_STOPGO_H */
