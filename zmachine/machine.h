#include "header.h"

/*
	Example of a function that takes three parameters and has five locals total
	Higher addresses
	local5
	local4
	local3/param3
	local2/param2
	local1/param1 (LP+3)
	SP Adjust (lower 4 bits are number of locals, upper 12 bits are storage address)
	Previous LP (lower 13 bits), and upper three bits of PC (upper 3 bits)
	return_address <- LP,SP

	First, we unpack the function address and get the number of locals. If the address
	is zero, the number of locals is always zero. The lesser of the number of locals
	and the number of extra parameters is then pushed onto the stack in reverse order
	from the previously evaluated array. Any extra locals are initialized from default
	values (or zero if v5).

	Then, we push the storage address from the instruction so that the return value can
	be written during teardown.

	Then we push the previous LP value, and finally the return value, which will
	be the address of the next instruction.

	When any sort of return is encountered (including a branch to offset 0/1), the result
	is recorded in an internal register. We then fetch the return address and restore LP.
	The return value is then written to the location specified by Storage Address in
	the context of the calling function.
*/

class machine {
public:
	void init(const void*);
	void run(uint32_t pc);
private:
	// first attribute (zero) is MSB of lowest byte.
	struct object_small {	
		uint8_t attr[4], parent, sibling, child;
		word propAddr;
		bool testAttribute(uint16_t a) const {
			return (attr[a >> 3] & (0x80 >> (a & 7))) != 0;
		}
		void setAttribute(uint16_t a) {
			attr[a >> 3] |= (0x80 >> (a & 7));
		}
		void clearAttribute(uint16_t a) {
			attr[a >> 3] &= ~(0x80 >> (a & 7));
		}
	};
	struct object_large {
		uint8_t attr[6];
		word parent, sibling, child, propAddr;
		bool testAttribute(uint16_t a) const {
			return (attr[a >> 3] & (0x80 >> (a & 7))) != 0;
		}
		void setAttribute(uint16_t a) {
			attr[a >> 3] |= (0x80 >> (a & 7));
		}
		void clearAttribute(uint16_t a) {
			attr[a >> 3] &= ~(0x80 >> (a & 7));
		}
	};
	struct object_header_small {
		word defaultProps[31];
		object_small objTable[0];
	};
	struct object_header_large {
		word defaultProps[63];
		object_large objTable[0];
	};
	uint32_t print_zscii(uint32_t addr);
	void printz(uint8_t ch);
	void print_char(uint8_t ch);
	uint8_t m_abbrev, m_shift;
	uint16_t m_extended;
	bool objIsChildOf(uint16_t o1,uint16_t o2) const {
		if (!o1 || o1 > m_objCount)
			fault("jin first object %d of range",o1);
		if (!o2 || o2 > m_objCount)
			fault("jin second object %d of range",o2);
		int deadman = 100;
		while (o1) {
			if (!--deadman) fault("infinite loop in object table");
			if (o1 == o2)
				return true;
			o1 = m_header->version<4
				? m_objectSmall->objTable[o1-1].parent 
				: m_objectLarge->objTable[o1-1].parent.getU();
		}
		return false;
	}
	bool objTestAttribute(uint16_t o,uint16_t attr) const {
		if (!o || o > m_objCount)
			fault("test_attr object %d of range",o);
		if (attr >= (m_header->version<4? 32 : 48))
			fault("test_attr attribute %d out of range",attr);
		return m_header->version < 4
			? m_objectSmall->objTable[o-1].testAttribute(attr)
			: m_objectLarge->objTable[o-1].testAttribute(attr);
	}
	void objSetAttribute(uint16_t o,uint16_t attr) {
		if (!o || o > m_objCount)
			fault("set_attr object %d of range",o);
		if (attr >= (m_header->version<4? 32 : 48))
			fault("set_attr attribute %d out of range",attr);
		return m_header->version < 4
			? m_objectSmall->objTable[o-1].setAttribute(attr)
			: m_objectLarge->objTable[o-1].setAttribute(attr);
	}
	void objClearAttribute(uint16_t o,uint16_t attr) {
		if (!o || o > m_objCount)
			fault("clear_attr object %d of range",o);
		if (attr >= (m_header->version<4? 32 : 48))
			fault("clear_attr attribute %d out of range",attr);
		return m_header->version < 4
			? m_objectSmall->objTable[o-1].clearAttribute(attr)
			: m_objectLarge->objTable[o-1].clearAttribute(attr);
	}
	void objMoveTo(uint16_t o1,uint16_t o2);
	word getObjProperty(uint16_t o,uint16_t prop) const;
	word getObjPropertyAddr(uint16_t o,uint16_t prop) const;
	word getObjNextProperty(uint16_t o,uint16_t prop) const;
	word objGetSibling(uint16_t o) const {
		if (!o || o>m_objCount)
			fault("get_sibling object %d out of range",o);
		return m_header->version < 4
			? byte2word(m_objectSmall->objTable[o-1].sibling)
			: m_objectLarge->objTable[o-1].sibling;				
	}
	word objGetChild(uint16_t o) const {
		if (!o || o>m_objCount)
			fault("get_child object %d out of range",o);
		return m_header->version < 4
			? byte2word(m_objectSmall->objTable[o-1].child)
			: m_objectLarge->objTable[o-1].child;		
	}
	word objGetParent(uint16_t o) const {
		if (!o || o>m_objCount)
			fault("get_parent object %d out of range",o);
		return m_header->version < 4
			? byte2word(m_objectSmall->objTable[o-1].parent)
			: m_objectLarge->objTable[o-1].parent;
	}
	word objGetPropertyLen(uint16_t propAddr) const;
	void objUnparent(uint16_t o);
	void objPrint(uint16_t o) const;

