/*
 * modcon.c - 모듈별 대화형 테스트 콘솔의 공통 틀
 */

#define _POSIX_C_SOURCE 200809L /* isatty, strncasecmp */

#include "modcon.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

enum { CUT_SPEED = 1, CUT_GEAR = 2, CUT_IGN = 4, CUT_LUX = 8, CUT_RAIN = 16,
       CUT_VEHICLE = CUT_SPEED | CUT_GEAR | CUT_IGN, CUT_ALL = 31 };

static const char *const LIGHT_SW_NAMES[] = {"OFF", "AUTO", "ON"};
static const char *const WIPER_SW_NAMES[] = {"OFF", "AUTO", "LOW", "HIGH"};
static const char *const GEAR_NAMES[] = {"P", "R", "N", "D"};

/* 정해진 값 밖이면 "정의 안 됨(7)"처럼 */
static const char *sw_name(const char *const *names, int n, int v)
{
    static char buf[2][32];
    static int k;
    if (v >= 0 && v < n)
        return names[v];
    k ^= 1;
    snprintf(buf[k], sizeof buf[k], "정의 안 됨(%d)", v);
    return buf[k];
}

/* ---- 입력 읽기 ---- */

static bool read_line(const char *prompt, char *buf, size_t size)
{
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)size, stdin))
        return false;
    buf[strcspn(buf, "\r\n")] = '\0';
    if (!isatty(0))   /* 파일·파이프로 입력할 때는 입력을 화면에 보여 줌 */
        printf("%s\n", buf);
    return true;
}

static bool ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return false;
    return *a == *b;
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

/* 정수 하나. 사양서 범위 밖 값도 받는다 (고장·무효 시험). 모듈 인터페이스가 int32라 그 범위까지. 빈 입력이면 취소 */
static bool ask_int(const char *prompt, int32_t *out)
{
    char buf[64];
    char *end;

    if (!read_line(prompt, buf, sizeof buf))
        return false;
    char *s = trim(buf);
    if (!*s)
        return false;
    errno = 0;
    long long v = strtoll(s, &end, 10);
    if (*end) {
        printf("  ! 정수를 입력하세요 (소수·글자는 모듈 입력 형식(int32)이 아니라 넘길 수 없습니다)\n");
        return false;
    }
    if (errno == ERANGE || v < INT32_MIN || v > INT32_MAX) {
        printf("  ! int32 범위(%ld ~ %ld) 밖이라 넘길 수 없습니다\n", (long)INT32_MIN, (long)INT32_MAX);
        return false;
    }
    *out = (int32_t)v;
    return true;
}

/* 보기 중 하나: 번호(1부터) 또는 이름. 빈 입력이면 취소 */
static int ask_choice(const char *prompt, const char *const *names, int n)
{
    char buf[64];

    printf("%s (", prompt);
    for (int i = 0; i < n; i++)
        printf("%s%d %s", i ? ", " : "", i + 1, names[i]);
    printf(")\n");
    if (!read_line("  선택> ", buf, sizeof buf))
        return -1;
    char *s = trim(buf);
    if (!*s)
        return -1;
    for (int i = 0; i < n; i++) {
        if (ieq(s, names[i]))
            return i;
        char num[16];
        snprintf(num, sizeof num, "%d", i + 1);
        if (strcmp(s, num) == 0)
            return i;
    }
    /* "FL(앞 왼쪽)"처럼 괄호 설명이 붙은 이름은 앞부분만 맞아도 된다 */
    for (int i = 0; i < n; i++) {
        size_t len = strlen(s);
        if (strncasecmp(names[i], s, len) == 0 && (names[i][len] == '(' || names[i][len] == '\0'))
            return i;
    }
    printf("  ! 보기 중에서 고르세요\n");
    return -1;
}

/* ---- 신호 송신과 한 주기 실행 ---- */

