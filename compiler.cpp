#include <stdint.h>
#include <map>
#include <string>

#include "opcodes.h"

/* Syntax is C-like except designed to be easy to parse */
enum { 
	K_STATIC, K_CLASS, K_STRUCT, K_SWITCH, K_CASE, K_IF, K_FOR, K_ELSE, 
	K_WHILE, K_DO, K_PUBLIC, K_PRIVATE, K_PROTECTED, K_VIRTUAL, K_NAMESPACE,
	K_IN, K_OUT, K_REF, K_CONTINUE, K_BREAK
};
std::map<std::string,int> keywords;

enum basetype_t : uint8_t { BT_VOID, BT_INT, BT_FLOAT, BT_BOOL };

struct type {
	type(basetype_t basetype,uint8_t width,bool is_signed) : 
		_basetype(basetype), _width(width), _is_signed(is_signed) { }
	basetype_t _basetype;
	uint8_t _width;
	bool _is_signed;
};

std::map<std::string,type> types;

class compiler {
public: void setup();
};

void compiler::setup() {

	keywords["switch"] = K_SWITCH;
	keywords["case"] = K_CASE;
	keywords["if"] = K_IF;
	keywords["for"] = K_FOR;
	keywords["else"] = K_ELSE;
	keywords["while"] = K_WHILE;
	keywords["do"] = K_DO;
	keywords["public"] = K_PUBLIC;
	keywords["private"] = K_PRIVATE;
	keywords["protected"] = K_PROTECTED;
	keywords["static"] = K_STATIC;
	keywords["class"] = K_CLASS;
	keywords["struct"] = K_STRUCT;
	keywords["virtual"] = K_VIRTUAL;
	keywords["namespace"] = K_NAMESPACE;
	keywords["in"] = K_IN;
	keywords["out"] = K_OUT;
	keywords["ref"] = K_REF;
	keywords["continue"] = K_CONTINUE;
	keywords["break"] = K_BREAK;

	types["int8_t"] = type(BT_INT,8,true);
	types["uint8_t"] = type(BT_INT,8,false);
	types["int16_t"] = type(BT_INT,16,true);
	types["uint16_t"] = type(BT_INT,16,false);
	types["int32_t"] = type(BT_INT,32,true);
	types["uint32_t"] = type(BT_INT,32,false);
	types["float"] = type(BT_FLOAT,32,false);
	types["void"] = type(BT_VOID,0,false);

}

union cell {
	ptrdiff_t i;
	size_t u;
	uint8_t *a;
	cell *c;
	float f;
};

#define NEXT break

#define BINARY(sym,t) ++sp; sp[0].t sym sp[1].t; NEXT

typedef void (*call0_0)();
typedef cell (*call0_1)();
typedef void (*call1_0)(cell);
typedef cell (*call1_1)(cell);
typedef void (*call2_0)(cell,cell);
typedef cell (*call2_1)(cell,cell);
typedef void (*call3_0)(cell,cell,cell);
typedef cell (*call3_1)(cell,cell,cell);
typedef void (*call4_0)(cell,cell,cell,cell);
typedef cell (*call4_1)(cell,cell,cell,cell);

/* method table */

/* field table */

/* local table */

class machine {
	public: void run(cell*,cell*,uint8_t*);
};

