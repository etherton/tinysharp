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
	int getWidth() { return 320; }
	int getHeight() { return 320; }
	int getScrollHeight() { return 480; }
	void setScroll(int);
protected:
	static void sendCommands(const uint8_t *cmds,size_t length);
	static inline void setRegion(int x,int y,int w,int h);
	static void initCommon(const uint8_t *memoryMode,size_t length);
};

class video_pico_3bpp: public video_pico {
public:
	void init() { initCommon((uint8_t*)"\x3A\x01\x61",3); }
	int getBpp() { return 3; }
	void draw(int,int,int,int,const void*);
	void fill(int,int,int,int,rgb color);
};

class video_pico_16bpp: public video_pico {
public:
	void init() { initCommon((uint8_t*)"\x3A\x01\x55",3); }
	int getBpp() { return 16; }
	void draw(int,int,int,int,const void*);
	void fill(int,int,int,int,rgb color);
};

} // namespace hal