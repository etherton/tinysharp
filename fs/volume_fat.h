#pragma once

#include "volume.h"

namespace fs {

class volumeFat: public volume {
public:
    bool init(hal::storage *s);
    static volumeFat* create(hal::storage*);
};

}


