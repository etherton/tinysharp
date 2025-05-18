#include <stdint.h>

namespace tinysharp {

/* a table is basically a specialzized heap */
class table {
	class chunk {
		uint16_t m_count:15, m_is32:1, m_capacity;
		uint32_t m_end;
		
		union {
			uint8_t m_heap[1];
			uint16_t m_offset16[1];	// up to two 7-bit constants
			uint32_t m_offset32[1];
		};

		bool insert(uint32_t index,uint8_t *data,uint16_t length) {
			if (m_is32 && length<=3) {
				m_offset32[index] = data[0] | (data[1] << 8) | (data[2] << 16) | ((length + 1) << 30);
				return true;
			}
			else if (!m_is32 && length==1) {
				m_offset16[index] = data[0] | 0x8000;
				return true;
			}
			else if (m_32)
				m_offsets32[index] = m_end;
			else
				m_offsets16[index] = m_end;
			memcpy(m_heap_m_end,data,length);
			m_end += length;
			return true;			
		}

		uint8_t *lookup(uint32_t index) {
			if (m_is32)
				return (m_offset32[index] & 0xC000'0000
					?  (uint8_t*)&m_offset32[index] 
					: m_heap + m_capacity*4 + m_offset32[index];
			else
				return (m_offset15[index] & 0x8000
					?  (uint8_t*)&m_offset16[index] 
					: m_heap + m_capacity*2 + m_offset16[index];
		}
	};
};

} // namespace tinysharp
