/*
 * modcon.c - 모듈별 테스트 콘솔 (신호 주입기)
 */

#define _POSIX_C_SOURCE 200809L /* isatty, strncasecmp, strtok_r */

#include "modcon.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/* 주기 신호 비트 (cut / send) */
enum { P_SPEED = 1, P_GEAR = 2, P_IGN = 4, P_LUX = 8, P_RAIN = 16,
       P_VEHICLE = P_SPEED | P_GEAR | P_IGN, P_ALL = 31 };

#define MAX_TIME_MS 600000L

/* ---- 코드표 (보여 주기만 하고 막지 않는다) ---- */

typedef struct { const char *word; int32_t code; } Alias;

static const Alias IGN_ALIAS[] = {{"off", 0}, {"on", 1}, {NULL, 0}};
static const Alias GEAR_ALIAS[] = {{"p", GEAR_P}, {"r", GEAR_R}, {"n", GEAR_N}, {"d", GEAR_D}, {NULL, 0}};
static const Alias DOOR_ALIAS[] = {{"close", 0}, {"닫힘", 0}, {"open", 1}, {"열림", 1}, {NULL, 0}};
static const Alias LIGHT_ALIAS[] = {{"off", LIGHT_SW_OFF}, {"auto", LIGHT_SW_AUTO}, {"on", LIGHT_SW_ON}, {NULL, 0}};
static const Alias WIPER_ALIAS[] = {{"off", WIPER_SW_OFF}, {"auto", WIPER_SW_AUTO}, {"low", WIPER_SW_LOW},
                                    {"high", WIPER_SW_HIGH}, {NULL, 0}};
static const Alias LEVEL_ALIAS[] = {{"stop", WIPER_STOP}, {"정지", WIPER_STOP}, {"int", WIPER_INTERMITTENT},
                                    {"간헐", WIPER_INTERMITTENT}, {"low", WIPER_LOW}, {"저속", WIPER_LOW},
                                    {"high", WIPER_HIGH}, {"고속", WIPER_HIGH}, {NULL, 0}};
static const Alias NO_ALIAS[] = {{NULL, 0}};

typedef enum { K_IGN, K_GEAR, K_SPEED, K_LUX, K_RAIN, K_FL, K_FR, K_RL, K_RR, K_LIGHT, K_WIPER, K_LEVEL, K_COUNT } SigKey;

typedef struct {
    const char *cmd;
    unsigned show_bit;
    const char *label;
    const char *codes;
    const Alias *alias;
    unsigned periodic; /* P_* (주기 신호면) */
} SigInfo;

static const SigInfo SIGS[K_COUNT] = {
    [K_IGN] = {"ign", SIG_IGN, "시동", "0 꺼짐(off), 1 켜짐(on)", IGN_ALIAS, P_IGN},
    [K_GEAR] = {"gear", SIG_GEAR, "기어", "0 P, 1 R, 2 N, 3 D", GEAR_ALIAS, P_GEAR},
    [K_SPEED] = {"speed", SIG_SPEED, "속도", "km/h, 유효 0~250", NO_ALIAS, P_SPEED},
    [K_LUX] = {"lux", SIG_LUX, "조도", "lux, 유효 0~60000", NO_ALIAS, P_LUX},
    [K_RAIN] = {"rain", SIG_RAIN, "강우량", "%, 유효 0~100", NO_ALIAS, P_RAIN},
    [K_FL] = {"fl", SIG_DOORS, "도어 FL(앞 왼쪽)", "0 닫힘(close), 1 열림(open)", DOOR_ALIAS, 0},
    [K_FR] = {"fr", SIG_DOORS, "도어 FR(앞 오른쪽)", "0 닫힘(close), 1 열림(open)", DOOR_ALIAS, 0},
    [K_RL] = {"rl", SIG_DOORS, "도어 RL(뒤 왼쪽)", "0 닫힘(close), 1 열림(open)", DOOR_ALIAS, 0},
    [K_RR] = {"rr", SIG_DOORS, "도어 RR(뒤 오른쪽)", "0 닫힘(close), 1 열림(open)", DOOR_ALIAS, 0},
    [K_LIGHT] = {"light", SIG_LIGHT_SW, "조명 스위치", "0 OFF, 1 AUTO, 2 ON", LIGHT_ALIAS, 0},
    [K_WIPER] = {"wiper", SIG_WIPER_SW, "와이퍼 스위치", "0 OFF, 1 AUTO, 2 LOW, 3 HIGH", WIPER_ALIAS, 0},
    [K_LEVEL] = {"level", SIG_WIPER_LEVEL, "와이퍼 단계(조명에 넘김)", "0 정지, 1 간헐, 2 저속, 3 고속", LEVEL_ALIAS, 0},
};

