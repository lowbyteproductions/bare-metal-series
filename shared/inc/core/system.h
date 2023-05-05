#ifndef INC_SYSTEM_H
#define INC_SYSTEM_H

#include "common-defines.h"

#define CPU_FREQ      (84000000)
#define SYSTICK_FREQ  (1000)

void system_setup(void);
void system_teardown(void);
uint64_t system_get_ticks(void);
void system_delay(uint64_t milleseconds);

#endif // INC_SYSTEM_H
