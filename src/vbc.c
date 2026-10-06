/*
 * vbc.c - 시스템 통합
 * 담당: A (이태식)
 */

#include "vbc.h"

void vbc_init(VbcSystem *sys)
{
    vbc_inputs_init(&sys->in);
    vbc_build_context(&sys->in, 0, &sys->ctx);
    vbc_outputs_init(&sys->out);
    wipers_init(&sys->wipers);
    lighting_init(&sys->lighting);
    doors_init(&sys->doors);
    dashboard_init(&sys->dashboard);
}

void vbc_step(VbcSystem *sys, vbc_ms_t now)
{
    vbc_build_context(&sys->in, now, &sys->ctx);

    wipers_step(&sys->wipers, &sys->ctx, &sys->out);
    lighting_step(&sys->lighting, &sys->ctx, sys->out.wiper, &sys->out);
    doors_step(&sys->doors, &sys->ctx, &sys->out);
    dashboard_step(&sys->dashboard, &sys->ctx, &sys->out);
}
