// based on keyboard.c from https://github.com/benob/picocalc-umac/

#include "keyboard_pico.h"

#include <hardware/gpio.h>
#include <hardware/i2c.h>

#include <stdio.h>

namespace hal {

typedef enum {
  KEY_STATE_IDLE = 0,
  KEY_STATE_PRESSED = 1,
  KEY_STATE_HOLD = 2,
  KEY_STATE_RELEASED = 3,
  KEY_STATE_LONG_HOLD = 4,
} key_state_t;

enum {
  MOD_SHIFT = 1,
  MOD_LSHIFT = 3,
  MOD_RSHIFT = 5,
  MOD_ALT = 8,
  MOD_CONTROL = 16,
};

typedef enum {
  KEY_NONE = 0,
  KEY_ERROR = -1,

  KEY_ALT = 0xA1,
  KEY_LSHIFT = 0xA2,
  KEY_RSHIFT = 0xA3,
  KEY_CONTROL = 0xA5,

  KEY_a = 'a',
  KEY_b = 'b',
  KEY_c = 'c',
  KEY_d = 'd',
  KEY_e = 'e',
  KEY_f = 'f',
  KEY_g = 'g',
  KEY_h = 'h',
  KEY_i = 'i',
  KEY_j = 'j',
  KEY_k = 'k',
  KEY_l = 'l',
  KEY_m = 'm',
  KEY_n = 'n',
  KEY_o = 'o',
  KEY_p = 'p',
  KEY_q = 'q',
  KEY_r = 'r',
  KEY_s = 's',
  KEY_t = 't',
  KEY_u = 'u',
  KEY_v = 'v',
  KEY_w = 'w',
  KEY_x = 'x',
  KEY_y = 'y',
  KEY_z = 'z',

  KEY_A = 'A',
  KEY_B = 'B',
  KEY_C = 'C',
  KEY_D = 'D',
  KEY_E = 'E',
  KEY_F = 'F',
  KEY_G = 'G',
  KEY_H = 'H',
  KEY_I = 'I',
  KEY_J = 'J',
  KEY_K = 'K',
  KEY_L = 'L',
  KEY_M = 'M',
  KEY_N = 'N',
  KEY_O = 'O',
  KEY_P = 'P',
  KEY_Q = 'Q',
  KEY_R = 'R',
  KEY_S = 'S',
  KEY_T = 'T',
  KEY_U = 'U',
  KEY_V = 'V',
  KEY_W = 'W',
  KEY_X = 'X',
  KEY_Y = 'Y',
  KEY_Z = 'Z',

  KEY_SPACE = ' ',
  KEY_TILDE = '~',
  KEY_DOLLAR = '$',

  KEY_BACKSPACE = '\b',
  KEY_ENTER = '\n',

  KEY_0 = '0',
  KEY_1 = '1',
  KEY_2 = '2',
  KEY_3 = '3',
  KEY_4 = '4',
  KEY_5 = '5',
  KEY_6 = '6',
  KEY_7 = '7',
  KEY_8 = '8',
  KEY_9 = '9',

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

  KEY_SHARP = '#',
  KEY_AT = '@',
  KEY_DELETE = 0xD4,
  KEY_END = 0xD5,
  KEY_CAPSLOCK = 0xC1,
  KEY_TAB = 0x09,
  KEY_HOME = 0xD2,
  KEY_ESC = 0xB1,
  KEY_BREAK = 0xd0,
  KEY_PAUSE = 0xd0,
  KEY_EQUAL = '=',
  KEY_PLUS = '+',
  KEY_MINUS = '-',
  KEY_UNDERSCORE = '_',
  KEY_BACKSLASH = '\\',
  KEY_PIPE = '|',
  KEY_BANG = '!',
  KEY_INSERT = 0xD1,
  KEY_STAR = '*',
  KEY_AMPERSAND = '&',
  KEY_CARRET = '^',
  KEY_PERCENT = '%',

  KEY_DOT = '.',
  KEY_GT = '>',

  KEY_SEMICOLON = ';',
  KEY_COLON = ':',

  KEY_COMMA = ',',
  KEY_LT = '<',

  KEY_BACKTICK = '`',

  KEY_DQUOTE = '"',
  KEY_APOSTROPHE = '\'',

  KEY_QMARK = '?',
  KEY_SLASH = '/',

  KEY_CPAREN = ')',
  KEY_OPAREN = '(',

  KEY_RIGHTBRACKET = ']',
  KEY_RIGHTBRACE = '}',

  KEY_LEFTBRACKET = '[',
  KEY_LEFTBRACE = '{',

  KEY_RIGHT = 0xb7,
  KEY_UP = 0xb5,
  KEY_DOWN = 0xb6,
  KEY_LEFT = 0xb4,
  KEY_PAGEUP = 0xd6,
  KEY_PAGEDOWN = 0xd7,
  KEY_ESCAPE = 0xB1,
} keycode_t;

typedef struct {
  unsigned char state;
  unsigned char modifiers;
  short code;
} input_event_t;


#define KBD_MOD    i2c1
#define KBD_SDA    6
#define KBD_SCL    7
#define KBD_SPEED  10000 // if dual i2c, then the speed of keyboard i2c should be 10khz
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
  REG_ID_C64_MTX = 0x0c, // read c64 matrix
  REG_ID_C64_JS = 0x0d,  // joystick io bits
};

static int keyboard_modifiers;

