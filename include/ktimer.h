#ifndef KTIMER_H
#define KTIMER_H

#include "utypes.h"

void ktimer_init(uint32_t hz);
void ktimer_irq_handler(void);

#endif