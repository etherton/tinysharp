#include "hal/video.h"
#include "hal/keyboard.h"
#include "hal/storage_pico_flash.h"
#include "hal/timer.h"

/* #include <stdio.h>
#include "pico/stdlib.h" */

#include "ide/editor.h"

//#include "font-4x6.h"
#include "font-6x8.h"
//#include "font-8x8.h"

int main()
{
    // stdio_init_all();

    auto k = hal::keyboard::create("");
    auto v = hal::video::create("bpp=3");

    //v->setFont(4,6,console_font_4x6,0);
    v->setFont(6,8,console_font_6x8,0);
    //v->setFont(8,8,console_font_8x8,0);

    ide::editor e(hal::storage::create("flash"));

    if (!e.quickLoad(false))
        e.newFile();
    e.draw();
    for (;;) {
        auto ev = k->waitKeyEvent(10);
        if (ev) {
            e.update(ev);
            e.draw();
        }
        else
            e.drawCursor();
        // printf("heartbeat\n");
    }
}
