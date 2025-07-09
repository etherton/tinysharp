#include "machine.h"
#include "opcodes.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void machine::init(const void *data) {
	uint8_t version = *(uint8_t*)data;
	if (version > 8 || !((1<<version) & (0b1'1011'1000))) {
		printf("only versions 3,4,5,7,8 supported\n");
		exit(1);
	}
	m_storyShift = version==3? 1 : version<=5? 2 : 3; 
	m_sp = m_lp = kStackSize;
	m_readOnly = (const uint8_t*) data;
	m_dynamicSize = m_header->staticMemoryAddr.getU();
	m_dynamic = new uint8_t[m_dynamicSize];
	memcpy(m_dynamic, m_readOnly, m_dynamicSize);
	m_globalsOffset = m_header->globalVarsTableAddr.getU();
	m_abbreviations = m_header->abbreviationsAddr.getU();
	m_readOnlySize = m_header->storyLength.getU() << m_storyShift;
	m_objectSmall = (object_header_small*) (m_dynamic + m_header->objectTableAddr.getU());
	m_objCount = m_header->version<4
		? (m_objectSmall->objTable[0].propAddr.getU() - (m_header->objectTableAddr.getU() + 31*2))/9
		: (m_objectLarge->objTable[0].propAddr.getU() - (m_header->objectTableAddr.getU() + 63*2))/14;
	printf("%d objects detected in story\n",m_objCount);
	memcpy(m_zscii,
		version>=5 && m_header->alphabetTableAddress.getU()? 
			(const char*)m_readOnly + m_header->alphabetTableAddress.getU() :
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"\033\n0123456789.,!?_#'\"/\\-:()",
		26*3);
	m_debug = true;
	run(m_header->initialPCAddr.getU());
}


void machine::print_char(uint8_t c) {
	putchar(c);
}

void machine::print_num(int16_t v) {
	if (v < 0) {
		putchar('-');
		v = -v;
	}
	int16_t b = 10000;
	while (b > v)
		b /= 10;
	while (b!=1) {
		putchar(v / b + '0');
		v %= b;
		b /= 10;
	}
	putchar(v + '0');
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
	m_sp -= localCount + 3;
	word *frame = m_stack + m_sp;
	if (m_header->version < 5) { // there are N initial values for locals here
		memcpy(frame+3,m_readOnly + newPc,localCount<<1);
		newPc += localCount<<1;
	}
	else // the values are always zero
		memset(frame+3,0,localCount<<1);
	if (opCount > localCount)
		fault("too many parameters for function");
	else
		memcpy(frame+3,operands,opCount<<1);
	frame[0].set(pc);
	frame[1].set(((pc >> 16) << 13) | m_lp);
	frame[2].set(localCount | (storage << 4));
	m_lp = m_sp;
	return newPc;
}

uint32_t machine::r_return(uint16_t v) {
	m_sp = m_lp;
	uint32_t pc = pop().getU();
	m_lp = pop().getU();
	m_lp &= (kStackSize-1);
	pc |= (m_lp >> 13) << 16;
	int addr = pop().getS();
	m_sp += addr & 15;
	addr >>= 4;
	if (addr != -1)
		ref(addr,true).set(v);
	return pc;
}

void machine::printz(uint8_t ch) {
	if (ch>=32)
		fault("invalid zchar %d",ch);
	if (m_abbrev) {
		int inner = m_abbrev-32+ch;
		m_abbrev = 0;
		print_zscii(read_mem16(m_abbreviations + (inner<<1)).getU() << 1);
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
	else if (m_shift==2 && ch==7)
		print_char(10);
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

static int32_t random_seed = 0;
static int randomNumber(void) {
	// borrowed from mojozork so I can use that project's validation script
    // this is POSIX.1-2001's potentially bad suggestion, but we're not exactly doing cryptography here.
    random_seed = random_seed * 1103515245 + 12345;
    return (int) ((unsigned int) (random_seed / 65536) % 32768);
}

void machine::run(uint32_t pc) {
	for (;;) {
		m_faultpc = pc;
		uint16_t opcode = read_mem8(pc++);
		if (opcode == 0xBE && m_header->version>=5)
			opcode = 0x100 | read_mem8(pc++);
		if (opcode >= 0x120)
			fault("invalid extended opcode");

		if (m_debug) printf("%06x: %s ",m_faultpc,opcode_names[opcode]);
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
		while (types != 0xFFFF) {
			uint8_t op = read_mem8(pc++);
			if (opCount)
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
						if (op==0) printf("-(sp)");
						else if (op<16) printf("L%d",op-1);
						else printf("G%d",op-16);
					}
					operands[opCount++] = ref(op, false); 
					if (m_debug)
						printf(" [$%04x]",operands[opCount-1].getU());
					break;
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
				if (!dest) printf(" -> (sp)");
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
					printf("?%s%s",branch_cond?"":"~",branch_offset?"rtrue":"rfalse");
				else
					printf("?%s%04x",branch_cond?"":"~",branch_offset);
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
				else
					pc += branch_offset - 2;
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
				case 0xE: ref(operands[1].getS(),true) = var(operands[0].getS());
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
				case 0xB8: if (!m_sp) fault("stack underflow in ret_popped"); pc = r_return(m_stack[--m_sp].getU()); break;
				case 0xB9: if (!m_sp) fault("stack underflow in pop"); --m_sp; break;
				case 0xBA: exit(0); break;
				case 0xBB: print_char(10); break;
				case 0xC1: if (opCount==2)
							branch(operands[0].getS() == operands[1].getS());
							else if (opCount==3)
							branch(operands[0].getS() == operands[1].getS() || operands[0].getS() == operands[2].getS());
							else if (opCount==4)
							branch(operands[0].getS() == operands[1].getS() || operands[0].getS() == operands[2].getS() || operands[0].getS() == operands[3].getS());
							else fault("impossible jz variant");
				case 0xE0: pc = call(pc,dest,operands,opCount); break;
				case 0xE1: write_mem16(operands[0].getU()+(operands[1].getU()<<1),operands[2]); break;
				case 0xE2: write_mem8(operands[0].getU()+operands[1].getU(),operands[2].lo); break;
				case 0xE3: objSetProperty(operands[0].getU(),operands[1].getU(),operands[2]); break;
				//case 0xE4: read
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
				case 0xEC: pc = call(pc,dest,operands,opCount); break;
				case 0xF9: pc = call(pc,-1,operands,opCount); break;
				case 0xFA: pc = call(pc,-1,operands,opCount); break;
				default: fault("unimplemented 0OP/VAR/EXT opcode"); break;
			}
		}
	}
}


int main(int argc,char **argv) {
	FILE *f = fopen(argv[1],"rb");
	fseek(f,0,SEEK_END);
	long size = ftell(f);
	rewind(f);
	char *story = new char[size];
	fread(story,1,size,f);
	fclose(f);
	
	machine m;
	m.init(story);
}