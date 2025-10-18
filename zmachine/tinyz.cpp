// tinyz, a minimal z machine compiler
#include <stdint.h>
#include <assert.h>
#include <map>
#include <vector>

#include "opcodes.h"

constexpr uint32_t cthash(const char *key) {
    uint32_t seed = 0;
    while (*key) {
        seed += *key++;
        seed += seed << 10;
        seed ^= seed >> 6;
    }
    seed += seed << 3;
    seed ^= seed >> 11;
    seed += seed << 15;
    return seed;
}

inline uint32_t hash(const char *key,uint32_t length) {
    uint32_t seed = 0;
    while (length--) {
        seed += *key++;
        seed += seed << 10;
        seed ^= seed >> 6;
    }
    seed += seed << 3;
    seed ^= seed >> 11;
    seed += seed << 15;
    return seed;
}

// these are reserved words inside routines
const uint32_t k_if = cthash("if");
const uint32_t k_else = cthash("else");
const uint32_t k_repeat = cthash("repeat");
const uint32_t k_while = cthash("while");
const uint32_t k_rtrue = cthash("rtrue");
const uint32_t k_rfalse = cthash("rfalse");
const uint32_t k_return = cthash("return");

// these are reserved words in toplevel syntax
const uint32_t k_attribute = cthash("attribute");
const uint32_t k_property = cthash("property");
const uint32_t k_location = cthash("location");
const uint32_t k_object = cthash("object");
const uint32_t k_routine = cthash("routine");
const uint32_t k_words = cthash("words");
const uint32_t k_global = cthash("global");
const uint32_t k_article = cthash("article");
const uint32_t k_action = cthash("action");
const uint32_t k_placeholder = cthash("placeholder");
const uint32_t k_direction = cthash("direction");
const uint32_t k_version = cthash("version");
const uint32_t k_v3 = cthash("#v3");
const uint32_t k_v5 = cthash("#v5");
const uint32_t k_v8 = cthash("#v8");



// Expression pointers are 4-byte aligned, so two LSB's are zero
// LSB's of 01 are for variables, 10 for small constants, 11, for large constants.
// The actual Z machine encodings are xor 3; large is 0, small is 1, variables is 2
#define SP              (0x1)
#define LOCAL(N)        (0x1 | (((N)+1)<<2))
#define GLOBAL(N)       (0x1 | (((N)+16)<<2))
#define SMALLCONST(N)   (0x2 | ((N)<<2))
#define BIGCONST(N)     (0x3 | ((N)<<2))

void fatal(const char*,...);

struct label {
    uint16_t offset; // offset of the branch value itself within the instruction.
    uint16_t size; // 6,8,14,16 the number of bits encoding the address
};

