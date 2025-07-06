#include "header.h"

/*
	Example of a function that takes three parameters and has five locals total
	Higher addresses
	local5
	local4
	local3/param3
	local2/param2
	local1/param1 (LP+4)
	Storage Address (0-255) (call_[12v]s or -1 if it's call_[12v]n
	SP Adjust (count of locals and the storage address)
	Previous LP, and upper eight bits of PC
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
	uint32_t print_zscii(uint32_t addr);
	void printz(uint8_t ch);
	void output_char(uint8_t ch);
	uint8_t m_abbrev, m_shift;
	uint16_t m_extended;

	uint8_t read_mem8(uint32_t addr) {
		return addr < m_dynamicSize? m_dynamic[addr] : m_readOnly[addr]
	}
	word read_mem16(uint32_t addr) {
		return addr+1 < m_dynamicSize? *(word*)(m_dynamic+addr) : *(word*)(m_readOnly+addr);
	}
	
	word &ref(int v,bool write) {
		if (v<0||v>255)
			fault("invalid reference %d",v);
		else if (!v) {
			if (write)
				return m_stack[--m_sp];
			else
				return m_stack[m_sp++];
		}
		else if (v <= 16)
			return m_stack[m_lp + v + 3];
		else
			return *(word*)(m_dynamic + m_globalsOffset + (v-16)*2);
	}
	word &var(int v) {
		if (v<=0||v>255)
			fault("invalid variable %d",v);
		else if (v <= 16)
			return m_stack[m_lp + v + 3];
		else
			return *(word*)(m_dynamic + m_globalsOffset + (v-16)*2);
	}
	void push(word w) {
		m_stack[--m_sp] = w;
	}
	word pop() {
		return m_stack[m_sp++];
	}
	void fault(const char*,...);
	union {
		storyHeader *m_header;	
		uint8_t *m_dynamic;		// everything up to 'static' cutoff
	};
	const uint8_t *m_readOnly; 	// can be in flash etc or memory mapped file
	word m_stack[2048];
	char m_zscii[26*3];
	uint16_t m_sp, m_lp, m_dynamicSize, m_globalsOffset;
};
