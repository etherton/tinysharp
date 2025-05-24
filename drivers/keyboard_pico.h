#pragma once

#include "keyboard.h"

namespace hal {

class keyboard_pico: public keyboard {
public:
	void init();
	bool getState(uint32_t state[4]);
	uint8_t getBattery();
};

}
