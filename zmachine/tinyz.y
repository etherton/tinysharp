/* tinyz.y */
%{
	#include "opcodes.h"
	#include <set>
	#include <map>

	bool is_object;
	void open_scope();
	void close_scope();
	int yylex();
	void yyerror(const char*);

	struct symbol {
		const char *name;
		int16_t token;
		int16_t ival;
	};
	struct dict_entry {
		uint16_t encoded[2];		// 6 letters for V3
		uint8_t payload;
	};
	std::set<dict_entry> dictionary;
	template <typename T> struct list_node{
		list_node<T>(T* a,list_node<T> *b) : car(a), cdr(b) { }
		T* car;
		list_node<T> *cdr;
	};
	struct expr {
		virtual void emit();
		virtual bool isConstand(int &v) { return false; }
	};
	struct expr_binary: public expr {
		expr_binary(expr *left,_2op op,expr *right);
	};
	struct expr_unary: public expr {
		expr_unary(_1op op,expr *e);
	};
	struct expr_branch: public expr {
		expr_branch(bool negated);
	};
	struct expr_binary_branch: public expr_branch {
		expr_binary_branch(expr *left,_2op op,bool negated,expr *right);
	};
	struct expr_unary_branch: public expr_branch {
		expr_unary_branch(_1op op,bool negated,expr *e);
	};
	struct expr_literal: public expr {
		expr_literal(int v) : value(v) { }
		int value;
	};
	struct expr_logical_and: public expr {
		expr_logical_and(expr *left,expr *right);
	};
	struct expr_logical_or: public expr {
		expr_logical_or(expr *left,expr *right);

	};
	struct expr_literals: public expr {
		expr_literals(int a) : count(1) { literals[0] = a; }
		expr_literals(int a,int b) : count(2) { literals[0] = a; literals[1] = b; }
		expr_literals(int a,int b,int c) : count(3) { literals[0] = a; literals[1] = b; literals[2] = c; }
		int16_t count, literals[3];
	};
	uint8_t attribute_next[3] = {0,31,31}; // 31 should be 47 for v4+
	uint8_t property_next[3] = {0,31,31}; // 31 should be 63 for v4+
	uint8_t next_value_in_scope(int sc,uint8_t *state) {
		uint8_t result = state[sc] | (sc << 6);
		if (sc) state[sc]--; else state[sc]++;
		return result;
	}
%}

%union {
	int ival;
	const char *sval;
	expr *eval;
	symbol *sym;
	dict_entry *dict;
	list_node<dict_entry> *dlist;
}

%token ATTRIBUTE PROPERTY DIRECTION GLOBAL OBJECT LOCATION ROUTINE ARTICLE PLACEHOLDER ACTION HAS HASNT
%token BYTE_ARRAY WORD_ARRAY CALL
%token <dict> DICT
%token <ival> ANAME PNAME LNAME GNAME RNAME INTLIT STRLIT
%token <sym> FWDREF // Can be NEWSYM or existing sym
%token <sym> NEWSYM
%token WHILE "while" REPEAT "repeat" IF "if" ELSE "else"
%token LE "<=" GE ">=" EQ "==" NE "!="
// %token LSH "<<" RSH ">>"
%token RFALSE "rfalse" RTRUE "rtrue" RETURN "return"
%token OR "or" AND "and" NOT "not"
%right '~' NOT
%left '*' '/' '%'
%left '+' '-'
// %left LSH RSH
%left '&'
%left '|'
%left AND
%left OR

%type <eval> expr rel_expr primary exprs aname
%type <ival> scope
%type <dlist> dict_list;


%%

prog
	: decl_list;

decl_list
	: decl_list decl
	| decl;

decl
	: attribute_def
	| property_def
	| direction_def
	| global_def
	| object_def
	| location_def
	| routine_def
	| article_def
	| placeholder_def
	| action_def
	;

attribute_def
	: ATTRIBUTE scope NEWSYM ';' { $3->token = ANAME; $3->ival = next_value_in_scope($2,attribute_next); }
	;

scope
	: GLOBAL		{ $$ = 0; }
	| LOCATION		{ $$ = 1; }
	| OBJECT		{ $$ = 2; }
	;

property_def
	: PROPERTY scope NEWSYM ';' { $3->token = PNAME; $3->ival = next_value_in_scope($2,property_next); }
	;

direction_def
	: DIRECTION PNAME dict_list ';' { 
		if (($2 >> 6)!=1) yyerror("direction property must be type location");
		if (($2 & 63) == 0 || ($2 & 63) > 15) yyerror("direction property index must be between 1 and 15");
		for (auto it=$3; it; it=it->cdr)
			if ($3->car->payload & 15) 
				yyerror("dictionary word already has direction bits set");
			else
				$3->car->payload |= ($2 & 15);
	}
	;

dict_list
	: DICT dict_list	{ $$ = new list_node<dict_entry>($1,$2); }
	| DICT				{ $$ = new list_node<dict_entry>($1,nullptr); }
	;


global_def
	: GLOBAL NEWSYM ';'
	| BYTE_ARRAY '(' INTLIT ')' NEWSYM opt_array_def ';'
	| WORD_ARRAY '(' INTLIT ')' NEWSYM opt_array_def ';'
	;

