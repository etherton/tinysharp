#include "hal/video.h"
#include "hal/keyboard.h"
#include "hal/storage_pico_flash.h"
#include "hal/storage_pico_sdcard.h"
#include "hal/timer.h"

#include "fs/mbr.h"
#include "fs/fat.h"

#include <stdio.h>
#include "pico/stdlib.h"

#include "ide/editor.h"

#include "font-4x7.h"
#include "font-5x8.h"
#include "font-6x8.h"
#include "font-8x8.h"

int main()
{
    stdio_init_all();

    auto k = hal::keyboard::create("");
    hal::video::create("bpp=3");

    hal::video::setFont(6,8,console_font_6x8,0);

    ide::editor e(hal::storage::create("flash"));

    auto sd = hal::storage_pico_sdcard::create();
    if (sd) {
        char *f = new char[512];
        e.setFile(f,512,512,true);
        e.setHex();
        sd->readBlock(0,f);
        fs::mbr &m = *(fs::mbr*)f;
        printf("signature %x (%u)\n",m.signature.get(),sizeof(m));
        printf("partition 1 type %d lba %u size %u\n",m.partitions[0].type,m.partitions[0].lba.get(),m.partitions[0].sizeInSectors.get());
    }
    else if (!e.quickLoad(false))
        e.newFile();
    e.draw();
    for (;;) {
        auto ev = k->waitKeyEvent(10);
        if (ev) {
            switch (ev) {
                case hal::modifier::LALT_BIT | hal::modifier::PRESSED_BIT | '1': hal::video::create("bpp=16"); break;
                case hal::modifier::LALT_BIT | hal::modifier::PRESSED_BIT | '2': hal::video::create("bpp=24"); break;
                case hal::modifier::LALT_BIT | hal::modifier::PRESSED_BIT | '3': hal::video::create("bpp=3"); break;
                case hal::modifier::LALT_BIT | hal::modifier::PRESSED_BIT | '4': hal::video::setFont(4,7,console_font_4x7,0); break;
                case hal::modifier::LALT_BIT | hal::modifier::PRESSED_BIT | '5': hal::video::setFont(5,8,console_font_5x8,0); break;
                case hal::modifier::LALT_BIT | hal::modifier::PRESSED_BIT | '6': hal::video::setFont(6,8,console_font_6x8,0); break;
                case hal::modifier::LALT_BIT | hal::modifier::PRESSED_BIT | '8': hal::video::setFont(8,8,console_font_8x8,0); break;
                default: e.update(ev);
            }
            e.draw();
        }
        else
            e.drawCursor();
        // printf("heartbeat\n");
    }
}
