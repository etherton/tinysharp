// Based on lcd.c from https://github.com/benob/picocalc-umac/

#include "video_pico.h"

#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"

#include <string.h>

namespace hal {

#define LCD_SCK 10
#define LCD_TX  11
#define LCD_RX  12
#define LCD_CS  13
#define LCD_DC  14
#define LCD_RST 15

void __not_in_flash_func(video_pico::sendCommands)(const uint8_t *commands,size_t length) {
    while (length) {
        gpio_put(LCD_DC, 0);
        spi_write_blocking(spi1, commands, 1);
        gpio_put(LCD_DC, 1);
        uint8_t argc = commands[1];
        if (argc)
          spi_write_blocking(spi1, commands+2, argc);
        commands += argc + 2;
        length -= (argc + 2);
    }
}

void __not_in_flash_func(video_pico::setRegion)(int x1,int y1,int w,int h) {
    int x2 = x1 + w - 1, y2 = y1 + h - 1;
    uint8_t commands[] = {
        0x2A, 4, uint8_t(x1>>8), uint8_t(x1), uint8_t(x2>>8), uint8_t(x2),
        0x2B, 4, uint8_t(y1>>8), uint8_t(y1), uint8_t(y2>>8), uint8_t(y2),
        0x2C, 0
    };
    gpio_put(LCD_CS, 0);
    sendCommands(commands,sizeof(commands));
}

void video_pico::setScroll(int y) {
    uint8_t commands[] = { 0x37, 2, uint8_t(y>>8), uint8_t(y) };
    gpio_put(LCD_CS, 0);
    sendCommands(commands,sizeof(commands));
    gpio_put(LCD_CS, 1);
}

void video_pico::setFixedRegions(int top,int bottom) {
    int middle = 320 - top - bottom;
    if (bottom)
        bottom = 160 + bottom;
    uint8_t commands[] = { 0x33, 6, 
      uint8_t(top>>8), uint8_t(top), 
      uint8_t(middle>>8), uint8_t(middle),
      uint8_t(bottom>>8), uint8_t(bottom) };
    gpio_put(LCD_CS, 0);
    sendCommands(commands,sizeof(commands));
    gpio_put(LCD_CS, 1);
}

void __not_in_flash_func(video_pico_3bpp::draw)(int x,int y,int w,int h,const void *data) {
    setRegion(x,y,w,h);
    spi_write_blocking(spi1,static_cast<const uint8_t*>(data),((w*h)+1)>>1);
    gpio_put(LCD_CS, 1);
}

void __not_in_flash_func(video_pico_3bpp::fill)(int x,int y,int w,int h,rgb c) {
    setRegion(x,y,w,h);
    int count = ((w * h) + 1)  >> 1;
    uint8_t packed2 = pack3(c);
    if (count==1)
        spi_write_blocking(spi1,&packed2,1);
    else {
        uint8_t run[8] = { packed2, packed2, packed2, packed2, packed2, packed2, packed2, packed2 };
        while (count >= 8) {
            spi_write_blocking(spi1,run,8);
            count-=8;
        }
        if (count)
            spi_write_blocking(spi1,run,count);
    }
    gpio_put(LCD_CS, 1);
}

void __not_in_flash_func(video_pico_16bpp::draw)(int x,int y,int w,int h,const void *data) {
    setRegion(x,y,w,h);
    spi_write_blocking(spi1,static_cast<const uint8_t*>(data),w*h*2);
    gpio_put(LCD_CS, 1);
}

void __not_in_flash_func(video_pico_16bpp::fill)(int x,int y,int w,int h,rgb c) {
    setRegion(x,y,w,h);
    int count = w*h;
    uint16_t packed = pack16(c);
    if (count >= 8) {
        uint16_t run[8] = { packed, packed, packed, packed, packed, packed, packed, packed };
        while (count >= 8) {
            spi_write_blocking(spi1,reinterpret_cast<const uint8_t*>(run),16);
            count -= 8;
        }
        if (count)
            spi_write_blocking(spi1,reinterpret_cast<const uint8_t*>(run),count<<1);
    }
    else
        while (count--)
            spi_write_blocking(spi1,reinterpret_cast<const uint8_t*>(&packed),2);
    gpio_put(LCD_CS, 1);
}    

void __not_in_flash_func(video_pico_18bpp::draw)(int x,int y,int w,int h,const void *data) {
    setRegion(x,y,w,h);
    spi_write_blocking(spi1,static_cast<const uint8_t*>(data),w*h*3);
    gpio_put(LCD_CS, 1);
}

void __not_in_flash_func(video_pico_18bpp::fill)(int x,int y,int w,int h,rgb c) {
    setRegion(x,y,w,h);
    int count = w*h;
    while (count--)
      spi_write_blocking(spi1,&c.r,3);
    gpio_put(LCD_CS, 1);
} 

void __not_in_flash_func(video_pico_24bpp::draw)(int x,int y,int w,int h,const void *data) {
    setRegion(x,y,w,h);
    spi_write_blocking(spi1,static_cast<const uint8_t*>(data),w*h*3);
    gpio_put(LCD_CS, 1);
}

void __not_in_flash_func(video_pico_24bpp::fill)(int x,int y,int w,int h,rgb c) {
    setRegion(x,y,w,h);
    int count = w*h;
    while (count--)
      spi_write_blocking(spi1,&c.r,3);
    gpio_put(LCD_CS, 1);
} 

#define LCD_SPI_SPEED       250000000 // (105 * 1000000)

#define LATCLR              5
#define LATSET              6

static void pin_set_bit(int pin, unsigned int offset) {
  switch (offset) {
    case LATCLR:
      gpio_set_pulls(pin, false, false);
      gpio_pull_down(pin);
      gpio_put(pin, 0);
      return;
    case LATSET:
      gpio_set_pulls(pin, false, false);
      gpio_pull_up(pin);
      gpio_put(pin, 1);
      return;
  }
}

void video_pico::initCommon(const uint8_t *memoryMode,size_t memoryModeSize) {
    // Init GPIO
    gpio_init(LCD_SCK);
    gpio_init(LCD_TX);
    gpio_init(LCD_RX);
    gpio_init(LCD_CS);
    gpio_init(LCD_DC);
    gpio_init(LCD_RST);

    gpio_set_dir(LCD_SCK, GPIO_OUT);
    gpio_set_dir(LCD_TX, GPIO_OUT);
    //gpio_set_dir(LCD_RX, GPIO_IN);
    gpio_set_dir(LCD_CS, GPIO_OUT);
    gpio_set_dir(LCD_DC, GPIO_OUT);
    gpio_set_dir(LCD_RST, GPIO_OUT);

    // Init SPI
    spi_init(spi1, LCD_SPI_SPEED);
    gpio_set_function(LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(LCD_TX, GPIO_FUNC_SPI);
    gpio_set_function(LCD_RX, GPIO_FUNC_SPI);
    gpio_set_input_hysteresis_enabled(LCD_RX, true);

    gpio_put(LCD_CS, 1);
    gpio_put(LCD_RST, 1);

    // Reset controller
    pin_set_bit(LCD_RST, LATSET);
    sleep_ms(10);
    pin_set_bit(LCD_RST, LATCLR);
    sleep_ms(10);
    pin_set_bit(LCD_RST, LATSET);
    sleep_ms(200);

    // Setup LCD
    gpio_put(LCD_CS, 0);

    static const uint8_t initCommands1[] = {
        // Positive Gamma Control
        0xE0, 15, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F,
        // Negative Gamma Control
        0xE1, 15, 0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F,
        0xC0, 2, 0x17, 0x15,          // Power Control 1
        0xC1, 1, 0x41,                // Power Control 2
        0xC5, 3, 0x00, 0x12, 0x80,    // VCOM Control
        0x36, 1, 0x40,                // Memory Access Control (0x48=BGR, 0x40=RGB)
    };
    static const uint8_t initCommands2[] = {
        0xB0, 1, 0x00,                // Interface Mode Control

        // Frame Rate Control
        0xB1, 2, 0xD0, 0x11,        // 60Hz
        ///0xB1, 2, 0xD0, 0x14,          // 90Hz
        0x21, 0,                      // Invert colors on
        0xB4, 1, 0x02,                // Display Inversion Control
        0xB6, 3, 0x02, 0x02, 0x3B,    // Display Function Control
        0xB7, 1, 0xC6,                // Entry Mode Set
        0xE9, 1, 0x00,
        0xF7, 4, 0xA9, 0x51, 0x2C, 0x82,  // Adjust Control 3
        0x11, 0,                      // Exit Sleep
        0x29, 0,
    };
    sendCommands(initCommands1,sizeof(initCommands1));
    sendCommands(memoryMode,memoryModeSize);
    sendCommands(initCommands2,sizeof(initCommands2));
    sleep_ms(120);
    gpio_put(LCD_CS, 1);
}

video *video::create(const char *opts) {
    const char *bpp = strstr(opts,"bpp=");
    if (bpp && bpp[4]=='3')
        return new video_pico_3bpp();
    else if (bpp && bpp[4]=='1'&&bpp[5]=='8')
        return new video_pico_16bpp();
    else if (bpp && bpp[4]=='2')
        return new video_pico_24bpp();
    else
        return new video_pico_16bpp();
}

} // namespace hal