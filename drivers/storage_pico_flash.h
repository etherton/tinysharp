#pragma once

#include "storage.h"

namespace hal {

class storage_pico_flash: public storage {
public:
	void init();
	void flush();
	size_t getBlockSize() const;
	size_t getBlockCount() const;
	bool readBlock(size_t index,void *dest);
	bool writeBlock(size_t index,const void *dest);
	const void *memoryMap(size_t index,size_t blockCount);
};

} // namespace hal