static int32_t *sig_value(ModCon *m, SigKey k)
{
    switch (k) {
    case K_IGN: return &m->ign;
    case K_GEAR: return &m->gear;
    case K_SPEED: return &m->speed;
    case K_LUX: return &m->lux;
    case K_RAIN: return &m->rain;
    case K_FL: return &m->in.door_open[DOOR_FL];
    case K_FR: return &m->in.door_open[DOOR_FR];
    case K_RL: return &m->in.door_open[DOOR_RL];
    case K_RR: return &m->in.door_open[DOOR_RR];
    case K_LIGHT: return &m->in.light_sw;
    case K_WIPER: return &m->in.wiper_sw;
    case K_LEVEL: return &m->wiper_level;
    default: return NULL;
    }
}

/* ---- 문자열 도우미 ---- */

static bool ieq(const char *a, const char *b)
{
    return strcasecmp(a, b) == 0;
}

static char *trim(char *s)
{
    while (isspace((unsigned char)*s))
        s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = '\0';
    return s;
}

/* 한 줄 읽기. 파일 입력이면 입력을 화면에 보여 주고, '#' 줄은 설명으로 표시하고 건너뛴다 */
static bool read_line(const char *prompt, char *buf, size_t size)
{
    char line[1024];

    printf("%s", prompt);
    fflush(stdout);
    for (;;) {
        if (!fgets(line, sizeof line, stdin))
            return false;
        if (!strchr(line, '\n')) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF)
                ;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (!isatty(0) && line[0] == '#') {
            printf("\n%s\n%s", line, prompt);
            continue;
        }
        if (!isatty(0))
            printf("%s\n", line);
        snprintf(buf, size, "%s", line);
        return true;
    }
}

/* 정수(int32 전체) 또는 별칭. 성공하면 true */
static bool parse_value(const char *s, const Alias *alias, int32_t *out)
{
    char *end;

    for (const Alias *a = alias; a && a->word; a++)
        if (ieq(s, a->word)) { *out = a->code; return true; }
    errno = 0;
    long long v = strtoll(s, &end, 10);
    if (end == s || *end || errno == ERANGE || v < INT32_MIN || v > INT32_MAX)
        return false;
    *out = (int32_t)v;
    return true;
}

/* "0.501" → 501ms (1ms 단위) */
static bool parse_seconds(const char *s, long *ms)
{
    char *end;
    double sec = strtod(s, &end);

    if (end == s || *end || !(sec > 0) || sec * 1000.0 > MAX_TIME_MS)
        return false;
    *ms = (long)(sec * 1000.0 + 0.5);
    return *ms > 0;
}

static unsigned parse_periodic(const char *s)
{
    if (!s) return 0;
    if (ieq(s, "speed")) return P_SPEED;
    if (ieq(s, "gear")) return P_GEAR;
    if (ieq(s, "ign")) return P_IGN;
    if (ieq(s, "lux")) return P_LUX;
    if (ieq(s, "rain")) return P_RAIN;
    if (ieq(s, "vehicle")) return P_VEHICLE;
    if (ieq(s, "all")) return P_ALL;
    return 0;
}

static int find_sig(const char *s)
{
    for (int k = 0; k < K_COUNT; k++)
        if (ieq(s, SIGS[k].cmd))
            return k;
    return -1;
}

/* ---- 송신·판단 ---- */

