#include "header.h"
#include <stdio.h>

storyHeader *getStory(const char *storyName) {
	FILE *f = fopen(storyName,"rb");
	fseek(f,0,SEEK_END);
	long size = ftell(f);
	rewind(f);
	storyHeader *story = (storyHeader*) new char[size];
	fread(story,1,size,f);
	fclose(f);
	return story;
}

static const char storyScales[] = "\000\002\002\002\004\004\010\010\010";

static const uint8_t opTypes[16] = {
	0b0101'1111,
	0b0101'1111,
	0b0110'1111,
	0b0110'1111,

	0b1001'1111,
	0b1001'1111,
	0b1010'1111,
	0b1010'1111,

	0b0011'1111,
	0b0111'1111,
	0b1011'1111,
	0b1111'1111,
};

// Bits 0-1 are for V1-3, 2-3 are for V4, 4-5 for V5/7/8, 6-7 for V6
const uint8_t St=0b01010101, Br=0b10101010, StBr=0b11111111;
static const uint8_t version_shift[9] = { 0,0,0,0, 2,4,6,4,4 };

static const uint8_t decode[256+32] = {
	// 2OP x 4 (0-0x7F)
	0,Br,Br,Br,Br,Br,Br,Br, St,St,Br,0,0,0,0,St, St,St,St,St,St,St,St,St, St,St,0,0,0,0,0,0,
	0,Br,Br,Br,Br,Br,Br,Br, St,St,Br,0,0,0,0,St, St,St,St,St,St,St,St,St, St,St,0,0,0,0,0,0,
	0,Br,Br,Br,Br,Br,Br,Br, St,St,Br,0,0,0,0,St, St,St,St,St,St,St,St,St, St,St,0,0,0,0,0,0,
	0,Br,Br,Br,Br,Br,Br,Br, St,St,Br,0,0,0,0,St, St,St,St,St,St,St,St,St, St,St,0,0,0,0,0,0,

	// 1OP x 3 (0xF is special; it's St on V1-4, 0 on V5+) (080-0xAF)
	Br,StBr,StBr,St,St,0,0,0, St,0,0,0,0,0,St,0b00000101,
	Br,StBr,StBr,St,St,0,0,0, St,0,0,0,0,0,St,0b00000101,
	Br,StBr,StBr,St,St,0,0,0, St,0,0,0,0,0,St,0b00000101,

	// 0OP (0xB0-0xBF), 0xBE is translated to 0x100 | XOP
	0,0,0,0,0,0b00000110,0b00000110,0, 0,0b01010000,0,0,0,Br,0,Br,
	
	// 2OP, VAR form 0xC0
	0,Br,Br,Br,Br,Br,Br,Br, St,St,Br,0,0,0,0,St, St,St,St,St,St,St,St,St, St,St,0,0,0,0,0,0,

	// VAR misc 0xE0
	St,0,0,0,0b01010000,0,0,St, 0,0b01000000,0,0,St,0,0,0, 0,0,0,0,0,0,St,StBr, St,0,0,0,0,0,0,Br,

	// EXT
	St,St,St,St,St,0,Br,0, 0,St,St,0,St,0,0,0, 0,0,0,St,0,0,0,0, Br,0,0,Br,0,0,0,0,0
}

