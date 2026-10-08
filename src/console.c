/*
 * console.c - 터미널 명령 입력 (입력/시연)
 * 담당: F (이태식)
 */

#include "console.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 256
#define MAX_TOKENS 3
#define MAX_WAIT_MS 600000u /* wait 한 번에 최대 10분 */
#define VALUE_LIMIT 1000000 /* 숫자 입력 허용 범위 (범위 밖 시험값 포함) */

/* 변화를 표시할 항목 */
typedef struct {
    VbcOutputs out;
    bool rx_lost;
    bool lux_fault;
    bool rain_fault;
} Watch;

static const char *const WIPER_WORDS[] = {"stop", "int", "low", "high"};
static const char *const GEAR_WORDS[] = {"p", "r", "n", "d"};
static const char *const WARNING_WORDS[] = {"none", "door", "lux", "rain"};

/* ---- 출력 도우미 ---- */

static void say(const Console *c, const char *fmt, ...)
{
    va_list ap;

    if (!c->out)
        return;
    va_start(ap, fmt);
    vfprintf(c->out, fmt, ap);
    va_end(ap);
}

static ConsoleResult err(const Console *c, const char *fmt, ...)
{
    va_list ap;

    if (c->out) {
        fputs("  ! ", c->out);
        va_start(ap, fmt);
        vfprintf(c->out, fmt, ap);
        va_end(ap);
        fputc('\n', c->out);
    }
    return CONSOLE_ERROR;
}

static unsigned sec_part(vbc_ms_t t) { return (unsigned)(t / 1000u); }
static unsigned ms_part(vbc_ms_t t) { return (unsigned)(t % 1000u); }

static const char *on_off(bool b) { return b ? "켜짐" : "꺼짐"; }
static const char *yes_no(bool b) { return b ? "예" : "아니오"; }

static const char *light_sw_name(LightSwitch sw)
{
    switch (sw) {
    case LIGHT_SW_OFF: return "OFF";
    case LIGHT_SW_AUTO: return "AUTO";
    case LIGHT_SW_ON: return "ON";
    }
    return "?";
}

static const char *wiper_sw_name(WiperSwitch sw)
{
    switch (sw) {
    case WIPER_SW_OFF: return "OFF";
    case WIPER_SW_AUTO: return "AUTO";
    case WIPER_SW_LOW: return "LOW";
    case WIPER_SW_HIGH: return "HIGH";
    }
    return "?";
}

/* ---- 변화 표시 ---- */

static Watch watch_take(const Console *c)
{
    Watch w;

    w.out = c->sys.out;
    w.rx_lost = c->sys.ctx.rx_lost;
    w.lux_fault = c->sys.ctx.lux_fault;
    w.rain_fault = c->sys.ctx.rain_fault;
    return w;
}

static void change(const Console *c, const char *label, const char *from, const char *to)
{
    say(c, "  [%3u.%03us] %s: %s → %s\n", sec_part(c->now), ms_part(c->now), label, from, to);
}

static void report_changes(const Console *c, const Watch *before)
{
    Watch now = watch_take(c);
    const VbcOutputs *a = &before->out;
    const VbcOutputs *b = &now.out;

    if (a->headlight_on != b->headlight_on)
        change(c, "전조등", on_off(a->headlight_on), on_off(b->headlight_on));
    if (a->lux_fault_lamp != b->lux_fault_lamp)
        change(c, "조도 고장 표시", on_off(a->lux_fault_lamp), on_off(b->lux_fault_lamp));
    if (a->wiper != b->wiper)
        change(c, "와이퍼", vbc_wiper_name(a->wiper), vbc_wiper_name(b->wiper));
    if (a->rain_fault_lamp != b->rain_fault_lamp)
        change(c, "강우 고장 표시", on_off(a->rain_fault_lamp), on_off(b->rain_fault_lamp));
    if (a->door_locked != b->door_locked)
        change(c, "도어", a->door_locked ? "잠김" : "풀림", b->door_locked ? "잠김" : "풀림");
    if (a->warning != b->warning)
        change(c, "계기판 경고", vbc_warning_name(a->warning), vbc_warning_name(b->warning));
    if (before->rx_lost != now.rx_lost)
        change(c, "(판정) 차량 상태 수신 중단", yes_no(before->rx_lost), yes_no(now.rx_lost));
    if (before->lux_fault != now.lux_fault)
        change(c, "(판정) 조도 센서 고장", yes_no(before->lux_fault), yes_no(now.lux_fault));
    if (before->rain_fault != now.rain_fault)
        change(c, "(판정) 강우 센서 고장", yes_no(before->rain_fault), yes_no(now.rain_fault));
}