static int i2c_kbd_write(unsigned char* data, int size) {
  int retval = i2c_write_timeout_us(KBD_MOD, KBD_ADDR, data, size, false, 500000);
  if (retval == PICO_ERROR_GENERIC || retval == PICO_ERROR_TIMEOUT) {
    printf("i2c_kbd_write: i2c write error\n");
    return 0;
  }
  return 1;
}

static int i2c_kbd_read(unsigned char* data, int size) {
  int retval = i2c_read_timeout_us(KBD_MOD, KBD_ADDR, data, size, false, 500000);
  if (retval == PICO_ERROR_GENERIC || retval == PICO_ERROR_TIMEOUT) {
    printf("i2c_kbd_read: i2c read error\n");
    return 0;
  }
  return 1;
}

static int i2c_kbd_command(unsigned char command) {
  return i2c_kbd_write(&command, 1);
}

static int i2c_kbd_queue_size() {
  if (!i2c_kbd_command(REG_ID_KEY)) return 0; // Read queue size 
  unsigned short result = 0;
  if (!i2c_kbd_read((unsigned char*)&result, 2)) return 0;
  return result & 0x1f; // bits beyond that mean something different
}

static unsigned short i2c_kbd_read_key() {
  if (!i2c_kbd_command(REG_ID_FIF)) return 0;
  unsigned short result = 0;
  if (!i2c_kbd_read((unsigned char*)&result, 2)) return KEY_NONE;
  return result;
}

static unsigned short i2c_kbd_read_bat() {
  if (!i2c_kbd_command(REG_ID_BAT)) return 255;
  uint8_t result[2];
  if (!i2c_kbd_read(result, 2)) return 255;
  else return result[1];  
}

static bool i2c_kbd_read_matrix(uint32_t state[4]) {
  if (!i2c_kbd_command(REG_ID_C64_MTX)) { state[0]=0xDCE; return false; }
  uint8_t matrix[10];
  if (!i2c_kbd_read(matrix, 10)) { state[0]=0xBAD; return false; }
  state[0] = matrix[1] | (matrix[2] << 8) | (matrix[3] << 16) | (matrix[4] << 24);
  state[1] = matrix[5] | (matrix[6] << 8) | (matrix[7] << 16) | (matrix[8] << 24);
  state[2] = matrix[9];
  return true;
}

static void update_modifiers(unsigned short value) {
  switch (value) {
    case (KEY_CONTROL << 8) | KEY_STATE_PRESSED: keyboard_modifiers |= MOD_CONTROL; break;
    case (KEY_CONTROL << 8) | KEY_STATE_RELEASED: keyboard_modifiers &= ~MOD_CONTROL; break;
    case (KEY_ALT << 8) | KEY_STATE_PRESSED: keyboard_modifiers |= MOD_ALT; break;
    case (KEY_ALT << 8) | KEY_STATE_RELEASED: keyboard_modifiers &= ~MOD_ALT; break;
    case (KEY_LSHIFT << 8) | KEY_STATE_PRESSED: keyboard_modifiers |= MOD_LSHIFT; break;
    case (KEY_LSHIFT << 8) | KEY_STATE_RELEASED: keyboard_modifiers &= ~MOD_LSHIFT; break;
    case (KEY_RSHIFT << 8) | KEY_STATE_PRESSED: keyboard_modifiers |= MOD_RSHIFT; break;
    case (KEY_RSHIFT << 8) | KEY_STATE_RELEASED: keyboard_modifiers &= ~MOD_RSHIFT; break;
  }
}

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

input_event_t keyboard_poll() {
  unsigned short value = i2c_kbd_read_key();
  update_modifiers(value);
  keyboard_check_special_keys(value);
  //if (value != 0 && (value >> 8) != KEY_ALT && (value >> 8) != KEY_CONTROL) printf("key = %d (%02x) / state = %d / modifiers = %02x\n", value >> 8, value >> 8, value & 0xff, keyboard_modifiers);
  return (input_event_t) {value & 0xff, keyboard_modifiers, value >> 8};
}

input_event_t keyboard_wait() {
  input_event_t event;
  do { 
    event = keyboard_poll();
    if (event.code == 0) sleep_ms(1);
  } while (event.code == 0);
  return event;
}

char keyboard_getchar() {
  return keyboard_wait().code;
}

void keyboard_pico::init() {
  gpio_set_function(KBD_SCL, GPIO_FUNC_I2C);
  gpio_set_function(KBD_SDA, GPIO_FUNC_I2C);
  i2c_init(KBD_MOD, KBD_SPEED);
  gpio_pull_up(KBD_SCL);
  gpio_pull_up(KBD_SDA);
  keyboard_modifiers = 0;
  //while (i2c_kbd_read_key() != 0); // Drain queue
}

bool keyboard_pico::getState(uint32_t state[4]) {
  uint8_t msg[2];
  msg[0] = 9;
  i2c_write_timeout_us(KBD_MOD, KBD_ADDR, msg, 1, false, 50000);
  //sleep_ms(16);
  uint16_t buff;
  i2c_read_timeout_us(KBD_MOD, KBD_ADDR, (unsigned char *) &buff, 2, false, 50000);
  state[0] = buff;
  return true;
  /* auto x = keyboard_poll();
  memcpy(&state[0],&x,4);
  return true; */
}

uint8_t keyboard_pico::getBattery() {
  return i2c_kbd_read_bat();
}

keyboard* keyboard::create(const char *options) {
	return new keyboard_pico();
}

}
