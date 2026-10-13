/*
 * modcon.h - 모듈별 테스트 콘솔 (신호 주입기)
 *
 * 한 모듈의 step 함수만 호출해서 그 모듈 코드를 직접 확인한다 (다른 모듈은 실행하지 않는다).
 *
 * 원칙: 테스트 도구는 입력을 거르지 않는다.
 *   - 모든 신호를 정수 코드 그대로 넣는다 (기어 7, 시동 2, 도어 5, 스위치 -1 같은 정의 안 된 코드도).
 *     정의 안 된 값을 어떻게 다룰지는 제품 코드(common.c, 각 모듈)가 정할 일이다.
 *   - 신호마다 따로 끊고(cut) 잇고(resume), 한 번만 보낼(send) 수 있다. 자동 재송신도 끌 수 있다.
 *   - 시간은 1ms 단위로 진행한다. 판단(step) 주기와 송신 주기를 바꿀 수 있다.
 * 판정(C2~C4, 코드 유효성)은 실제와 같은 common.c의 vbc_build_context()가 한다.
 *
 * 입력 방법: 메뉴 번호, 또는 명령 한 줄 (docs/test/README.md, help 명령).
 * 파일로 넣으면 '#' 줄은 설명, tc/e/obs 명령으로 테스트 케이스 결과를 자동 판정·기록한다.
 */

#ifndef MODCON_H
#define MODCON_H

#include "common.h"
#include "drive.h"

/* 메뉴에 보여 줄 신호 (모듈이 쓰는 입력). 명령으로는 모든 신호를 넣을 수 있다 */
enum {
    SIG_IGN = 1 << 0,
    SIG_GEAR = 1 << 1,
    SIG_SPEED = 1 << 2,
    SIG_LUX = 1 << 3,
    SIG_RAIN = 1 << 4,
    SIG_DOORS = 1 << 5,
    SIG_LIGHT_SW = 1 << 6,
    SIG_WIPER_SW = 1 << 7,
    SIG_WIPER_LEVEL = 1 << 8 /* 조명 콘솔 전용: L6에 넘길 와이퍼 단계 */
};

/* 바뀌면 알려 줄 출력 */
enum {
    OUT_HEADLIGHT = 1 << 0,
    OUT_LUX_LAMP = 1 << 1,
    OUT_WIPER = 1 << 2,
    OUT_RAIN_LAMP = 1 << 3,
    OUT_LOCK = 1 << 4,
    OUT_WARNING = 1 << 5
};

/* 콘솔이 다루는 모듈 (ModCon.swc) */
enum { SWC_LIGHTING = 1 << 0, SWC_WIPER = 1 << 1, SWC_DOORS = 1 << 2, SWC_DASHBOARD = 1 << 3 };

typedef struct ModCon ModCon;

struct ModCon {
    /* 콘솔이 정하는 것 */
    const char *name;  /* "doors" 등. TC 파일명·JSON용 */
    unsigned swc;      /* SWC_* */
    const char *title;
    const char *about;
    unsigned show;     /* SIG_* */
    unsigned outputs;  /* OUT_* */
    void (*mod_init)(ModCon *m);
    void (*mod_step)(ModCon *m);
    int32_t start_ign;  /* 시작값 (계기판·와이퍼는 유효한 속도 판정을 위해 시동·기어를 정해 둔다) */
    int32_t start_gear;

    /* 직전 상태 주입 (선택). 사양서 입력이 아니라 모듈이 기억하는 직전 상태(예: 도어 잠금)를
     * TC 전제 조건으로 바로 만든다. 넣은 뒤 그 시각에 바로 판단하므로, 규칙이 바꾸면 곧 바뀐다 */
    const char *state_cmd;   /* 명령 이름 (예: "lock"). NULL이면 없음 */
    const char *state_label; /* 메뉴 표시 (예: "직전 잠금 상태") */
    const char *state_words[2]; /* 0, 1의 이름 (예: "풀림", "잠김") */
    bool (*mod_get_state)(ModCon *m);
    void (*mod_set_state)(ModCon *m, bool v); /* 상태와 그에 맞는 출력을 함께 바꾼다 */

    /* 주행 모델 (R1~R5, 선택). true면 drive·pedal 명령을 쓸 수 있다 (ECU 콘솔).
     * drive on이면 시동·기어·속도를 운전 조작으로 만든다 (REAL SIM src/console.c와 같은 규칙) */
    bool has_drive;

    /* 지금 보내고 있는 원래 값 (코드 그대로) */
    int32_t ign, gear, speed, lux, rain;
    int32_t wiper_level;
    unsigned cut;            /* 송신을 끊은 주기 신호 */
    bool auto_send;          /* 주기 신호 자동 재송신 */
    vbc_ms_t send_period_ms; /* 기본 50 (사양서 3.1: 0.5초보다 짧게) */
    vbc_ms_t step_period_ms; /* 기본 1 (1ms마다 판단) */

    Drive drive;   /* 주행 모델 상태 (has_drive일 때만 씀) */
    bool drive_on; /* true면 ign·gear는 주행 모델 요청(R1, R2), speed는 주행 모델이 50ms마다 정한다 */
    /* 화면 표시용 (state JSON만, 터미널 출력과 판정에는 쓰지 않음). drive on 중 마지막 시동·기어 요청
     * (받아들여졌든 거부됐든, -1 = 없음), 요청 횟수 (같은 값을 다시 요청해도 화면이 알 수 있게),
     * 마지막 거부 (NULL, "R1", "R2": 다음 요청이 받아들여지거나 drive on|off가 바뀔 때까지) */
    int32_t req_ign, req_gear;
    unsigned req_ign_n, req_gear_n;
    const char *refused;

    bool interactive; /* modcon_run이 켬: 인자 없으면 되묻기. 꺼져 있으면 오류로 처리 */
    bool err;         /* 이번 명령에서 '!' 오류가 났는지 */

    VbcInputs in;
    VbcContext ctx;
    VbcOutputs out;
    vbc_ms_t now;

    /* 테스트 케이스 기록 */
    char tc[48];
    int tc_count, tc_failed, checks, failures, observations;
    bool tc_has_fail;
};

/* 카운터를 0으로 하고 시작 상태로 둔다 (C5, 0초에 첫 입력 송신) */
void modcon_start(ModCon *m);
/* 명령 한 줄 실행. 0 정상, 1 오류('!' 출력), 2 quit */
int modcon_exec(ModCon *m, const char *line);
/* ms만큼 진행. 바뀐 출력·판정 수를 돌려준다 ("변화 없음"은 출력하지 않는다) */
int modcon_advance(ModCon *m, long ms);
/* 진행 중인 TC를 닫고 요약 줄을 낸다. FAIL이 있으면 1 */
int modcon_finish(ModCon *m);
/* 화면용 상태를 JSON 한 줄로 만든다 (정적 버퍼 4096바이트, 다음 호출 전까지 유효).
 * 정의 안 된 코드가 들어 있어도 올바른 JSON이고, 이름 배열 밖 enum은 "INVALID"로 낸다 */
const char *modcon_state_json(const ModCon *m);
/* 시작 후 터미널에서 대화형으로 돈다. expect가 하나라도 틀리면 1을 돌려준다 */
int modcon_run(ModCon *m);

#endif /* MODCON_H */
