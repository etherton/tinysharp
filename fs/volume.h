#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hal { class storage; }

namespace fs {

class volume {
public:
	virtual bool mount(hal::storage *s);
};

extern volume *g_root;

};
