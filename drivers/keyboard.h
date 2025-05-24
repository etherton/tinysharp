#pragma once

#include <stdint.h>
#include <stddef.h>

namespace hal {

class keyboard {
public:
	static keyboard *create(const char*options);

	virtual void init() = 0;
	virtual bool getState(uint32_t state[4]) = 0;
	virtual uint8_t getBattery() = 0;
};

}
