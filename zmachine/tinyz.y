/* tinyz.y */
%{
	#include "opcodes.h"
	#include <set>
	#include <map>

	void open_scope();
	void close_scope();
	int yylex();
	void yyerror(const char*);
	int encode_string(const char*);

	template <typename T> struct list_node {
		list_node<T>(T a,list_node<T> *b) : car(a), cdr(b) { }
		T car;
		list_node<T> *cdr;
	};
	struct symbol {
		const char *name;
		int16_t token;	// if zero, it's a NEWSYM
		union {
			int16_t ival;
			struct object *optr;
		};
	};
	struct property {
		uint8_t index;
		uint8_t size;
		uint8_t payload[8];	// can be up to 8 for v3
	};
	struct object {
		symbol *child, *sibling, *parent;
		uint32_t attributes;
		const char *descr;
		list_node<property> *properties;
	} *cdef;
	struct dict_entry {
		uint16_t encoded[2];		// 6 letters for V3
		uint8_t payload;
	};
	std::set<dict_entry> dictionary;
	// if upper N bits are all zero or all one, it's an ival, else sym
	union pvalue {
		symbol *sym;
		ptrdiff_t ival;
		list_node<dict_entry*> *wval;
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
	struct expr_logical_and: public expr_branch {
		expr_logical_and(expr_branch *left,expr_branch *right);
	};
	struct expr_logical_or: public expr_branch {
		expr_logical_or(expr_branch *left,expr_branch *right);
	};
	struct expr_literals: public expr {
		expr_literals(int a) : count(1) { literals[0] = a; }
		expr_literals(int a,int b) : count(2) { literals[0] = a; literals[1] = b; }
		expr_literals(int a,int b,int c) : count(3) { literals[0] = a; literals[1] = b; literals[2] = c; }
		int16_t count, literals[3];
	};
	struct expr_call: public expr { // first arg is func address
		expr_call(list_node<expr*> *a) : args(a) { }
		list_node<expr*> *args;
	};
	enum scope_enum: uint8_t { SCOPE_GLOBAL, SCOPE_OBJECT, SCOPE_LOCATION };
	uint8_t expected_scope;
	const uint8_t SCOPE_OBJECT_MASK = 0x40;
	const uint8_t SCOPE_LOCATION_MASK = 0x80;
	const uint8_t scope_masks[3] = { SCOPE_OBJECT_MASK | SCOPE_LOCATION_MASK, SCOPE_OBJECT_MASK, SCOPE_LOCATION_MASK };
	uint8_t attribute_next[3] = {31,0,0}; // 31 should be 47 for v4+
	uint8_t property_next[3] = {31,0,0}; // 31 should be 63 for v4+
	uint8_t next_value_in_scope(scope_enum sc,uint8_t *state) {
		uint8_t result = state[sc] | scope_masks[sc];
		if (sc==SCOPE_GLOBAL) state[sc]--; else state[sc]++;
		return result;
	}
	struct stmt {
		virtual void emit();
	};
	struct stmts: public stmt {
		stmts(list_node<stmt*> *s): slist(s) { }
		list_node<stmt*> *slist;
		void emit() {
			for (auto i=slist; i; i=i->cdr)
				i->car->emit();
		}
	};
	struct stmt_flow: public stmt {
	};
	struct stmt_if: public stmt_flow {
		stmt_if(expr_branch *e,stmt *t,stmt *f): cond(e), ifTrue(t), ifFalse(f) { }
		expr_branch *cond;
		stmt *ifTrue, *ifFalse;
	};
	struct stmt_while: public stmt_flow {
		stmt_while(expr_branch *e,stmt *b): cond(e), body(b) { }
		expr_branch *cond;
		stmt *body;
	};
	struct stmt_repeat: public stmt_flow {
		stmt_repeat(stmt *b,expr_branch *e): body(b), cond(e) { }
		stmt *body;
		expr_branch *cond;
	};
	struct stmt_1op: public stmt {
		stmt_1op(_1op op,expr *e) : opcode(op), operand(e) { }
		_1op opcode;
		expr *operand;
	};	
	struct stmt_0op: public stmt {
		stmt_0op(_0op op) : opcode(op) { }
		_0op opcode;
	};
	struct stmt_assign: public stmt {
		stmt_assign(uint8_t d,expr *e) : dest(d), value(e) { }
		uint8_t dest;
		expr* value;
	};
	struct stmt_call: public stmt {
		stmt_call(list_node<expr*> *a) : call(a) { }
		void emit() {
			call.emit();
			// emitOpcode(_op0::pop); // throw out result
		}
		expr_call call;
	};
	struct stmt_print_ret: public stmt {
		stmt_print_ret(const char *s) : string(s) { }
		const char *string;
		void emit() {
			// emitOpcode(_op0::print_ret);
		}
	};
	int16_t emit_routine(int numLocals,stmt *body);
%}

%union {
	int ival;
	const char *sval;
	expr *eval;
	expr_branch *ebval;
	symbol *sym;
	scope_enum scopeval;
	dict_entry *dict;
	list_node<dict_entry*> *dlist;
	list_node<expr*> *elist;
	list_node<int> *ilist;
	pvalue pval;
	list_node<stmt*> *stlist;
	stmt *stval;
}

%token ATTRIBUTE PROPERTY DIRECTION GLOBAL OBJECT LOCATION ROUTINE ARTICLE PLACEHOLDER ACTION HAS HASNT
%token BYTE_ARRAY WORD_ARRAY CALL
%token <dict> DICT
%token <ival> ANAME PNAME LNAME GNAME RNAME INTLIT
%token <sval> STRLIT
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

%type <eval> expr primary exprs aname arg
%type <ebval> rel_expr cond_expr
%type <ival> init routine_body
%type <scopeval> scope
%type <dlist> dict_list;
%type <ilist> init_list;
%type <elist> opt_call_args arg_list
%type <sym> opt_parent
%type <pval> pvalue
%type <stval> stmt opt_else
%type <stlist> stmts

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
	: GLOBAL		{ $$ = SCOPE_GLOBAL; }
	| LOCATION		{ $$ = SCOPE_LOCATION; }
	| OBJECT		{ $$ = SCOPE_OBJECT; }
	;

property_def
	: PROPERTY scope NEWSYM ';' { $3->token = PNAME; $3->ival = next_value_in_scope($2,property_next); }
	;

direction_def
	: DIRECTION PNAME dict_list ';' { 
		if (($2 & SCOPE_LOCATION_MASK) != 0)
			yyerror("direction property must be type location");
		if (($2 & 63) == 0 || ($2 & 63) > 14) 
			yyerror("direction property index must be between 1 and 14");
		for (auto it=$3; it; it=it->cdr)
			if ($3->car->payload & 15) 
				yyerror("dictionary word already has direction bits set");
			else
				$3->car->payload |= ($2 & 15);
	}
	;

dict_list
	: DICT dict_list	{ $$ = new list_node<dict_entry*>($1,$2); }
	| DICT				{ $$ = new list_node<dict_entry*>($1,nullptr); }
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
	: init					{ $$ = new list_node<int>($1,nullptr); }
	| init ',' init_list	{ $$ = new list_node<int>($1,$3); }
	;

init
	: INTLIT			{ $$ = $1; }
	| STRLIT			{ $$ = encode_string($1); }
	;

object_def
	: OBJECT { expected_scope = SCOPE_OBJECT_MASK; } object_or_location_def
	;

location_def
	: LOCATION { expected_scope = SCOPE_LOCATION_MASK; } object_or_location_def
	;

object_or_location_def
	: NEWSYM STRLIT opt_parent '{' { 
		cdef = new object; 
		$1->optr = cdef;
		cdef->child = cdef->sibling = nullptr;
		cdef->parent = $3;
		cdef->descr = $2;
		cdef->attributes = 0;
		cdef->properties = nullptr;
	} property_or_attribute_list '}' ';'
	;

opt_parent
	: 						{ $$ = nullptr; }
	| '(' FWDREF ')'		{ $$ = $2; }
	;

property_or_attribute_list
	: property_or_attribute_list property_or_attribute
	| property_or_attribute
	;

property_or_attribute
	: PNAME ':' pvalue ';'		
			{ 
				if (!($1 & expected_scope))
					yyerror("wrong type of property"); 
				uint8_t thisIndex = $1 & 63;
				for (auto p = cdef->properties; p; p=p->cdr)
					if (p->car.index == thisIndex)
						yyerror("already have this property set");
				list_node<property> **pPrev = &cdef->properties;
				// properties are kept in descending order
				while (*pPrev && (*pPrev)->car.index < thisIndex)
					pPrev = &((*pPrev)->cdr);
				list_node<property> *newNode = new list_node<property>({},*pPrev);
				*pPrev = newNode;
				/* newNode->car.index = thisIndex;
				newNode->car.size =  */
			}
	| ANAME ';'
			{ 
				if (!($1 & expected_scope)) 
					yyerror("wrong type of attribute"); 
				uint32_t mask = 1 << ($1 & 31); // V4+ needs better here
				if (cdef->attributes & mask)
					yyerror("already have this attribute set");
				cdef->attributes |= mask;
			}
	;

pvalue
	: FWDREF { $$.sym = $1; }
	| STRLIT
		{
			// string literal is just a shorthand for the address of a routine that calls print_ret with that string
			$$.ival = emit_routine(0,new stmt_print_ret($1));
		}
	| INTLIT { $$.ival = $1; }
	| routine_body { $$.ival = $1; }
	| dict_list { $$.wval = $1; }
	;

routine_def
	: ROUTINE NEWSYM routine_body { $2->token = RNAME; $2->ival = $3; }
	;

article_def
	: ARTICLE dict_list ';'
		{
			for (auto it=$2; it; it = it->cdr)
				if (it->car->payload & 15)
					yyerror("already an article or direction");
				else
					it->car->payload |= 15;
		}
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
	: stmt stmts	{ $$ = new list_node<stmt*>($1,$2); }
	| stmt			{ $$ = new list_node<stmt*>($1,nullptr); }
	;

stmt
	: "if" cond_expr stmt opt_else ';'		{ $$ = new stmt_if($2,$3,$4); }
	| "repeat" stmt "while" cond_expr ';'	{ $$ = new stmt_repeat($2,$4); }
	| "while" cond_expr stmt ';'			{ $$ = new stmt_while($2,$3); }
	| '{' stmts '}'			{ $$ = new stmts($2); }
	| LNAME '=' expr ';'	{ $$ = new stmt_assign($1+1,$3); }
	| GNAME '=' expr ';'	{ $$ = new stmt_assign($1+16,$3); }
	| RETURN expr ';'		{ $$ = new stmt_1op(_1op::ret,$2); }
	| RFALSE ';'			{ $$ = new stmt_0op(_0op::rfalse); }
	| RTRUE ';'				{ $$ = new stmt_0op(_0op::rtrue); }
	| CALL expr opt_call_args ';'	{ $$ = new stmt_call(new list_node<expr*>($2,$3));  }
	| RNAME opt_call_args ';'		{ $$ = new stmt_call(new list_node<expr*>(new expr_literal($1),$2)); }
	; 
	
opt_else
	:				{ $$ = nullptr; }
	| "else" stmt	{ $$ = $2; }
	;

cond_expr
	: '(' rel_expr ')' { $$ = $2; }
	;

opt_call_args
	:					{ $$ = nullptr; }
	| STRLIT			{ $$ = new list_node<expr*>(new expr_literal(encode_string($1)),nullptr); }
	| '(' arg_list ')'	{ $$ = $2; }
	;

arg_list
	: arg ',' arg_list	{ $$ = new list_node<expr*>($1,$3); }
	| arg				{ $$ = new list_node<expr*>($1,nullptr); }
	;

arg
	: expr				{ $$ = $1; }
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