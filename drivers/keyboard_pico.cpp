// based on keyboard.c from https://github.com/benob/picocalc-umac/

#include "keyboard_pico.h"

#include <hardware/gpio.h>
#include <hardware/watchdog.h>
#include <hardware/i2c.h>

#include <stdio.h>

namespace hal {

// lower byte: 0=idle,1=pressed,2=hold,3=released

enum keycode_t {
  KEY_ALT = 0xA1,
  KEY_LSHIFT = 0xA2,
  KEY_RSHIFT = 0xA3,
  KEY_CONTROL = 0xA5,

  KEY_BACKSPACE = '\b',
  KEY_ENTER = '\n',

  KEY_F1 = 0x81,
  KEY_F2 = 0x82,
  KEY_F3 = 0x83,
  KEY_F4 = 0x84,
  KEY_F5 = 0x85,
  KEY_F6 = 0x86,
  KEY_F7 = 0x87,
  KEY_F8 = 0x88,
  KEY_F9 = 0x89,
  KEY_F10 = 0x90,

  KEY_DELETE = 0xD4,
  KEY_END = 0xD5,
  KEY_CAPSLOCK = 0xC1,
  KEY_TAB = 0x09,
  KEY_HOME = 0xD2,
  KEY_ESC = 0xB1,
  KEY_BREAK = 0xd0,
  KEY_PAUSE = 0xd0,
  KEY_INSERT = 0xD1,
  KEY_RIGHT = 0xb7,
  KEY_UP = 0xb5,
  KEY_DOWN = 0xb6,
  KEY_LEFT = 0xb4,
  KEY_PAGEUP = 0xd6,
  KEY_PAGEDOWN = 0xd7,
  KEY_ESCAPE = 0xB1,
};

#define KBD_MOD    i2c1
#define KBD_SDA    6
#define KBD_SCL    7
#define KBD_SPEED  20000 // if dual i2c, then the speed of keyboard i2c should be 10khz
#define KBD_ADDR   0x1F

// Commands defined by the keyboard driver
enum {
  REG_ID_VER = 0x01,     // fw version
  REG_ID_CFG = 0x02,     // config
  REG_ID_INT = 0x03,     // interrupt status
  REG_ID_KEY = 0x04,     // key status
  REG_ID_BKL = 0x05,     // backlight
  REG_ID_DEB = 0x06,     // debounce cfg
  REG_ID_FRQ = 0x07,     // poll freq cfg
  REG_ID_RST = 0x08,     // reset
  REG_ID_FIF = 0x09,     // fifo
  REG_ID_BK2 = 0x0A,     //keyboard backlight
  REG_ID_BAT = 0x0b,     // battery
  REG_ID_C64_MTX = 0x0c, // read c64 matrix (not hooked up)
  REG_ID_C64_JS = 0x0d,  // joystick io bits
};

#if 0
#include "pico/bootrom.h"
#include "hardware/watchdog.h"

static void keyboard_check_special_keys(unsigned short value) {
  if ((value & 0xff) == KEY_STATE_RELEASED && keyboard_modifiers == (MOD_CONTROL|MOD_ALT)) {
    if ((value >> 8) == KEY_F1) {
      printf("rebooting to usb boot\n");
      reset_usb_boot(0, 0);
    } else if ((value >> 8) == KEY_DELETE) {
      printf("rebooting via watchdog\n");
      watchdog_reboot(0, 0, 0);
      watchdog_enable(0, 1);
    }
  }
}
#endif

uint16_t i2c_kbd_read() {
  uint8_t cmd = REG_ID_FIF;
  i2c_write_timeout_us(KBD_MOD, KBD_ADDR, &cmd, 1, false, 500000);
  uint16_t result = 0;
  i2c_read_timeout_us(KBD_MOD, KBD_ADDR, (uint8_t*) &result, 2, false, 500000);
  return result;
}

void keyboard_pico::init() {
  gpio_set_function(KBD_SCL, GPIO_FUNC_I2C);
  gpio_set_function(KBD_SDA, GPIO_FUNC_I2C);
  i2c_init(KBD_MOD, KBD_SPEED);
  gpio_pull_up(KBD_SCL);
  gpio_pull_up(KBD_SDA);
  while (i2c_kbd_read());
}

uint16_t keyboard_pico::getKeyEvent() {
  uint16_t result = i2c_kbd_read();
  switch (result) {
    case 0x0000: return 0;
    case 0xA101: sm_Modifiers |= mod::LALT; break;
    case 0xA103: sm_Modifiers &= ~mod::LALT; break;
    case 0xA201: sm_Modifiers |= mod::LSHIFT; break;
    case 0xA203: sm_Modifiers &= ~mod::LSHIFT; break;
    case 0xA301: sm_Modifiers |= mod::RSHIFT; break;
    case 0xA303: sm_Modifiers &= ~mod::RSHIFT; break;
    case 0xA501: sm_Modifiers |= mod::LCTRL; break;
    case 0xA503: sm_Modifiers &= ~mod::LCTRL; break;
    case 0xD403: // Ctrl+Alt+Delete?
      if (sm_Modifiers == (mod::LCTRL | mod::LALT)) {
        watchdog_reboot(0, 0, 0);
        watchdog_enable(0, 1);
      }
      else break;
  }
  if ((result & 255) == 3)
    return (result >> 8) | sm_Modifiers;
  else
    return (result >> 8) | sm_Modifiers | mod::PRESSED;
}

uint8_t keyboard_pico::getBattery() {
  uint8_t msg[2] = { REG_ID_BAT, 0 };
  i2c_write_timeout_us(KBD_MOD, KBD_ADDR, msg, 1, false, 500000);
  i2c_read_timeout_us(KBD_MOD, KBD_ADDR, msg, 2, false, 500000);
  return msg[1];
}

keyboard* keyboard::create(const char *options) {
	return new keyboard_pico();
}

}
