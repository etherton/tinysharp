#include <stdio.h>
#include "pico/stdlib.h"
#include "drivers/video.h"
#include "drivers/keyboard.h"
#include "drivers/timer.h"

#include "font-6x8.h"
#include "font-8x8.h"
#include "font-4x6.h"

namespace hal { extern uint32_t actual_speed; }

int main()
{
    stdio_init_all();

    /* while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    } */
   auto v = hal::video::create("bpp=18");
   v->init();
   v->setFont(6,8,console_font_6x8,0);

   uint32_t fillTime = hal::getUsTime32();
   v->fill(0,0,320,16,hal::black);
   v->fill(0,16,320,480-16,hal::green); // fill should be green
   fillTime = hal::getUsTime32() - fillTime;
   // 21.2ms for 3bpp (14.0ms unrolled 32x, 12.6ms unrolled 64x)
   // 59.6ms for 16bpp
   // 167ms for 18bpp/24bpp
   int top = 24, bottom = 0, middle = 320 - top - bottom;
   hal::palette wh, bl, gr, re;
   v->setColor(wh, hal::white, hal::black);
   v->setColor(bl, hal::blue, hal::white);
   v->setColor(gr, hal::green, hal::black);
   v->setColor(re, hal::red, hal::black);

   v->setFixedRegions(top,bottom);
   for (int i=16; i<480; i+=8) {
        char buf[16];
        v->drawStringf(i/2,i,bl,"Line %d",i);
   }

    v->drawStringf(100,100,hal::white,"Fill time %u us",fillTime);
    // v->drawStringf(100,120,hal::rgb{255,0,0},"actual spi speed %u",hal::actual_speed);

    v->setFont(8,8,console_font_8x8,0);

    fillTime = hal::getUsTime32();
    for (int i=2; i<40; i++)
        v->drawString(0,i*v->getFontHeight(),wh,
            "1234567890abcdefghijklmnopqrstuvwxyz!@#$");
    fillTime = hal::getUsTime32() - fillTime;
    v->drawStringf(100,140,bl,"%u us to draw chars",fillTime);
    // 52.0ms for 16bpp with 8 bit writes
    // 50.9ms for 16bpp with 16 bit writes
    // 11.2ms for 3bpp
    // 49.8ms for 16bpp
    // 93.1ms for 18bpp
    
    auto k = hal::keyboard::create("");
    k->init();

    v->setFont(8,8,console_font_8x8,0);

   int line = top;
   uint8_t bat = k->getBattery();
   uint16_t event = 0;
    while (true) {
        //v->drawStringf(100,0,re,"Scroll %d",line);
        uint16_t thisEvent = k->getKeyEvent();
        if ((uint8_t)thisEvent)
            event = thisEvent;
        v->drawStringf(100,16,wh,"event %04x (%s) bat %d",event,event? hal::keyboard::sm_Labels[event & 255] : "???",bat);
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