static void send(ModCon *m, vbc_ms_t t, unsigned mask)
{
    unsigned s = mask & ~m->cut;

    if (s & CUT_SPEED) vbc_rx_speed(&m->in, t, m->speed);
    if (s & CUT_GEAR) vbc_rx_gear(&m->in, t, m->gear);
    if (s & CUT_IGN) vbc_rx_ignition(&m->in, t, m->ign);
    if (s & CUT_LUX) vbc_rx_lux(&m->in, t, m->lux);
    if (s & CUT_RAIN) vbc_rx_rain(&m->in, t, m->rain);
}

static void change(const ModCon *m, const char *label, const char *from, const char *to)
{
    printf("  → [%3u.%03us] %s: %s → %s\n", (unsigned)(m->now / 1000), (unsigned)(m->now % 1000), label, from, to);
}

static const char *on_off(bool b) { return b ? "켜짐" : "꺼짐"; }

/* 이번 시각으로 판정을 만들고 모듈 step을 한 번 부른다. 바뀐 출력 개수를 돌려준다 */
static int step(ModCon *m)
{
    VbcOutputs a = m->out;
    const VbcOutputs *b = &m->out;
    unsigned o = m->outputs;
    int n = 0;

    vbc_build_context(&m->in, m->now, &m->ctx);
    m->mod_step(m);

    if ((o & OUT_HEADLIGHT) && a.headlight_on != b->headlight_on) { change(m, "전조등", on_off(a.headlight_on), on_off(b->headlight_on)); n++; }
    if ((o & OUT_LUX_LAMP) && a.lux_fault_lamp != b->lux_fault_lamp) { change(m, "조도 고장 표시", on_off(a.lux_fault_lamp), on_off(b->lux_fault_lamp)); n++; }
    if ((o & OUT_WIPER) && a.wiper != b->wiper) { change(m, "와이퍼", vbc_wiper_name(a.wiper), vbc_wiper_name(b->wiper)); n++; }
    if ((o & OUT_RAIN_LAMP) && a.rain_fault_lamp != b->rain_fault_lamp) { change(m, "강우 고장 표시", on_off(a.rain_fault_lamp), on_off(b->rain_fault_lamp)); n++; }
    if ((o & OUT_LOCK) && a.door_locked != b->door_locked) { change(m, "도어", a.door_locked ? "잠김" : "풀림", b->door_locked ? "잠김" : "풀림"); n++; }
    if ((o & OUT_WARNING) && a.warning != b->warning) { change(m, "계기판 경고", vbc_warning_name(a.warning), vbc_warning_name(b->warning)); n++; }
    return n;
}

/* 값을 바꾼 직후: 바뀐 신호를 지금 시각에 보내고 바로 반영 */
static void apply(ModCon *m, unsigned mask)
{
    send(m, m->now, mask);
    if (step(m) == 0)
        printf("  → 출력 변화 없음\n");
}

static void advance(ModCon *m, vbc_ms_t ms)
{
    int changes = 0;
    vbc_ms_t end = m->now + ms;

    while (m->now + MODCON_TICK_MS <= end) {
        m->now += MODCON_TICK_MS;
        send(m, m->now, CUT_ALL);
        changes += step(m);
    }
    if (changes == 0)
        printf("  → %u.%03u초 동안 출력 변화 없음\n", (unsigned)(ms / 1000), (unsigned)(ms % 1000));
}

/* 콘솔이 정한 시작값 (계기판·와이퍼는 유효한 속도 판정을 위해 시동·기어를 미리 정해 둔다) */
static bool start_ign;
static Gear start_gear;

static void reset(ModCon *m)
{
    m->ign = start_ign;
    m->gear = start_gear;
    m->speed = 0;
    m->lux = 30000;
    m->rain = 0;
    m->cut = 0;
    m->wiper_level = WIPER_STOP;
    m->now = 0;
    vbc_inputs_init(&m->in);
    vbc_outputs_init(&m->out);
    m->mod_init(m);
    send(m, 0, CUT_ALL);
    step(m);
}

/* ---- 화면 ---- */

