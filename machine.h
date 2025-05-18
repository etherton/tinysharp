#include <stdint.h>

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
	void run
};

} // namespace tinysharp
