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
private:
    static uint8_t *sm_heap;
    static uint16_t sm_pc;
};

class stmt: public node {
public:
protected:
    // these declare a stack that `break` and `continue` jump to
    static void beginLoopBody();
    static void endLoopBody(label_t breakAddr,label_t continueAddr);

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
public:
    void emit();
};

class stmt_continue: public stmt {
public:
    void emit();
};

class expr: public node {
public:
    virtual bool isConstant(constant_t &outValue) {
        return false;
    }
    uint32_t m_type;
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
