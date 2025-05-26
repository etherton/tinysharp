#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hal {

struct rgb { 
	uint8_t r, g, b;
};

// This encodes the background and foreground colors in
// a device-dependent format.
union palette {
	uint8_t as8[4];		// back/back, back/fore, fore/back, fore/fore
	uint16_t as16[2];	// back,fore
	rgb asRgb[2];		// back,fore
};

const rgb black = rgb { 0,0,0 };
const rgb white = rgb { 255,255,255 };
const rgb red = rgb { 255,0,0 };
const rgb green = rgb { 0,255,0 };
const rgb blue = rgb { 0,0,255 };

class video {
public:
	static video *create(const char *options);

	video() { }
	
	virtual void init() = 0;
	virtual int getBpp() = 0;
	virtual void setScroll(int) = 0;
	virtual void setFixedRegions(int top,int bottom) = 0;
	virtual void draw(int x,int y,int width,int height,const void *data) = 0;
	virtual void fill(int x,int y,int width,int height,rgb color) = 0;
	// this version uses multiple separate fill commands
	virtual void drawGlyph(int x,int y,int width,int height,const uint8_t *glyph,rgb fore);
	// this version generates an unpacked blob and sends it to draw
	virtual void drawGlyph(int x,int y,int width,int height,const uint8_t *glyph,const palette &p) = 0;
	virtual void setColor(palette&dest,rgb fore,rgb back) = 0;

	void setFont(uint8_t width,uint8_t height,const uint8_t *fontDef,uint8_t baseChar = 32);
	virtual void drawString(int x,int y,rgb fore,const char *string);
	virtual void drawString(int x,int y,const palette &p,const char *string);
	virtual void drawString(int x,int y,const palette *p,const uint8_t *attr,const char *string);
	void drawStringf(int x,int y,rgb fore,const char *fmt,...);
	void drawStringf(int x,int y,const palette &p,const char *fmt,...);

	static uint8_t getFontWidth() {
		return sm_fontWidth;
	}
	static uint8_t getFontHeight() {
		return sm_fontHeight;
	}
	static uint16_t getScreenWidth() {
		return sm_screenWidth;
	}
	static uint16_t getScreenHeight() {
		return sm_screenHeight;
	}
	static uint16_t getScrollHeight() {
		return sm_scrollHeight;
	}
protected:
	static uint8_t sm_fontWidth, sm_fontHeight, sm_baseChar;
	static uint16_t sm_screenWidth, sm_screenHeight, sm_scrollHeight;
	static const uint8_t *sm_fontDef;
};

extern video *g_video;

}