static void send(ModCon *m, vbc_ms_t t, unsigned mask)
{
    if (mask & P_SPEED) vbc_rx_speed(&m->in, t, m->speed);
    if (mask & P_GEAR) vbc_rx_gear(&m->in, t, m->gear);
    if (mask & P_IGN) vbc_rx_ignition(&m->in, t, m->ign);
    if (mask & P_LUX) vbc_rx_lux(&m->in, t, m->lux);
    if (mask & P_RAIN) vbc_rx_rain(&m->in, t, m->rain);
}

static const char *on_off(bool b) { return b ? "켜짐" : "꺼짐"; }
static const char *yes_no(bool b) { return b ? "예" : "아니오"; }

static void change(const ModCon *m, const char *label, const char *from, const char *to)
{
    printf("  → [%3u.%03us] %s: %s → %s\n", (unsigned)(m->now / 1000), (unsigned)(m->now % 1000), label, from, to);
}

/* 바뀌면 알려 줄 판정 */
typedef struct {
    bool rx_lost, ign_on, speed_valid, lux_fault, rain_fault;
    bool ign_valid, gear_valid, doors_valid, light_valid, wiper_valid;
} Judge;

static Judge judge_of(const VbcContext *x)
{
    Judge j = {x->rx_lost, x->ignition_on, x->speed_valid, x->lux_fault, x->rain_fault, x->ignition_valid,
               x->gear_valid, x->door_valid[0] && x->door_valid[1] && x->door_valid[2] && x->door_valid[3],
               x->light_sw_valid, x->wiper_sw_valid};
    return j;
}

/* 지금 시각으로 판정을 만들고 모듈 step을 한 번. 바뀐 출력·판정 수를 돌려준다 */
static int step(ModCon *m)
{
    VbcOutputs a = m->out;
    Judge ja = judge_of(&m->ctx);
    int n = 0;

    vbc_build_context(&m->in, m->now, &m->ctx);
    m->mod_step(m);

    const VbcOutputs *b = &m->out;
    unsigned o = m->outputs;
    if ((o & OUT_HEADLIGHT) && a.headlight_on != b->headlight_on) { change(m, "전조등", on_off(a.headlight_on), on_off(b->headlight_on)); n++; }
    if ((o & OUT_LUX_LAMP) && a.lux_fault_lamp != b->lux_fault_lamp) { change(m, "조도 고장 표시", on_off(a.lux_fault_lamp), on_off(b->lux_fault_lamp)); n++; }
    if ((o & OUT_WIPER) && a.wiper != b->wiper) { change(m, "와이퍼", vbc_wiper_name(a.wiper), vbc_wiper_name(b->wiper)); n++; }
    if ((o & OUT_RAIN_LAMP) && a.rain_fault_lamp != b->rain_fault_lamp) { change(m, "강우 고장 표시", on_off(a.rain_fault_lamp), on_off(b->rain_fault_lamp)); n++; }
    if ((o & OUT_LOCK) && a.door_locked != b->door_locked) { change(m, "도어", a.door_locked ? "잠김" : "풀림", b->door_locked ? "잠김" : "풀림"); n++; }
    if ((o & OUT_WARNING) && a.warning != b->warning) { change(m, "계기판 경고", vbc_warning_name(a.warning), vbc_warning_name(b->warning)); n++; }

    Judge jb = judge_of(&m->ctx);
#define JCH(f, label) if (ja.f != jb.f) { change(m, "(판정) " label, yes_no(ja.f), yes_no(jb.f)); n++; }
    JCH(rx_lost, "수신 중단")
    JCH(ign_on, "시동 켜짐으로 봄")
    JCH(speed_valid, "유효한 속도")
    JCH(lux_fault, "조도 고장")
    JCH(rain_fault, "강우 고장")
    JCH(ign_valid, "시동 코드 정의됨")
    JCH(gear_valid, "기어 코드 정의됨")
    JCH(doors_valid, "도어 코드 정의됨")
    JCH(light_valid, "조명 스위치 코드 정의됨")
    JCH(wiper_valid, "와이퍼 스위치 코드 정의됨")
#undef JCH
    return n;
}

