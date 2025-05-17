#include <stdint.h>

#define OPCODE(op) OP_##op,

/* _B means one literal byte in opcode stream
   _H means one literal halfword in opcode stream
   _W means one literal word in opcode stream
   _A means one literal address in opcode stream */
#define OPCODE_LIST \
    OPCODE(ADDU) \
    OPCODE(SUBU) \
    OPCODE(ADDF) \
    OPCODE(SUBF) \
    OPCODE(MULI) \
    OPCODE(MULU) \
    OPCODE(MULF) \
    OPCODE(SHLU) \
    OPCODE(SHRU) \
    OPCODE(LIT0) \
    OPCODE(LIT1) \
    OPCODE(LIT2) \
    OPCODE(LIT3) \
    OPCODE(LITM1) \
    OPCODE(LIT_B) \
    OPCODE(LIT_H) \
    OPCODE(LIT_3B) \
    OPCODE(LIT_W) \
    OPCODE(TOKEN_W) \
    OPCODE(CALL_W) \
    OPCODE(ENTER_B) \
    OPCODE(RET0) \
    OPCODE(RET1) \
    OPCODE(RET2) \
    OPCODE(RET3) \
    OPCODE(RET4) \
    OPCODE(RET_B) \
    OPCODE(LDLOC0) \
    OPCODE(LDLOC1) \
    OPCODE(LDLOC2) \
    OPCODE(LDLOC3) \
    OPCODE(LDLOC_B) \
    OPCODE(LDLOCA_B) \
    OPCODE(STLOC0) \
    OPCODE(STLOC1) \
    OPCODE(STLOC2) \
    OPCODE(STLOC3) \
    OPCODE(LDARG0) \
    OPCODE(LDARG1) \
    OPCODE(LDARG2) \
    OPCODE(LDARG3) \
    OPCODE(LDARG_B) \
    OPCODE(LDARGA_B) \
    OPCODE(NATIVE0R0_A) \
    OPCODE(NATIVE0R1_A) \
    OPCODE(NATIVE1R0_A) \
    OPCODE(NATIVE1R1_A) \
    OPCODE(NATIVE2R0_A) \
    OPCODE(NATIVE2R1_A) \
    OPCODE(NATIVE3R0_A) \
    OPCODE(NATIVE3R1_A) \
    OPCODE(NATIVE4R0_A) \
    OPCODE(NATIVE4R1_A) \

enum opcode_t : unit8_t {
    OPCODE_LIST
};

