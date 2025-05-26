#include "pico/stdlib.h"

namespace hal {

uint32_t getUsTime32() {
	return time_us_32();
}

uint64_t getUsTime64() {
	return time_us_64();
}

}