namespace codegen {
    static uint8_t buffer[4096];
    uint16_t pc;
    void emitByte(uint8_t b) {
        if (pc == sizeof(buffer))
            fatal("routine too long");
        buffer[pc++] = b;
    }
    size_t convertOp(size_t op) {
        if ((op & 3)==3 && op <= 1023)
            return op-1;// convert BIGCONST to SMALLCONST
        else
            return op;
    }
    size_t convertOp(void *e) {
        size_t op = (size_t)e;
        if (!(op & 3))
            return SP;  // pointer to expr converts to TOS
        else
            return convertOp(op);
    }
    void emitOperand(size_t operand) {
        if ((operand & 3)==3)
            emitByte(operand >> 3);
        emitByte(operand >> 2);
    }
    void emitBranchPart(int16_t delta,bool negated) {
        if (delta >= 0 && delta <= 63)
            emitByte((negated << 7) | 0x40 | delta);
        else {
            assert(delta <= 8191 && delta >= -8192);
            emitByte((negated << 7) | ((delta >> 8) & 63));
            emitByte(delta);
        }
    }
    void emitOpcode0(_0op opcode) {
        emitByte(0xB0 + (uint8_t)opcode);
    }
    void emitOpcode1(_1op opcode,size_t unary,uint8_t dest = 255,int16_t delta = -32768,bool negated = false) {
        emitByte(0x80 + (((unary ^ 3) & 3) << 4) + (uint8_t)opcode);
        emitOperand(unary);
        if ((1 << (uint8_t)opcode) & 0b1000'0000'0000'0110)
            emitByte(dest);
        else
            assert(dest==255);
        if ((1 << (uint8_t)opcode) & 0b0000'0000'0000'0111)
            emitBranchPart(delta,negated);
        else
            assert(delta == -32768);
    }
    void emitOpcode2(_2op opcode,size_t left,size_t right,uint8_t dest,int16_t delta,bool negated) {
        if ((left & 3)==2 && (right & 3)==2)
            emitByte(0x00 + (uint8_t)opcode);
        else if ((left & 3)==2 && (right & 3)==1)
            emitByte(0x20 + (uint8_t)opcode);
        else if ((left & 3)==1 && (right & 3)==2)
            emitByte(0x40 + (uint32_t)opcode);
        else if ((left & 3)==1 && (right & 3)==1)
            emitByte(0x60 + (uint8_t)opcode);
        else { // VAR form, extra byte with types.
            emitByte(0xC0 + (uint8_t)opcode);
            emitByte(0xF |
                (((left & 3) ^ 3) << 6) |
                (((right & 3) ^ 3) << 4));
        }
        emitOperand(left);
        emitOperand(right);
        if ((1U << (uint8_t)opcode) & 0b0000'0011'1111'1111'1000'0011'0000'0000)
            emitByte(dest);
        else
            assert(dest==255);
        if ((1U << (uint8_t)opcode) & 0b0000'0000'0000'0000'0000'0100'1111'1110)
            emitBranchPart(delta,negated);
        else
            assert(delta == -32768);
    }

    static label emitForwardJump(size_t distance) {
        if (distance > 255) {
            emitByte(0x8C);
            auto result = label { codegen::pc, 16 };
            codegen::pc+=2;
            return result;
        }
        else {
            codegen::emitByte(0x9C);
            return label { codegen::pc++, 8 };
        }
    }
    static void emitBackwardJump(expr *c,bool negate,uint16_t dest);
    static void emitJump(expr *c,bool negate,unsigned dest);
    static void placeLabel(label l) {
        int delta = pc - l.offset + 2;
        switch (l.size) {
            case 6: codegen::buffer[l.offset] = (buffer[l.offset] & 0xC0) | (delta & 0x3F); break;
            case 8: buffer[l.offset] = delta; break;
            case 14: buffer[l.offset] = (buffer[l.offset] & 0xC0) | ((delta>>8) & 0x3F);
                    buffer[l.offset+1] = delta; break;
            case 16: buffer[l.offset] = delta >> 8; buffer[l.offset+1] = delta; break;
        }
    }
    static uint16_t placeLabelHere()

}

struct expr {
    virtual bool isConstant(int &value) const = 0;
    static bool isConstant(expr *e,int &value) {
        switch ((size_t)e & 3) {
            case 0: return e->isConstant(value);
            case 1: return false;
            case 2: case 3: value = ((ptrdiff_t)e >> 2);
        }
    }
    virtual void emit(uint8_t dest,int16_t delta = -32768,bool negated = false) const = 0;
    virtual bool isBranch() const { return false; }
    virtual bool isNegated() const { return false; }
    virtual bool isLeaf() const { return false; }
    virtual size_t computeSize() const = 0;
    static size_t computeSize(expr *e) const {
        switch ((size_t)e & 3) {
            case 0: return e->computeSize();
            case 1: case 2: return 1;
            case 3: return 2;
        }
    }
};

// g_var = 12 + l_var * 17
// stmt_store(g_var, expr_add(12, expr_mul(l_var,17)))
// @mul l_var, 17 -> @sp
// @add 12, @sp -> g_var
// stmt_store::emit:
//   expr_add::emit_value(g_var):
//     if (!left->isLeaf()) left->emit_value(@sp);
//     if (!right->isLeaf()) right->emit_value(@sp);

// g_var = 12 + l_var * 9 + 66 * (l_var_2 - l_var_3)
// stmt_store(g_var, expr_add(12,expr_add(expr_mul(l_var,9),expr_mul(66,expr_sub(l_var_2,l_var_3))))
// @sub l_var_2, l_var_3 -> @sp
// @mul 66,@sp -> @sp
// @mul l_var,9 -> @sp
// @add @sp,@sp -> @sp
// @add 12,@sp -> g_var


struct expr_unary: public expr {
    expr_unary(expr *e) : m_unary(e) { }
    expr *m_unary;
    void emit(uint8_t dest,int16_t delta,bool negated) const {
        if (!((size_t)m_unary & 3))
            m_unary->emit(0);
        codegen::emitOpcode1(unaryOpcode(),
            codegen::convertOp(m_unary),dest,delta,negated);
    }
    virtual _1op unaryOpcode() const = 0;
};

struct expr_binary: public expr {
    expr_binary(expr *l,expr *r) : m_left(l), m_right(r) { }
    expr *m_left, *m_right;
    void emit(uint8_t dest,int16_t delta,bool negated) const {
        if (!((size_t)m_left & 3))
            m_left->emit(0);
        if (!((size_t)m_right & 3))
            m_right->emit(0);
        codegen::emitOpcode2(binaryOpcode(),
            codegen::convertOp(m_left),
            codegen::convertOp(m_right),dest,delta,negated);
    }
    virtual _2op binaryOpcode() const = 0;
};

struct expr_unary_branch: public expr_unary {
    expr_unary_branch(expr *e) : expr_unary(e) { }
    virtual bool isBranch() const { return true; }
};

struct expr_binary_branch: public expr_binary {
    expr_binary_branch(expr *l,expr *r) : expr_binary(l,r) { }
    virtual bool isBranch() const { return true; }
};

#define UNARY_IS_CONSTANT(C_OP,Z_OP) \
    virtual bool isConstant(int &value) const { \
        if (expr::isConstant(m_unary,value)) { \
            value = C_OP value; \
            return true; \
        } \
        else \
            return false; \
    } \
    _1op unaryOpcode() const { return _1op::Z_OP; }

#define BINARY_IS_CONSTANT(C_OP,Z_OP) \
    virtual bool isConstant(int &value) const { \
        int l, r; \
        if (expr::isConstant(m_left,l) && expr::isConstant(m_right,r)) { \
            value = l C_OP r; \
            return true; \
        } \
        else \
            return false; \
    } \
    _2op binaryOpcode() const { return _2op::Z_OP; }


// NAME should match the z machine opcode, OP is the C binary operator
#define IMPL_UNARY(NAME,OP) \
struct expr_##NAME: public expr_unary { \
    expr_##NAME(expr *u) : expr_unary(u) { } \
    UNARY_IS_CONSTANT(OP,NAME) \
};

#define IMPL_BINARY(NAME,OP) \
struct expr_##NAME: public expr_binary { \
    expr_##NAME(expr *l,expr *r) : expr_binary(l,r) { } \
    BINARY_IS_CONSTANT(OP,NAME) \
};

#define IMPL_UNARY_BRANCH(NAME,C_OP,Z_OP,NEGATED) \
struct expr_##NAME: public expr_unary_branch { \
    expr_##NAME(expr *u) : expr_unary_branch(u) { } \
    UNARY_IS_CONSTANT(C_OP,Z_OP) \
    bool isNegated() const { return NEGATED; } \
};

#define IMPL_BINARY_BRANCH(NAME,C_OP,Z_OP,NEGATED) \
struct expr_##NAME: public expr_binary_branch { \
    expr_##NAME(expr *l,expr *r) : expr_binary_branch(l,r) { } \
    BINARY_IS_CONSTANT(C_OP,Z_OP) \
    bool isNegated() const { return NEGATED; } \
};

