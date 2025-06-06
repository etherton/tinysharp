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
	void setColor(palette&p,rgb fore,rgb back);
	virtual void reinit() = 0;

protected:
	static void sendCommands(const uint8_t *cmds,size_t length);
	static inline void setRegion(int x,int y,int w,int h);
	static void initCommon(const uint8_t *memoryMode,size_t length);
	void setMode(const uint8_t *,size_t);
};

class video_pico_3bpp: public video_pico {
public:
	void init() { initCommon(sm_modeString,5); }
	int getBpp() { return 3; }
	void draw(int,int,int,int,const void*);
	void fill(int,int,int,int,const palette &p);
	void drawGlyph(int x,int y,int width,int height,const uint8_t *glyph,const palette &p);
	void drawString(int x,int y,const palette &p,const char *string,size_t len);
	void reinit() { setMode(sm_modeString,5); }
private:
	static const uint8_t sm_modeString[5];
};

class video_pico_16bpp: public video_pico {
public:
	void init() { initCommon(sm_modeString,5); }
	int getBpp() { return 16; }
	void draw(int,int,int,int,const void*);
	void fill(int,int,int,int,const palette &p);
	void drawGlyph(int x,int y,int width,int height,const uint8_t *glyph,const palette &p);
	void drawString(int x,int y,const palette &p,const char *string,size_t len);
	void reinit() { setMode(sm_modeString,5); }
private:
	static const uint8_t sm_modeString[5];
};

class video_pico_18bpp: public video_pico {
public:
	void init() { initCommon(sm_modeString,5); }
	int getBpp() { return 18; }
	void draw(int,int,int,int,const void*);
	void fill(int,int,int,int,const palette &p);
	void drawGlyph(int x,int y,int width,int height,const uint8_t *glyph,const palette &p);
	void reinit() { setMode(sm_modeString,5); }
private:
	static const uint8_t sm_modeString[5];
};


class video_pico_24bpp: public video_pico_18bpp {
public:
	void init() { initCommon(sm_modeString,5); }
	int getBpp() { return 24; }
	void reinit() { setMode(sm_modeString,5); }
private:
	static const uint8_t sm_modeString[5];
};

} // namespace hal