static void step_now(ModCon *m)
{
    if (step(m) == 0)
        printf("  → 변화 없음\n");
}

/* ms만큼 진행: step 주기마다 판단, 자동 재송신이면 send 주기의 배수 시각에 끊지 않은 신호를 보낸다 */
static void advance(ModCon *m, long ms)
{
    vbc_ms_t end = m->now + (vbc_ms_t)ms;
    vbc_ms_t sp = m->step_period_ms ? m->step_period_ms : 1;
    int changes = 0;

    while (m->now + sp <= end) {
        vbc_ms_t from = m->now;
        m->now += sp;
        if (m->auto_send && m->send_period_ms) {
            vbc_ms_t first = (from / m->send_period_ms + 1) * m->send_period_ms;
            for (vbc_ms_t t = first; t <= m->now; t += m->send_period_ms)
                send(m, t, P_ALL & ~m->cut);
        }
        changes += step(m);
    }
    m->now = end; /* step 주기로 나누어떨어지지 않는 나머지 (판단은 다음 step에서) */
    if (changes == 0)
        printf("  → %ld.%03ld초 동안 변화 없음\n", ms / 1000, ms % 1000);
}

static void reset(ModCon *m)
{
    m->ign = m->start_ign;
    m->gear = m->start_gear;
    m->speed = 0;
    m->lux = 30000;
    m->rain = 0;
    m->wiper_level = WIPER_STOP;
    m->cut = 0;
    m->auto_send = true;
    m->send_period_ms = 50;
    m->step_period_ms = 1;
    m->now = 0;
    vbc_inputs_init(&m->in);
    vbc_outputs_init(&m->out);
    m->mod_init(m);
    send(m, 0, P_ALL);
    vbc_build_context(&m->in, 0, &m->ctx);
    m->mod_step(m);
}

/* ---- 화면 ---- */

static void fmt_code(char *buf, size_t n, SigKey k, int32_t v)
{
    for (const Alias *a = SIGS[k].alias; a && a->word; a++)
        if (a->code == v && (unsigned char)a->word[0] < 0x80) { snprintf(buf, n, "%d %s", (int)v, a->word); return; }
    if (SIGS[k].alias != NO_ALIAS)
        snprintf(buf, n, "%d 정의 안 됨", (int)v);
    else
        snprintf(buf, n, "%d", (int)v);
}

static void print_status(ModCon *m)
{
    const VbcOutputs *o = &m->out;
    const VbcContext *x = &m->ctx;
    unsigned w = m->outputs;

    printf("\n== %s ==  시각 %u.%03u초  (판단 %ums마다, 자동 송신 %s %ums마다)\n", m->title,
           (unsigned)(m->now / 1000), (unsigned)(m->now % 1000), (unsigned)m->step_period_ms,
           m->auto_send ? "켬" : "끔", (unsigned)m->send_period_ms);
    printf("  [출력]");
    if (w & OUT_HEADLIGHT) printf(" 전조등 %s |", on_off(o->headlight_on));
    if (w & OUT_LUX_LAMP) printf(" 조도 고장 표시 %s |", on_off(o->lux_fault_lamp));
    if (w & OUT_WIPER) printf(" 와이퍼 %s |", vbc_wiper_name(o->wiper));
    if (w & OUT_RAIN_LAMP) printf(" 강우 고장 표시 %s |", on_off(o->rain_fault_lamp));
    if (w & OUT_LOCK) printf(" 도어 %s |", o->door_locked ? "잠김" : "풀림");
    if (w & OUT_WARNING) printf(" 계기판 경고 %s |", vbc_warning_name(o->warning));
    printf("\n  [판정] 수신 중단 %s | 시동 %s | 유효한 속도 ", yes_no(x->rx_lost), on_off(x->ignition_on));
    if (x->speed_valid) printf("%d", (int)x->speed_kmh); else printf("없음");
    printf(" | 조도 %s | 강우 %s\n", x->lux_fault ? "고장" : x->lux_valid ? "정상" : "값 없음",
           x->rain_fault ? "고장" : x->rain_valid ? "정상" : "값 없음");
    printf("         코드 정의됨: 시동 %s, 기어 %s, 도어 %s/%s/%s/%s, 조명SW %s, 와이퍼SW %s\n",
           yes_no(x->ignition_valid), yes_no(x->gear_valid), yes_no(x->door_valid[0]), yes_no(x->door_valid[1]),
           yes_no(x->door_valid[2]), yes_no(x->door_valid[3]), yes_no(x->light_sw_valid), yes_no(x->wiper_sw_valid));

    printf("  [입력 바꾸기] 번호 또는 명령 (예: gear 7, speed 15, cut speed, t 0.501)\n");
    int no = 0;
    for (int k = 0; k < K_COUNT; k++) {
        if (!(m->show & SIGS[k].show_bit))
            continue;
        char val[48];
        fmt_code(val, sizeof val, (SigKey)k, *sig_value(m, (SigKey)k));
        printf("   %2d) %-6s %s [%s]%s\n", ++no, SIGS[k].cmd, SIGS[k].label, val,
               (SIGS[k].periodic && (m->cut & SIGS[k].periodic)) ? " (송신 끊김)" : "");
    }
    if (m->state_cmd)
        printf("   %2d) %-6s %s [%s] (모듈 상태 주입, 0 %s / 1 %s)\n", ++no, m->state_cmd, m->state_label,
               m->state_words[m->mod_get_state(m)], m->state_words[0], m->state_words[1]);
    printf("  [기타] t 시간 진행 | cut/resume/send <신호> | auto on/off | period step/send <ms> | e·obs·tc 판정 | r 처음으로 | h 도움말 | q 종료\n");
}