#define IMPL_BINARY_BRANCH_NC(NAME,Z_OP,NEGATED) \
struct expr_##NAME: public expr_binary_branch { \
    expr_##NAME(expr *l,expr *r) : expr_binary_branch(l,r) { } \
    _2op binaryOpcode() const { return _2op::Z_OP; } \
    bool isConstant(int&) const { return false; } \
    bool isNegated() const { return NEGATED; } \
};

IMPL_BINARY(mul,*)
IMPL_BINARY(div,/)
IMPL_BINARY(mod,%)
IMPL_BINARY(add,+)
IMPL_BINARY(sub,-)
IMPL_BINARY(and_,&)
IMPL_BINARY(or_,|)

// IMPL_UNARY(neg,-)
IMPL_UNARY(not_,~)

IMPL_BINARY_BRANCH(greater_than,>,jg,false)
IMPL_BINARY_BRANCH(less_equal,<=,jg,true)
IMPL_BINARY_BRANCH(less_than,<,jl,false)
IMPL_BINARY_BRANCH(greater_equal,>=,jl,true)
IMPL_BINARY_BRANCH(equal,==,je,false)
IMPL_BINARY_BRANCH(not_equal,!=,je,true)

IMPL_BINARY_BRANCH_NC(test_attr,test_attr,false);
IMPL_BINARY_BRANCH_NC(not_test_attr,test_attr,true);

