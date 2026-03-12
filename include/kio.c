#include "kio.h"
#include "kdisplay.h"

void kio_write(const char* s) {
    if (!s) {
        return;
    }

    kdisplay_write(s);
}

void kio_writeln(const char* s) {
    if (s) {
        kdisplay_write(s);
    }

    kdisplay_write("\n");
}

void kio_clear(void) {
    kdisplay_clear();
}
