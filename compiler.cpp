#include <stdint.h>
#include <map>
#include <string>

#include "opcodes.h"

namespace tinysharp {

/* Syntax is C-like except designed to be easy to parse */
enum { 
	K_STATIC = 256, K_CLASS, K_STRUCT, K_SWITCH, K_CASE, K_IF, K_FOR, K_ELSE, 
	K_WHILE, K_DO, K_PUBLIC, K_PRIVATE, K_PROTECTED, K_VIRTUAL, K_NAMESPACE,
	K_IN, K_OUT, K_REF, K_CONTINUE, K_BREAK
};
std::map<std::string,int> keywords;

enum basetype_t : uint8_t { BT_VOID, BT_BOOL, BT_S8, BT_U8, BT_S16, BT_U16, BT_S32, BT_U32, BT_FLOAT };

struct type {
	type(basetype_t basetype) : 
		_basetype(basetype) { }
	basetype_t _basetype;
};

std::map<std::string,type> types;

class compiler {
public: void setup();
void block();
void stmt();
void stmts();
void error();
int m_token;
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

	types.insert(std::make_pair("int8_t",type(BT_S8)));
	types.insert(std::make_pair("uint8_t",type(BT_U8)));
	types.insert(std::make_pair("int16_t",type(BT_S16)));
	types.insert(std::make_pair("uint16_t",type(BT_U16)));
	types.insert(std::make_pair("int32_t",type(BT_S32)));
	types.insert(std::make_pair("uint32_t",type(BT_U32)));
	types.insert(std::make_pair("float",type(BT_FLOAT)));
	types.insert(std::make_pair("void",type(BT_VOID)));

}

// https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/language-specification/grammar

void compiler::block() {
	if (m_token == '{') {
		stmts();
		match_next_token('}');
	}
	else
		stmt();
}

void compiler::block() {

}

void compiler::statement() {
	if (m_token == K_TYPE)
		declaration_statement();
	else
		embedded_statement();
}

void compiler::declaration_statement() {

}

void compiler::stmt() {
	if (m_token == K_TYPE)
		decl();
	else if (m_token == K_IF) {
		match_next_token('(');
		boolean_expression();
		match_next_token(')');
		embedded_statement();
		if (m_token == K_ELSE) {
			next_token();
			embedded_statement();
		}
	}
		bool hadElse = false;
		auto after = emit_forward_jump(OP_JZ);
		while (m_token == K_ELSE) {
			if (next_token() == K_IF) {
			}
			else if (hadElse)
				error("if with extra else");
			else {
				hadElse = true;
				block();
			}
		}
	}

	expr();
}


} // namespace tinysharp