/* ---- 입력 송신과 시간 진행 ---- */

/* mask 중 송신 중단이 아닌 입력을 t 시각에 보낸다 */
static void send_signals(Console *c, vbc_ms_t t, unsigned mask)
{
    VbcInputs *in = &c->sys.in;
    const Panel *p = &c->panel;
    unsigned m = mask & ~p->cut;

    if (m & CON_SPEED) vbc_rx_speed(in, t, p->speed);
    if (m & CON_GEAR) vbc_rx_gear(in, t, p->gear);
    if (m & CON_IGNITION) vbc_rx_ignition(in, t, p->ignition);
    if (m & CON_LUX) vbc_rx_lux(in, t, p->lux);
    if (m & CON_RAIN) vbc_rx_rain(in, t, p->rain);
}

/* 현재 시각에서 한 주기 실행하고 바뀐 것을 표시 */
static void step_now(Console *c)
{
    Watch before = watch_take(c);

    vbc_step(&c->sys, c->now);
    report_changes(c, &before);
}

/* 주기 입력 값을 바꾼 직후: 바뀐 입력을 지금 시각에 보내고 바로 반영 */
static void apply_now(Console *c, unsigned mask)
{
    send_signals(c, c->now, mask);
    step_now(c);
}

void console_advance(Console *c, vbc_ms_t ms)
{
    vbc_ms_t end = c->now + ms;

    while (c->now + CONSOLE_TICK_MS <= end) {
        c->now += CONSOLE_TICK_MS;
        if (c->drive_on) {
            drive_step(&c->drive, CONSOLE_TICK_MS);
            c->panel.speed = drive_speed_kmh(&c->drive);
        }
        send_signals(c, c->now, CON_ALL);
        step_now(c);
    }
}

void console_init(Console *c, FILE *out)
{
    vbc_init(&c->sys);
    c->panel.ignition = false;
    c->panel.gear = GEAR_P;
    c->panel.speed = 0;
    c->panel.lux = 30000; /* 낮 */
    c->panel.rain = 0;
    c->panel.cut = 0;
    c->now = 0;
    c->out = out;
    c->expect_failures = 0;
    drive_init(&c->drive);
    c->drive_on = false;

    send_signals(c, 0, CON_ALL);
    vbc_step(&c->sys, 0);
}

/* ---- 파싱 ---- */

static bool ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return false;
    return *a == *b;
}

static bool parse_int(const char *s, int32_t *v)
{
    char *end;
    long x;

    if (!s)
        return false;
    x = strtol(s, &end, 10);
    if (end == s || *end != '\0' || x < -VALUE_LIMIT || x > VALUE_LIMIT)
        return false;
    *v = (int32_t)x;
    return true;
}

/* "2.9" → 2900ms */
static bool parse_seconds(const char *s, vbc_ms_t *ms)
{
    char *end;
    double sec;

    if (!s)
        return false;
    sec = strtod(s, &end);
    if (end == s || *end != '\0' || !(sec > 0.0) || sec * 1000.0 > (double)MAX_WAIT_MS)
        return false;
    *ms = (vbc_ms_t)(sec * 1000.0 + 0.5);
    return true;
}

static int parse_on_off(const char *s)
{
    if (!s) return -1;
    if (ieq(s, "on")) return 1;
    if (ieq(s, "off")) return 0;
    return -1;
}

static int parse_word(const char *s, const char *const *words, int count)
{
    if (!s)
        return -1;
    for (int i = 0; i < count; i++)
        if (ieq(s, words[i]))
            return i;
    return -1;
}

static unsigned parse_signal(const char *s)
{
    if (!s) return 0;
    if (ieq(s, "speed")) return CON_SPEED;
    if (ieq(s, "gear")) return CON_GEAR;
    if (ieq(s, "ign") || ieq(s, "ignition")) return CON_IGNITION;
    if (ieq(s, "lux")) return CON_LUX;
    if (ieq(s, "rain")) return CON_RAIN;
    if (ieq(s, "vehicle")) return CON_VEHICLE;
    if (ieq(s, "all")) return CON_ALL;
    return 0;
}