IMPL_UNARY_BRANCH(zero,0==,jz,false)
IMPL_UNARY_BRANCH(not_zero,0!=,jz,true)

struct expr_not: public expr_unary_branch {
    expr_not(expr* u) : expr_unary_branch(u) { }
    bool isNegated() const { return !m_unary->isNegated(); }
    bool isConstant(int &value) const {
        if (expr::isConstant(m_unary,value)) {
            value = !value;
            return true;
        }
        else
            return false;
    }
    _1op unaryOpcode() const;
};

struct expr_logical_and: public expr_binary_branch {
    expr_logical_and(expr *l,expr *r) : expr_binary_branch(l,r) { }
    bool isConstant(int &value) const {
        if (expr::isConstant(m_left,value)) {
            if (value) {
                if (expr::isConstant(m_right,value))
                    return true;
            }
            else // if first is false, expr is constant (and zero)
                return true;
        }
        else
            return false;
    }
};

struct expr_logical_or: public expr_binary_branch {
    expr_logical_or(expr *l,expr *r) : expr_binary_branch(l,r) { }
    bool isConstant(int &value) const {
        if (expr::isConstant(m_left,value)) {
            if (value)
                return true;
            else
                return expr::isConstant(m_right,value);
        }
        else
            return false;
    }
    void emit(uint8_t dest,int16_t delta,bool negated) const {

    }
    _2op binaryOpcode() const { exit(1); }
};


struct stmt {
    virtual void emit() = 0;
    virtual bool isConstantReturn(int &) const { return false; }
    // This is an upper bound on size, for determining size of forward jumps.
    virtual size_t computeSize() const = 0;
};

