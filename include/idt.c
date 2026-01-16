#include "idt.h"

static short chega_eu_nao_aguento_mais = 1;

void division_error_handler(){
    if (chega_eu_nao_aguento_mais){
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("Zero Division Error");
        chega_eu_nao_aguento_mais = 0;
    }

    asm("hlt");
}