/* ---- 명령 ---- */

static ConsoleResult cmd_gear(Console *c, const char *a)
{
    static const char *const GEARS[] = {"p", "r", "n", "d"};
    int g = parse_word(a, GEARS, 4);

    if (g < 0)
        return err(c, "사용법: gear P|R|N|D");
    if (c->drive_on && !drive_request_gear(&c->drive, (Gear)g)) {
        say(c, "  ✘ 거부(R2): 기어 변경은 정지 상태(속도 0)에서만 됩니다. 현재 %s, 속도 %dkm/h\n",
            vbc_gear_name(c->drive.gear), (int)c->panel.speed);
        return CONSOLE_OK;
    }
    c->panel.gear = (Gear)g;
    apply_now(c, CON_GEAR);
    return CONSOLE_OK;
}

static ConsoleResult cmd_number(Console *c, const char *a, int32_t *field, unsigned sig,
                                const char *usage)
{
    int32_t v;

    if (!parse_int(a, &v))
        return err(c, "사용법: %s", usage);
    *field = v;
    apply_now(c, sig);
    return CONSOLE_OK;
}

/* drive on|off: 속도·기어·시동을 운전 조작으로 만들지(on) 직접 입력할지(off) */
static ConsoleResult cmd_drive(Console *c, const char *a)
{
    int on = parse_on_off(a);

    if (on < 0)
        return err(c, "사용법: drive on|off");
    if (on && !c->drive_on) {
        drive_init(&c->drive);
        c->drive.ignition = c->panel.ignition;
        c->drive.gear = c->panel.gear;
        drive_set_speed_kmh(&c->drive, c->panel.speed);
        c->panel.speed = drive_speed_kmh(&c->drive);
    }
    c->drive_on = on;
    drive_set_pedals(&c->drive, THROTTLE_NONE, false);
    say(c, "  주행 모델 %s\n", on ? "켜짐 (pedal, gear, ign으로 운전)" : "꺼짐 (speed로 직접 입력)");
    return CONSOLE_OK;
}

/* pedal <스로틀 0|1|2> <브레이크 0|1>: 누르고 있는 상태 (다음 pedal 명령까지 유지) */
static ConsoleResult cmd_pedal(Console *c, const char *t, const char *b)
{
    int32_t th, br;

    if (!c->drive_on)
        return err(c, "pedal은 drive on 이후에 쓸 수 있습니다");
    if (!parse_int(t, &th) || !parse_int(b, &br) || th < 0 || th > 2 || br < 0 || br > 1)
        return err(c, "사용법: pedal <스로틀 0|1|2> <브레이크 0|1>  (2 = 풀 스로틀)");
    drive_set_pedals(&c->drive, (Throttle)th, br != 0);
    return CONSOLE_OK;
}

static ConsoleResult cmd_door(Console *c, const char *which, const char *state)
{
    static const char *const DOORS[] = {"fl", "fr", "rl", "rr"};
    int d = parse_word(which, DOORS, 4);
    bool all = which && ieq(which, "all");
    bool open;

    if ((d < 0 && !all) || !state)
        return err(c, "사용법: door FL|FR|RL|RR|ALL open|close");
    if (ieq(state, "open"))
        open = true;
    else if (ieq(state, "close") || ieq(state, "closed"))
        open = false;
    else
        return err(c, "사용법: door FL|FR|RL|RR|ALL open|close");

    for (int i = 0; i < DOOR_COUNT; i++)
        if (all || i == d)
            vbc_set_door(&c->sys.in, (DoorId)i, open);
    step_now(c);
    return CONSOLE_OK;
}

static ConsoleResult cmd_cut_resume(Console *c, const char *a, bool cut)
{
    unsigned m = parse_signal(a);

    if (!m)
        return err(c, "사용법: %s speed|gear|ign|lux|rain|vehicle|all", cut ? "cut" : "resume");
    if (cut) {
        c->panel.cut |= m;
        say(c, "  %s 송신 중단 (%u.%03us부터)\n", a, sec_part(c->now), ms_part(c->now));
    } else {
        c->panel.cut &= ~m;
        apply_now(c, m);
    }
    return CONSOLE_OK;
}