static void help(const ModCon *m)
{
    printf("\n  %s\n\n", m->about);
    printf("  명령 (대소문자 무관)\n");
    for (int k = 0; k < K_COUNT; k++)
        printf("    %-6s <값>        %s: %s. 그 밖의 정수도 그대로 보냄\n", SIGS[k].cmd, SIGS[k].label, SIGS[k].codes);
    if (m->state_cmd)
        printf("    %-6s on|off      %s 주입 (0·off %s, 1·on %s). 사양서 입력이 아닌 모듈 상태, 넣은 시각에 바로 판단\n",
               m->state_cmd, m->state_label, m->state_words[0], m->state_words[1]);
    printf("    t <초>              시간 진행, 1ms 단위 (예: t 0.499, t 0.001, t 3)\n"
           "    cut <신호>          주기 신호 송신 끊기: speed gear ign lux rain vehicle(앞 셋) all\n"
           "    resume <신호>       송신 다시 시작 (그 시각에 바로 보냄)\n"
           "    send <신호>         지금 한 번만 보내기 (끊긴 신호도). auto off와 함께 쓰면 송신 시각을 직접 정함\n"
           "    auto on|off         주기 신호 자동 재송신 켜기/끄기\n"
           "    period step <ms>    판단(step) 주기, 기본 1\n"
           "    period send <ms>    자동 재송신 주기, 기본 50\n"
           "    tc <ID> | <요구사항> | <기법> | <입력> | <기대>   테스트 케이스 시작 (결과 기록용)\n"
           "    e <항목> <값>       기대 확인 → PASS/FAIL\n"
           "    obs <항목>          관찰만 기록 (사양서에 정해지지 않은 동작)\n"
           "      항목: lock headlight luxlamp rainlamp (on|off), wiper (stop|int|low|high), warning (none|door|lux|rain),\n"
           "            rxlost ignon speedvalid luxfault rainfault ignvalid gearvalid doorvalid lightvalid wipervalid (yes|no)\n"
           "    show                지금 상태,  r 처음 상태(C5)로,  q 종료\n"
           "  입력을 바꾸면 그 시각에 바로 판단하고, 바뀐 출력·판정을 '→'로 보여 줍니다. 다른 모듈은 실행되지 않습니다.\n");
}

/* ---- 판정 (e / obs) ---- */