void dis(storyHeader *h,int pc) {
	uint8_t *b = (uint8_t*) h;
	// keep track of the furthest forward branch we've seen.
	// if we encounter an unconditional return and we're at or beyond, we're done.
	// 0b00 - large constant (2 bytes)
	// 0b01 - small constant (1 byte)
	// 0b10 - variable (0=tos, 1-15=local, 16-255=global 1-240)
	// 0b11 - omitted altogether
	int highest = pc;
	for (;;) {
		printf("%06x: ",pc);
		uint16_t opcode = b[pc++];
		if (opcode == 0xBE && story->version>=5)
			opcode = 0x100 | b[pc++];
		uint16_t types = opTypes[opcode >> 4] << 8;
		if (!types)
			types = b[pc++] << 8;
		if (opcode==236 || opcode==250)
			types |= b[pc++];
		else
			types |= 255;
		int16_t operands[8];
		
		uint8_t store_variable = 0xFF;
		uint8_t decode_byte = decode[opcode] >> version_shift[story->version];
		if ((decode_byte & 1)
			store_variable = b[pc++];
		int16_t branch_offset = -1;
		bool branch_cond = false;
		if (decode_byte & 2) {
			branch_offset = b[pc++];
			branch_cond = branch_offset >> 7;
			if (branch_offset & 64)
				branch_offset &= 63;
			else {
				if (branch_offset & 32)
					branch_offset |= 0xC0;
				branch_offset = (branch_offset << 8) | b[pc++];
			}
			// track the furthest forward branch we've seen to detect end of routine.
			if (branch_offset > 1 && pc + branch_offset - 2 > highest)
				highest = pc + branch_offset - 2;
		}

		bool ret = false;	
		const char *nm = "?illegal";
		if (opcode < 0x80 || (opcode >= 0xC0 && opcode < 0xE0)) {
			switch (opcode & 31) {	 // 2OP
				case 0x01: nm="je"; break;
				case 0x02: nm="jl"; break;
				case 0x03: nm="jg"; break;
				case 0x04: nm="dec_chk"; break;
				case 0x05: nm="inc_chk"; break;
				case 0x06: nm="jin"; break;
				case 0x07: nm="test"; break;
				case 0x08: nm="or"; break;
				case 0x09: nm="and"; break;
				case 0x0A: nm="test_attr"; break;
				case 0x0B: nm="set_attr"; break;
				case 0x0C: nm="clear_attr"; break;
				case 0x0D: nm="store"; break;
				case 0x0E: nm="insert_obj"; break;
				case 0x0F: nm="loadw"; break;
				case 0x10: nm="loadb"; break;
				case 0x11: nm="get_prop"; break;
				case 0x12: nm="get_prop_addr"; break;
				case 0x13: nm="get_next_propr"; break;
				case 0x14: nm="add"; break;
				case 0x15: nm="sub"; break;
				case 0x16: nm="mul"; break;
				case 0x17: nm="div"; break;
				case 0x18: nm="mod"; break;
				case 0x19: nm="call_2s"; break;
				case 0x1A: nm="call_2n"; break;
				case 0x1B: nm="set_colour"; break;
				case 0x1C: nm="throw"; break;
			}
		}	
		else if (opcode < 0xB0) {
			switch (opcode & 15) {	// 1OP
				case 0x0: nm="jz"; break;
				case 0x1: nm="get_sibling"; break;
				case 0x2: nm="get_child"; break;
				case 0x3: nm="get_parent"; break;
				case 0x4: nm="get_prop_len"; break;
				case 0x5: nm="inc"; break;
				case 0x6: nm="dec"; break;
				case 0x7: nm="print_addr"; break;
				case 0x8: nm="call_1s"; break;
				case 0x9: nm="remove_obj"; break;
				case 0xA: nm="print_obj"; break;
				case 0xB: ret = true; nm="ret"; break;
				case 0xC: nm="jump"; break;
				case 0xD: nm="print_paddr"; break;
				case 0xE: nm="load"; break;
				case 0xF: nm="call_1n"; break;
			}
		}
		else if (opcode < 0xC0) {
			switch (opcode & 15) {	// 0OP
				case 0x00: ret = true; nm="rtrue"; break;
				case 0x01: ret = true; nm="rfalse"; break;
				case 0x02: nm="print"; break;
				case 0x03: ret = true; nm="print_ret"; break;
				case 0x04: nm="nop"; break;
				case 0x05: nm="save"; break;
				case 0x06: nm="restore"; break;
				case 0x07: nm="restart"; break;
				case 0x08: ret = true; nm="ret_popped"; break;
				case 0x09: nm=story->version>=5?"catch:"pop"; break;
				case 0x0A: nm="quit"; break;
				case 0x0B: nm="new_line"; break;
				case 0x0C: nm="show_status"; break;
				case 0x0D: nm="verify"; break;
				case 0x0E: break;
				case 0x0F: nm="piracy"; break;
			}
		}
		else switch (opcode) { // VAR/EXT
			case 224: nm=story->version<4?"call":"call_vs"; break;
			case 225: nm="storew"; break;
			case 226: nm="storeb"; break;
			case 227: nm="put_prop"; break;
			case 228: nm="sread"; break;
			case 229: nm="print_char"; break;
			case 230: nm="print_num"; break;
			case 231: nm="random"; break;
			case 232: nm="push"; break;
			case 233: nm="pull"; break;
			case 234: nm="split_window"; break;
			case 235: nm="set_window"; break;
			case 236: nm="call_vs2"; break;
			case 237: nm="erase_window"; break;
			case 238: nm="erase_line"; break;
			case 239: nm="set_cursor"; break;
			case 240: nm="get_cursor"; break;
			case 241: nm="set_text_style"; break;
			case 242: nm="buffer_mode"; break;
			case 243: nm="output_stream"; break;
			case 244: nm="input_stream"; break;
			case 245: nm="sound_effect"; break;
			case 246: nm="read_char"; break;
			case 247: nm="scan_table"; break;
			case 248: nm="not"; break;
			case 249: nm="call_vn"; break;
			case 250: nm="call_vn2"; break;
			case 251: nm="tokenise"; break;
			case 252: nm="encode_text"; break;
			case 253: nm="copy_table"; break;
			case 254: nm="print_table"; break;
			case 255: nm="check_arg_count"; break;

			case 256: nm="save"; break;
			case 257: nm="restore"; break;
			case 258: nm="log_shift"; break;
			case 259: nm="art_shift"; break;
			case 260: nm="set_font"; break;
			case 261: nm="draw_picture"; break;
			case 262: nm="picture_data"; break;
			case 263: nm="erase_picture"; break;
			case 264: nm="set_margins"; break;
			case 265: nm="save_undo"; break;
			case 266: nm="restore_undo"; break;
			case 267: nm="print_unicode"; break;
			case 268: nm="check_unicode"; break;
			case 272: nm="move_window"; break;
			case 273: nm="window_size"; break;
			case 274: nm="window_style"; break;
			case 275: nm="get_wind_prop"; break;
			case 276: nm="scroll_window"; break;
			case 277: nm="pop_stack"; break;
			case 278: nm="read_mousee"; break;
			case 279: nm="mouse_window"; break;
			case 280: nm="push_stack"; break;
			case 281: nm="put_wind_prop"; break;
			case 282: nm="print_form"; break;
			case 283: nm="make_menu"; break;
			case 284: nm="picture_table"; break;
		}
	}
}

void routine(storyHeader *h,int pa) {
	int pc = pa * storyScales[h->version];
	uint8_t *b = (uint8_t*) h + pc;
	printf("routine at %d, %d locals\n",pc,b[0]);
	if (h->version < 5) {
		word *locals = (word*)(b + 1);
		printf("initial values: ");
		for (int i=0; i<b[0]; i++)
			printf("[%d] ",locals[i]->getS());
		printf("\n");
		pc += 2 * b[0];
	}
	++pc;
	dis(h,pc);
}
int main(int argc,char **argv) {
	storyHeader *story = getStory(argv[1]);
	printf("version %d\n",story->version);
	printf("high memory: %d\n",story->highMemoryAddr.getU());
	printf("initial pc: %d\n",story->initialPCAddr.getU());
	printf("dictionary: %d\n",story->dictionaryAddr.getU());
	printf("globals: %d\n",story->globalVarsTableAddr.getU());
	printf("static memory: %d\n",story->staticMemoryAddr.getU());
	printf("abbreviations: %d\n",story->abbreviationsAddr.getU());
	printf("story length: %d\n",story->storyLength.getU() * storyScales[story->version]);
}