struct stmt_list: public stmt {
    stmt_list(stmt *a,stmt *d) : m_car(a), m_cdr(d) { }
    stmt *m_car, *m_cdr;
    void emit() {
        if (m_car)
            m_car->emit();
        if (m_cdr)
            m_cdr->emit();
    }
    bool isConstantReturn(int &value) const {
        return !m_car && m_cdr && m_cdr->isConstantReturn(value);
    }
    size_t computeSize() const {
        return (m_car? m_car->computeSize() : 0) + (m_cdr? m_cdr->computeSize() : 0);
};


struct stmt_flow: public stmt {
    stmt_flow(expr *c) {
        int value;
        if (expr::isConstant(c,value))
            m_cond = (expr*)(value>=0&&value<=255? SMALLCONST(value) : BIGCONST(value));
        else
            m_cond = c;
    }
    expr *m_cond;
    // emit a conditional jump on the negated branch expression
    static label emitForwardJump(expr *c,bool negate,size_t distance) {
        c->emit(0,int16_t(distance),negate);
        //return label { codegen::currentOffset, distance };
    }
    static label emitForwardJump(size_t distance) {
        if (distance > 255) {
            codegen::emitByte(0x8C);
            codegen::emitByte(0);
            codegen::emitByte(0);
            return label { uint16_t(codegen::pc-2), 16 };
        }
        else {
            codegen::emitByte(0x9C);
            return label { codegen::pc++, 8 };
        }
    }
    static void emitBackwardJump(expr *c,bool negate,uint16_t dest) {
        int delta = codegen::pc - dest + 2;
        c->emit(0,delta,negate);
    }
    static void emitJump(expr *c,bool negate,unsigned dest);
    static void placeLabel(label l) {
        int delta = codegen::pc - l.offset + 2;
        switch (l.size) {
            case 6: codegen::buffer[l.offset] = (buffer[l.offset] & 0xC0) | (pc - l.offset - 2); break;
            case 8: buffer[l.offset] = (pc - l.offset - 2); break;
            case 14: buffer[l.offset] = (buffer[l.offset] & 0xC0) | (((pc - l.offset - 2) >> 8) & 63);
                    buffer[l.offset+1] = ((pc

    }
    static uint16_t placeLabelHere() {
        return pc;
    }
};

struct stmt_if: public stmt_flow {
    stmt_if(expr *c,stmt *t,stmt *f) : stmt_flow(c), m_ifTrue(t), m_ifFalse(f) { }
    stmt *m_ifTrue, *m_ifFalse;
    void emit() {
        int value;
        if (expr::isConstant(m_cond,value)) {
            if (value)
                m_ifTrue->emit();
            else if (m_ifFalse)
                m_ifFalse->emit();
        }
        else if (m_ifFalse) {
            if (m_ifTrue->isConstantReturn(value) && (value==0||value==1)) {
                emitJump(m_cond,false,value);
                m_ifFalse->emit();
            }
            else if (m_ifFalse->isConstantReturn(value) && (value==0||value==1)) {
                emitJump(m_cond,true,value);
                m_ifTrue->emit();
            }
            else {
                auto failed = emitForwardJump(m_cond,true,m_ifTrue->computeSize());
                m_ifTrue->emit();
                auto bottom = emitForwardJump(m_ifFalse->computeSize());
                placeLabel(failed);
                m_ifFalse->emit();
                placeLabel(bottom);
            }
        }
        else {
            if (m_ifTrue->isConstantReturn(value) && (value==0||value==1))
                emitJump(m_cond,false,value);
            else {
                auto bottom = emitForwardJump(m_cond,true,m_ifTrue->computeSize());
                m_ifTrue->emit();
                placeLabel(bottom);
            }
        }
    }
    size_t computeSize() const {
        return expr::computeSize(m_cond) + m_ifTrue->computeSize() + (m_ifFalse? 3 + m_ifFalse->computeSize() : 0);
    }
};

struct stmt_while: public stmt_flow {
    stmt_while(expr *c,stmt *b) : stmt_flow(c), m_body(b) { }
    stmt *m_body;
    void emit() {
        int value;
        if (expr::isConstant(m_cond,value)) {
            if (value) {
                auto top = placeLabelHere();
                m_body->emit();
                emitBackwardJump(nullptr,false,top);
            }
        }
        else {
            auto top = placeLabelHere();
            auto bottom = emitForwardJump(m_cond,false);
            m_body->emit();
            // TODO - if the condition is simple enough, just emit it again
            // But it won't save any space because all backward conditional jumps take
            // at least three bytes. Would just be a minor performance improvement.
            emitBackwardJump(nullptr,false,top);
            placeLabel(bottom);
        }
    }
    size_t computeSize() const {
        // TODO: if m_body's size is larger than a 14 bit signed constant,
        // we could adjust the size and reverse the test to skip a jump.
        // in practice 8k is a pretty huge function.
        return expr::computeSize(m_cond) + m_body->computeSize() + 3;
    }
};

struct stmt_repeat: public stmt_flow {
    stmt_repeat(expr *c,stmt *b) : stmt_flow(c), m_body(b) { }
    stmt *m_body;
    void emit() {
        int value = 1;
        if (expr::isConstant(m_cond,value) && value==0)
            m_body->emit();
        else {
            auto top = placeLabelHere();
            m_body->emit();
            emitBackwardJump(value? m_cond : nullptr,true,top);
        }
    }
    size_t computeSize() const {
        return expr::computeSize(m_cond) + m_body->computeSize() + 3;
    }
};

struct stmt_return: public stmt {
    stmt_return(expr *e) : m_value(e) { }
    expr *m_value;
    bool isConstantReturn(int &value) const {
        return expr::isConstant(m_value,value);
    }
    void emit() {
        int value;
        if (expr::isConstant(m_value,value) && (value==0 || value==1))
            codegen::emitOpcode0(value? _0op::rtrue : _0op::rfalse);
        else {
            if (!(size_t(m_value) & 3))
                m_value->emit(0);
            codegen::emitOpcode1(_1op::ret,codegen::convertOp(m_value));
        }
    }
};

struct stmt_store: public stmt {
    stmt_store(uint8_t d,expr *e) : m_dest(d), m_value(e) { }
    uint8_t m_dest;
    expr *m_value;
    void emit() {
        int value;
        if (expr::isConstant(m_value,value))
            codegen::emitOpcode2(_2op::store,codegen::convertOp(SMALLCONST(m_dest)),codegen::convertOp(BIGCONST(value)),0xFF);
        else
            m_value->emit(m_dest);
    }
};

struct operator_def {
    int8_t arity, precedence;
    expr* (*generator)();
    void init(int8_t a,int8_t p,expr* (*g)()) {
        arity = a;
        precedence = p;
        generator = g;
    }
};

struct compiler {
    compiler();
    stmt *parse_stmt();
    expr *parse_expr();
    expr *parse_branch_expr();
    void next_token();
    void match_token(uint32_t);
    void match_and_consume(uint32_t);
    static expr* pop_output_stack();
    uint32_t m_current_token;
    std::map<uint32_t,operator_def> m_operators;
};

#define UNARY_OPERATOR(SYM,PREC,TYPE) m_operators[cthash(SYM)].init(1,PREC,[] ->expr* { \
    return new TYPE(compiler::pop_output_stack()); })

#define BINARY_OPERATOR(SYM,PREC,TYPE) m_operators[cthash(SYM)].init(2,PREC,[] ->expr* { \
    auto r = compiler::pop_output_stack(), \
        l = compiler::pop_output_stack(); \
        return new TYPE(l,r); })

