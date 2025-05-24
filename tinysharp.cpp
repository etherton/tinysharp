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
   auto v = hal::video::create("");
   v->fill(0,0,320,320,hal::rgb { 255,0,0 });
}
