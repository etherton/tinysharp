#include "video.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

namespace hal {
	
video *g_video;

uint8_t video::sm_fontWidth, video::sm_fontHeight, video::sm_baseChar;
const uint8_t *video::sm_fontDef;

void video::drawGlyph(int x,int y,int w,int h,const uint8_t *glyph,rgb fore) {
	for (;h; h--,y++,glyph++) {
		for (int i=0; i<w; i++)
			if (*glyph & (1<<i))
				fill(x+i,y,1,1,fore);
	}
}

void video::drawGlyph(int x,int y,int w,int h,const uint8_t *glyph,rgb fore,rgb back) {
	for (;h; h--,y++,glyph++) {
		for (int i=0; i<w; i++)
			fill(x+i,y,1,1,*glyph & (1<<i)? fore : back);
	}
}

void video::setFont(uint8_t width,uint8_t height,const uint8_t *fontDef,uint8_t baseChar) {
	sm_fontWidth = width;
	sm_fontHeight = height;
	sm_fontDef = fontDef;
	sm_baseChar = baseChar;
}

void video::drawString(int x,int y,rgb fore,const char *string) {
	for (; *string; string++,x+=sm_fontWidth)
		drawGlyph(x,y,sm_fontWidth,sm_fontHeight,sm_fontDef + sm_fontHeight * (*string - sm_baseChar),fore);
}

void video::drawString(int x,int y,rgb fore,rgb back,const char *string) {
	for (; *string; string++,x+=sm_fontWidth)
		drawGlyph(x,y,sm_fontWidth,sm_fontHeight,sm_fontDef + sm_fontHeight * (*string - sm_baseChar),fore,back);
}

void video::drawStringf(int x,int y,rgb fore,const char *fmt,...) {
	va_list args;
	char line[256];
	va_start(args,fmt);
	vsprintf(line,fmt,args);
	va_end(args);
	drawString(x,y,fore,line);
}

void video::drawStringf(int x,int y,rgb fore,rgb back,const char *fmt,...) {
	va_list args;
	char line[256];
	va_start(args,fmt);
	vsprintf(line,fmt,args);
	va_end(args);
	drawString(x,y,fore,back,line);
}

}
