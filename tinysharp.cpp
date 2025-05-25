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
   auto v = hal::video::create("bpp=16");
   v->init();
   v->setFont(8,8,&font8x8_basic[0][0]);

   uint32_t fillTime = hal::getUsTime32();
   v->fill(0,0,320,480,hal::green); // fill should be green
   fillTime = hal::getUsTime32() - fillTime;
   // 21.2ms for 3bpp (14.0ms unrolled 32x, 12.6ms unrolled 64x)
   // 59.6ms for 16bpp
   // 167ms for 18bpp/24bpp

   int top = 24, bottom = 0, middle = 320 - top - bottom;
   v->setFixedRegions(top,bottom);
   for (int i=0; i<480; i+=8) {
        char buf[16];
        v->drawStringf(i/2,i,hal::rgb { 0, 0, uint8_t(128 + i/4) }, hal::rgb{},"Line %d",i);
   }

    v->drawStringf(100,100,hal::white,"Fill time %u us",fillTime);
    // v->drawStringf(100,120,hal::rgb{255,0,0},"actual spi speed %u",hal::actual_speed);


    fillTime = hal::getUsTime32();
    for (int i=0; i<40; i++)
        v->drawString(0,i*8,hal::white,hal::black,
            "1234567890abcdefghijklmnopqrstuvwxyz!@#$");
    fillTime = hal::getUsTime32() - fillTime;
    v->drawStringf(100,140,hal::white,hal::blue,"%u us to draw chars",fillTime);
    // 52.0ms for 16bpp with 8 bit writes
    // 50.9ms for 16bpp with 16 bit writes
    
    auto k = hal::keyboard::create("");
    k->init();

   int line = top;
   uint8_t bat = k->getBattery();
   uint16_t event = 0;
    while (true) {
        v->drawStringf(100,0,hal::rgb{255,0,0},hal::rgb{},"Scroll %d",line);
        uint16_t thisEvent = k->getKeyEvent();
        if (thisEvent)
            event = thisEvent;
        v->drawStringf(100,8,hal::rgb{255,255,255},hal::rgb{},"event %04x (%c) bat %d",event,event? event & 255 : ' ',bat);
        sleep_ms(16);
       // v->fill(0,0,320,480,hal::rgb { 255,255,255 });
       // v->fill(0,0,320,480,hal::rgb { 0,0,0 });
       v->setScroll(line);
       if (++line == (bottom? top+middle : 160 - top)) {
        line = top;
        bat = k->getBattery();
       }
    }
}