/* sp is current top of stack
   fp positive values are parameters; negative values are locals
   OP_CALL pushes the current pc onto the stack
   OP_ENTER pushes current fp onto stack, updates fp with sp, 
		and lowers sp to reserve space for locals 
   Some opcodes seem redundant; this is so we can reliably reproduce source from the opcodes.
   short-circuit-and is JZ; short-circuit-or is JNZ
   ternary operator is two instructions; 
   WHILE, ELSE, and ENDIF are aliases for JZ
   DO is an alias for JNZ
*/
void machine::run(cell *sp,cell *fp,uint8_t *pc) {
	for (;;) {
		switch (*pc++) {
			case OP_ADDU: BINARY(+=,u);
			case OP_SUBU: BINARY(-=,u);
			case OP_ADDF: BINARY(+=,f);
			case OP_SUBF: BINARY(-=,f);
			case OP_MULI: BINARY(*=,i);
			case OP_MULU: BINARY(*=,u);
			case OP_MULF: BINARY(*=,f);
			case OP_SHLU: BINARY(<<=,u);
			case OP_SHRU: BINARY(>>=,u);
			case OP_LIT0: --sp->i = 0; NEXT;
			case OP_LIT1: --sp->i = 1; NEXT;
			case OP_LIT2: --sp->i = 2; NEXT;
			case OP_LIT3: --sp->i = 3; NEXT;
			case OP_LITM1: --sp->i = -1; NEXT;
			case OP_LIT_B: --sp->u = *pc++; NEXT;
			case OP_LIT_H: --sp->u = *(uint16_t*)pc; pc+=2; NEXT;
			case OP_LIT_W: --sp->u = *(uint32_t*)pc; pc+=4; NEXT;
			case OP_CALL_W: --sp->a = pc; pc = *(uint8_t**)pc; NEXT;
			case OP_ENTER_B: --sp->c = fp; fp = sp; sp -= *pc++; NEXT;
			case OP_RET0: sp = fp; pc = sp->a; sp++; NEXT;
			case OP_RET1: sp = fp; pc = sp->a; sp+=2; NEXT;
			case OP_RET2: sp = fp; pc = sp->a; sp+=3; NEXT;
			case OP_RET3: sp = fp; pc = sp->a; sp+=4; NEXT;
			case OP_RET4: sp = fp; pc = sp->a; sp+=5; NEXT;
			case OP_RET_B: sp = fp; pc = sp->a; sp += *pc++; NEXT;
			case OP_LDLOC0: *--sp = fp[-1]; NEXT;
			case OP_LDLOC1: *--sp = fp[-2]; NEXT;
			case OP_LDLOC2: *--sp = fp[-3]; NEXT;
			case OP_LDLOC3: *--sp = fp[-4]; NEXT;
			case OP_LDLOC_B: *--sp = fp[-*pc++]; NEXT;
			case OP_LDARG0: *--sp = fp[1]; NEXT;
			case OP_LDARG1: *--sp = fp[2]; NEXT;
			case OP_LDARG2: *--sp = fp[3]; NEXT;
			case OP_LDARG3: *--sp = fp[4]; NEXT;
			case OP_LDARG_B: *--sp = fp[-*pc++]; NEXT;
			case OP_NATIVE0R0_A: (*(call0_0)pc)(); pc+=sizeof(void*); NEXT;
			case OP_NATIVE0R1_A: *--sp = (*(call0_1)pc)(); pc+=sizeof(void*); NEXT;
			case OP_NATIVE1R0_A: (*(call1_0)pc)(sp[0]); ++sp; pc+=sizeof(void*); NEXT;
			case OP_NATIVE1R1_A: sp[0] = (*(call1_1)pc)(sp[0]); pc+=sizeof(void*); NEXT;
			case OP_NATIVE2R0_A: (*(call2_0)pc)(sp[0],sp[1]); sp+=2; pc+=sizeof(void*); NEXT;
			case OP_NATIVE2R1_A: sp[1] = (*(call2_1)pc)(sp[0],sp[1]); ++sp; pc+=sizeof(void*); NEXT;
			case OP_NATIVE3R0_A: (*(call3_0)pc)(sp[0],sp[1],sp[2]); sp+=3; pc+=sizeof(void*); NEXT;
			case OP_NATIVE3R1_A: sp[2] = (*(call3_1)pc)(sp[0],sp[1],sp[2]); sp+=2; pc+=sizeof(void*); NEXT;
			case OP_NATIVE4R0_A: (*(call4_0)pc)(sp[0],sp[1],sp[2],sp[3]); sp+=4; pc+=sizeof(void*); NEXT;
			case OP_NATIVE4R1_A: sp[3] = (*(call4_1)pc)(sp[0],sp[1],sp[2],sp[3]); sp+=3; pc+=sizeof(void*); NEXT;
		}
	}
}
