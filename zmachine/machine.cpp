#include "machine.h"
#include "opcodes.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>

// https://github.com/sindresorhus/macos-terminal-size/blob/main/terminal-size.c
#include <fcntl.h>     // open(), O_EVTONLY, O_NONBLOCK
#include <unistd.h>    // close()
#include <sys/ioctl.h> // ioctl()


#define HEIGHT 0x20
#define WIDTH 0x21

const char *attribute_names[] = {
	"clothing",
	"staggered",
	"fightbit",
	"visitied",
	"water_room",
	"maze_room",
	"dry_land",
	"concealed",

	"scope_inside",
	"sacred",
	"supporter",
	"open",
	"transparent",
	"trytakebit",
	"scenery",
	"turnable",

	"readable",
	"takeable",
	"rmingbit",
	"container",
	"light",
	"edible",
	"drinkable",
	"door",

	"climbable",
	"flame",
	"flammable",
	"vehicle",
	"toolbit",
	"weapon",
	"animate",
	"on"
};

const char *property_names[] = {
	nullptr,
	"?1",
	"container_action",
	"?3",
	"pseudo",
	"contains",
	"vtype",
	"strength",

	"text_string",
	"initial2",
	"capacity",
	"description",
	"trophy_value",
	"take_value",
	"initial",
	"size",

	"adjectives",
	"action",
	"name",
	"land_to",
	"out_to",
	"in_to",
	"down_to",
	"up_to",

	"southwest_to",
	"southeast_to",
	"northwest_to",
	"northeast_to",
	"south_to",
	"west_to",
	"east_to",
	"north_to",
};

static struct termios orig_termios, raw_termios;

static void standard_mode() {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void raw_mode() {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios);
}

void machine::init(const void *data,bool debug) {
	uint8_t version = *(uint8_t*)data;
	if (version > 8 || !((1<<version) & (0b1'1011'1000))) {
		printf("only versions 3,4,5,7,8 supported\n");
		exit(1);
	}
	m_storyShift = version==3? 1 : version<=5? 2 : 3; 
	m_sp = m_lp = 0;
	m_readOnly = (const uint8_t*) data;
	m_dynamicSize = m_header->staticMemoryAddr.getU();
	m_dynamic = new uint8_t[m_dynamicSize];
	m_undoDynamic = new uint8_t[m_dynamicSize];
	memcpy(m_dynamic, m_readOnly, m_dynamicSize);
	m_globalsOffset = m_header->globalVarsTableAddr.getU();
	m_abbreviations = m_header->abbreviationsAddr.getU();
	m_readOnlySize = m_header->storyLength.getU() << m_storyShift;
	memcpy(m_zscii,
		version>=5 && m_header->alphabetTableAddress.getU()? 
			(const char*)m_readOnly + m_header->alphabetTableAddress.getU() :
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"\033\n0123456789.,!?_#'\"/\\-:()",
		26*3);
	m_objectSmall = (object_header_small*) (m_dynamic + m_header->objectTableAddr.getU());
	m_objCount = m_header->version<4
		? (m_objectSmall->objTable[0].propAddr.getU() - (m_header->objectTableAddr.getU() + 31*2))/9
		: (m_objectLarge->objTable[0].propAddr.getU() - (m_header->objectTableAddr.getU() + 63*2))/14;
	if (debug) {
		printf("%d objects detected in story\n",m_objCount);
		printObjTree();
	}
	updateExtents();
	m_debug = debug;
	m_windowSplit = m_header->version < 4;
	m_currentWindow = 0;
	m_outputEnables = 3; // buffering enabled in window 0, stream 1 enabled
	m_cursorX = m_cursorY = 1;
	m_printed = 0;
	m_stored = 0;

	tcgetattr(STDIN_FILENO, &orig_termios);
	atexit(standard_mode);
	cfmakeraw(&raw_termios);

	run(m_header->initialPCAddr.getU());
}

void machine::printObjTree() {
	for (int i=1; i<=m_objCount; i++) {
		printf("Object %d named [",i);
		objPrint(i);
		printf("] parent %d child %d sibling %d\n",objGetParent(i).getU(),objGetChild(i).getU(),objGetSibling(i).getU());
	}
}

void machine::finishChar(uint8_t c) {
	if (c == 10) {
		m_cursorX = 1;
		if (m_currentWindow==1 && m_cursorY < m_windowSplit)
			m_cursorY++;
		else if (!m_currentWindow && m_cursorY < m_dynamic[HEIGHT])
			m_cursorY++;
	}
	else
		m_cursorX++;
}