static bool item_value(const ModCon *m, const char *item, int *v, const char *const **words, int *nwords)
{
    static const char *const ONOFF[] = {"off", "on"};
    static const char *const YESNO[] = {"no", "yes"};
    static const char *const WIPER_W[] = {"stop", "int", "low", "high"};
    static const char *const WARN_W[] = {"none", "door", "lux", "rain"};
    const VbcOutputs *o = &m->out;
    const VbcContext *x = &m->ctx;

    *words = ONOFF;
    *nwords = 2;
    if (ieq(item, "lock")) *v = o->door_locked;
    else if (ieq(item, "headlight")) *v = o->headlight_on;
    else if (ieq(item, "luxlamp")) *v = o->lux_fault_lamp;
    else if (ieq(item, "rainlamp")) *v = o->rain_fault_lamp;
    else if (ieq(item, "wiper")) { *v = (int)o->wiper; *words = WIPER_W; *nwords = 4; }
    else if (ieq(item, "warning")) { *v = (int)o->warning; *words = WARN_W; *nwords = 4; }
    else {
        *words = YESNO;
        if (ieq(item, "rxlost")) *v = x->rx_lost;
        else if (ieq(item, "ignon")) *v = x->ignition_on;
        else if (ieq(item, "speedvalid")) *v = x->speed_valid;
        else if (ieq(item, "luxfault")) *v = x->lux_fault;
        else if (ieq(item, "rainfault")) *v = x->rain_fault;
        else if (ieq(item, "ignvalid")) *v = x->ignition_valid;
        else if (ieq(item, "gearvalid")) *v = x->gear_valid;
        else if (ieq(item, "doorvalid")) *v = x->door_valid[0] && x->door_valid[1] && x->door_valid[2] && x->door_valid[3];
        else if (ieq(item, "lightvalid")) *v = x->light_sw_valid;
        else if (ieq(item, "wipervalid")) *v = x->wiper_sw_valid;
        else return false;
    }
    return true;
}

static const char *tc_label(const ModCon *m)
{
    return m->tc[0] ? m->tc : "-";
}

static void cmd_expect(ModCon *m, const char *item, const char *want)
{
    int v, nw;
    const char *const *w;

    if (!item || !want || !item_value(m, item, &v, &w, &nw)) {
        printf("  ! 사용법: e <항목> <값>  (h 참고)\n");
        return;
    }
    int expected = -1;
    for (int i = 0; i < nw; i++)
        if (ieq(want, w[i])) expected = i;
    if (expected < 0 && (ieq(want, "on") || ieq(want, "yes"))) expected = 1;
    if (expected < 0 && (ieq(want, "off") || ieq(want, "no"))) expected = 0;
    if (expected < 0) {
        printf("  ! %s의 값을 알 수 없습니다: %s\n", item, want);
        return;
    }
    m->checks++;
    if (v == expected) {
        printf("  ✔ PASS [%s] %s = %s  (%u.%03us)\n", tc_label(m), item, w[v],
               (unsigned)(m->now / 1000), (unsigned)(m->now % 1000));
    } else {
        m->failures++;
        m->tc_has_fail = true;
        printf("  ✘ FAIL [%s] %s: 기대 %s, 실제 %s  (%u.%03us)\n", tc_label(m), item, w[expected],
               (v >= 0 && v < nw) ? w[v] : "?", (unsigned)(m->now / 1000), (unsigned)(m->now % 1000));
    }
}

static void cmd_observe(ModCon *m, const char *item)
{
    int v, nw;
    const char *const *w;

    if (!item || !item_value(m, item, &v, &w, &nw)) {
        printf("  ! 사용법: obs <항목>  (h 참고)\n");
        return;
    }
    m->observations++;
    printf("  ◎ OBS  [%s] %s = %s  (%u.%03us) — 사양서에 정해지지 않은 동작, 관찰만 기록\n", tc_label(m), item,
           (v >= 0 && v < nw) ? w[v] : "?", (unsigned)(m->now / 1000), (unsigned)(m->now % 1000));
}

static void tc_close(ModCon *m)
{
    if (!m->tc[0])
        return;
    if (m->tc_has_fail)
        m->tc_failed++;
    m->tc[0] = '\0';
    m->tc_has_fail = false;
}

