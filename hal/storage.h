#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hal {

class storage {
public:
	static storage *create(const char *opts);

	// call this after one or more writes to make sure nothing is cached
	virtual void flush() = 0;
	virtual size_t getBlockSize() const = 0;
	virtual size_t getBlockCount() const = 0;
	virtual bool readBlock(size_t index,void *dest) = 0;
	virtual bool writeBlock(size_t index,const void *dest) = 0;
	virtual const void *memoryMap(size_t index,size_t blockCount) = 0;
protected:
	virtual void init() = 0;
};

} // namespace hal
