#include "compiler.h"
#include "table.h"

#include <map>
#include <string>

#include "opcodes.h"

namespace tinysharp {

void error(const char*,...);

/* Syntax is C-like except designed to be easy to parse */

std::map<std::string,int> keywords;

/*
	type
	reference-to type
	array-of[N] type
*/
enum basetype_t : uint8_t { BT_VOID, BT_BOOL, BT_S8, BT_U8, BT_S16, BT_U16, BT_S32, BT_U32, BT_FLOAT, BT_STRING };

const uint8_t typeSizes[] = { 0, 1, 1, 1,  2, 2, 4, 4, 4 };

const uint32_t boolType = (TI_TYPES<<24) | 0;

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
	auto top = emitLabel();
	beginLoopBody();
	m_body->emit();
	emitBackwardJump(OP_JNZ,top);
	auto bottom = emitLabel();
	endLoopBody(bottom,top);
}

void stmt_for::emit() {
	if (m_initializer)
		m_initializer->emit();
	auto top = emitLabel();
	uint16_t bottom;
	if (m_cond) {
		m_cond->emit();
		bottom = emitForwardJump(OP_JZ);
	}
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

void expr_binary::emit() {
	m_right->emit();
	m_left->emit();
	emitOp(m_opcode);     	
}

// https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/language-specification/grammar

void compiler::block() {
	if (m_token == '{') {
		statements();
		match_next_token('}');
	}
	else
		statement();
}

void compiler::block() {

}

stmt* compiler::statement() {
	if (m_token == K_TYPE)
		return declaration_statement();
	else
		return embedded_statement();
}

stmt* compiler::declaration_statement() {

}

expr* compiler::boolean_expression() {
	expr *e = expression();
	if (e->m_type != boolType)
		error("expected boolean expression here");
	return e;
}

stmt* compiler::statement() {
	if (m_token == K_TYPE)
		declaration_statement();
	else if (m_token == K_IF) {
		match_next_token('(');
		expr* cond = boolean_expression();
		match_next_token(')');
		stmt* ifTrue = embedded_statement(), *ifFalse = nullptr;
		if (m_token == K_ELSE) {
			next_token();
			ifFalse = embedded_statement();
		}
		return new stmt_if(cond,ifTrue,ifFalse);
	}
	else if (m_token == K_WHILE) {
		match_next_token('(');
		expr *cond = boolean_expression();
		match_next_token(')');
		stmt *body = embedded_statement();
		return new stmt_while(cond,body)
	}
}


} // namespace tinysharp
