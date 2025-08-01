#include "hal/video.h"
#include "hal/keyboard.h"
#include "hal/storage_pico_flash.h"
#include "hal/storage_pico_sdcard.h"
#include "hal/timer.h"

#include "fs/mbr.h"
#include "fs/fat_structs.h"
#include "fs/volume_fat.h"

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

#include "ide/editor.h"

#include "font-4x6.h"
#include "font-5x8.h"
#include "font-6x8.h"
#include "font-8x8.h"

void walkTree(fs::volume &v,fs::directoryEntry *de,uint8_t level) {
    fs::directory d;
    if (v.openDir(d,de)) {
        fs::directoryEntry e;
        while (v.readDir(d,e)) {
            for (uint8_t i=0; i<level; i++)
                printf(" ");
            printf("[%s]\n",e.filename);
            if (e.directory && strcmp(e.filename,".") && strcmp(e.filename,".."))
                walkTree(v,&e,level+2);
        }
    }
}

int main()
{
    stdio_init_all();

    auto k = hal::keyboard::create("");
    hal::video::create("bpp=3");

    hal::video::setFont(6,8,console_font_6x8,0);

    ide::editor e(hal::storage::create("flash"));

    auto sd = hal::storage_pico_sdcard::create();
    auto volume = fs::volumeFat::create(sd);
    walkTree(*volume,nullptr,0);

    fs::directoryEntry de;
    if (volume->locateEntry(de,"/ADVENT.BAS")) {
        //printf("found file! %u bytes, cluster %d\n",de.size,de.firstCluster);
        uint32_t roundedSize = (de.size + 512) & ~511;
        char *s = new char[roundedSize];
        volume->readFile(de,s,0,de.size);
        e.setFile(s,de.size,roundedSize);
        e.convertNewlines();
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
                case hal::modifier::LALT_BIT | hal::modifier::PRESSED_BIT | '4': hal::video::setFont(4,6,console_font_4x6,0); break;
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
