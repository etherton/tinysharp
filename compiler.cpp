#include "compiler.h"
#include "table.h"

#include <map>
#include <string>

#include "opcodes.h"

namespace tinysharp {

void error(const char*,...);

/* Syntax is C-like except designed to be easy to parse */

/*
	type
	reference-to type
	array-of[N] type
*/
enum basetype_t : uint8_t { BT_VOID, BT_BOOL, BT_S8, BT_U8, BT_S16, BT_U16, BT_S32, BT_U32, BT_FLOAT, BT_STRING };

const uint8_t typeSizes[] = { 0, 1, 1, 1,  2, 2, 4, 4, 4, 4 };

const uint32_t boolType = (TI_TYPES<<24) | 0;

void compiler::setup() {

	m_types["bool"] = table::insert(TI_TYPES,BT_BOOL);
	m_types["char"] = table::insert(TI_TYPES,BT_S8);
	m_types["byte"] = table::insert(TI_TYPES,BT_U8);
	m_types["uchar"] = table::insert(TI_TYPES,BT_U8);
	m_types["short"] = table::insert(TI_TYPES,BT_S16);
	m_types["ushort"] = table::insert(TI_TYPES,BT_U16);
	m_types["int"] = table::insert(TI_TYPES,BT_S32);
	m_types["uint"] = table::insert(TI_TYPES,BT_U32);
	m_types["string"] = table::insert(TI_TYPES,BT_STRING);
	m_types["float"] = table::insert(TI_TYPES,BT_FLOAT);
	m_types["void"] = table::insert(TI_TYPES,BT_VOID);
}

int compiler::nextChar() {
	if (m_fileOffset == m_fileSize)
		m_ch = -1;
	else
		m_ch = m_fileBase[m_fileOffset++];
	return m_ch;
}

inline bool isdigit(int c) { return c>='0'&&c<='9'; }
inline bool isalpha(int c) { return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_'; }
inline bool isalnum(int c) { return isdigit(c) || isalpha(c); }

int compiler::nextToken() {
	char *endPtr;
	while (m_ch == 9 || m_ch == 32 || m_ch == 10 || m_ch == 13)
		nextChar();
	if (m_ch=='.') {
		nextChar();
		if (!isdigit(m_ch))			
			return '.';
		m_lval.f = strtof(m_fileBase+m_fileOffset-1,&endPtr);
		m_fileOffset = endPtr - m_fileBase;
		return K_FLOATLIT;
	}
	else if (isdigit(m_ch)) {
		m_lval.i = strtol(m_fileBase+m_fileOffset,&endPtr,10);
		m_fileOffset = endPtr - m_fileBase;
		nextChar();
		return K_INTLIT;
	}
	if (isalpha(m_ch)) {
		uint32_t s = m_fileOffset;
		while (isalnum(nextChar()))
			;
		m_lval.s = s | ((m_fileOffset - s)<<24);
#define K(x) case cthash(#x): return K_##x;
		switch (hash(m_fileBase + s,m_fileOffset-s)) {
			K(break)
			K(case)
			K(continue)
			K(do)
			K(else)
			K(for)
			K(if)
			K(in)
			K(interface)
			K(namespace)
			K(out)
			K(private)
			K(protected)
			K(public)
			K(ref)
			K(static)
			K(struct)
			K(virtual)
			K(while)
#undef K
		}
	}
	else switch(m_ch) {
		case '!': nextChar(); return '!';
		case '+': nextChar(); if (m_ch=='+') { nextChar(); return K_INCR; } else return '+';
		case '-': nextChar(); if (m_ch=='-') { nextChar(); return K_DECR; } else return '-';
		case '/': nextChar(); return '/';
		case '%': nextChar(); return '%';
		case '(': nextChar(); return '(';
		case ')': nextChar(); return ')';
	}
	return -1;
}

void stmt_if::emit() {
	m_cond->emit();
	auto ifFalse = emitForwardJump(OP_JZ);
	m_ifTrue->emit();
	if (m_ifFalse) {
		auto past = emitForwardJump(OP_J);
		placeForwardJump(ifFalse);
		m_ifFalse->emit();
		placeForwardJump(past);
	}
	else
		placeForwardJump(ifFalse);	
}

void stmt_while::emit() {
	auto eval = emitLabel();
	m_cond->emit();
	auto past = emitForwardJump(OP_JZ);
	beginLoopBody();
	m_body->emit();
	emitBackwardJump(OP_J,eval);
	placeForwardJump(past);
	endLoopBody(past,eval);	// break, continue addresses
}

void stmt_do::emit() {
	label_t top = emitLabel();
	beginLoopBody();
	m_body->emit();
	emitBackwardJump(OP_JNZ,top);
	label_t bottom = emitLabel();
	endLoopBody(bottom,top);
}

void stmt_for::emit() {
	if (m_initializer)
		m_initializer->emit();
	auto top = emitLabel();
	uint16_t bottom;
	if (m_cond)
		m_cond->emit();
	bottom = emitForwardJump(OP_JZ);
	beginLoopBody();
	m_body->emit();
	auto step = m_step? emitLabel() : top;
	if (m_step) {
		m_step->emit();
		emitBackwardJump(OP_J,top);
	}
	emitBackwardJump(OP_J,top);
	placeForwardJump(bottom);
	endLoopBody(bottom,step);
}

void expr_unary::emit() {
    m_unary->emit();
    emitOp(m_opcode);	
}

bool expr_unary::isConstant(constant_t &outValue) {
	if (m_unary->isConstant(outValue)) {
		switch (m_opcode) {
			case OP_NOTU: outValue.u = ~outValue.u; break;
			case OP_NEGI: outValue.i = -outValue.i; break;
			case OP_NEGF: outValue.f = -outValue.f; break;
			default: assert(false);
		}
        return true;
    }
	else
    	return false;
}


void expr_binary::emit() {
	m_right->emit();
	m_left->emit();
	emitOp(m_opcode);     	
}

bool expr_binary::isConstant(constant_t &outValue) {
    constant_t l, r;
    if (m_left->isConstant(l) && m_right->isConstant(r)) {
        switch (m_opcode) {
            case OP_MULI: outValue.u = l.u * r.u; break;
            case OP_ADDU: outValue.u = l.u + r.u; break;
            case OP_ADDF: outValue.f = l.f + r.f; break;
            case OP_SUBU: outValue.u = l.u - r.u; break;
			case OP_SHRI: outValue.i = l.i >> r.i; break;
			case OP_SHRU: outValue.u = l.u >> r.u; break;
			case OP_SHLU: outValue.u = l.u << r.u; break;
            default: assert(false);
        }
        return true;
    }
    else
        return false;
}

// https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/language-specification/grammar

void compiler::block() {
	if (m_token == '{') {
		statements();
		matchNextToken('}');
	}
	else
		statement();
}

stmt* compiler::declaration_statement() {
	return nullptr;
}

expr* compiler::boolean_expression() {
	expr *e = expression();
	if (e->m_type != boolType)
		error("expected boolean expression here");
	return e;
}

stmt* compiler::statement() {
	if (m_token == K_TYPE)
		return declaration_statement();
	else if (m_token == K_if) {
		matchNextToken('(');
		expr* cond = boolean_expression();
		matchNextToken(')');
		stmt* ifTrue = embedded_statement(), *ifFalse = nullptr;
		if (m_token == K_else) {
			nextToken();
			ifFalse = embedded_statement();
		}
		constant_t v;
		// deadstrip unnecessary conditions
		if (cond->isConstant(v))
			return v.u? ifTrue : ifFalse;
		else
			return new stmt_if(cond,ifTrue,ifFalse);
	}
	else if (m_token == K_while) {
		matchNextToken('(');
		expr *cond = boolean_expression();
		matchNextToken(')');
		stmt *body = embedded_statement();
		constant_t v;
		// deadstrip a while(false)
		if (cond->isConstant(v) && !v.u)
			return statement();
		else
			return new stmt_while(cond,body);
	}
	else if (m_token == K_do) {
		stmt *body = embedded_statement();
		matchNextToken(K_while);
		matchNextToken('(');
		expr *cond = boolean_expression();
		matchNextToken(')');
		constant_t v;
		// detect do { } while (0)
		if (cond->isConstant(v) && !v.u)
			return body;
		else
			return new stmt_do(body,cond);
	}
	else if (m_token == K_for) {
		matchNextToken('(');
		stmt *i = (m_token != ';')? declaration_statement() : nullptr;
		matchNextToken(';');
		expr *c = (m_token != ';')? boolean_expression() : nullptr;
		matchNextToken(';');
		expr *n = (m_token != ')')? boolean_expression() : nullptr;
		matchNextToken(')');
		stmt *body = embedded_statement();
		return new stmt_for(i,c,n,body);
	}
	else {
		error("expected a statement here");
		return nullptr;
	}
}


} // namespace tinysharp