static ConsoleResult cmd_wait(Console *c, const char *a)
{
    vbc_ms_t ms;

    if (!parse_seconds(a, &ms))
        return err(c, "사용법: wait <초>  (예: wait 2.9, 최대 600)");
    if (ms % CONSOLE_TICK_MS != 0)
        return err(c, "wait는 %u.%03u초 단위로 입력하세요", sec_part(CONSOLE_TICK_MS),
                   ms_part(CONSOLE_TICK_MS));
    console_advance(c, ms);
    say(c, "  → %u.%03us  전조등 %s | 와이퍼 %s | 도어 %s | 경고 %s\n",
        sec_part(c->now), ms_part(c->now), on_off(c->sys.out.headlight_on),
        vbc_wiper_name(c->sys.out.wiper), c->sys.out.door_locked ? "잠김" : "풀림",
        vbc_warning_name(c->sys.out.warning));
    return CONSOLE_OK;
}

/* expect <항목> <기대값>: 지금 출력이 기대와 같은지 확인하고 기록 */
static ConsoleResult cmd_expect(Console *c, const char *item, const char *want)
{
    const VbcOutputs *o = &c->sys.out;
    const char *const *words = NULL; /* NULL이면 on/off 항목 */
    int count = 0;
    int actual;
    int expected;

    if (!item || !want)
        return err(c, "사용법: expect <항목> <기대값>  (help 참고)");

    if (ieq(item, "speed")) { /* 지금 보내는 속도(km/h)와 숫자로 비교 */
        int32_t w;
        if (!parse_int(want, &w))
            return err(c, "speed의 기대값은 숫자입니다: %s", want);
        if (c->panel.speed == w) {
            say(c, "  ✔ speed %d  (%u.%03us)\n", (int)w, sec_part(c->now), ms_part(c->now));
        } else {
            c->expect_failures++;
            say(c, "  ✘ speed: 기대 %d, 실제 %d  (%u.%03us)\n", (int)w, (int)c->panel.speed,
                sec_part(c->now), ms_part(c->now));
        }
        return CONSOLE_OK;
    }

    if (ieq(item, "headlight")) actual = o->headlight_on;
    else if (ieq(item, "ign")) actual = c->panel.ignition;
    else if (ieq(item, "gear")) { actual = (int)c->panel.gear; words = GEAR_WORDS; count = 4; }
    else if (ieq(item, "luxlamp")) actual = o->lux_fault_lamp;
    else if (ieq(item, "rainlamp")) actual = o->rain_fault_lamp;
    else if (ieq(item, "lock")) actual = o->door_locked;
    else if (ieq(item, "rxlost")) actual = c->sys.ctx.rx_lost;
    else if (ieq(item, "wiper")) { actual = (int)o->wiper; words = WIPER_WORDS; count = 4; }
    else if (ieq(item, "warning")) { actual = (int)o->warning; words = WARNING_WORDS; count = 4; }
    else
        return err(c, "expect 항목: headlight luxlamp wiper rainlamp lock warning rxlost ign gear speed");

    expected = words ? parse_word(want, words, count) : parse_on_off(want);
    if (expected < 0)
        return err(c, "%s의 기대값을 알 수 없습니다: %s", item, want);

    if (actual == expected) {
        say(c, "  ✔ %s %s  (%u.%03us)\n", item, want, sec_part(c->now), ms_part(c->now));
    } else {
        c->expect_failures++;
        say(c, "  ✘ %s: 기대 %s, 실제 %s  (%u.%03us)\n", item, want,
            words ? words[actual] : (actual ? "on" : "off"), sec_part(c->now), ms_part(c->now));
    }
    return CONSOLE_OK;
}

