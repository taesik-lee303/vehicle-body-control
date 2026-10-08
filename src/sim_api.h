/*
 * sim_api.h - 그래픽 시뮬레이션(브라우저)용 API
 * 담당: F (이태식)
 *
 * WebAssembly로 컴파일해 JavaScript에서 부른다 (make wasm). 일반 gcc로도 빌드되어 make test로 확인한다.
 * 화면은 판단하지 않는다: 명령 문자열을 넘기고, 상태 JSON을 받아 그리기만 한다.
 */

#ifndef SIM_API_H
#define SIM_API_H

/* quiet가 0이면 변화 로그("[2.900s] 전조등: 꺼짐 → 켜짐")를 stdout으로 낸다 (브라우저에서는 이벤트 로그) */
void sim_init(int quiet);

/* 콘솔 명령 한 줄 (docs/console.md). 반환: 0 OK, 1 오류, 2 quit */
int sim_exec(const char *line);

/* 시뮬레이션 시간을 ms 진행. 50ms 단위로 진행하고 남는 값은 다음 호출로 넘긴다 */
void sim_advance(unsigned ms);

/* 현재 입력·판정·출력을 JSON 한 줄로. 다음 호출 전까지 유효 */
const char *sim_state(void);

/* 레이더·Stop & Go (exp/radar-stop-and-go): Stop & Go, 맵 상태, 트랙, 세계 물체(시뮬레이터 정답)를 JSON으로 */
const char *sim_adas(void);

/* 맵이 차량 좌표로 바꾼 검출점. 점마다 float 6개: 센서, x, y, z, 도플러, 표시(1 정지 점, 2 노면 점) */
const float *sim_points(void);
int sim_points_count(void);

/* 실제 지도: 주행 경로(전역 x, y 점 n개, 닫힌 고리)와 점마다 차로 정보 4개(same_r, same_l, opp_l, lw). n이 0이면 직선 도로 */
void sim_set_path(const float *xy, const float *lanes, int n);
/* 화면의 자차 앞 범퍼 자세 (전역 x, y, 방향 yaw, 요레이트). 부를 때마다 세계의 자차가 그 자세가 된다 */
void sim_set_ego(float x, float y, float yaw, float yaw_rate);

#endif /* SIM_API_H */