	uint8_t read_mem8(uint32_t addr) const {
		if (addr >= m_readOnlySize)
			fault("out of range address %x (highest is %x)",addr,m_readOnlySize);
		return addr < m_dynamicSize? m_dynamic[addr] : m_readOnly[addr];
	}
	word read_mem16(uint32_t addr) const {
		return addr+1 < m_dynamicSize? *(word*)(m_dynamic+addr) : *(word*)(m_readOnly+addr);
	}
	
	word &ref(int v,bool write) {
		if (v<0||v>255)
			fault("invalid reference %d",v);
		if (!v) {
			if (write)
				return m_stack[--m_sp];
			else
				return m_stack[m_sp++];
		}
		else if (v < 16)
			return m_stack[m_lp + v + 2];
		else
			return *(word*)(m_dynamic + m_globalsOffset + (v-16)*2);
	}
	word &var(int v) {
		if (v<=0||v>255)
			fault("invalid variable %d",v);
		if (v < 16)
			return m_stack[m_lp + v + 2];
		else
			return *(word*)(m_dynamic + m_globalsOffset + (v-16)*2);
	}
	void push(word w) {
		if (!m_sp)
			fault("stack overflow in push");
		m_stack[--m_sp] = w;
	}
	word pop() {
		if (m_sp == kStackSize)
			fault("stack underflow in pop");
		return m_stack[m_sp++];
	}
	// return value of both is new pc value.
	uint32_t call(uint32_t pc,int dest,word operands[],uint8_t opCount);
	uint32_t r_return(uint16_t v);
	void fault(const char*,...) const;
	union {
		const uint8_t *m_readOnly; 	// can be in flash etc or memory mapped file
		const storyHeader *m_header;	
	};
	union {
		object_header_small *m_objectSmall;
		object_header_large *m_objectLarge;
	};
	uint8_t *m_dynamic;		// everything up to 'static' cutoff
	static const uint16_t kStackSize = 2048;
	word m_stack[kStackSize];
	char m_zscii[26*3];
	uint16_t m_sp, m_lp, m_dynamicSize, m_globalsOffset, m_abbreviations, m_objCount;
	uint32_t m_readOnlySize;
	uint32_t m_faultpc;
	uint8_t m_storyShift;
	bool m_debug;
};