opt_array_def
	:
	| array_def
	;

array_def
	: '{' init_list '}'
	;

init_list
	: init
	| init_list ',' init
	;

init
	: INTLIT
	;

object_def
	: OBJECT { is_object = true; } object_or_location_def
	;

location_def
	: LOCATION { is_object = false; } object_or_location_def
	;

object_or_location_def
	: NEWSYM STRLIT opt_parent '{' property_or_attribute_list '}' ';'
	;

opt_parent
	:
	| '(' FWDREF ')'
	;

property_or_attribute_list
	: property_or_attribute_list property_or_attribute
	| property_or_attribute
	;

property_or_attribute
	: PNAME ':' pvalue ';'
	| ANAME ';'
	;

pvalue
	: FWDREF
	| STRLIT
	| INTLIT
	| routine_body
	| dict_list
	;

routine_def
	: ROUTINE NEWSYM routine_body
	;

article_def
	: ARTICLE dict_list ';'
	;

placeholder_def
	: PLACEHOLDER NEWSYM ';'
	;

action_def
	: ACTION NEWSYM '{' '}'
	;

routine_body
	: '[' { open_scope(); } opt_params_list opt_locals_list ']' '{' stmts '}' { close_scope(); }
	;

opt_params_list
	: 
	| params_list
	;

params_list
	: params_list param
	| param
	;

param
	:  NEWSYM
	;

opt_locals_list
	: 
	| ';' locals_list
	;

locals_list
	: locals_list local
	| local
	;

local
	: NEWSYM
	;

stmts
	: stmts stmt
	| stmt
	;

stmt
	: "if" cond_expr stmt opt_else ';'
	| "repeat" stmt "while" cond_expr ';'
	| "while" cond_expr stmt ';'
	| '{' stmts '}'
	| LNAME '=' expr
	| GNAME '=' expr ';'
	| RETURN expr ';'
	| RFALSE ';'
	| RTRUE ';'
	| CALL expr opt_call_args ';'
	| RNAME opt_call_args ';'
	; 
	
opt_else
	:
	| "else" stmt
	;

cond_expr
	: '(' rel_expr ')'
	;

opt_call_args
	:
	| STRLIT
	| '(' arg_list ')'
	;

arg_list
	: arg_list ',' arg
	| arg
	;

arg
	: expr
	;

expr
	: expr '+' expr { $$ = new expr_binary($1,_2op::add,$3); }
	| expr '-' expr { $$ = new expr_binary($1,_2op::sub,$3); }
	| expr '*' expr { $$ = new expr_binary($1,_2op::mul,$3); }
	| expr '/' expr { $$ = new expr_binary($1,_2op::div,$3); }
	| expr '%' expr { $$ = new expr_binary($1,_2op::mod,$3); }
	| '~' expr      { $$ = new expr_unary(_1op::not_,$2); }
	| expr '&' expr { $$ = new expr_binary($1,_2op::and_,$3); }
	| expr '|' expr { $$ = new expr_binary($1,_2op::or_,$3); }
	| '(' expr ')'  { $$ = $2; }
	| primary       { $$ = $1; }
	| INTLIT        { $$ = new expr_literal($1); }
	;

primary
	: aname
	| PNAME			{ $$ = new expr_literal($1); }
	;

aname
	: ANAME			{ $$ = new expr_literal($1); }
	| LNAME			{ $$ = new expr_literal($1 + 1); }
	| GNAME			{ $$ = new expr_literal($1 + 16); }
	;

rel_expr
	: expr '<' expr             { $$ = new expr_binary_branch($1,_2op::jl,false,$3); }
	| expr "<=" expr			{ $$ = new expr_binary_branch($1,_2op::jg,true,$3); }
	| expr '>' expr				{ $$ = new expr_binary_branch($1,_2op::jg,false,$3); }
	| expr ">=" expr			{ $$ = new expr_binary_branch($1,_2op::jl,true,$3); }
	| expr "==" exprs			{ $$ = new expr_binary_branch($1,_2op::je,false,$3); }
	| expr "!=" exprs			{ $$ = new expr_binary_branch($1,_2op::je,true,$3); }
	| NOT rel_expr				{ $$ = $2; /* FIXME */ }
	| rel_expr AND rel_expr		{ $$ = new expr_logical_and($1,$3); }
	| rel_expr OR rel_expr		{ $$ = new expr_logical_or($1,$3); }
	| primary HAS aname			{ $$ = new expr_binary_branch($1,_2op::test_attr,false,$3); }
	| primary HASNT aname		{ $$ = new expr_binary_branch($1,_2op::test_attr,true,$3); }
	| expr /* same as expr != 0 */ { $$ = new expr_unary_branch(_1op::jz,true,$1); }
	;

exprs
	: expr								{ $$ = $1; }
	| INTLIT ',' INTLIT					{ $$ = new expr_literals($1,$3); }
	| INTLIT ',' INTLIT ',' INTLIT		{ $$ = new expr_literals($1,$3,$5); }
	;

%%

int yylex() {
	return INTLIT;
}

void yyerror(const char *msg) {
	fprintf(stderr,"%s\n",msg);
	exit(1);
}