static void cmd_tc(ModCon *m, char *rest)
{
    tc_close(m);
    char *bar = strchr(rest, '|');
    if (bar)
        *bar = '\0';
    snprintf(m->tc, sizeof m->tc, "%s", trim(rest));
    m->tc_count++;
    printf("  ▶ TC %s%s%s\n", m->tc, bar ? " |" : "", bar ? bar + 1 : "");
}

/* ---- 명령 처리 ---- */

static void set_signal(ModCon *m, SigKey k, int32_t v)
{
    *sig_value(m, k) = v;
    if (SIGS[k].periodic && !(m->cut & SIGS[k].periodic))
        send(m, m->now, SIGS[k].periodic); /* 바뀐 값을 지금 시각에 보냄 */
    step_now(m);
}

/* 직전 상태 주입: 모듈 상태와 출력을 바꾸고 그 시각에 바로 판단한다 (D2처럼 규칙이 바로 바꾸면 '→'로 보임) */
static void set_state(ModCon *m, const char *arg)
{
    const Alias words[] = {{"off", 0}, {"on", 1}, {m->state_words[0], 0}, {m->state_words[1], 1}, {NULL, 0}};
    int32_t v;

    if (!arg || !parse_value(arg, words, &v) || (v != 0 && v != 1)) {
        printf("  ! %s on|off (0·off %s, 1·on %s). 모듈 상태라 0/1만 됩니다\n", m->state_cmd, m->state_words[0],
               m->state_words[1]);
        return;
    }
    bool before = m->mod_get_state(m);
    m->mod_set_state(m, v);
    printf("  [%3u.%03us] %s 주입: %s → %s\n", (unsigned)(m->now / 1000), (unsigned)(m->now % 1000), m->state_label,
           m->state_words[before], m->state_words[v]);
    step_now(m);
}

static void ask_and_set(ModCon *m, SigKey k)
{
    char buf[128];
    int32_t v;

    printf("  %s 코드: %s\n", SIGS[k].label, SIGS[k].codes);
    if (!read_line("  값 (아무 정수도 됨)> ", buf, sizeof buf) || !*trim(buf))
        return;
    if (!parse_value(trim(buf), SIGS[k].alias, &v)) {
        printf("  ! 정수(int32) 또는 이름(%s)을 입력하세요. 소수·글자는 이 신호의 형식(정수)으로 보낼 수 없습니다\n", SIGS[k].codes);
        return;
    }
    set_signal(m, k, v);
}

