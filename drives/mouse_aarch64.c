/*
 * mouse_aarch64.c — Mouse stubs for aarch64
 * No PS/2 mouse on aarch64.
 */
#include "mouse.h"

void mouse_init(void) {}
int mouse_get_scroll(void) { return 0; }