void machine::flushMainWindow() {
	if (m_stored) {
		for (int i=0; i<m_stored; i++)
			putchar(m_lineBuffer[i]);
		m_cursorX += m_stored;
		m_stored = 0;
	}
}

void machine::print_char(uint8_t c) {
	if (m_outputEnables & (1 << 1)) {
		// are we buffering?
		if (m_currentWindow==0 && (m_outputEnables&1)) {
			if (c==10 || c==32) {
				if (m_stored && m_cursorX + m_stored > m_dynamic[WIDTH]) {
					putchar(10);
					finishChar(10);
				}
				flushMainWindow();
				if (m_cursorX != m_dynamic[WIDTH] + 1) {
					putchar(c);
					finishChar(c);
				}
				else
					finishChar(10);
			}
			else
				m_lineBuffer[m_stored++] = c;
		}
		else {
			putchar(c);
			finishChar(c);
			m_printed++;
		}
	}
	if (m_outputEnables & (1 << 3)) {
		word *cp = (word*)(m_dynamic + m_outputBuffer);
		write_mem8(m_outputBuffer + 2 + cp->getU(),c);
		cp->inc();
	}
}

void machine::print_num(int16_t v) {
	char buf[8], *b = buf;
	snprintf(buf,sizeof(buf),"%d",v);
	while (*b)
		print_char(*b++);
}

uint32_t machine::call(uint32_t pc,int storage,word operands[],uint8_t opCount) {
	if (!opCount)
		fault("impossible call with no address");
	uint32_t newPc = operands[0].getU() << m_storyShift;
	++operands;
	--opCount;
	// a call to zero does nothing except return zero
	if (newPc == 0) {
		if (storage != -1)
			ref(storage,true).setByte(0);
		return pc;
	}
	uint8_t localCount = read_mem8(newPc++);
	uint8_t larger = localCount > opCount? localCount : opCount;
	if (m_sp + larger + 3 > kStackSize)
		fault("stack overflow in routine call");
	word *frame = m_stack + m_sp;
	if (m_header->version < 5) { // there are N initial values for locals here
		memcpy(frame+3,m_readOnly + newPc,localCount<<1);
		newPc += localCount<<1;
	}
	else // the values are always zero
		memset(frame+3,0,localCount<<1);
	memcpy(frame+3,operands,opCount<<1);
	frame[0].set(pc);
	frame[1].set(((pc >> 16) << 13) | m_lp);
	frame[2].set((storage<<4) | larger);
	m_lp = m_sp;
	m_sp += larger + 3;
	if (m_debug > 1)
		printf("call to %06x, %d locals, sp now %03x and lp now %03x\n",newPc,larger,m_sp,m_lp);
	return newPc;
}

uint32_t machine::r_return(uint16_t v) {
	if (m_debug > 1)
		printf("returning %04x to caller, sp now %03x; ",v,m_lp);
	m_sp = m_lp;
	int32_t pc = m_stack[m_sp].getU() | ((m_stack[m_sp+1].getU() >> 13) << 16);
	m_lp = m_stack[m_sp+1].getU() & (kStackSize-1);
	int addr = m_stack[m_sp+2].getS() >> 4;
	if (m_debug > 1)
		printf("new PC is %06x, new lp is %03x, storage addr is %d\n",pc,m_lp,addr);
	if (addr != -1)
		ref(addr,true).set(v);
	return pc;
}

void machine::printz(uint8_t ch) {
	if (ch>=32)
		fault("invalid zchar %d",ch);
	if (m_abbrev) {
		print_zscii(read_mem16(m_abbreviations + ((m_abbrev-32+ch)<<1)).getU() << 1);
		m_shift = 0;
	}
	else if (m_extended) {
		m_extended = (m_extended << 5) | ch;
		if (m_extended > 1023) {
			print_char(m_extended & 255);
			m_extended = 0;
		}
	}
	else if (ch == 0)
		print_char(32);
	else if (ch < 4)
		m_abbrev = ch << 5;
	else if (ch == 4)
		m_shift = 1;
	else if (ch == 5)
		m_shift = 2;
	else if (m_shift==2 && ch==6)
		m_shift = 0, m_extended = 1;
	else {
		print_char(m_zscii[m_shift*26+(ch-6)]);
		m_shift = 0;
	}
}

