/*
 * drive.c - 주행 모델 (R1~R5)
 * 담당: F (이태식)
 */

#include "drive.h"

/* km/h per second × ms = 1/1000 km/h */
static int32_t delta_mkmh(int32_t rate_kmh_per_s, vbc_ms_t dt_ms)
{
    return rate_kmh_per_s * (int32_t)dt_ms;
}

static int32_t max_mkmh(Gear gear)
{
    return (gear == GEAR_R ? DRIVE_MAX_REVERSE : DRIVE_MAX_FORWARD) * 1000;
}

void drive_init(Drive *d)
{
    d->speed_mkmh = 0;
    d->gear = GEAR_P;
    d->ignition = false;
    d->throttle = THROTTLE_NONE;
    d->brake = false;
}

void drive_set_pedals(Drive *d, Throttle throttle, bool brake)
{
    d->throttle = throttle;
    d->brake = brake;
}

bool drive_request_ignition(Drive *d, bool on)
{
    if (d->ignition == on)
        return true;
    if (d->speed_mkmh != 0) /* R1 */
        return false;
    d->ignition = on;
    return true;
}

bool drive_request_gear(Drive *d, Gear gear)
{
    if (d->gear == gear)
        return true;
    if (d->speed_mkmh != 0) /* R2 */
        return false;
    d->gear = gear;
    return true;
}

void drive_step(Drive *d, vbc_ms_t dt_ms)
{
    int32_t v = d->speed_mkmh;
    bool can_move = d->ignition && (d->gear == GEAR_D || d->gear == GEAR_R); /* R1, R3 */

    if (!d->ignition) {
        v = 0; /* R1 */
    } else if (d->brake) {
        v -= delta_mkmh(DRIVE_BRAKE, dt_ms); /* R4: 브레이크 우선 */
    } else if (d->throttle != THROTTLE_NONE && can_move) {
        v += delta_mkmh(d->throttle == THROTTLE_FULL ? DRIVE_ACCEL_FULL : DRIVE_ACCEL_NORMAL, dt_ms);
    } else {
        v -= delta_mkmh(DRIVE_COAST, dt_ms); /* R4: 가속도 브레이크도 없음. P·N 가속도 여기 (R3) */
    }

    if (v < 0)
        v = 0;
    if (v > max_mkmh(d->gear)) /* R5 */
        v = max_mkmh(d->gear);
    d->speed_mkmh = v;
}

int32_t drive_speed_kmh(const Drive *d)
{
    return (d->speed_mkmh + 500) / 1000;
}

void drive_set_speed_kmh(Drive *d, int32_t kmh)
{
    int32_t v = kmh < 0 ? 0 : kmh * 1000;

    if (v > max_mkmh(d->gear))
        v = max_mkmh(d->gear);
    d->speed_mkmh = v;
}
