#include <stddef.h>

namespace hal {

extern uint32_t getUsTime32();
extern uint64_t getUsTime64();
extern void sleepMs(uint32_t t);

}
