#include <stdio.h>
#include "pico/stdlib.h"
#include "drivers/video.h"
#include "drivers/keyboard.h"
#include "drivers/timer.h"

#include "font8x8_basic.h"

namespace hal { extern uint32_t actual_speed; }

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

   uint32_t fillTime = hal::getUsTime32();
   v->fill(0,0,320,480,hal::rgb{0,255,0}); // fill should be green
   fillTime = hal::getUsTime32() - fillTime;
   // 21.2ms for 3bpp (14.0ms unrolled 32x, 12.6ms unrolled 64x)
   // 59.6ms for 16bpp
   // 167ms for 18bpp/24bpp

    v->drawStringf(100,100,hal::rgb{255,255,255},"Fill time %u us",fillTime);
    v->drawStringf(100,120,hal::rgb{255,0,0},"actual spi speed %u",hal::actual_speed);
   while(1);
#if 0
   auto k = hal::keyboard::create("");
   k->init();

   int top = 24, bottom = 0, middle = 320 - top - bottom;
   v->setFixedRegions(top,bottom);
   for (int i=0; i<480; i+=8) {
        char buf[16];
        sprintf(buf,"Line %d",i); // Line textshould be blue
        v->drawString(i/2,i,buf,hal::rgb { 0, 0, uint8_t(128 + i/4) }, hal::rgb{});
   }

   int line = top;
   uint8_t bat = k->getBattery();
   uint16_t event = 0;
    while (true) {
        char buf[40];
        sprintf(buf,"Scroll %d",line); // scroll should be red
        v->drawString(100,0,buf,hal::rgb{255,0,0},hal::rgb{});
        uint16_t thisEvent = k->getKeyEvent();
        if (thisEvent)
            event = thisEvent;
        sprintf(buf,"event %04x (%c) bat %d",event,event >> 8,bat);
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
    #endif
}
