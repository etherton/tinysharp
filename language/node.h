#pragma once

#include "opcodes.h"

namespace tinysharp {

typedef uint16_t label_t;

union constant_t {
    float f;
    int i;
    unsigned u;
};

class node {
public:
    node() { }
    virtual void emit() = 0;
protected:
    static void emitOp(opcode_t op) {
        sm_heap[sm_pc++] = op;
    }
    static void emitByte(uint8_t b) {
        sm_heap[sm_pc++] = b;
    }
    static void emitHalf(uint16_t h) {
        sm_heap[sm_pc++] = h;
        sm_heap[sm_pc++] = h >> 8;
    }
    static void emitWord(uint32_t w) {
        sm_heap[sm_pc++] = w;
        sm_heap[sm_pc++] = w >> 8;
        sm_heap[sm_pc++] = w >> 16;
        sm_heap[sm_pc++] = w >> 24;
    }    
    static label_t emitForwardJump(opcode_t op) {
        emitOp(op);
        sm_pc += 2;
        return sm_pc - 2;
    }
    static void emitBackwardJump(opcode_t op,label_t dest) {
        emitOp(op);
        sm_heap[sm_pc++] = dest;
        sm_heap[sm_pc++] = dest>>8;
    }
    static void placeForwardJump(label_t &addr) {
        sm_heap[addr] = sm_pc;
        sm_heap[addr+1] = sm_pc >> 8;
        addr = sm_pc;
    }
    static label_t emitLabel() {
        return sm_pc;
    }
protected:
    static uint8_t *sm_heap;
    static label_t sm_pc;
};

class stmt_break;
class stmt_continue;

class stmt: public node {
public:
    struct state { stmt_break *b; stmt_continue *c; };
protected:
    // these declare a stack that `break` and `continue` jump to
    static state beginLoopBody();
    static void endLoopBody(state prev,label_t breakAddr,label_t continueAddr);
    static state sm_current;
};

class expr;

class stmt_if: public stmt {
public:
    stmt_if(expr *c,stmt *t,stmt *f) : m_cond(c), m_ifTrue(t), m_ifFalse(f) { }
    void emit();
private:
    expr *m_cond;
    stmt *m_ifTrue, *m_ifFalse;
};

class stmt_while: public stmt {
public:
    stmt_while(expr *c,stmt *b) : m_cond(c), m_body(b) { }
    void emit();
private:
    expr *m_cond;
    stmt *m_body;
};

class stmt_do: public stmt {
public:
    stmt_do(stmt *b,expr *c) : m_body(b), m_cond(c) { }
    void emit();
private:
    stmt *m_body;
    expr *m_cond;
};

class stmt_for: public stmt {
public:
    stmt_for(stmt *i,expr *c,expr *n,stmt *b) : m_initializer(i), m_cond(c), m_step(n), m_body(b) { }
    void emit();
private:
    stmt *m_initializer;
    expr *m_cond;
    expr *m_step;
    stmt *m_body;
};

class stmt_break: public stmt {
    friend class stmt;
public:
    void emit();
private:
    stmt_break *m_nextBreak;
    label_t m_offset;
};

class stmt_continue: public stmt {
    friend class stmt;
public:
    void emit();
private:
    stmt_continue *m_nextContinue;
    label_t m_offset;
};

class expr: public node {
public:
    virtual bool isConstant(constant_t &outValue) {
        return false;
    }
    uint32_t m_type;
};

class expr_integer_literal: public expr {
public:
    expr_integer_literal(int i) : m_literal(i) { }
    void emit();
    bool isConstant(constant_t &outValue);
private:
    int m_literal;
};

class expr_float_literal: public expr {
public:
    expr_float_literal(float f) : m_literal(f) { }
    void emit();
    bool isConstant(constant_t &outValue);
private:
    float m_literal;
};

class expr_unary: public expr {
public:
    expr_unary(expr *e,opcode_t o) : m_unary(e), m_opcode(o) { }
    void emit();
    bool isConstant(constant_t &outValue);
private:
    expr *m_unary;
    opcode_t m_opcode;
};

class expr_binary: public expr {
public:
    expr_binary(expr *l,expr *r,opcode_t o) : m_left(l), m_right(r), m_opcode(o) { }
    void emit();
    bool isConstant(constant_t &outValue);
private:
    expr *m_left, *m_right;
    opcode_t m_opcode;
};
 
} // tinysharp