static void print_outputs(const ModCon *m)
{
    const VbcOutputs *o = &m->out;
    unsigned w = m->outputs;

    printf("  [출력]");
    if (w & OUT_HEADLIGHT) printf(" 전조등 %s |", on_off(o->headlight_on));
    if (w & OUT_LUX_LAMP) printf(" 조도 고장 표시 %s |", on_off(o->lux_fault_lamp));
    if (w & OUT_WIPER) printf(" 와이퍼 %s |", vbc_wiper_name(o->wiper));
    if (w & OUT_RAIN_LAMP) printf(" 강우 고장 표시 %s |", on_off(o->rain_fault_lamp));
    if (w & OUT_LOCK) printf(" 도어 %s |", o->door_locked ? "잠김" : "풀림");
    if (w & OUT_WARNING) printf(" 계기판 경고 %s |", vbc_warning_name(o->warning));
    printf("\n");
}

/* 판정: 모듈이 실제로 받는 값 (common.c가 C2~C4를 반영해 만든 것) */
static void print_judgment(const ModCon *m)
{
    const VbcContext *x = &m->ctx;

    printf("  [판정] 수신 중단 %s | 시동 %s | 유효한 속도 ", x->rx_lost ? "예" : "아니오", on_off(x->ignition_on));
    if (x->speed_valid) printf("%d km/h", (int)x->speed_kmh); else printf("없음");
    if (m->inputs & (IN_LUX | IN_CUT_LUX))
        printf(" | 조도 %s", x->lux_fault ? "고장" : x->lux_valid ? "정상" : "값 없음");
    if (m->inputs & (IN_RAIN | IN_CUT_RAIN))
        printf(" | 강우 %s", x->rain_fault ? "고장" : x->rain_valid ? "정상" : "값 없음");
    printf("\n");
}

static const char *cut_label(const ModCon *m, unsigned bits)
{
    return (m->cut & bits) == bits ? "끊김" : "보내는 중";
}

/* 메뉴 한 항목: 번호와 입력 종류 */
typedef struct { unsigned in; } Item;

static int build_items(const ModCon *m, Item *items)
{
    static const unsigned order[] = {IN_IGN, IN_GEAR, IN_SPEED, IN_LUX, IN_RAIN, IN_DOORS, IN_LIGHT_SW,
                                     IN_WIPER_SW, IN_WIPER_SW_CODE, IN_LIGHT_SW_CODE, IN_WIPER_LEVEL,
                                     IN_CUT_VEHICLE, IN_CUT_LUX, IN_CUT_RAIN};
    int n = 0;

    for (size_t i = 0; i < sizeof order / sizeof order[0]; i++)
        if (m->inputs & order[i])
            items[n++].in = order[i];
    return n;
}