uint32_t machine::print_zscii(uint32_t addr) {
	uint16_t w;
	m_abbrev = 0;
	m_extended = 0;
	m_shift = 0;
	do {
		w = read_mem16(addr).getU();
		printz((w >> 10) & 31);
		printz((w >> 5) & 31);
		printz(w & 31);
		addr+=2;
	} while (!(w & 0x8000));
	return addr;
}

void machine::fault(const char *fmt,...) const {
	va_list args;
	va_start(args,fmt);
	printf("fault at address %x, opcode bytes %x %x...: ",
		m_faultpc, read_mem8(m_faultpc), read_mem8(m_faultpc+1));
	vprintf(fmt,args);
	printf("\n");
	va_end(args);
	exit(1);
}

void machine::memfault(const char *fmt,...) const {
	va_list args;
	va_start(args,fmt);
	printf("memfault at address %x: ",m_faultpc);
	vprintf(fmt,args);
	printf("\n");
	va_end(args);
	exit(1);
}

void machine::updateExtents() {
	int fd = open("/dev/tty",O_EVTONLY | O_NONBLOCK);
	if (fd != -1) {
		struct winsize ws;
		int result = ioctl(fd,TIOCGWINSZ, &ws);
		close(fd);
		if (result != -1) {
			m_dynamic[HEIGHT] = ws.ws_row;
			m_dynamic[WIDTH] = ws.ws_col;
		}
	}
}

void machine::setTextStyle(uint8_t style) {
	if (style == 1)
		printf("\033[7m");
	else if (style == 0)
		printf("\033[0m");
}

void machine::showStatus() {
	updateExtents();
	if (m_debug || m_header->version > 3)
		return;
	uint16_t globals = m_header->globalVarsTableAddr.getU();
	flushMainWindow();
	setWindow(1);
	setTextStyle(1);
	m_printed = 0;
	objPrint(read_mem16(globals).getU());
	uint8_t screenWidth = read_mem8(WIDTH);
	int16_t score = read_mem16(globals+2).getS();
	uint16_t moves = read_mem16(globals+4).getU();
	char scoreBuf[16];
	snprintf(scoreBuf,16,"%d/%d",score,moves);
	screenWidth -= strlen(scoreBuf);
	while (m_printed < screenWidth)
		print_char(' ');
	printf("%s",scoreBuf);
	setTextStyle(0);
	setWindow(0);
	fflush(stdout);
}

void machine::setWindow(uint8_t window) {
	if (window && !m_windowSplit)
		fault("setWindow %d called with no split",window);
	if (m_currentWindow == 0) {
		flushMainWindow();
		m_saveX = m_cursorX;
		m_saveY = m_cursorY;
	}
	/* window 1 is always the top (aka status line) */
	if (window) {
		printf("\0337\033[H");
		m_cursorX = m_cursorY = 1;
	}
	else if (!window) {
		printf("\0338");
		m_cursorX = m_saveX;
		m_cursorY = m_saveY;
	}
	fflush(stdout);
	m_currentWindow = window;
}

void machine::eraseWindow(int16_t cmd) {
	if (cmd == 1)
		printf("\033[H\033[2K");
	else
		printf("\033[\033[2J");
}

void machine::setCursor(uint8_t x,uint8_t y) {
	printf("\033[%d;%dH",y,x);
	fflush(stdout);
	m_cursorX = x;
	m_cursorY = y;
}

void machine::setOutput(int enable,uint16_t tableAddr) {
	if (enable > 0) {
		m_outputEnables |= (1 << enable);
		if (enable == 3) {
			write_mem16(tableAddr,byte2word(0));
			m_outputBuffer = tableAddr;
		}
	}
	else if (enable < 0)
		m_outputEnables &= ~(1 << -enable);
}

static int32_t random_seed = 0;
static int randomNumber(void) {
	// borrowed from mojozork so I can use that project's validation script
    // this is POSIX.1-2001's potentially bad suggestion, but we're not exactly doing cryptography here.
    random_seed = random_seed * 1103515245 + 12345;
    return (int) ((unsigned int) (random_seed / 65536) % 32768);
}

