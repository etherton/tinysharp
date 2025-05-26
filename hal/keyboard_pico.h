#pragma once

#include "keyboard.h"

namespace hal {

class keyboard_pico: public keyboard {
public:
	void init();
	uint16_t getKeyEvent();
	uint8_t getBattery();
};

}
