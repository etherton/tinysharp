#include "machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

// https://github.com/sindresorhus/macos-terminal-size/blob/main/terminal-size.c
#include <fcntl.h>     // open(), O_EVTONLY, O_NONBLOCK
#include <unistd.h>    // close()
#include <sys/ioctl.h> // ioctl()

static struct termios orig_termios, raw_termios;

static void standard_mode() {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void raw_mode() {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios);
}

void interface::init() {
	tcgetattr(STDIN_FILENO, &orig_termios);
	atexit(standard_mode);
	cfmakeraw(&raw_termios);
}

void interface::putchar(int ch) {
    putc(ch, stdout);
}

int interface::readline(char *dest,unsigned destSize) {
    return strlen(fgets(dest,destSize,stdin));
}

int interface::readchar() {
    char result;
    raw_mode(); 
    read(STDIN_FILENO, &result, 1); 
    standard_mode();
    return result;
}


void interface::setTextStyle(uint8_t style) {
	if (style == 1)
		printf("\033[7m");
	else if (style == 0)
		printf("\033[0m");
}

void interface::setWindow(uint8_t window) {
    if (window)
    	printf("\0337\033[H");
    else
    	printf("\0338");
    fflush(stdout);
}

void interface::eraseWindow(uint8_t cmd) {
    if (cmd == 1)
		printf("\033[H\033[2K");
	else
		printf("\033[\033[2J");
}

void interface::setCursor(uint8_t x,uint8_t y) {
	printf("\033[%d;%dH",y,x);
	fflush(stdout);
}

void interface::updateExtents(uint8_t &width,uint8_t &height) {
	int fd = open("/dev/tty",O_EVTONLY | O_NONBLOCK);
	if (fd != -1) {
		struct winsize ws;
		int result = ioctl(fd,TIOCGWINSZ, &ws);
		close(fd);
		if (result != -1) {
			height = ws.ws_row;
			width = ws.ws_col;
		}
	}
}

bool interface::writeSaveData(chunk *chunks,uint32_t count) {
	FILE *f = fopen("save.dat","wb");
	if (!f)
		return false;
	for (uint32_t i=0; i<count; i++)
		fwrite(chunks[i].data,1,chunks[i].size,f);
	fclose(f);
	return true;
}

bool interface::readSaveData(chunk *chunks,uint32_t count) {
	FILE *f = fopen("save.dat","rb");
	if (!f)
		return false;
	for (uint32_t i=0; i<count; i++)
		fread(chunks[i].data,1,chunks[i].size,f);
	fclose(f);
	return true;
}

char* interface::readStory(const char *name) {
	FILE *f = fopen(name,"rb");
    if (!f)
        return nullptr;
	fseek(f,0,SEEK_END);
	long size = ftell(f);
	rewind(f);
	char *story = new char[size];
	fread(story,1,size,f);
	fclose(f);
    return story;
}