void machine::encode_text(word dest[],const char *src,uint8_t len) {
	int maxStore = m_header->version>=4? 9 : 6, stored = 0;
	auto store = [&](uint8_t c) {
		if (stored < maxStore) {
			//printf("{{storing zchar %d}}\n",c);
			dest[stored/3].setZscii(stored%3,c);
			++stored;
		}
	};
	for (; len; len--,src++) {
		const char *a0 = (char*)memchr(m_zscii+0,*src,26);
		const char *a1 = (char*)memchr(m_zscii+26,*src,26);
		const char *a2 = (char*)memchr(m_zscii+52,*src,26);
		int shift, ch;
		if (*src==' ')
			shift=0, ch=0;
		else if (a0)
			shift=0, ch=a0 - m_zscii + 6;
		else if (a1)
			shift=1, ch=a1 - m_zscii - 26 + 6;
		else if (a2)
			shift=1, ch=a2 - m_zscii - 52 + 6;
		else {
			store(2); store(6); store(*src >> 5); store(*src);
			continue;
		}
		if (shift)
			store(shift+3);
		store(ch);
	}
	// pad with unused shift char
	while (stored < maxStore)
		store(5);
	// terminate the zscii.
	auto &last = dest[m_header->version>=4?2:1];
	last.set(last.getU() | 0x8000);
}

uint8_t machine::read_input(uint16_t textAddr,uint16_t parseAddr) {
	char buffer[256];
	bool internal;
	flushMainWindow();
	do {
		fgets(buffer,sizeof(buffer),stdin);
		while (strlen(buffer) && buffer[strlen(buffer)-1]==10)
			buffer[strlen(buffer)-1] = 0;
		// printf("[[%s]]\n",buffer);
		for (char *t = buffer; *t; t++)
			if (*t>='A'&&*t<='Z') 
				*t +=32; 
		internal = false;
		if (!strncmp(buffer,"#random ",8)) {
			random_seed = atoi(buffer+9);
			printf("{random_seed set to %d}\n",random_seed);
			internal = true;
		}
		else if (!strncmp(buffer,"#objtree",8)) {
			printObjTree();
			internal = true;
		}
	} while (strlen(buffer) >= 240 || internal);
	uint8_t sl = strlen(buffer), offset;
	if (m_header->version < 5) {
		uint8_t s = read_mem8(textAddr);
		if (sl > s-1)
			sl = s-1;
		if (textAddr + 1 + s - 1 > m_dynamicSize)
			fault("read_input (v1-4) past dynamic memory");
		memcpy(m_dynamic + textAddr + 1,buffer,sl);
		write_mem8(textAddr + 1 + sl,0);
		//printf("{{read [%*.*s]}}\n",sl,sl,m_dynamic+textAddr+1);
		offset = 1;
	}
	else {
		uint8_t s = read_mem8(textAddr);
		uint8_t soFar = read_mem8(textAddr+1);
		if (soFar)
			printf("{{%d inputs bytes already there}}\n",soFar);
		if (sl > s - soFar)
			sl = s - soFar;
		m_dynamic[textAddr+1] = sl;
		if (textAddr + 2 + soFar + sl > m_dynamicSize)
			fault("read_input (v5+) past dynamic memory");
		memcpy(m_dynamic + textAddr + 2 + soFar,buffer,sl);
		// printf("{{read [%*.*s]}}\n",sl,sl,m_dynamic+textAddr+2);
		offset = 2;
	}
	if (parseAddr)	
		return tokenise(textAddr,parseAddr,offset);
	else
		return 13;
}

