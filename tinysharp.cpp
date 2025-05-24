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
   int top = 24, middle = 320 - 24 - 48, bottom = 200-16; // top + middle - 8;
   v->setFixedRegions(top,middle,bottom);
   for (int i=0; i<480; i+=8) {
        char buf[16];
        if (i < top)
            sprintf(buf,"TopFixed %d",i);
        else if (i < top + middle)
            sprintf(buf,"Scroll %d",i - top);
        else
            sprintf(buf,"BottomFixed %d",i-top-middle);
        v->drawString(i/2,i,buf,hal::rgb { 128, 128, 255 }, hal::rgb{});
   }

   uint8_t r = 128;
   int line = top;
    while (true) {
        char buf[15];
        sprintf(buf,"Scroll %d",line);
        v->drawString(100,0,buf,hal::rgb{0,0,255},hal::rgb{});
        sleep_ms(16);
       // v->fill(0,0,320,480,hal::rgb { 255,255,255 });
       // v->fill(0,0,320,480,hal::rgb { 0,0,0 });
       v->setScroll(line);
       if (++line == top+middle)
        line = top;
    }
}