ConsoleResult console_exec(Console *c, const char *line)
{
    char buf[MAX_LINE];
    char *tok[MAX_TOKENS + 1];
    int n = 0;
    char *hash;
    const char *cmd;
    const char *a1;
    const char *a2;

    snprintf(buf, sizeof buf, "%s", line);
    hash = strchr(buf, '#');
    if (hash)
        *hash = '\0';
    for (char *t = strtok(buf, " \t\r\n"); t && n <= MAX_TOKENS; t = strtok(NULL, " \t\r\n"))
        tok[n++] = t;
    if (n == 0)
        return CONSOLE_OK;
    if (n > MAX_TOKENS)
        return err(c, "인자가 너무 많습니다. help 참고");

    cmd = tok[0];
    a1 = n > 1 ? tok[1] : NULL;
    a2 = n > 2 ? tok[2] : NULL;

    if (ieq(cmd, "ign") || ieq(cmd, "ignition")) {
        int on = parse_on_off(a1);
        if (on < 0)
            return err(c, "사용법: ign on|off");
        if (c->drive_on && !drive_request_ignition(&c->drive, on)) {
            say(c, "  ✘ 거부(R1): 시동은 정지 상태(속도 0)에서만 켜고 끌 수 있습니다. 속도 %dkm/h\n",
                (int)c->panel.speed);
            return CONSOLE_OK;
        }
        c->panel.ignition = on;
        apply_now(c, CON_IGNITION);
        return CONSOLE_OK;
    }
    if (ieq(cmd, "gear"))
        return cmd_gear(c, a1);
    if (ieq(cmd, "speed") && c->drive_on)
        return err(c, "drive on 중에는 속도가 페달로 정해집니다. 직접 입력하려면 drive off");
    if (ieq(cmd, "speed"))
        return cmd_number(c, a1, &c->panel.speed, CON_SPEED, "speed <km/h>  (유효 0~250)");
    if (ieq(cmd, "drive"))
        return cmd_drive(c, a1);
    if (ieq(cmd, "pedal"))
        return cmd_pedal(c, a1, a2);
    if (ieq(cmd, "lux"))
        return cmd_number(c, a1, &c->panel.lux, CON_LUX, "lux <lux>  (유효 0~60000)");
    if (ieq(cmd, "rain"))
        return cmd_number(c, a1, &c->panel.rain, CON_RAIN, "rain <%>  (유효 0~100)");
    if (ieq(cmd, "door"))
        return cmd_door(c, a1, a2);
    if (ieq(cmd, "light")) {
        static const char *const SW[] = {"off", "auto", "on"};
        int sw = parse_word(a1, SW, 3);
        if (sw < 0)
            return err(c, "사용법: light off|auto|on");
        vbc_set_light_switch(&c->sys.in, (LightSwitch)sw);
        step_now(c);
        return CONSOLE_OK;
    }
    if (ieq(cmd, "wiper")) {
        static const char *const SW[] = {"off", "auto", "low", "high"};
        int sw = parse_word(a1, SW, 4);
        if (sw < 0)
            return err(c, "사용법: wiper off|auto|low|high");
        vbc_set_wiper_switch(&c->sys.in, (WiperSwitch)sw);
        step_now(c);
        return CONSOLE_OK;
    }
    if (ieq(cmd, "cut"))
        return cmd_cut_resume(c, a1, true);
    if (ieq(cmd, "resume"))
        return cmd_cut_resume(c, a1, false);
    if (ieq(cmd, "wait"))
        return cmd_wait(c, a1);
    if (ieq(cmd, "expect"))
        return cmd_expect(c, a1, a2);
    if (ieq(cmd, "show") || ieq(cmd, "status")) {
        console_print_status(c);
        return CONSOLE_OK;
    }
    if (ieq(cmd, "reset")) {
        int failures = c->expect_failures;
        console_init(c, c->out);
        c->expect_failures = failures;
        say(c, "  초기 상태(C5)로 되돌림, 시각 0.000s\n");
        return CONSOLE_OK;
    }
    if (ieq(cmd, "help") || ieq(cmd, "?")) {
        console_print_help(c);
        return CONSOLE_OK;
    }
    if (ieq(cmd, "quit") || ieq(cmd, "exit"))
        return CONSOLE_QUIT;

    return err(c, "알 수 없는 명령: %s  (help 참고)", cmd);
}

/* ---- 화면 ---- */