compiler::compiler() {
    m_operators[cthash("(")].init(-1,0,nullptr);
    UNARY_OPERATOR("not",2,expr_not);
    UNARY_OPERATOR("~",2,expr_not_);

	//operators["++>"] = new operator_def(2,3,"inc_chk");
	//operators["--<"] = new operator_def(2,3,"dec_chk");
    BINARY_OPERATOR("has",4,expr_test_attr);
    BINARY_OPERATOR("hasnt",4,expr_not_test_attr);
    BINARY_OPERATOR("*",5,expr_mul);
    BINARY_OPERATOR("/",5,expr_div);
    BINARY_OPERATOR("%",5,expr_mod);
    BINARY_OPERATOR("+",6,expr_add);
    BINARY_OPERATOR("-",6,expr_sub);
    BINARY_OPERATOR("<",9,expr_less_than);
    BINARY_OPERATOR(">=",9,expr_greater_equal);
    BINARY_OPERATOR(">",9,expr_greater_than);
    BINARY_OPERATOR("<=",9,expr_less_equal);
    BINARY_OPERATOR("==",10,expr_equal);
    BINARY_OPERATOR("!=",10,expr_not_equal);
    BINARY_OPERATOR("&",11,expr_and_);
    BINARY_OPERATOR("|",12,expr_or_);
    BINARY_OPERATOR("and",14,expr_logical_and);
    BINARY_OPERATOR("or",15,expr_logical_or);

	//operators["get_sibling"] = new operator_def(0,1,"get_sibling");
	//operators["get_child"] = new operator_def(0,1,"get_child");
}

expr* compiler::parse_branch_expr() {
    match_and_consume('(');

}

stmt* compiler::parse_stmt() {
    if (m_current_token == k_if) {
        next_token();
        auto e = parse_branch_expr();
        auto ifTrue = parse_stmt();
        if (m_current_token == k_else)
            return new stmt_if(e,ifTrue,parse_stmt());
        else
            return new stmt_if(e,ifTrue,nullptr);
    }
    else if (m_current_token == k_while) {
        next_token();
        auto e = parse_branch_expr();
        return new stmt_while(e,parse_stmt());
    }
    else if (m_current_token == k_repeat) {
        next_token();
        auto body = parse_stmt();
        match_token(k_while);
        next_token();
        return new stmt_repeat(parse_branch_expr(),body);
    }
    else if (m_current_token == k_rtrue) {
        next_token();
        match_and_consume(';');
        return new stmt_return((expr*)SMALLCONST(0));
    }
    else if (m_current_token == k_rfalse) {
        next_token();
        match_and_consume(';');
        return new stmt_return((expr*)SMALLCONST(1));
    }
    else if (m_current_token == k_return) {
        next_token();
        auto r = new stmt_return(parse_expr());
        match_and_consume(';');
        return r;
    }
    else if (m_current_token == k_local || current_token == k_global) {
        next_token();
        match_and_consume('=');
        return new stmt_store(LOCAL(0),parse_expr);
    }
    else if (m_current_token == '{');
        next_token();
        stmt *list = nullptr;
        while (m_current_token != '}')
            list = new stmt_list(list,parse_stmt());
        next_token();
    }
}