uint8_t machine::tokenise(uint16_t textAddr,uint16_t parseAddr,uint8_t offset) {
	uint16_t dictAddr = m_header->dictionaryAddr.getU();
	// the separators are actually stored as parsed words. spaces are not.
	uint8_t numSeparators = read_mem8(dictAddr++);
	uint16_t separators = dictAddr;
	dictAddr += numSeparators;
	uint8_t entryLength = read_mem8(dictAddr++);
	uint16_t numWords = read_mem16(dictAddr).getU();
	dictAddr+=2;
	uint8_t sl = m_header->version<5? strlen((char*)m_dynamic+textAddr+1) : m_dynamic[textAddr+1];
	uint8_t stop = offset + sl;
	uint8_t maxParsed = read_mem8(parseAddr);
	uint8_t numParsed = 0;
	//printf("{{%d separators, %d words, %d bytes per entry}}\n",numSeparators,numWords,entryLength)
	// printf("{{offset=%d,stop=%d}}\n",offset,stop);
	while (offset < stop && numParsed < maxParsed) {
		// printf("{{offset=%d}}\n",offset);
		// skip spaces
		while (m_dynamic[textAddr+offset] == 32 && offset<stop)
			++offset;
		if (offset==stop)
			break;
		uint8_t wordLen = 1;
		// if it's not a word separator, keep looking until we get to end, space, or a word separator
		if (!memchr(m_dynamic + separators,m_dynamic[textAddr+offset],numSeparators)) {
			while (offset+wordLen < stop && m_dynamic[textAddr+offset+wordLen]!=32 && 
					!memchr(m_dynamic + separators,m_dynamic[textAddr+offset+wordLen],numSeparators))
				++wordLen;
		}
		word zword[3];
		// printf("{{encoding %*.*s}}\n",wordLen,wordLen,m_dynamic+textAddr+offset);
		encode_text(zword,(char*)m_dynamic + textAddr + offset,wordLen);
		// printf("{{%04x,%04x}}\n",zword[0].getU(),zword[1].getU());
		uint8_t byteCount = m_header->version<5? 4 : 6;
		// we could do a binary search here but machines are orders of magnitude faster now.
		uint16_t i;
		for (i=0; i<numWords; i++)
			if (!memcmp(zword,m_readOnly + dictAddr + i * entryLength,byteCount))
				break;
		if (i == numWords)
			write_mem16(parseAddr+2+numParsed*4,byte2word(0));
		else
			write_mem16(parseAddr+2+numParsed*4,word2word(dictAddr + i * entryLength));
		write_mem8(parseAddr+2+numParsed*4+2,wordLen);
		write_mem8(parseAddr+2+numParsed*4+3,offset);
		/* printf("{{%02x%02x%02x%02x}}\n",m_dynamic[parseAddr+2+numParsed*4],m_dynamic[parseAddr+2+numParsed*4+1],
			m_dynamic[parseAddr+2+numParsed*4+2],m_dynamic[parseAddr+2+numParsed*4+3]); */
		offset += wordLen;
		++numParsed;
	}
	write_mem8(parseAddr+1,numParsed);
	// printf("{{%d words parsed}}\n",numParsed);
	return 13;
}

void machine::printTable(uint16_t zsciiAddr,uint16_t width,uint16_t height,uint16_t skip) {
	printf("{{%d slots in zscii table @%04x,w=%d,h=%d}}\n",read_mem16(zsciiAddr).getU(),zsciiAddr,width,height);
}

