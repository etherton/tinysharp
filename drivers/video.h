#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hal {

struct rgb { 
	uint8_t r, g, b;
};

class video {
public:
	static video *create(const char *options);

	video() { }
	
	virtual void init() = 0;
	virtual int getWidth() = 0;
	virtual int getHeight() = 0;
	virtual int getBpp() = 0;
	virtual int getScrollHeight() = 0;
	virtual void setScroll(int) = 0;
	virtual void setFixedRegions(int top,int middle,int bottom) = 0;
	virtual void draw(int x,int y,int width,int height,const void *data) = 0;
	virtual void fill(int x,int y,int width,int height,rgb color) = 0;
	// this version uses multiple separate fill commands
	virtual void drawGlyph(int x,int y,int width,int height,const uint8_t *glyph,rgb fore);
	// this version generates an unpacked blob and sends it to draw
	virtual void drawGlyph(int x,int y,int width,int height,const uint8_t *glyph,rgb fore,rgb back);

	void setFont(uint8_t width,uint8_t height,const uint8_t *fontDef,uint8_t baseChar = 32);
	virtual void drawString(int x,int y,const char *string,rgb fore);
	virtual void drawString(int x,int y,const char *string,rgb fore,rgb back);
protected:
	static uint8_t sm_fontWidth, sm_fontHeight, sm_baseChar;
	static const uint8_t *sm_fontDef;

};

}
