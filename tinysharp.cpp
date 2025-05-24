#include <stdio.h>
#include "pico/stdlib.h"
#include "drivers/video.h"

#include "font8x8_basic.h"

int main()
{
    stdio_init_all();

    /* while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    } */
   auto v = hal::video::create("bpp=16");
   v->init();
   v->setFont(8,8,&font8x8_basic[0][0]);
   v->drawString(120,120,"Hello!",hal::rgb { 128, 128, 255 }, hal::rgb{});

   uint8_t r = 128;
   int line = 0;
    while (true) {
       v->fill(0,0,100,100,hal::rgb { 255,255,255 });
       v->fill(0,0,100,100,hal::rgb { 0,0,0 });
       v->setScroll(++line & 63);
    }
}