static void print_menu(const ModCon *m, const Item *items, int n)
{
    printf("\n== %s ==  시각 %u.%03u초\n", m->title, (unsigned)(m->now / 1000), (unsigned)(m->now % 1000));
    print_outputs(m);
    print_judgment(m);
    printf("  [입력 바꾸기]\n");
    for (int i = 0; i < n; i++) {
        printf("   %2d) ", i + 1);
        switch (items[i].in) {
        case IN_IGN: printf("시동 [%s] (누르면 전환)", m->ign ? "ON" : "OFF"); break;
        case IN_GEAR: printf("기어 [%s]", GEAR_NAMES[m->gear]); break;
        case IN_SPEED: printf("속도 [%d km/h] (유효 0~250)", (int)m->speed); break;
        case IN_LUX: printf("조도 [%d lux] (유효 0~60000)", (int)m->lux); break;
        case IN_RAIN: printf("강우량 [%d %%] (유효 0~100)", (int)m->rain); break;
        case IN_DOORS:
            printf("도어 [FL %s, FR %s, RL %s, RR %s]", m->in.door_open[0] ? "열림" : "닫힘",
                   m->in.door_open[1] ? "열림" : "닫힘", m->in.door_open[2] ? "열림" : "닫힘",
                   m->in.door_open[3] ? "열림" : "닫힘");
            break;
        case IN_LIGHT_SW: printf("조명 스위치 [%s]", sw_name(LIGHT_SW_NAMES, 3, (int)m->in.light_sw)); break;
        case IN_WIPER_SW: printf("와이퍼 스위치 [%s]", sw_name(WIPER_SW_NAMES, 4, (int)m->in.wiper_sw)); break;
        case IN_LIGHT_SW_CODE: printf("조명 스위치를 코드로 직접 (0 OFF, 1 AUTO, 2 ON, 그 밖의 값도 그대로)"); break;
        case IN_WIPER_SW_CODE: printf("와이퍼 스위치를 코드로 직접 (0 OFF, 1 AUTO, 2 LOW, 3 HIGH, 그 밖의 값도 그대로)"); break;
        case IN_WIPER_LEVEL: printf("와이퍼 단계(조명에 넘길 값) [%s]", vbc_wiper_name(m->wiper_level)); break;
        case IN_CUT_VEHICLE: printf("속도·기어·시동 송신 [%s] (누르면 끊기/잇기, C4)", cut_label(m, CUT_VEHICLE)); break;
        case IN_CUT_LUX: printf("조도 송신 [%s] (누르면 끊기/잇기, C2)", cut_label(m, CUT_LUX)); break;
        case IN_CUT_RAIN: printf("강우량 송신 [%s] (누르면 끊기/잇기, C2)", cut_label(m, CUT_RAIN)); break;
        }
        printf("\n");
    }
    printf("  [시간·기타]  t) 시간 진행   r) 처음 상태로(C5)   h) 도움말   q) 종료\n");
}

static void edit(ModCon *m, unsigned what)
{
    int32_t v;
    int c;
    static const char *const WIPER_LEVEL_NAMES[] = {"정지", "간헐", "저속", "고속"};
    static const char *const DOOR_PICK[] = {"FL(앞 왼쪽)", "FR(앞 오른쪽)", "RL(뒤 왼쪽)", "RR(뒤 오른쪽)", "ALL(모두)"};
    static const char *const OPEN_CLOSE[] = {"열림", "닫힘"};

    switch (what) {
    case IN_IGN:
        m->ign = !m->ign;
        printf("  시동 %s\n", m->ign ? "ON" : "OFF");
        apply(m, CUT_IGN);
        break;
    case IN_GEAR:
        if ((c = ask_choice("  기어", GEAR_NAMES, 4)) >= 0) { m->gear = (Gear)c; apply(m, CUT_GEAR); }
        break;
    case IN_SPEED:
        if (ask_int("  속도 km/h (범위 밖 값도 됨, 예: 300)> ", &v)) { m->speed = v; apply(m, CUT_SPEED); }
        break;
    case IN_LUX:
        if (ask_int("  조도 lux (범위 밖 값도 됨, 예: 70000)> ", &v)) { m->lux = v; apply(m, CUT_LUX); }
        break;
    case IN_RAIN:
        if (ask_int("  강우량 % (범위 밖 값도 됨, 예: 101)> ", &v)) { m->rain = v; apply(m, CUT_RAIN); }
        break;
    case IN_DOORS:
        if ((c = ask_choice("  어느 문", DOOR_PICK, 5)) >= 0) {
            int s = ask_choice("  상태", OPEN_CLOSE, 2);
            if (s >= 0) {
                for (int i = 0; i < DOOR_COUNT; i++)
                    if (c == 4 || c == i)
                        vbc_set_door(&m->in, (DoorId)i, s == 0);
                apply(m, 0);
            }
        }
        break;
    case IN_LIGHT_SW:
        if ((c = ask_choice("  조명 스위치", LIGHT_SW_NAMES, 3)) >= 0) { vbc_set_light_switch(&m->in, (LightSwitch)c); apply(m, 0); }
        break;
    case IN_WIPER_SW:
        if ((c = ask_choice("  와이퍼 스위치", WIPER_SW_NAMES, 4)) >= 0) { vbc_set_wiper_switch(&m->in, (WiperSwitch)c); apply(m, 0); }
        break;
    case IN_LIGHT_SW_CODE:
        if (ask_int("  조명 스위치 코드 (정수, 예: 7, -1)> ", &v)) { vbc_set_light_switch(&m->in, (LightSwitch)v); apply(m, 0); }
        break;
    case IN_WIPER_SW_CODE:
        if (ask_int("  와이퍼 스위치 코드 (정수, 예: 7, -1)> ", &v)) { vbc_set_wiper_switch(&m->in, (WiperSwitch)v); apply(m, 0); }
        break;
    case IN_WIPER_LEVEL:
        if ((c = ask_choice("  와이퍼 단계", WIPER_LEVEL_NAMES, 4)) >= 0) { m->wiper_level = (WiperLevel)c; apply(m, 0); }
        break;
    case IN_CUT_VEHICLE:
    case IN_CUT_LUX:
    case IN_CUT_RAIN: {
        unsigned bits = what == IN_CUT_VEHICLE ? CUT_VEHICLE : what == IN_CUT_LUX ? CUT_LUX : CUT_RAIN;
        if ((m->cut & bits) == bits) {
            m->cut &= ~bits;
            printf("  송신 다시 시작\n");
            apply(m, bits);
        } else {
            m->cut |= bits;
            printf("  송신 끊음 (0.5초가 지나야 수신 중단·고장으로 판정됩니다. t로 시간을 진행해 보세요)\n");
        }
        break;
    }
    }
}

