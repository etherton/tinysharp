#pragma once

#include <stdint.h>
#include <stddef.h>

namespace hal {

namespace mod {

const uint16_t RELEASED = 0x0000;
const uint16_t PRESSED 	= 0x0100;
const uint16_t CAPSLOCK = 0x0200;
const uint16_t LSHIFT 	= 0x0400;
const uint16_t RSHIFT 	= 0x0800;
const uint16_t LCTRL 	= 0x1000;
const uint16_t RCTRL 	= 0x2000; // not on PicoCalc
const uint16_t LALT 	= 0x4000;
const uint16_t RALT 	= 0x8000; // not on PicoCalc

const uint16_t SHIFT    = (LSHIFT | RSHIFT);
const uint16_t CTRL 	= (LCTRL | RCTRL);
const uint16_t ALT 		= (LALT | RALT);

}

class keyboard {
public:
	static keyboard *create(const char*options);

	virtual void init() = 0;
	virtual uint16_t getKeyEvent() = 0; // lower 8 bits are ascii, upper 8 are modifiers
	virtual uint8_t getBattery() = 0;

	static uint16_t sm_Modifiers;
};

extern keyboard *g_keyboard;

}
