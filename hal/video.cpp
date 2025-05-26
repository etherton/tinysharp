#include "video.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

namespace hal {
	
video *g_video;

uint8_t video::sm_fontWidth, video::sm_fontHeight, video::sm_baseChar;
const uint8_t *video::sm_fontDef;
uint16_t video::sm_screenWidth, video::sm_screenHeight, video::sm_scrollHeight;

void video::drawGlyph(int x,int y,int w,int h,const uint8_t *glyph,const palette &p) {
	for (;h; h--,y++,glyph++) {
		for (int i=0; i<w; i++)
			if (*glyph & (0x80>>i))
				fill(x+i,y,1,1,p);
	}
}

void video::setFont(uint8_t width,uint8_t height,const uint8_t *fontDef,uint8_t baseChar) {
	sm_fontWidth = width;
	sm_fontHeight = height;
	sm_fontDef = fontDef;
	sm_baseChar = baseChar;
}

void video::drawString(int x,int y,const palette &p,const char *string) {
	for (; *string; string++,x+=sm_fontWidth)
		drawGlyph(x,y,sm_fontWidth,sm_fontHeight,sm_fontDef + sm_fontHeight * (*string - sm_baseChar),p);
}

void video::drawString(int x,int y,const palette &p,const char *string,size_t len) {
	for (; len--; string++,x+=sm_fontWidth)
		drawGlyph(x,y,sm_fontWidth,sm_fontHeight,sm_fontDef + sm_fontHeight * (*string - sm_baseChar),p);
}

void video::drawString(int x,int y,const palette *p,const uint8_t *attr,const char *string,size_t len) {
	for (; len--; string++,x+=sm_fontWidth,attr++)
		drawGlyph(x,y,sm_fontWidth,sm_fontHeight,sm_fontDef + sm_fontHeight * (*string - sm_baseChar),p[*attr]);
}

void video::drawStringf(int x,int y,const palette &p,const char *fmt,...) {
	va_list args;
	char line[256];
	va_start(args,fmt);
	vsprintf(line,fmt,args);
	va_end(args);
	drawString(x,y,p,line,strlen(line));
}

}
