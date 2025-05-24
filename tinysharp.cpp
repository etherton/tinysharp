#include <stdio.h>
#include "pico/stdlib.h"
#include "drivers/video.h"
#include "drivers/keyboard.h"

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
   v->fill(0,0,320,480,hal::rgb{0,255,0}); // fill should be green

   auto k = hal::keyboard::create("");
   k->init();
   uint32_t state[4];

   int top = 24, bottom = 0, middle = 320 - top - bottom;
   v->setFixedRegions(top,bottom);
   for (int i=0; i<480; i+=8) {
        char buf[16];
        sprintf(buf,"Line %d",i); // Line textshould be blue
        v->drawString(i/2,i,buf,hal::rgb { 0, 0, uint8_t(128 + i/4) }, hal::rgb{});
   }

   int line = top;
   uint8_t bat = k->getBattery();
    while (true) {
        char buf[40];
        sprintf(buf,"Scroll %d",line); // scroll should be red
        v->drawString(100,0,buf,hal::rgb{255,0,0},hal::rgb{});
        k->getState(state);
        sprintf(buf,"%08x%08x bat %d",state[0],state[1],bat);
        v->drawString(100,8,buf,hal::rgb{255,255,255},hal::rgb{});
        sleep_ms(16);
       // v->fill(0,0,320,480,hal::rgb { 255,255,255 });
       // v->fill(0,0,320,480,hal::rgb { 0,0,0 });
       v->setScroll(line);
       if (++line == top+middle) {
        line = top;
        bat = k->getBattery();
       }
    }
}
