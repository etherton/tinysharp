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
	St,St,St,St,St,0,Br,0, 0,St,St,0,St,0,0,0, 0,0,0,St,0,0,0,0, Br,0,0,Br,0,0,0,0
};

static const char *opcode_names[256+32] = {
	// 00-0x7F
	nullptr, "je", "jl", "jg", "dec_chk", "inc_chk", "jin", "test", "or", "and", "test_attr", "set_attr", "clear_attr", "store", "insert_obj", "loadw", "loadb", "get_prop", "get_prop_addr", "get_next_prop", "add", "sub", "mul", "div", "mod", "call_2s", "call_2n", "set_colour", "throw", nullptr, nullptr, nullptr,
	nullptr, "je", "jl", "jg", "dec_chk", "inc_chk", "jin", "test", "or", "and", "test_attr", "set_attr", "clear_attr", "store", "insert_obj", "loadw", "loadb", "get_prop", "get_prop_addr", "get_next_prop", "add", "sub", "mul", "div", "mod", "call_2s", "call_2n", "set_colour", "throw", nullptr, nullptr, nullptr,
	nullptr, "je", "jl", "jg", "dec_chk", "inc_chk", "jin", "test", "or", "and", "test_attr", "set_attr", "clear_attr", "store", "insert_obj", "loadw", "loadb", "get_prop", "get_prop_addr", "get_next_prop", "add", "sub", "mul", "div", "mod", "call_2s", "call_2n", "set_colour", "throw", nullptr, nullptr, nullptr,
	nullptr, "je", "jl", "jg", "dec_chk", "inc_chk", "jin", "test", "or", "and", "test_attr", "set_attr", "clear_attr", "store", "insert_obj", "loadw", "loadb", "get_prop", "get_prop_addr", "get_next_prop", "add", "sub", "mul", "div", "mod", "call_2s", "call_2n", "set_colour", "throw", nullptr, nullptr, nullptr,

	// 0x80-0xAF
	"jz", "get_sibling","get_child","get_parent","get_prop_len","inc","dec","print_addr","call_1s","remove_obj","print_obj","ret","jump","print_paddr","load","not/call1n",
	"jz", "get_sibling","get_child","get_parent","get_prop_len","inc","dec","print_addr","call_1s","remove_obj","print_obj","ret","jump","print_paddr","load","not/call1n",
	"jz", "get_sibling","get_child","get_parent","get_prop_len","inc","dec","print_addr","call_1s","remove_obj","print_obj","ret","jump","print_paddr","load","not/call1n",

	// 0xB0-0xBF
	"rtrue","rfalse","print","print_ret","nop","save","restore","restart","ret_popped","pop/catch","quit","new_line","show_status","verify",nullptr,"piracy",

	// 0xC0-0xDF
	nullptr, "je", "jl", "jg", "dec_chk", "inc_chk", "jin", "test", "or", "and", "test_attr", "set_attr", "clear_attr", "store", "insert_obj", "loadw", "loadb", "get_prop", "get_prop_addr", "get_next_prop", "add", "sub", "mul", "div", "mod", "call_2s", "call_2n", "set_colour", "throw", nullptr, nullptr, nullptr,

	//0xE0-0xFF
	"call_vs","storew","storeb","put_prop","sread","print_char","print_num","random","push","pull","split_window","set_window","call_vs2","erase_window","erase_line","set_cursor",
	"get_cursor","set_text_style","buffer_mode","output_stream","input_stream","sound_effect","read_char","scan_table","not","call_vn","call_vn2","tokenise","encode_text","copy_table","print_table","check_arg_count",

	// 0x100-0x1F
	"save","restore","log_shift","art_shift","set_font","draw_picture","picture_data","erase_picture","set_margins","save_undo","restore_undo","print_unicode","check_unicode",nullptr,nullptr,nullptr,
	"move_window","window_size","window_style","get_wind_prop","scroll_window","pop_stack","read_mouse","mouse_window","push_stack","put_wind_prop","print_form","make_menu","picture_table",nullptr,nullptr,nullptr
};

