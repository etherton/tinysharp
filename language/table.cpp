#include "table.h"

namespace tinysharp {

table table::sm_tables[TI_COUNT];

uint32_t unpack(const uint8_t* &p) {
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

void pack(uint32_t i,uint8_t *&dest) {
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


} // namespace tinysharp
