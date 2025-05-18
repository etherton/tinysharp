#include <stdint.h>
#include <string.h>
#include <assert.h>

namespace tinysharp {

enum tableindex_x {
	TI_TYPES,
	TI_FIELDS,
	TI_METHODS,
};
const unsigned TI_COUNT = 3;

/* a table is basically a specialzized heap */
class table {
	class chunk {
		uint16_t m_count, m_capacity;
		uint32_t m_heapOffset, m_heapSize;
		uint32_t m_is32:1, unused:31;
		
		static chunk *create(bool large,size_t maxSize) {
			chunk *c = (chunk*) new uint8_t[maxSize];
			c->m_count = 0;
			c->m_capacity = 0;
			c->m_heapOffset = 0;
			c->m_heapSize = maxSize - sizeof(chunk);
			c->m_is32 = large;
		}
		union {
			uint8_t m_heap[0];
			uint16_t m_offset16[0];
			uint32_t m_offset32[0];
		};

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
			assert(m_heapOffset + m_capacity*(2<<m_is32) <= m_heapSize);
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
	static uint32_t insert(uint32_t index,const uint8_t *data,uint16_t count) {
		return insert(data,count) | (index << 24);
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
