/*
 * kdisplay.h — PV: Pudim na Vitrine (Pudding in the Display Case)
 *
 * Decides how to show the pudding to the customer:
 * if there is a glass display case (VGA monitor), use it;
 * otherwise serve through the kitchen window (serial).
 */
#ifndef KDISPLAY_H
#define KDISPLAY_H

#define KDISPLAY_MODE_VGA    0  /* VGA + serial mirror */
#define KDISPLAY_MODE_SERIAL 1  /* serial only (no monitor) */

void kdisplay_init(void);
int  kdisplay_mode(void);

/* Unified output — routes to VGA or serial based on mode */
void kdisplay_write(const char* s);
void kdisplay_clear(void);
void kdisplay_putchar(char c);

#endif
