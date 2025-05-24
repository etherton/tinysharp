#include <stdio.h>
#include "pico/stdlib.h"
#include "drivers/video.h"

int main()
{
    stdio_init_all();

    /* while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    } */
   auto v = hal::video::create("bpp=16");
   v->init();
   uint8_t r = 128;
    while (true) {
       v->fill(0,0,320,320,hal::rgb { r++,255,0 });
        printf("Hello, world!\n");
        sleep_ms(10);
    }
}
