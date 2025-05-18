#pragma once

#include <stdint.h>
#include <stddef.h>

namespace tinysharp {

union cell {
	ptrdiff_t i;
	size_t u;
	uint8_t *a;
	cell *c;
	float f;
};

class machine {
public:
	void run(cell*,cell*,uint8_t*);
};

} // namespace tinysharp