void machine::run(uint32_t pc) {
	random_seed = 2;
	for (;;) {
		m_faultpc = pc;
		// if (pc == 0x8c6) __builtin_debugtrap();
		uint16_t opcode = read_mem8(pc++);
		if (opcode == 0xBE && m_header->version>=5)
			opcode = 0x100 | read_mem8(pc++);
		if (opcode >= 0x120)
			fault("invalid extended opcode");

		int opcodeLen = strlen(opcode_names[opcode]);
		if (strchr(opcode_names[opcode],'$'))
			opcodeLen = strchr(opcode_names[opcode],'$') - opcode_names[opcode] - 1;
		if (m_debug) printf("%06x: %*.*s ",m_faultpc,opcodeLen,opcodeLen,opcode_names[opcode]);
		uint16_t types = opTypes[opcode >> 4] << 8;
		if (!types)
			types = read_mem8(pc++) << 8;
		if (opcode==0xEC || opcode==0xFA)
			types |= read_mem8(pc++);
		else
			types |= 255;
		// remember the last op (used for jumps)
		uint8_t opCount = 0;
		word operands[8];
		const char *nextType = strchr(opcode_names[opcode],'$');
		while (types != 0xFFFF) {
			uint8_t op = read_mem8(pc++);
			if (m_debug && opCount)
				printf(", ");
			switch (types & 0xC000) {
				case 0x0000: 
					operands[opCount++].setHL(op,read_mem8(pc++)); 
					if (m_debug) 
						printf("$%04x",operands[opCount-1].getU()); 
					break;
				case 0x4000: 
					operands[opCount++].setByte(op); 
					if (m_debug)
						printf("$%02x",op);
					break;
				case 0x8000: 
					if (m_debug) {
						if (op==0) printf("--(sp)");
						else if (op<16) printf("L%d",op-1);
						else printf("G%d",op-16);
					}
					operands[opCount++] = ref(op, false); 
					if (m_debug)
						printf(" [$%04x]",operands[opCount-1].getU());
					break;
			}
			if (m_debug && nextType) {
				if (nextType[1]=='o') {
					print_char('{');
					if (operands[opCount-1].notZero())
						objPrint(operands[opCount-1].getU());
					print_char('}');
					print_char(' ');
				}
				else if (nextType[1] == 'a') {
					printf(",%s ",attribute_names[operands[opCount-1].lo]);
				}
				else if (nextType[1] == 'p')
					printf("p?%s ",property_names[operands[opCount-1].lo]);
				else
					fault("bug in opcode type string");
				nextType = strchr(nextType+1,'$');
			}
			types = (types << 2) | 0x3;
		}
			
		uint8_t decode_byte = decode[opcode] >> version_shift[m_header->version];
		int dest = -1; // invalid value
		int16_t branch_offset = -32768;
		bool branch_cond;
		if (decode_byte & 1) {
			dest = read_mem8(pc++);
			if (m_debug) {
				if (!dest) printf(" -> (sp)++");
				else if (dest < 16) printf(" -> L%d",dest-1);
				else printf(" -> G%d",dest-16);
			}
		}
		if (decode_byte & 2) {
			branch_offset = read_mem8(pc++);
			branch_cond = branch_offset >> 7;
			branch_offset &= 127;
			if (branch_offset & 64)
				branch_offset &= 63;
			else {
				if (branch_offset & 32)
					branch_offset |= 0xC0;
				branch_offset = (branch_offset << 8) | read_mem8(pc++);
			}
			if (m_debug) {
				if (branch_offset==0||branch_offset==1)
					printf(" ?%s%s",branch_cond?"":"~",branch_offset?"rtrue":"rfalse");
				else
					printf(" ?%s (%04d)",branch_cond?"":"~",branch_offset);
			}
		}
		if (m_debug)
			printf("\n");
		auto branch = [&](bool test) {
			if (branch_offset == -32768)
				fault("interpreter bug, branch set up incorrectly");
			if (test == branch_cond) {
				if (branch_offset == 0)
					pc = r_return(0);
				else if (branch_offset == 1)
					pc = r_return(1);
				else {
					if (branch_offset < 0 && pc < -branch_offset)
						fault("branch to invalid address below zero");
					else if (branch_offset > 0 && pc + branch_offset >= m_readOnlySize)
						fault("branch to invalid address past end of story");
					pc += branch_offset - 2;
					if (pc < m_dynamicSize)
						fault("likely invalid branch into dynamic memory");
				}
			}
		};
		// B2 and B3 are inline zscii 
		if (opcode < 0x80 || (opcode >= 0xC2 && opcode < 0xE0)) { // 2OP (except for jz VAR that can take more than two ops)
			if (opCount != 2)
				fault("2OP with something other than two operands");
			switch (opcode & 31) {
				case 0x01: branch(operands[0].getS() == operands[1].getS()); break;
				case 0x02: branch(operands[0].getS() < operands[1].getS()); break; 
				case 0x03: branch(operands[0].getS() > operands[1].getS()); break;
				case 0x04: branch(var(operands[0].getS()).dec() < operands[1].getS()); break;
				case 0x05: branch(var(operands[0].getS()).inc() > operands[1].getS()); break;
				case 0x06: branch(objIsChildOf(operands[0].getU(),operands[1].getU())); break;
				case 0x07: branch((operands[0].getU() & operands[1].getU()) == operands[1].getU()); break;
				case 0x08: ref(dest,true).set(operands[0].getU() | operands[1].getU()); break;
				case 0x09: ref(dest,true).set(operands[0].getU() & operands[1].getU()); break;
				case 0x0A: branch(objTestAttribute(operands[0].getU(),operands[1].getU())); break;
				case 0x0B: objSetAttribute(operands[0].getU(),operands[1].getU()); break;
				case 0x0C: objClearAttribute(operands[0].getU(),operands[1].getU()); break;
				case 0x0D: var(operands[0].getS()) = operands[1]; break;
				case 0x0E: objMoveTo(operands[0].getU(),operands[1].getU()); break;
				case 0x0F: ref(dest,true) = read_mem16(operands[0].getU() + (operands[1].getU()<<1)); break;
				case 0x10: ref(dest,true).setByte(read_mem8(operands[0].getU() + operands[1].getU())); break;
				case 0x11: ref(dest,true) = objGetProperty(operands[0].getU(),operands[1].getU()); break;
				case 0x12: ref(dest,true) = objGetPropertyAddr(operands[0].getU(),operands[1].getU()); break;
				case 0x13: ref(dest,true) = objGetNextProperty(operands[0].getU(),operands[1].getU()); break;
				case 0x14: ref(dest,true).set(operands[0].getS() + operands[1].getS()); break;
				case 0x15: ref(dest,true).set(operands[0].getS() - operands[1].getS()); break;
				case 0x16: ref(dest,true).set(operands[0].getS() * operands[1].getS()); break;
				case 0x17: if (!operands[1].getS()) fault("division by zero"); 
					ref(dest,true).set(operands[0].getS() / operands[1].getS()); break;
				case 0x18: if (!operands[1].getS()) fault("modulo by zero"); 
					ref(dest,true).set(operands[0].getS() % operands[1].getS()); break;
				case 0x19: pc = call(pc,dest,operands,opCount); break;
				case 0x1A: pc = call(pc,-1,operands,opCount); break;
				default: fault("unimplemented 2OP opcode"); break;
			}
		}
		else if (opcode >= 0x80 && opcode < 0xB0) {
			if (opCount != 1)
				fault("1OP with something other than one operand");
			switch (opcode & 15) {
				case 0x0: branch(!operands[0].getU()); break;
				case 0x1: branch((ref(dest,true) = objGetSibling(operands[0].getU())).notZero()); break;
				case 0x2: branch((ref(dest,true) = objGetChild(operands[0].getU())).notZero()); break;
				case 0x3: ref(dest,true) = objGetParent(operands[0].getU()); break;
				case 0x4: ref(dest,true) = objGetPropertyLen(operands[0].getU()); break;
				case 0x5: var(operands[0].getS()).inc(); break;
				case 0x6: var(operands[0].getS()).dec(); break;
				case 0x7: print_zscii(operands[0].getU()); break;
				case 0x8: pc = call(pc,dest,operands,opCount); break;
				case 0x9: objUnparent(operands[0].getU()); break;
				case 0xA: objPrint(operands[0].getU()); break;
				case 0xB: pc = r_return(operands[0].getS()); break;
				case 0xC: pc += operands[0].getS() - 2; break;
				case 0xD: print_zscii(operands[0].getU() << m_storyShift); break;
				case 0xE: ref(dest,true) = var(operands[0].getS()); break;
				case 0xF: if (m_header->version < 5) ref(dest,true).set(~operands[0].getU());
					  else pc = call(pc,-1,operands,opCount); break;
			}
		}
		else {
			switch (opcode) {
				case 0xB0: pc = r_return(1); break;
				case 0xB1: pc = r_return(0); break;
				case 0xB2: pc = print_zscii(pc); break;
				case 0xB3: pc = print_zscii(pc); pc = r_return(1); break;
				case 0xB4: break; // nop
				case 0xB5: { chunk c[3]; 
							c[0].data = m_dynamic; c[0].size = m_dynamicSize;
							c[1].data = &m_sp; c[1].size = (kStackSize + 2) * 2;
							c[2].data = &pc; c[2].size = 4;
							if (writeSaveData(c,3)) {
								if (m_header->version<4) 
									branch(true);
							} }
							break;
				case 0xB6: { chunk c[3];
							c[0].data = m_dynamic; c[0].size = m_dynamicSize;
							c[1].data = &m_sp; c[1].size = (kStackSize + 2) * 2;
							c[2].data = &pc; c[2].size = 4;
							readSaveData(c,3); updateExtents(); }
							break;
				case 0xB7: m_sp =  m_lp = 0; 
							memcpy(m_dynamic, m_readOnly, m_dynamicSize); 
							updateExtents();
							pc = m_header->initialPCAddr.getU();
							 break;
				case 0xB8: if (!m_sp) fault("stack underflow in ret_popped"); pc = r_return(m_stack[--m_sp].getU()); break;
				case 0xB9: if (!m_sp) fault("stack underflow in pop"); --m_sp; break;
				case 0xBA: exit(0); break;
				case 0xBB: print_char(10); break;
				case 0xBC: showStatus(); break;
				case 0xC1: if (opCount==2)
							branch(operands[0].getS() == operands[1].getS());
							else if (opCount==3)
							branch(operands[0].getS() == operands[1].getS() || operands[0].getS() == operands[2].getS());
							else if (opCount==4)
							branch(operands[0].getS() == operands[1].getS() || operands[0].getS() == operands[2].getS() || operands[0].getS() == operands[3].getS());
							else fault("impossible jz variant");
							break;
				case 0xE0: pc = call(pc,dest,operands,opCount); break;
				case 0xE1: write_mem16(operands[0].getU()+(operands[1].getU()<<1),operands[2]); break;
				case 0xE2: write_mem8(operands[0].getU()+operands[1].getU(),operands[2].lo); break;
				case 0xE3: objSetProperty(operands[0].getU(),operands[1].getU(),operands[2]); break;
				case 0xE4: if (opCount != 2) fault("only two operand read opcode supported");
						   showStatus();
						   if (m_header->version>=5)
						   	ref(dest,true).setByte(read_input(operands[0].getU(),operands[1].getU()));
							else read_input(operands[0].getU(),operands[1].getU());
							break;
				case 0xE5: print_char(operands[0].lo); break;
				case 0xE6: print_num(operands[0].getS()); break;
				case 0xE7: if (operands[0].getS() == 0)
								random_seed = time(NULL);
							else if (operands[0].getS() < 0)
								random_seed = -operands[0].getS();
							ref(dest,true).set(operands[0].getS() > 1? ((randomNumber() % (operands[0].getS() - 1)) + 1) : 0);
							break;
				case 0xE8: push(operands[0]); break;
				case 0xE9: var(operands[0].getS()) = pop(); break;
				case 0xEA: m_windowSplit = operands[0].getU(); break;
				case 0xEB: setWindow(operands[0].getU()); break;
				case 0xEC: pc = call(pc,dest,operands,opCount); break;
				case 0xED: eraseWindow(operands[0].getS()); break; // erase_window
				case 0xEF: setCursor(operands[1].getU(),operands[0].getU()); break; // set_cursor line col
				case 0xF1: setTextStyle(operands[0].lo); break; // set_text_style
				case 0xF2: if (operands[0].notZero()) m_outputEnables |= 1; else m_outputEnables &= ~1; break; // buffer_mode
				case 0xF3: setOutput(operands[0].getS(),opCount>1?operands[1].getU():0); break; // output_stream
				case 0xF5: break; // sound_effect
				case 0xF6: raw_mode(); read(STDIN_FILENO, &ref(dest,true).lo, 1); standard_mode(); break; // read_char
				case 0xF7: branch(scanTable(dest,operands[0],operands[1].getU(),operands[2].getU(),
							m_header->version>=5&&opCount==4?operands[3].lo:0x82));
							break;
				case 0xF9: pc = call(pc,-1,operands,opCount); break;
				case 0xFA: pc = call(pc,-1,operands,opCount); break;
				case 0xFB: 
						if (opCount != 2) fault("only two-operand form of tokenise is supported");
						tokenise(operands[0].getU(),operands[1].getU());
						break;
				case 0xFE: printTable(operands[0].getU(),operands[1].getU(),opCount>2?operands[2].getU():1,
						opCount>3?operands[3].getU():0);
						break;
				case 0xFF: branch(operands[0].getU() <= (m_stack[m_lp+2].lo & 15)); break;
				case 0x109: 
					ref(dest,true) = byte2word(1); // save_undo
					memcpy(m_undoDynamic, m_dynamic, m_dynamicSize);
					m_undoSp = m_sp; m_undoLp = m_lp; m_undoPc = pc;
					memcpy(m_undoStack, m_stack, sizeof(m_stack));
					break;
				case 0x10A:
					memcpy(m_dynamic, m_undoDynamic, m_dynamicSize);
					m_sp = m_undoSp; m_lp = m_undoLp; pc = m_undoPc;
					memcpy(m_stack, m_undoStack, sizeof(m_stack));
					break;
				default: fault("unimplemented 0OP/VAR/EXT opcode"); break;
			}
		}
	}
}

bool machine::writeSaveData(chunk *chunks,uint32_t count) {
	FILE *f = fopen("save.dat","wb");
	if (!f)
		return false;
	for (uint32_t i=0; i<count; i++)
		fwrite(chunks[i].data,1,chunks[i].size,f);
	fclose(f);
	return true;
}

bool machine::readSaveData(chunk *chunks,uint32_t count) {
	FILE *f = fopen("save.dat","rb");
	if (!f)
		return false;
	for (uint32_t i=0; i<count; i++)
		fread(chunks[i].data,1,chunks[i].size,f);
	fclose(f);
	return true;
}

int main(int argc,char **argv) {
	FILE *f = fopen(argv[1],"rb");
	fseek(f,0,SEEK_END);
	long size = ftell(f);
	rewind(f);
	char *story = new char[size];
	fread(story,1,size,f);
	fclose(f);
	
	machine *m = new machine;
	m->init(story,argc>2&&!strcmp(argv[2],"-debug"));
}