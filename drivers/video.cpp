#include "video.h"

namespace hal {

void video::drawGlyph(int x,int y,int w,int h,const uint8_t *glyph,rgb fore) {
	for (;h; h--,y++,glyph++) {
		uint8_t bit = 0x80;
		for (int i=0; i<w; i++, bit>>=1)
			if (*glyph & bit)
				fill(x+i,y,1,1,fore);
	}
}

void video::drawGlyph(int x,int y,int w,int h,const uint8_t *glyph,rgb fore,rgb back) {
	for (;h; h--,y++,glyph++) {
		uint8_t bit = 0x80;
		for (int i=0; i<w; i++, bit>>=1)
			fill(x+i,y,1,1,*glyph & bit? fore : back);
	}
}

}
