/*
 * consoles.h - 모듈별 콘솔 정의 함수 (터미널 main과 시뮬레이터가 함께 쓴다)
 */

#ifndef CONSOLES_H
#define CONSOLES_H

#include "modcon.h"

void con_ecu_define(ModCon *m);
void con_doors_define(ModCon *m);
void con_dashboard_define(ModCon *m);
void con_lighting_define(ModCon *m);
void con_wipers_define(ModCon *m);

#endif /* CONSOLES_H */
