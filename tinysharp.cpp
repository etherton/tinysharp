#include "hal/video.h"
#include "hal/keyboard.h"
#include "hal/storage_pico_flash.h"
#include "hal/timer.h"

#include <stdio.h>
#include "pico/stdlib.h"

#include "ide/editor.h"

#include "font-8x8.h"

int main()
{
    stdio_init_all();

    auto k = hal::keyboard::create("");

    auto v = hal::video::create("bpp=3");
    v->setFont(8,8,console_font_8x8,0);
    v->fill(0,0,hal::video::getScreenWidth(),hal::video::getScreenHeight(),hal::blue);
    hal::palette p;
    v->setColor(p,hal::red,hal::white);

    /*ide::editor e(new hal::storage_pico_flash);

    if (!e.quickLoad(true))
        e.newFile(); */
    int n = 0;
    for (;;) {
        //e.draw();
        //auto ev = k->getKeyEvent();
        sleep_ms(16);
        //e.update(k->getKeyEvent());
        //printf("heartbeat\n");
        v->drawStringf(280,300,p,"%d",++n);
    }
}