/* 한 줄 실행. q면 false */
static bool exec_line(ModCon *m, char *line)
{
    char *s = trim(line);
    char *save;

    if (!*s || s[0] == '#')
        return true;
    if (strncasecmp(s, "tc ", 3) == 0) { cmd_tc(m, s + 3); return true; }

    char *cmd = strtok_r(s, " \t", &save);
    char *a1 = strtok_r(NULL, " \t", &save);
    char *a2 = strtok_r(NULL, " \t", &save);

    if (ieq(cmd, "q") || ieq(cmd, "quit")) return false;
    if (ieq(cmd, "h") || ieq(cmd, "help")) { help(m); return true; }
    if (ieq(cmd, "show")) { print_status(m); return true; }
    /* 초기화해도 진행 중인 TC는 이어진다 (TC 안에서 "처음부터"를 만들 때 쓴다) */
    if (ieq(cmd, "r") || ieq(cmd, "reset")) { reset(m); printf("  처음 상태(C5), 시각 0초\n"); return true; }
    if (ieq(cmd, "e")) { cmd_expect(m, a1, a2); return true; }
    if (ieq(cmd, "obs")) { cmd_observe(m, a1); return true; }

    if (ieq(cmd, "t")) {
        char tb[64];
        long ms;
        if (!a1) {
            if (!read_line("  몇 초 진행할까요? (1ms 단위, 예: 0.499, 1, 2.9)> ", tb, sizeof tb))
                return false;
            a1 = trim(tb);
        }
        if (!parse_seconds(a1, &ms)) { printf("  ! 0.001초 이상, 600초 이하로 입력하세요\n"); return true; }
        advance(m, ms);
        return true;
    }
    if (ieq(cmd, "cut") || ieq(cmd, "resume") || ieq(cmd, "send")) {
        unsigned b = parse_periodic(a1);
        if (!b) { printf("  ! %s speed|gear|ign|lux|rain|vehicle|all\n", cmd); return true; }
        if (ieq(cmd, "cut")) {
            m->cut |= b;
            printf("  송신 끊음: %s (마지막 수신 후 0.5초가 지나야 판정이 바뀝니다)\n", a1);
        } else if (ieq(cmd, "resume")) {
            m->cut &= ~b;
            send(m, m->now, b);
            step_now(m);
        } else {
            send(m, m->now, b);
            printf("  %s 한 번 보냄 (%u.%03us)\n", a1, (unsigned)(m->now / 1000), (unsigned)(m->now % 1000));
            step_now(m);
        }
        return true;
    }
    if (ieq(cmd, "auto")) {
        if (a1 && (ieq(a1, "on") || ieq(a1, "off"))) {
            m->auto_send = ieq(a1, "on");
            printf("  자동 재송신 %s\n", m->auto_send ? "켬" : "끔");
        } else {
            printf("  ! auto on|off\n");
        }
        return true;
    }
    if (ieq(cmd, "period")) {
        int32_t v;
        if (a1 && a2 && parse_value(a2, NO_ALIAS, &v) && v >= 1 && v <= 10000 && (ieq(a1, "step") || ieq(a1, "send"))) {
            if (ieq(a1, "step")) m->step_period_ms = (vbc_ms_t)v; else m->send_period_ms = (vbc_ms_t)v;
            printf("  %s 주기 %dms\n", ieq(a1, "step") ? "판단" : "자동 송신", (int)v);
        } else {
            printf("  ! period step|send <1~10000 ms>\n");
        }
        return true;
    }

    if (m->state_cmd && ieq(cmd, m->state_cmd)) {
        char sb[64];
        if (!a1) {
            if (!read_line("  on(1) 또는 off(0)> ", sb, sizeof sb))
                return false;
            a1 = trim(sb);
        }
        set_state(m, a1);
        return true;
    }

    int k = find_sig(cmd);
    if (k >= 0) {
        int32_t v;
        if (!a1) { ask_and_set(m, (SigKey)k); return true; }
        if (!parse_value(a1, SIGS[k].alias, &v)) {
            printf("  ! %s: 정수(int32) 또는 이름(%s). 소수·글자는 신호 형식(정수)으로 보낼 수 없습니다\n", SIGS[k].cmd, SIGS[k].codes);
            return true;
        }
        set_signal(m, (SigKey)k, v);
        return true;
    }

    char *end;
    long n = strtol(cmd, &end, 10);
    if (!*end && n >= 1) {
        int no = 0;
        for (int i = 0; i < K_COUNT; i++)
            if ((m->show & SIGS[i].show_bit) && ++no == n) { ask_and_set(m, (SigKey)i); return true; }
        if (m->state_cmd && ++no == n) {
            char sb[64];
            if (read_line("  on(1) 또는 off(0)> ", sb, sizeof sb))
                set_state(m, trim(sb));
            return true;
        }
    }
    printf("  ! 알 수 없는 입력: %s  (h 도움말)\n", cmd);
    return true;
}

int modcon_run(ModCon *m)
{
    char buf[1024];

    reset(m);
    printf("%s 테스트 콘솔 (신호 주입기)\n  %s\n  h: 명령 목록\n", m->title, m->about);
    for (;;) {
        if (isatty(0))
            print_status(m);
        if (!read_line(isatty(0) ? "선택> " : "> ", buf, sizeof buf))
            break;
        if (!exec_line(m, buf))
            break;
    }
    tc_close(m);
    printf("\n== %s 결과: TC %d개 (FAIL 있는 TC %d개), 확인 %d건 중 FAIL %d건, 관찰 %d건\n", m->title, m->tc_count,
           m->tc_failed, m->checks, m->failures, m->observations);
    return m->failures ? 1 : 0;
}
