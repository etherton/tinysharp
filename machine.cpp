#include "machine.h"
#include "opcodes.h"

namespace tinysharp {

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
			case OP_RET0: sp = fp; fp = sp->c; pc = sp[1].a; sp+=2; NEXT;
			case OP_RET1: sp = fp; fp = sp->c; pc = sp[1].a; sp+=3; NEXT;
			case OP_RET2: sp = fp; fp = sp->c; pc = sp[1].a; sp+=4; NEXT;
			case OP_RET3: sp = fp; fp = sp->c; pc = sp[1].a; sp+=5; NEXT;
			case OP_RET4: sp = fp; fp = sp->c; pc = sp[1].a; sp+=6; NEXT;
			case OP_RET_B: sp = fp; fp = sp->c; pc = sp[1].a; sp+=*pc++; NEXT;
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

} // namespace tinysharp

