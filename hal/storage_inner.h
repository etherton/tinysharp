#include "storage.h"

namespace hal {

class storage_inner: public storage {
public:
	storage_inner(storage *i,size_t  blockStart,size_t blockCount) : 
		m_inner(i), m_innerBlockStart(blockStart), m_innerBlockCount(blockCount) {}

	void flush() { m_inner->flush(); }
	size_t getBlockSize() const { return m_inner->getBlockSize(); }
	size_t getBlockCount() const { return m_innerBlockCount; }
	bool readBlock(size_t index,void *dest) {
		return (index >= m_innerBlockCount)? false : m_inner->readBlock(index + m_innerBlockStart,dest);
	}
	bool writeBlock(size_t index,const void *src) {
		return (index >= m_innerBlockCount)? false : m_inner->writeBlock(index + m_innerBlockStart,src);
	}
	const void *memoryMap(size_t index,size_t blockCount) {
		return (m_innerBlockStart + blockCount > m_innerBlockCount)? nullptr : m_inner->memoryMap(index + m_innerBlockStart,blockCount);
	}
private:
	void init() { }
	storage *m_inner;
	size_t m_innerBlockStart, m_innerBlockCount;
};

} // namespace hal
