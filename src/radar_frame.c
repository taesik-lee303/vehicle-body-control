/*
 * radar_frame.c - 레이더 장착 정보와 제원 (요구사항 8.1), 검출점 유효 범위 (2.2)
 */

#include "radar_frame.h"

const RadarMount RADAR_MOUNTS[RADAR_COUNT] = {
    [RADAR_FR] = {"FR", 0.0f, 0.0f, 0.5f, 0.0f, 60.0f, 15.0f, 0.5f, 200.0f, true, 512,
                  0.2f, 1.0f, 2.0f, 0.1f, 0.05f, 0.2f, 0.1f, 0.05f, 200.0f},
    [RADAR_FCL] = {"FCL", -0.2f, 0.75f, 0.5f, 45.0f, 75.0f, 8.0f, 0.2f, 80.0f, false, 128,
                   0.3f, 2.0f, 0.0f, 0.2f, 0.1f, 0.5f, 0.0f, 0.1f, 80.0f},
    [RADAR_FCR] = {"FCR", -0.2f, -0.75f, 0.5f, -45.0f, 75.0f, 8.0f, 0.2f, 80.0f, false, 128,
                   0.3f, 2.0f, 0.0f, 0.2f, 0.1f, 0.5f, 0.0f, 0.1f, 80.0f},
};

bool radar_detection_valid(const RadarMount *m, const RadarDetection *d)
{
    if (!(d->range_m >= m->range_min_m && d->range_m <= m->range_max_m))
        return false;
    if (!(d->az_deg >= -m->az_fov_deg && d->az_deg <= m->az_fov_deg))
        return false;
    if (m->has_elevation && !(d->el_deg >= -m->el_fov_deg && d->el_deg <= m->el_fov_deg))
        return false;
    if (!(d->vr_mps >= -70.0f && d->vr_mps <= 70.0f))
        return false;
    if (!(d->snr_db >= 0.0f && d->snr_db <= 60.0f))
        return false;
    return true;
}
