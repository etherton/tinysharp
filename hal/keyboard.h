#pragma once

#include <stdint.h>
#include <stddef.h>

namespace hal {

namespace modifier {
const uint16_t RELEASED_BIT = 0x0000;
const uint16_t PRESSED_BIT  = 0x0100;
const uint16_t CAPSLOCK_BIT = 0x0200;
const uint16_t LSHIFT_BIT 	= 0x0400;
const uint16_t RSHIFT_BIT 	= 0x0800;
const uint16_t SHIFT_BITS   = 0x0C00;
const uint16_t LCTRL_BIT 	= 0x1000;
const uint16_t RCTRL_BIT 	= 0x2000;  // not on PicoCalc
const uint16_t CTRL_BITS 	= 0x3000;
const uint16_t LALT_BIT 	= 0x4000;
const uint16_t RALT_BIT 	= 0x8000; // not on PicoCalc
const uint16_t ALT_BITS 	= 0xC000;
};

namespace key { 

const uint8_t UP	= 0x80;
const uint8_t DOWN	= 0x81;
const uint8_t LEFT	= 0x82;
const uint8_t RIGHT = 0x83;
const uint8_t F1	= 0x84;
const uint8_t F2	= 0x85;
const uint8_t F3	= 0x86;
const uint8_t F4	= 0x87;
const uint8_t F5	= 0x88;
const uint8_t F6	= 0x89;
const uint8_t F7	= 0x8A;
const uint8_t F8	= 0x8B;
const uint8_t F9	= 0x8C;
const uint8_t F10	= 0x8D;
const uint8_t F11	= 0x8E;
const uint8_t F12	= 0x8F;
const uint8_t HOME	= 0x90;
const uint8_t END	= 0x91;
const uint8_t INS	= 0x92;
const uint8_t DEL	= 0x93;
const uint8_t PGUP	= 0x94;
const uint8_t PGDN	= 0x95;
const uint8_t BREAK = 0x96;
const uint8_t CAPSLOCK = 0x97;

};

class keyboard {
public:
	static keyboard *create(const char*options);

public:
	virtual uint16_t getKeyEvent() = 0; // lower 8 bits are ascii, upper 8 are modifiers
	virtual uint8_t getBattery() = 0;

	uint16_t waitKeyEvent();

protected:
	virtual void init() = 0;
	static uint16_t sm_Modifiers;
	static const char *sm_Labels[0x98];
};

extern keyboard *g_keyboard;

}
