#pragma once

#include <stdint.h>
#include <stddef.h>

namespace hal {

class keyboard {
public:
	static keyboard *create(const char*options);

	virtual void init() = 0;
	virtual uint16_t getKeyEvent() = 0;
	virtual uint8_t getBattery() = 0;
};

}
