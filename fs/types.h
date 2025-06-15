#pragma once

#include <stdint.h>

namespace fs {

struct word {
	uint8_t lo, hi;
	uint16_t get() const { return lo | (hi << 8); }
};

struct dword {
	word lo, hi;
 	uint32_t get() const { return lo.get() | (hi.get() << 16); }
};

}
