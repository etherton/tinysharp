#include "video.h"

namespace hal {

void video::drawGlyph(int x,int y,int w,int h,const uint8_t *glyph,rgb fore) {
	for (;h; h--,y++,glyph++) {
		for (int i=0; i<w; i++)
			if (*glyph & (0x80>>i))
				fill(x+i,y,1,1,fore);
	}
}

void video::drawGlyph(int x,int y,int w,int h,const uint8_t *glyph,rgb fore,rgb back) {
	for (;h; h--,y++,glyph++) {
		for (int i=0; i<w; i++)
			fill(x+i,y,1,1,*glyph & (0x80>>i)? fore : back);
	}
}

}
