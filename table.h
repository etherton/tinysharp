#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "trap.h"

namespace tinysharp {

enum tableindex_t {
	TI_TYPES,
	TI_FIELDS,
	TI_METHODS,
};
const unsigned TI_COUNT = 3;

inline uint32_t unpack(const uint8_t* &p) {
	if (p[0] < 0x80)
		return *p++;
	else if (p[0] < 0xA0)  {
		p+=2;
		return ((p[-2]&0x1F) | (p[-1]<<5)) + 0x80;
	}
	else if (p[0] < 0xC0) {
		p+=3;
		return ((p[-3]&0x1F) | (p[-2]<<5) | (p[-1]<<13)) + 0x1F80;
	}
	else if (p[0] < 0xE0) {
		p+=4;
		return ((p[-4]&0x1F) | (p[-3]<<5) | (p[-2]<<13) | (p[-3]<<21)) + 0x1F1F80;
	}
	else {
		p+=5;
		return (p[-4]) | (p[-3]<<8) | (p[-2]<<16) | (p[-3]<<24);
	}
}

inline void pack(uint32_t i,uint8_t *&dest) {
	if (i < 0x80)
		*dest++ = i;
	else if (i < 0x1F80) {
		i -= 0x80;
		*dest++ = (i & 0x1F) | 0x80;
		*dest++ = i >> 5;
	}
	else if (i < 0x1F1F80) {
		i -= 0x1F80;
		*dest++ = (i & 0x1F) | 0xA0;
		*dest++ = i >> 5;
		*dest++ = i >> 13;
	}
	else if (i < 0x1F1F'1F80) {
		i -= 0x1F'1F80;
		*dest++ = (i & 0x1F) | 0xC0;
		*dest++ = i >> 5;
		*dest++ = i >> 13;
		*dest++ = i >> 21;
	}
	else {
		// no reason to do fancy packing here
		*dest++ = 0xE0;
		*dest++ = i;
		*dest++ = i >> 8;
		*dest++ = i >> 16;
		*dest++ = i >> 24;
	}
}

/* a table is basically a specialzized heap */
class table {
	class chunk {
	private:
		uint16_t m_count, m_capacity;
		uint32_t m_heapOffset, m_heapSize;
		uint32_t m_is32:1, m_baseIndex:31;
		union {
			uint8_t m_heap[0];
			uint16_t m_offset16[0];
			uint32_t m_offset32[0];
		};
	public:
		static chunk *create(bool large,uint32_t baseIndex,size_t maxSize) {
			chunk *c = (chunk*) new uint8_t[maxSize];
			c->m_count = 0;
			c->m_capacity = 0;
			c->m_heapOffset = 0;
			c->m_heapSize = maxSize - sizeof(chunk);
			c->m_is32 = large;
			c->m_baseIndex = baseIndex;
			return c;
		}

		bool hasRoomFor(const uint8_t *data,uint16_t length) {
			// if it's short enough and there's room in the offset table we know it will fit.
			if (m_count < m_capacity && length <= (m_is32? 3 : 1))
				return true;
			return ((m_capacity+(m_count==m_capacity))  * (2 << m_is32) + m_heapOffset + length <= m_heapSize);
		}
		int insert(const uint8_t *data,uint16_t length) {
			if (m_count == m_capacity) {
				const uint32_t step = 32;
				if (m_is32)
					memmove(m_heap + m_capacity*4 + step*4,
						m_heap + m_capacity*4, m_heapSize);
				else
					memmove(m_heap + m_capacity*2 + step*2,
						m_heap + m_capacity*2, m_heapSize);
				m_capacity += step;
			}
			if (m_is32 && length<=3) {
				m_offset32[m_count] = data[0] | (data[1] << 8) | (data[2] << 16) | ((length + 1) << 30);
				return m_count++;
			}
			else if (!m_is32 && length==1) {
				m_offset16[m_count] = data[0] | 0x8000;
				return m_count++;
			}
			else if (m_is32) {
				m_offset32[m_count] = m_heapOffset;
				memcpy(m_heap + m_capacity*4 + m_heapOffset,data,length);
			}
			else {
				m_offset16[m_count] = m_heapOffset;
				memcpy(m_heap + m_capacity*2 + m_heapOffset,data,length);
			}
			m_heapOffset += length;
			trapgt(m_heapOffset + m_capacity*(2<<m_is32),m_heapSize);
			return m_count++;			
		}

		const uint8_t *lookup(uint32_t index) {
			if (m_is32)
				return m_offset32[index] & 0xC000'0000
					?  (uint8_t*)&m_offset32[index] 
					: m_heap + m_capacity*4 + m_offset32[index];
			else
				return m_offset16[index] & 0x8000
					?  (uint8_t*)&m_offset16[index] 
					: m_heap + m_capacity*2 + m_offset16[index];
		}
	};
public:
	int insert(const uint8_t *data,uint16_t count) {
		if (!m_count || !m_chunks[m_count-1]->hasRoomFor(data,count)) {
			if (m_count == kMaxChunks)
				return -1;
			m_chunks[m_count++] = chunk::create(false,m_count * 64,4096);
		}
		return m_chunks[m_count-1]->insert(data,count);
	}
	static uint32_t insert(uint32_t index,const uint8_t *data,uint16_t count) {
		return sm_tables[index].insert(data,count) | (index << 24);
	}
	static uint32_t insert(uint32_t index,uint8_t data) {
		return insert(index,&data,1);
	}
private:
	static const unsigned kMaxChunks = 32;
	chunk *m_chunks[kMaxChunks];
	static table sm_tables[TI_COUNT];
	uint32_t m_count;
};

} // namespace tinysharp