static void help(const ModCon *m)
{
    printf("\n  %s\n", m->about);
    printf("  - 번호를 눌러 입력을 바꾸면 그 시각에 바로 모듈을 실행하고, 바뀐 출력을 '→'로 보여 줍니다.\n");
    printf("  - 시간은 t로만 흐릅니다. 시간이 흐르는 동안 입력은 0.05초마다 자동으로 다시 보내집니다 (사양서 3.1).\n");
    printf("  - [판정]은 모듈이 실제로 받는 값입니다 (common.c가 C2~C4를 반영). 다른 모듈은 실행되지 않습니다.\n");
    printf("  - 값을 묻는 곳에서 그냥 Enter를 누르면 취소됩니다.\n");
}

int modcon_run(ModCon *m)
{
    Item items[16];
    int n = build_items(m, items);
    char buf[64];

    start_ign = m->ign;
    start_gear = m->gear;
    reset(m);
    printf("%s 테스트 콘솔\n", m->title);
    help(m);
    for (;;) {
        print_menu(m, items, n);
        if (!read_line("선택> ", buf, sizeof buf))
            break;
        char *s = trim(buf);
        if (!*s)
            continue;
        if (ieq(s, "q"))
            break;
        if (ieq(s, "h")) { help(m); continue; }
        if (ieq(s, "r")) { reset(m); printf("  처음 상태(C5)로 되돌림, 시각 0초\n"); continue; }
        if (ieq(s, "t")) {
            char tb[32];
            if (!read_line("  몇 초 진행할까요? (0.05초 단위, 예: 0.5, 1, 2.9)> ", tb, sizeof tb))
                break;
            char *end;
            double sec = strtod(trim(tb), &end);
            long ms = (long)(sec * 1000.0 + 0.5);
            if (end == trim(tb) || *end || sec <= 0 || sec > 600 || ms % (long)MODCON_TICK_MS) {
                printf("  ! 0.05초 단위의 양수로 입력하세요\n");
                continue;
            }
            advance(m, (vbc_ms_t)ms);
            continue;
        }
        char *end;
        long k = strtol(s, &end, 10);
        if (*end || k < 1 || k > n) {
            printf("  ! 메뉴 번호, t, r, h, q 중에서 고르세요\n");
            continue;
        }
        edit(m, items[k - 1].in);
    }
    printf("종료\n");
    return 0;
}
