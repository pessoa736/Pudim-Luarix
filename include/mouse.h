#ifndef MOUSE_H
#define MOUSE_H

void mouse_init(void);
void mouse_irq_handler(void);
int mouse_get_scroll(void);

#endif