void console_print_status(const Console *c)
{
    const Panel *p = &c->panel;
    const VbcContext *x = &c->sys.ctx;
    const VbcOutputs *o = &c->sys.out;
    const int32_t *door = c->sys.in.door_open;

    say(c, "  시각 %u.%03us\n", sec_part(c->now), ms_part(c->now));
    say(c, "  [입력] 시동 %s  기어 %s  속도 %dkm/h  조도 %dlux  강우 %d%%\n",
        p->ignition ? "ON" : "OFF", vbc_gear_name(p->gear), (int)p->speed, (int)p->lux, (int)p->rain);
    say(c, "         도어 FL:%s FR:%s RL:%s RR:%s  조명SW %s  와이퍼SW %s\n",
        door[DOOR_FL] ? "열림" : "닫힘", door[DOOR_FR] ? "열림" : "닫힘",
        door[DOOR_RL] ? "열림" : "닫힘", door[DOOR_RR] ? "열림" : "닫힘",
        light_sw_name((LightSwitch)c->sys.in.light_sw), wiper_sw_name((WiperSwitch)c->sys.in.wiper_sw));
    if (p->cut) {
        say(c, "         송신 중단:%s%s%s%s%s\n",
            p->cut & CON_SPEED ? " speed" : "", p->cut & CON_GEAR ? " gear" : "",
            p->cut & CON_IGNITION ? " ign" : "", p->cut & CON_LUX ? " lux" : "",
            p->cut & CON_RAIN ? " rain" : "");
    }
    say(c, "  [판정] 수신 중단 %s  시동 %s  유효한 속도 ", yes_no(x->rx_lost), on_off(x->ignition_on));
    if (x->speed_valid)
        say(c, "%dkm/h", (int)x->speed_kmh);
    else
        say(c, "없음");
    say(c, "  조도 %s  강우 %s\n", x->lux_fault ? "고장" : "정상", x->rain_fault ? "고장" : "정상");
    say(c, "  [출력] 전조등 %s  조도 고장 표시 %s  와이퍼 %s  강우 고장 표시 %s\n",
        on_off(o->headlight_on), on_off(o->lux_fault_lamp), vbc_wiper_name(o->wiper),
        on_off(o->rain_fault_lamp));
    say(c, "         도어 %s  계기판 경고 %s\n", o->door_locked ? "잠김" : "풀림",
        vbc_warning_name(o->warning));
}

void console_print_help(const Console *c)
{
    say(c,
        "  [차량 상태] 0.05초마다 자동으로 다시 보냄\n"
        "    ign on|off              시동\n"
        "    gear P|R|N|D            기어\n"
        "    speed <km/h>            속도 (유효 0~250, 범위 밖 값도 그대로 보냄)\n"
        "    lux <lux>               조도 (유효 0~60000)\n"
        "    rain <%%>                강우량 (유효 0~100)\n"
        "  [주행 모델 R1~R5]\n"
        "    drive on|off            on: 속도·기어·시동을 운전 조작으로 만듦 (off: speed로 직접 입력)\n"
        "    pedal <0|1|2> <0|1>     스로틀(0 없음, 1 보통, 2 풀) 브레이크(0|1). 다음 pedal까지 유지\n"
        "                            ign, gear는 정지 상태에서만 바뀜 (R1, R2)\n"
        "  [운전자 조작]\n"
        "    door FL|FR|RL|RR|ALL open|close\n"
        "    light off|auto|on       조명 스위치\n"
        "    wiper off|auto|low|high 와이퍼 스위치\n"
        "  [고장 시험]\n"
        "    cut speed|gear|ign|lux|rain|vehicle|all     송신 중단 (C2, C4)\n"
        "    resume speed|gear|ign|lux|rain|vehicle|all  송신 재개\n"
        "  [시간·확인]\n"
        "    wait <초>               시간 진행 (0.05초 단위). 그 사이 바뀐 출력을 시각과 함께 표시\n"
        "    show                    입력·판정·출력 전체 보기\n"
        "    expect <항목> <값>      기대 확인. 항목: headlight luxlamp rainlamp lock rxlost ign (on|off),\n"
        "                            speed (km/h 숫자), gear (p|r|n|d),\n"
        "                            wiper (stop|int|low|high), warning (none|door|lux|rain)\n"
        "    reset                   초기 상태(C5)로\n"
        "    help, quit\n"
        "  '#' 뒤는 주석. 같은 명령을 파일에 적어 ./build/vbc <파일> 로 실행할 수 있다.\n");
}
