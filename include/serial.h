#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_init_irq(void);
void serial_putchar(char c);
void serial_print(const char* s);
int serial_can_read(void);
char serial_getchar(void);
int serial_read_nonblock(char* out);
void serial_irq_handler(void);

#endif
