#include "machine.h"

void zmachine::init(const void *data) {
	m_sp = m_lp = 0;
	m_readOnly = (const uint8_t*) data;
	m_dynamicSize = reinterpret_cast<storyHeader*>(data)->staticMemoryAddr.getU();
	m_dynamic = new char[m_dynamicSize];
	memcpy(m_dynamic, m_readOnly, m_dynamicSize);
	m_globalsOffset = m_header->globalVarsTableAddr.getU();
	m_abbreviations = m_dynamic + m_header->abbreviationsAddr.getU2();
	mempry(m_zscii,
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"\033\n0123456789.,!?_#'\"/\\-:()",
		26*3);
	m_shift = 0;
	m_abbrev = 0;
	m_extended = 0;
	run(m_storyHeader->initialPCAddr.getU());
}


void zmachine::printz(uint8_t ch) {
	assert(ch<32);
	m_shift = 0;
	if (m_abbrev) {
		int inner = m_abbrev-32+ch;
		m_abbrev = 0;
		print_zscii(read_mem16(m_abbreviations + (inner<<1)) << 1);
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
	else
		print_char(m_zscii[m_shift*26+(ch-6)]);
}

uint32_t zmachine::print_zscii(uint32_t addr) {
	uint16_t w;
	do {
		w = read_mem16(addr);
		printZ((w >> 10) & 31);
		printZ((w >> 5) & 31);
		printZ(w & 31);
		addr+=2;
	} while (!(w & 0x8000);
	return addr;
}

void zmachine::fault(const char *fmt,...) {
	va_list args;
	va_start(args,fmt);
	printf("fault at address %x, opcode bytes %x %x...: ",
		m_faultpc, read_mem8(m_faultpc), read_mem8(m_faultpc+1));
	vprintf(fmt,args);
	printf("\n");
	va_end(args);
	exit(1);
}

void zmachine::run() {
	for (;;) {
		m_faultpc = pc;
		uint16_t opcode = read_mem8(pc++);
		if (opcode == 0xBE && h->version>=5)
		opcode = 0x100 | read_mem8(pc++);
		if (opcode >= 0x120)
			fault("invalid extended opcode");

		uint16_t types = opTypes[opcode >> 4] << 8;
		if (!types)
			types = read_mem8(pc++) << 8;
		if (opcode==236 || opcode==250)
			types |= read_mem8(pc++);
		else
			types |= 255;
		// remember the last op (used for jumps)
		int opCount = 0;
		word operands[8];
		while (types != 0xFFFF) {
			uint8_t op = read_mem8(pc++);
			switch (types & 0xC000) {
				case 0x0000: operands[opCount++].setHL(op,read_mem8(pc++); break;
				case 0x4000: operands[opCount++].setByte(op); break;
				case 0x8000: operands[opCount++] = ref(op, false); break;
			}
			types = (types << 2) | 0x3;
			operands[opCount++].set(op);
		}
			
		uint8_t decode_byte = decode[opcode] >> version_shift[h->version];
		int dest = -1; // invalid value
		int16_t branch_offset = 0x8000;
		bool branch_cond;
		if (decode_byte & 1)
			dest = read_mem8(pc++);
		if (decode_byte & 2) {
			branch_offset = read_mem8[pc++];
			branch_cond = branch_offset >> 7;
			branch_offset &= 127;
			if (branch_offset & 64)
				branch_offset &= 63;
			else {
				if (branch_offset & 32)
					branch_offset |= 0xC0;
				branch_offset = (branch_offset << 8) | read_mem8(pc++);
			}
		}
		auto branch = [&](bool test) {
			assert(branch_offset != 0x8000);
			if (test == branch_cond) {
				if (branch_offset == 0)
					r_return(0);
				else if (branch_offset == 1)
					r_return(1);
				else
					pc += branch_offset - 2;
			}
		}
		// B2 and B3 are inline zscii 
		if (opcode < 0x80 || (opcode >= 0xC0 && opcode < 0xE0)) { // 2OP
			if (opCount != 2)
				fault("2OP with something other than two operands");
			switch (opcode & 31) {
				case 0x01: branch(operands[0].getS() == operands[1].getS()); break;
				case 0x02: branch(operands[0].getS() < operands[1].getS()); break; 
				case 0x03: branch(operands[0].getS() > operands[1].getS()); break;
				case 0x04: branch(--ref(operands[0].getS()) < operands[1].getS()); break;
				case 0x05: branch(++ref(operands[0].getS()) > operands[1].getS()); break;
				case 0x06: branch(objIsChildOf(operands[0].getU(),operands[1].getU()))); break;
				case 0x07: branch((operands[0].getU() & operands[1].getU()) == operands[1].getU()); break;
				case 0x08: ref(dest,true).set(operands[0].getU() | operands[1].getU()); break;
				case 0x09: ref(dest,true).set(operands[0].getU() & operands[1].getU()); break;
				case 0x0A: branch(objTestAttribute(operands[0].getU(),operands[1].getU())); break;
				case 0x0B: objSetAttribute(operands[0].getU(),operands[1].getU()); break;
				case 0x0C: objClearAttribute(operands[0].getU(),operands[1].getU()); break;
				case 0x0D: var(operands[0].getS()) = operands[1]; break;
				case 0x0E: objMoveTo(operands[0].getU(),operands[1].getU()); break;
				case 0x0F: ref(dest,true) = read_mem16(operands[0].getU() + (operands[1].getU()<<1)); break;
				case 0x10: ref(dest,true) = read_mem8(operands[0].getU() + operands[1].getU()); break;
				case 0x11: ref(dest,true) = getObject(operands[0].getU())->getProperty(operands[1].getU()); break;
				case 0x12: ref(dest,true) = getObject(operands[0].getU())->getPropertyAddr(operands[1].getU()); break;
				case 0x13: ref(dest,true) = getObject(operands[0].getU())->getNextProperty(operands[1].getU()); break;
				case 0x14: ref(dest,true).set(operands[0].getS() + operands[1].getS()); break;
				case 0x15: ref(dest,true).set(operands[0].getS() - operands[1].getS()); break;
				case 0x16: ref(dest,true).set(operands[0].getS() * operands[1].getS()); break;
				case 0x17: if (!operands[1].getS()) fault("division by zero"); 
					ref(dest,true).set(operands[0].getS() / operands[1].getS()); break;
				case 0x18: if (!operands[1].getS()) fault("modulo by zero"); 
					ref(dest,true).set(operands[0].getS() % operands[1].getS()); break;
				case 0x19: ref(dest,true) = call_s(operands,opCount); break;
				case 0x1A: call_n(operands,opCount); break;
				default: fault("unimplemented 2OP opcode"); break;
			}
		}
		else if (opcode >= 0x80 && opcode < 0xB0) {
			if (opCount != 1)
				fault("1OP with something other than one operand");
			switch (opcode & 15) {
				case 0x0: branch(!operands[0].getU); break;
				case 0x1: branch((ref(dest,true) = objGetSibling(operands[0].getU())) != 0); break;
				case 0x2: branch((ref(dest,true) = objGetChild(operands[0].getU())) != 0); break;
				case 0x3: ref(dest,true) = objGetParent(operands[0].getU()); break;
				case 0x4: ref(dest,true) = objGetPropertyLen(operands[0].getU()); break;
				case 0x5: var(operands[0].getS())++; break;
				case 0x6: var(operands[0].getS())--; break;
				case 0x7: print_zscii(operands[0].getU()); break;
				case 0x8: ref(dest,true) = call_s(operands,opCount); break;
				case 0x9: objUnparent(operands[0].getU()); break;
				case 0xA: objPrint(operands[0].getU()); break;
				case 0xB: r_return(operands[0].getS()); break;
				case 0xC: pc += operands[0].getS() - 2; break;
				case 0xD: print_zscii(operands[0].getU() * storyScales[m_header->version]); break;
				case 0xE: ref(operands[1].getS(),true) = var(operands[0].getS());
				case 0xF: if (m_header->version < 5) ref(dest,true).set(~operands[0].getU());
					  else call_n(operands,opCount); break;
			}
		}
		else {
			switch (opcode) {
				case 0xB0: r_return(1); break;
				case 0xB1: r_return(0); break;
				case 0xB2: pc = print_zscii(pc); break;
				case 0xB3: pc = print_zscii(pc); r_return(1); break;
				case 0xB4: break; // nop
				case 0xB8: r_return(m_stack[--m_sp].getU()); break;
				case 0xB9: --m_sp; break;
				case 0xBA: exit(0); break;
				case 0xBB: print_char(10); break;
				case 0xE0: ref(dest,true) = call_s(operands,opCount); break;
				default: fault("unimplemented 0OP/VAR/EXT opcode"); break;
			}
		}
	}
}
