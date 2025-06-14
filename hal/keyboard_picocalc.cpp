// based on keyboard.c from https://github.com/benob/picocalc-umac/

#include "keyboard_picocalc.h"
#include "timer.h"

#include <pico/bootrom.h>
#include <hardware/gpio.h>
#include <hardware/watchdog.h>
#include <hardware/i2c.h>

#include <stdio.h>

namespace hal {

// lower byte: 0=idle,1=pressed,2=hold,3=released

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
  REG_ID_KEY = 0x04,     // key status (lower 5 bits is count)
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



uint16_t i2c_kbd_read() {
  uint8_t cmd = REG_ID_FIF;
  i2c_write_timeout_us(KBD_MOD, KBD_ADDR, &cmd, 1, false, 500000);
  uint16_t result = 0;
  i2c_read_timeout_us(KBD_MOD, KBD_ADDR, (uint8_t*) &result, 2, false, 500000);
  return result;
}

uint8_t i2c_kbd_status() {
  uint8_t cmd = REG_ID_KEY;
  i2c_write_timeout_us(KBD_MOD, KBD_ADDR, &cmd, 1, false, 500000);
  i2c_read_timeout_us(KBD_MOD, KBD_ADDR, &cmd, 1, false, 500000);
  return cmd;  
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
  uint8_t status = i2c_kbd_status();
  if (!(status & 0x1F))
    return 0;
  uint16_t result = i2c_kbd_read();
  switch (result) {
    case 0x0000: return 0;
    case 0xA101: sm_Modifiers |= modifier::LALT_BIT; break;
    case 0xA103: sm_Modifiers &= ~modifier::LALT_BIT; break;
    case 0xA201: sm_Modifiers |= modifier::LSHIFT_BIT; break;
    case 0xA203: sm_Modifiers &= ~modifier::LSHIFT_BIT; break;
    case 0xA301: sm_Modifiers |= modifier::RSHIFT_BIT; break;
    case 0xA303: sm_Modifiers &= ~modifier::RSHIFT_BIT; break;
    case 0xA501: sm_Modifiers |= modifier::LCTRL_BIT; break;
    case 0xA503: sm_Modifiers &= ~modifier::LCTRL_BIT; break;
    case 0xD403: // Ctrl+Alt+Delete?
      if ((sm_Modifiers & modifier::CTRL_BITS) && (sm_Modifiers & modifier::ALT_BITS)) {
        watchdog_reboot(0, 0, 0);
        watchdog_enable(0, 1);
      }
      else 
        break;
    case 0x8103: // Ctrl+Alt+F1, boolset
      if ((sm_Modifiers & modifier::CTRL_BITS) && (sm_Modifiers & modifier::ALT_BITS))
          reset_usb_boot(0, 0);
      else 
        break;
    }
  auto remap = [](uint8_t k) -> uint8_t {
    switch (k) {
      case 0xA1: return 0; // Alt
      case 0xA2: return 0; // LeftShift
      case 0xA3: return 0; // RightShift
      case 0xA5: return 0; // LeftCtrl
      case 0x81: return key::F1;
      case 0x82: return key::F2;
      case 0x83: return key::F3;
      case 0x84: return key::F4;
      case 0x85: return key::F5;
      case 0x86: return key::F6;
      case 0x87: return key::F7;
      case 0x88: return key::F8;
      case 0x89: return key::F9;
      case 0x90: return key::F10;
      case 0xD4: return key::DEL;
      case 0xD5: return key::END;
      case 0xC1: return key::CAPSLOCK;
      case 0xD2: return key::HOME;
      case 0xB1: return 27;
      case 0xD0: return key::BREAK;
      case 0xD1: return key::INS;
      case 0xB7: return key::RIGHT;
      case 0xB5: return key::UP;
      case 0xB6: return key::DOWN;
      case 0xB4: return key::LEFT;
      case 0xD6: return key::PGUP;
      case 0xD7: return key::PGDN;
      default: return k;
    }
  };
  
  if (status & 0x20)
    sm_Modifiers |= modifier::CAPSLOCK_BIT;
  else
    sm_Modifiers &= ~modifier::CAPSLOCK_BIT;
  if ((result & 255) == 3)
    return remap(result >> 8) | sm_Modifiers;
  else
    return remap(result >> 8) | sm_Modifiers | modifier::PRESSED_BIT;
}

uint8_t keyboard_pico::getBattery() {
  static uint8_t lastBattery;
  static uint64_t lastTime;
  uint64_t now = getUsTime64();
  if (!lastTime || now - lastTime > 10'000'000) { // poll every ten seconds
    uint8_t msg[2] = { REG_ID_BAT, 0 };
    i2c_write_timeout_us(KBD_MOD, KBD_ADDR, msg, 1, false, 500000);
    i2c_read_timeout_us(KBD_MOD, KBD_ADDR, msg, 2, false, 500000);
    lastBattery = msg[1];
  }
  return lastBattery;
}

keyboard* keyboard::create(const char * /*options*/) {
	g_keyboard = new keyboard_pico();
  g_keyboard->init();
  return g_keyboard;
}

}
