#include "machine.h"

void zmachine::init(const void *data) {
	m_sp = m_lp = 0;
	m_readOnly = (const uint8_t*) data;
	m_dynamicSize = reinterpret_cast<storyHeader*>(data)->staticMemoryAddr.get();
	m_dynamic = new char[m_dynamicSize];
	memcpy(m_dynamic, m_readOnly, m_dynamicSize);
	m_globalsOffset = m_header->globalVarsTableAddr.get();
	run(m_storyHeader->initialPCAddr.get());
}

void zmachine::run() {
	for (;;) {
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
		uint8_t dest = 0;
		int16_t branch_offset;
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
				case 0x01: branch(operands[0].get() == operands[1].get()); break;
				case 0x02: branch(operands[0].get() < operands[1].get()); break; 
				case 0x03: branch(operands[0].get() > operands[1].get()); break;
				case 0x04: branch(--ref(operands[0].get()) < operands[1].get()); break;
				case 0x05: branch(++ref(operands[0].get()) > operands[1].get()); break;
				case 0x06: branch(getParent(operands[0].get()) == operands[1].get()); break;
				case 0x07: branch((operands[0].get() & operands[1].get()) == operands[1].get()); break;
				case 0x08: ref(dest,true).set(operands[0].get() | operands[1].get()); break;
				case 0x09: ref(dest,true).set(operands[0].get() & operands[1].get()); break;
				case 0x14: ref(dest,true).set(operands[0].get() + operands[1].get()); break;
				case 0x15: ref(dest,true).set(operands[0].get() - operands[1].get()); break;
				case 0x16: ref(dest,true).set(operands[0].get() * operands[1].get()); break;
				case 0x17: if (!operands[1].get()) fault("division by zero"); 
					ref(dest,true).set(operands[0].get() / operands[1].get()); break;
				case 0x18: if (!operands[1].get()) fault("modulo by zero"); 
					ref(dest,true).set(operands[0].get() % operands[1].get()); break;
			}
		}
		else if (opcode >= 0x80 && opcode < 0xB0) {
			if (opCount != 1)
				fault("1OP with something other than one operand");
			switch (opcode & 15) {
			}
		}
		else {
			switch (opcode) {
				case 0xB0: r_return(1); break;
				case 0xB1: r_return(0); break;
				case 0xB2: pc = print_inline(pc); break;
				case 0xB3: print_inline(pc+1); r_return(1); break;
				case 0xB4: ++pc; break; // nop
				case 0xB5: 
				case 0xB6: 
				case 0xB7: 
				case 0xB8: 
				case 0xB9: 
				case 0xBA: exit(0); break;
				case 0xBB: print_char(10); ++pc; break;
				case 0xBC: 
				case 0xBD: 
				case 0xBF: branch(1,pc+1); break; // piracy
				default: fault("illegal %x opcode at %x",opcode,pc); break;
			}
		}
	}
}
