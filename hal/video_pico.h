#pragma once

#include "video.h"

namespace hal {

inline uint8_t pack3(rgb c) {
	uint8_t packed = ((c.r & 128) >> 5) | ((c.g & 128) >> 6) | ((c.b & 128) >> 7);
    return packed | (packed << 3);
}

inline uint16_t pack16(rgb c) {
	return ((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3);
}

class video_pico: public video {
public:
	void setScroll(int);
	void setFixedRegions(int,int);
protected:
	static void sendCommands(const uint8_t *cmds,size_t length);
	static inline void setRegion(int x,int y,int w,int h);
	static void initCommon(const uint8_t *memoryMode,size_t length);
};

class video_pico_3bpp: public video_pico {
public:
	void init() { initCommon((uint8_t*)"\x3A\x01\x22\x39",5); }
	int getBpp() { return 3; }
	void draw(int,int,int,int,const void*);
	void fill(int,int,int,int,const palette &p);
	void drawGlyph(int x,int y,int width,int height,const uint8_t *glyph,const palette &p);
	void drawString(int x,int y,const palette &p,const char *string,size_t len);
	void setColor(palette&p,rgb fore,rgb back);
};

class video_pico_16bpp: public video_pico {
public:
	void init() { initCommon((uint8_t*)"\x3A\x01\x55",3); }
	int getBpp() { return 16; }
	void draw(int,int,int,int,const void*);
	void fill(int,int,int,int,const palette &p);
	void drawGlyph(int x,int y,int width,int height,const uint8_t *glyph,const palette &p);
	void drawString(int x,int y,const palette &p,const char *string,size_t len);
	void setColor(palette&p,rgb fore,rgb back);
};

class video_pico_18bpp: public video_pico {
public:
	void init() { initCommon((uint8_t*)"\x3A\x01\x66",3); }
	int getBpp() { return 18; }
	void draw(int,int,int,int,const void*);
	void fill(int,int,int,int,const palette &p);
	void drawGlyph(int x,int y,int width,int height,const uint8_t *glyph,const palette &p);
	void setColor(palette&p,rgb fore,rgb back);
};


class video_pico_24bpp: public video_pico_18bpp {
public:
	void init() { initCommon((uint8_t*)"\x3A\x01\x77",3); }
	int getBpp() { return 24; }
};

} // namespace hal