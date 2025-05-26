#include "hal/video.h"
#include "hal/keyboard.h"
#include "hal/storage_pico_flash.h"
#include "hal/timer.h"

#include "ide/editor.h"

#include "font-8x8.h"

int main()
{
    auto v = hal::video::create("bpp=3");
    auto k = hal::keyboard::create("");

    v->init();
    v->setFont(8,8,console_font_8x8,0);

    ide::editor e(new hal::storage_pico_flash);

    if (!e.quickLoad(true))
        e.newFile();
    for (;;) {
        e.draw();
        e.update(k->waitKeyEvent());
    }
}