static const char* varTypes[] = {
	"(sp)", "local0", "local1", "local2", "local3", "local4", "local5", "local6", "local7",
	"local8", "local9", "local10", "local11", "local12", "local13", "local14", "local15",
	"global0","global1","global2","global3","global4","global5","global6","global7",
	"global8","global9","global10","global11","global12","global13","global14","global15",
	"global16","global17","global18","global19","global20","global21","global22","global23",
	"global24","global25","global26","global27","global28","global29","global30","global31",
	"global32","global33","global34","global35","global36","global37","global38","global39",
	"global40","global41","global42","global43","global44","global45","global46","global47",
	"global48","global49","global50","global51","global52","global53","global54","global55",
	"global56","global57","global58","global59","global60","global61","global62","global63",
	"global64","global65","global66","global67","global68","global69","global70","global71",
	"global72","global73","global74","global75","global76","global77","global78","global79",
	"global80","global81","global82","global83","global84","global85","global86","global87",
	"global88","global89","global90","global91","global92","global93","global94","global95",
	"global96","global97","global98","global99","global100","global101","global102","global103",
	"global104","global105","global106","global107","global108","global109","global110","global111",
	"global112","global113","global114","global115","global116","global117","global118","global119",
	"global120","global121","global122","global123","global124","global125","global126","global127",
	"global128","global129","global130","global131","global132","global133","global134","global135",
	"global136","global137","global138","global139","global140","global141","global142","global143",
	"global144","global145","global146","global147","global148","global149","global150","global151",
	"global152","global153","global154","global155","global156","global157","global158","global159",
	"global160","global161","global162","global163","global164","global165","global166","global167",
	"global168","global169","global170","global171","global172","global173","global174","global175",
	"global176","global177","global178","global179","global180","global181","global182","global183",
	"global184","global185","global186","global187","global188","global189","global190","global191",
	"global192","global193","global194","global195","global196","global197","global198","global199",
	"global200","global201","global202","global203","global204","global205","global206","global207",
	"global208","global209","global210","global211","global212","global213","global214","global215",
	"global216","global217","global218","global219","global220","global221","global222","global223",
	"global224","global225","global226","global227","global228","global229","global230","global231",
	"global232","global233","global234","global235","global236","global237","global238","global239" 
};


void dis(storyHeader *h,int pc) {
	// keep track of the furthest forward branch we've seen.
	// if we encounter an unconditional return and we're at or beyond, we're done.
	// 0b00 - large constant (2 bytes)
	// 0b01 - small constant (1 byte)
	// 0b10 - variable (0=tos, 1-15=local, 16-255=global 1-240)
	// 0b11 - omitted altogether
	int highest = pc;
	int end = h->storyLength.getU() * storyScales[h->version];
	uint8_t *b = (uint8_t*) h;

	while (pc < end) {
		printf("%06x: ",pc);
		uint16_t opcode = b[pc++];
		if (opcode == 0xBE && h->version>=5)
			opcode = 0x100 | b[pc++];
		printf("%s ",opcode_names[opcode]);

		uint16_t types = opTypes[opcode >> 4] << 8;
		if (!types)
			types = b[pc++] << 8;
		if (opcode==236 || opcode==250)
			types |= b[pc++];
		else
			types |= 255;
		while (types != 0xFFFF) {
			int16_t op = b[pc++];
			switch (types & 0xC000) {
				case 0x0000: printf("%d ",int16_t((op << 8) | b[pc++])); break;
				case 0x4000: printf("%d ",op); break;
				case 0x8000: printf("%s ",varTypes[op]); break;
			}
			types = (types << 2) | 0x3;
		}
		
		uint8_t decode_byte = decode[opcode] >> version_shift[h->version];
		if (decode_byte & 1)
			printf("-> %s ",varTypes[b[pc++]]);
		if (decode_byte & 2) {
			int16_t branch_offset = -1;
			bool branch_cond = false;
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
			if (branch_offset <= 1) {
				opcode = 176 - branch_offset;
				printf("?%s%s",branch_cond?"":"~",branch_offset?"rtrue":"rfalse");
			}
			else {
				if (pc + branch_offset - 2 > highest)
					highest = pc + branch_offset - 2;
				printf("?%s%x",branch_cond?"":"~",pc + branch_offset - 2);
			}
		}
		printf("\n");

		// If pc is beyond furthest branch and it's a return, it's end of function
		if (pc > highest && (opcode==0x8B||opcode==0x8C||opcode==0x9B||opcode==0xAB||opcode==0xB0||opcode==0xB1||opcode==0xB3||opcode==0xB8))
			break;
	}
}

void routine(storyHeader *h,int pc) {
	uint8_t *b = (uint8_t*) h;
	printf("routine at %x, %d locals\n",pc,b[pc]);
	if (h->version < 5 && b[pc]) {
		word *locals = (word*)(b + pc + 1);
		printf("initial values: ");
		for (int i=0; i<b[pc]; i++)
			printf("[%d] ",locals[i].getS());
		printf("\n");
		pc += 2 * b[pc];
	}
	dis(h,pc+1);
}

int main(int argc,char **argv) {
	storyHeader *story = getStory(argv[1]);
	printf("version %d\n",story->version);
	printf("high memory: %x\n",story->highMemoryAddr.getU());
	printf("initial pc: %x\n",story->initialPCAddr.getU());
	printf("dictionary: %x\n",story->dictionaryAddr.getU());
	printf("globals: %x\n",story->globalVarsTableAddr.getU());
	printf("static memory: %x\n",story->staticMemoryAddr.getU());
	printf("abbreviations: %x\n",story->abbreviationsAddr.getU());
	printf("story length: %x\n",story->storyLength.getU() * storyScales[story->version]);
	// skip the count of 0 locals
	dis(story,story->initialPCAddr.getU());
}
