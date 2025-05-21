#include "compiler.h"
#include "table.h"
#include "node.h"

#include <map>
#include <string>

namespace tinysharp {

void error(const char* fmt,...) {
	va_list args;
	va_start(args,fmt);
	vfprintf(stderr,fmt,args);
	fputc('\n',stderr);
	va_end(args);
}


/* Syntax is C-like except designed to be easy to parse */

/*
	type
	reference-to type
	array-of[N] type
*/
enum basetype_t : uint8_t { 
	BT_VOID, BT_BOOL, BT_S8, BT_U8, 
	BT_S16, BT_U16, 
	BT_S32, BT_U32, BT_FLOAT, BT_STRING,
	BT_S64, BT_U64 
};

const uint8_t typeSizes[] = { 0, 1, 1, 1,  2, 2, 4, 4, 4, 4, 8, 8 };

const uint32_t boolType = (TI_TYPES<<24) | 0;

void compiler::setup() {

	m_types["bool"] = table::insert(TI_TYPES,BT_BOOL);
	m_types["char"] = table::insert(TI_TYPES,BT_S8);
	m_types["sbyte"] = table::insert(TI_TYPES,BT_S8);
	m_types["uchar"] = table::insert(TI_TYPES,BT_U8);
	m_types["short"] = table::insert(TI_TYPES,BT_S16);
	m_types["ushort"] = table::insert(TI_TYPES,BT_U16);
	m_types["int"] = table::insert(TI_TYPES,BT_S32);
	m_types["uint"] = table::insert(TI_TYPES,BT_U32);
	m_types["long"] = table::insert(TI_TYPES,BT_S64);
	m_types["ulong"] = table::insert(TI_TYPES,BT_U64);
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

expr* compiler::expression() {
	return additiveExpression();
}

expr* compiler::primaryExpression() {
	lval_t lval;
	if (nextTokenIs(K_INTLIT,lval)) {
		return new expr_integer_literal(lval.i);
	}
	else if (nextTokenIs('(')) {
		expr *r = expression();
		matchNextToken(')');
		return r;
	}
	error("unknown element in primary expr");
	return nullptr;
}

bool compiler::nextTokenIs(int token) {
	if (m_token == token) {
		nextToken();
		return true;
	}
	else
		return false;
}

bool compiler::nextTokenIs(int token,lval_t &outval) {
	if (m_token == token) {
		outval = m_lval;
		nextToken();
		return true;
	}
	else
		return false;
}

void compiler::matchNextToken(int token) {
	if (!nextTokenIs(token))
		error("expected '%c' here",token);
}

expr* compiler::multiplicativeExpression() {
	expr *left = primaryExpression();
	for (;;) {
		if (nextTokenIs('*'))
			left = new expr_binary(left,primaryExpression(),OP_MULI);
		else if (nextTokenIs('/'))
			left = new expr_binary(left,primaryExpression(),OP_DIVI);
		else if (nextTokenIs('%'))
			left = new expr_binary(left,primaryExpression(),OP_MODI);

	}
}
expr* compiler::additiveExpression() {
	expr *left = multiplicativeExpression();
	for (;;) {
		if (nextTokenIs('+'))
			left = new expr_binary(left,multiplicativeExpression(),OP_ADDU);
		else if (nextTokenIs('-'))
			left = new expr_binary(left,multiplicativeExpression(),OP_SUBU);
		else
			break;
	}
	return left;
}

expr* compiler::unaryExpr() {
	if (nextTokenIs('-'))
		return new expr_unary(unaryExpr(),OP_NEGI);
	else if (nextTokenIs('~'))
		return new expr_unary(unaryExpr(),OP_NOTU);
	else if (nextTokenIs('!'))
		return new expr_unary(unaryExpr(),OP_LNOT);
	else
		return primaryExpression();
}

expr* compiler::boolean_expression() {
	expr *e = expression();
	if (e->m_type != boolType)
		error("expected boolean expression here");
	return e;
}

stmt* compiler::statements() {
	return statement();
}

stmt* compiler::statement() {
	if (m_token == K_TYPE)
		return declaration_statement();
	else if (m_token == K_if) {
		matchNextToken('(');
		expr* cond = boolean_expression();
		matchNextToken(')');
		stmt* ifTrue = statement(), *ifFalse = nullptr;
		if (m_token == K_else) {
			nextToken();
			ifFalse = statement();
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
		stmt *body = statement();
		constant_t v;
		// deadstrip a while(false)
		if (cond->isConstant(v) && !v.u)
			return statement();
		else
			return new stmt_while(cond,body);
	}
	else if (m_token == K_do) {
		stmt *body = statement();
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
		stmt *body = statement();
		return new stmt_for(i,c,n,body);
	}
	else if (m_token == K_break) {
		matchNextToken(';');
		return new stmt_break();
	}
	else if (m_token == K_continue) {
		matchNextToken(';');
		return new stmt_continue();
	}	
	else {
		error("expected a statement here");
		return nullptr;
	}
}

} // namespace tinysharp
