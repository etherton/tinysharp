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
   auto v = hal::video::create("bpp=3");
   v->init();
   v->setFont(8,8,&font8x8_basic[0][0]);
   v->fill(0,0,320,320,hal::rgb{0,255,0});

   auto k = hal::keyboard::create("");
   k->init();
   uint32_t state[4];

   int top = 24, bottom = 0, middle = 320 - top - bottom;
   v->setFixedRegions(top,bottom);
   for (int i=0; i<480; i+=8) {
        char buf[16];
        sprintf(buf,"Line %d",i);
        v->drawString(i/2,i,buf,hal::rgb { 128, 128, 255 }, hal::rgb{});
   }

   uint8_t r = 128;
   int line = top;
    while (true) {
        char buf[40];
        sprintf(buf,"Scroll %d",line);
        v->drawString(100,0,buf,hal::rgb{0,0,255},hal::rgb{});
        // k->getState(state);
        sprintf(buf,"%08x%08x",state[0],state[1]);
        v->drawString(100,8,buf,hal::rgb{255,255,255},hal::rgb{});
        sleep_ms(16);
       // v->fill(0,0,320,480,hal::rgb { 255,255,255 });
       // v->fill(0,0,320,480,hal::rgb { 0,0,0 });
       v->setScroll(line);
       if (++line == top+middle)
        line = top;
    }
}
