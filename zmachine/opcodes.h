#include <stdio.h>
#include <stdint.h>

static const char storyScales[] = "\000\002\002\002\004\004\010\010\010";

static const uint8_t opTypes[16+2] = {
	0b0101'1111, // 0x00-0x3F
	0b0101'1111,
	0b0110'1111,
	0b0110'1111,

	0b1001'1111, // 0x40-0x7F
	0b1001'1111,
	0b1010'1111,
	0b1010'1111,

	0b0011'1111, // 0x80-0xBF
	0b0111'1111,
	0b1011'1111,
	0b1111'1111,

	// Remaining entries are all zero to indicate types follow
	0,
	0,
	0,
	0,

	0,
	0
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

#if ENABLE_DEBUG
static const char *opcode_names[256+32] = {
	// 00-0x7F
	"?00", "je", "jl", "jg", "dec_chk", "inc_chk", "jin $o $o", "test", "or", "and", "test_attr $o $a", "set_attr $o $a", "clear_attr $o $a", "store", "insert_obj $o $o", "loadw", "loadb", "get_prop $o $p", "get_prop_addr $o $p", "get_next_prop $o $p", "add", "sub", "mul", "div", "mod", "call_2s", "call_2n", "set_colour", "throw", "?1D", "?1E", "?1F",
	"?20", "je", "jl", "jg", "dec_chk", "inc_chk", "jin $o $o", "test", "or", "and", "test_attr $o $a", "set_attr $o $a", "clear_attr $o $a", "store", "insert_obj $o $o", "loadw", "loadb", "get_prop $o $p", "get_prop_addr $o $p", "get_next_prop $o $p", "add", "sub", "mul", "div", "mod", "call_2s", "call_2n", "set_colour", "throw", "?3D", "?3E", "31F",
	"?40", "je", "jl", "jg", "dec_chk", "inc_chk", "jin $o $o", "test", "or", "and", "test_attr $o $a", "set_attr $o $a", "clear_attr $o $a", "store", "insert_obj $o $o", "loadw", "loadb", "get_prop $o $p", "get_prop_addr $o $p", "get_next_prop $o $p", "add", "sub", "mul", "div", "mod", "call_2s", "call_2n", "set_colour", "throw", "?5D", "?5E", "?5F",
	"?60", "je", "jl", "jg", "dec_chk", "inc_chk", "jin $o $o", "test", "or", "and", "test_attr $o $a", "set_attr $o $a", "clear_attr $o $a", "store", "insert_obj $o $o", "loadw", "loadb", "get_prop $o $p", "get_prop_addr $o $p", "get_next_prop $o $p", "add", "sub", "mul", "div", "mod", "call_2s", "call_2n", "set_colour", "throw", "?7D", "?7E", "?7F",

	// 0x80-0xAF
	"jz", "get_sibling $o","get_child $o","get_parent $o","get_prop_len","inc","dec","print_addr","call_1s","remove_obj $o","print_obj $o","ret","jump","print_paddr","load","not/call1n",
	"jz", "get_sibling $o","get_child $o","get_parent $o","get_prop_len","inc","dec","print_addr","call_1s","remove_obj $o","print_obj $o","ret","jump","print_paddr","load","not/call1n",
	"jz", "get_sibling $o","get_child $o","get_parent $o","get_prop_len","inc","dec","print_addr","call_1s","remove_obj $o","print_obj $o","ret","jump","print_paddr","load","not/call1n",

	// 0xB0-0xBF
	"rtrue","rfalse","print","print_ret","nop","save","restore","restart","ret_popped","pop/catch","quit","new_line","show_status","verify","EXT_BE","piracy",

	// 0xC0-0xDF
	"?C0", "je", "jl", "jg", "dec_chk", "inc_chk", "jin $o $o", "test", "or", "and", "test_attr $o $a", "set_attr $o $a", "clear_attr $o $a", "store", "insert_obj $o $o", "loadw", "loadb", "get_prop $o $p", "get_prop_addr $o $p", "get_next_prop$o $p", "add", "sub", "mul", "div", "mod", "call_2s", "call_2n", "set_colour", "throw", "?DD", "?DE", "?DF",

	//0xE0-0xFF
	"call_vs","storew","storeb","put_prop $o $p","sread","print_char","print_num","random","push","pull","split_window","set_window","call_vs2","erase_window","erase_line","set_cursor",
	"get_cursor","set_text_style","buffer_mode","output_stream","input_stream","sound_effect","read_char","scan_table","not","call_vn","call_vn2","tokenise","encode_text","copy_table","print_table","check_arg_count",

	// 0x100-0x1F
	"save","restore","log_shift","art_shift","set_font","draw_picture","picture_data","erase_picture","set_margins","save_undo","restore_undo","print_unicode","check_unicode","ext0D","ext0E","ext0F",
	"move_window","window_size","window_style","get_wind_prop","scroll_window","pop_stack","read_mouse","mouse_window","push_stack","put_wind_prop","print_form","make_menu","picture_table","ext1D","ext1E","ext1F"
};
#endif
