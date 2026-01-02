/* tinyz.y */
%{
	#include "opcodes.h"
	#include "header.h"
	#include <set>
	#include <map>

	void open_scope();
	void close_scope();
	int yylex();
	void yyerror(const char*,...);
	int encode_string(const char*);
	uint8_t next_global, next_local, story_shift = 1;
	storyHeader the_header;
	struct operand { // 0=long, 1=small, variable=2, omitted=3
		int value:30;
		optype type:2;
	};
	void emitByte(uint8_t b);
	void emitOperand(operand o) {
		if (o.type==optype::large_constant)
			emitByte(o.value >> 8);
		emitByte(o.value);
	}
	void emitBranch(uint16_t target);
	void emit2op(operand l,_2op op,operand r,int dest = -1);
	void emit1op(_1op op,operand un,int dest = -1);
	const uint8_t TOS = 0, SCRATCH = 4;

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
		uint8_t payload[64];	// can be up to 8 for v3, 64 for v4+
	};
	struct object {
		symbol *child, *sibling, *parent;
		uint64_t attributes;
		const char *descr;
		list_node<property> *properties;
	} *cdef;

	struct dict_entry {
		uint16_t encoded[3];		// 6 letters for V3, 9 letters for V4+
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
		virtual void emit(uint8_t dest) { }
		virtual void eval(operand &o) {
			o.value = TOS;
			o.type = optype::variable;
			emit(TOS);
		}
		virtual bool isLogical() { return false; }
	};

	struct expr_binary: public expr {
		expr_binary(expr *l,_2op op,expr *r);
		expr *left, *right;
		_2op opcode;
		void emit(uint8_t dest) {
			operand lval, rval;
			right->eval(rval);
			left->eval(lval);
			emit2op(lval,opcode,rval,dest);
		}
		void eval(operand &o) {
			o.value = TOS;
			o.type = optype::variable;
			emit(TOS);
		}
	};
	struct expr_unary: public expr {
		expr_unary(_1op op,expr *e);
		expr *unary;
		_1op opcode;
		void emit(uint8_t dest) {
			operand uval;
			unary->eval(uval);
			emit1op(opcode,uval,dest);
		}

	};
	struct expr_branch: public expr {
		expr_branch(bool n) : negated(n) { }
		static expr_branch * to_branch(expr *e) {
			if (e->isLogical())
				return static_cast<expr_branch*>(e);
			else {
				yyerror("semantic error, must be boolean expression");
				return nullptr;
			}
		}
		bool negated;
		bool isLogical() { return true; }
	};
	struct expr_binary_branch: public expr_branch {
		expr_binary_branch(expr *l,_2op op,bool negated,expr *r) : left(l), opcode(op), right(r), expr_branch(negated) { }
		_2op opcode;
		expr *left, *right;
	};
	struct expr_unary_branch: public expr_branch {
		expr_unary_branch(_1op op,bool negated,expr *e);
	};
	struct expr_operand: public expr {
		operand op;
		void eval(operand &o) {
			op = o;
		}
	};
	struct expr_literal: public expr_operand {
		expr_literal(int value) {
			op.type =  value >= 0 && value <= 255? optype::small_constant : optype::large_constant;
			op.value = value;
		}
	};
	struct expr_variable: public expr_operand {
		expr_variable(uint8_t v) {
			op.type = optype::variable;
			op.value = v;
		}
	};
	struct expr_logical_not: public expr_branch {
		expr_logical_not(expr *e) : unary(to_branch(e)), expr_branch(!to_branch(e)->negated) { }
		expr_branch *unary;
	};
	struct expr_logical_and: public expr_branch {
		expr_logical_and(expr *l,expr *r) : left(to_branch(l)), right(to_branch(r)), expr_branch(false) { }
		expr_branch *left, *right;
	};
	struct expr_logical_or: public expr_branch {
		expr_logical_or(expr *l,expr *r) : left(to_branch(l)), right(to_branch(r)), expr_branch(false) { }
		expr_branch *left, *right;
	};
	struct expr_grouped: public expr {
		expr_grouped(expr*a,expr *b,expr *c = nullptr);
	};
	struct expr_call: public expr { // first arg is func address
		expr_call(list_node<expr*> *a) : args(a) { }
		list_node<expr*> *args;
	};
	struct expr_save: public expr_branch {
		expr_save() : expr_branch(false) { }
	};
	struct expr_restore: public expr_branch {
		expr_restore(): expr_branch(false) { }
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
		stmt_if(expr *e,stmt *t,stmt *f): cond(e), ifTrue(t), ifFalse(f) { }
		expr *cond;
		stmt *ifTrue, *ifFalse;
	};
	struct stmt_while: public stmt_flow {
		stmt_while(expr *e,stmt *b): cond(e), body(b) { }
		expr *cond;
		stmt *body;
	};
	struct stmt_repeat: public stmt_flow {
		stmt_repeat(stmt *b,expr *e): body(b), cond(e) { }
		stmt *body;
		expr *cond;
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
		void emit() {
			value->emit(dest);
		}
	};
	struct stmt_call: public stmt {
		stmt_call(list_node<expr*> *a) : call(a) { }
		void emit() {
			call.emit(SCRATCH);
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
%token WHILE REPEAT IF ELSE
%token LE "<=" GE ">=" EQ "==" NE "!="
%token DEC_CHK "--<" INC_CHK "++>"
%token GET_SIBLING GET_CHILD SAVE RESTORE
// %token LSH "<<" RSH ">>"
%token RFALSE RTRUE RETURN
%token OR AND NOT

%right '~' NOT
%left '*' '/' '%'
%left '+' '-'
%nonassoc HAS HASNT
%left '<' LE '>' GE
%left EQ NE
// %left LSH RSH
%left '&'
%left '|'
%left AND
%left OR

%type <eval> expr primary aname arg cond_expr
%type <ival> init routine_body vname
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
		{ 
			if (next_global==240)
				yyerror("too many globals");
			$2->token = GNAME; 
			$2->ival = next_global++;
		}
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
				uint64_t mask = 1ULL << ($1 & 63);
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
	: param params_list
	| param
	;

param
	:  NEWSYM 
		{ 
			if (next_local==3) 
				yyerror("too many params (limit is 3 for v3)"); 
			$1->token = LNAME; 
			$1->ival = next_local++; 
		}
	;

opt_locals_list
	: 
	| ';' locals_list
	;

locals_list
	: local locals_list
	| local
	;

local
	: NEWSYM
		{ 
			if (next_local==15) 
				yyerror("too many params + locals"); 
			$1->token = LNAME; 
			$1->ival = next_local++; 
		}
	;

stmts
	: stmt stmts	{ $$ = new list_node<stmt*>($1,$2); }
	| stmt			{ $$ = new list_node<stmt*>($1,nullptr); }
	;

stmt
	: IF cond_expr stmt opt_else ';'	{ $$ = new stmt_if($2,$3,$4); }
	| REPEAT stmt WHILE cond_expr ';'	{ $$ = new stmt_repeat($2,$4); }
	| WHILE cond_expr stmt ';'			{ $$ = new stmt_while($2,$3); }
	| '{' stmts '}'			{ $$ = new stmts($2); }
	| vname '=' expr ';'	{ $$ = new stmt_assign($1,$3); }
	| RETURN expr ';'		{ $$ = new stmt_1op(_1op::ret,$2); }
	| RFALSE ';'			{ $$ = new stmt_0op(_0op::rfalse); }
	| RTRUE ';'				{ $$ = new stmt_0op(_0op::rtrue); }
	| CALL expr opt_call_args ';'	{ $$ = new stmt_call(new list_node<expr*>($2,$3));  }
	| RNAME opt_call_args ';'		{ $$ = new stmt_call(new list_node<expr*>(new expr_literal($1),$2)); }
	; 
	
opt_else
	:				{ $$ = nullptr; }
	| ELSE stmt	{ $$ = $2; }
	;

cond_expr
	: '(' expr ')' { $$ = $2; }
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
	: expr '+' expr 	{ $$ = new expr_binary($1,_2op::add,$3); }
	| expr '-' expr 	{ $$ = new expr_binary($1,_2op::sub,$3); }
	| expr '*' expr 	{ $$ = new expr_binary($1,_2op::mul,$3); }
	| expr '/' expr 	{ $$ = new expr_binary($1,_2op::div,$3); }
	| expr '%' expr 	{ $$ = new expr_binary($1,_2op::mod,$3); }
	| '~' expr      	{ $$ = new expr_unary(_1op::not_,$2); }
	| expr '&' expr 	{ $$ = new expr_binary($1,_2op::and_,$3); }
	| expr '|' expr 	{ $$ = new expr_binary($1,_2op::or_,$3); }
	| '(' expr ')'  	{ $$ = $2; }
	| primary       	{ $$ = $1; }
	| INTLIT        	{ $$ = new expr_literal($1); }
	| expr '<' expr		{ $$ = new expr_binary_branch($1,_2op::jl,false,$3); }
	| expr LE expr		{ $$ = new expr_binary_branch($1,_2op::jg,true,$3); }
	| expr '>' expr		{ $$ = new expr_binary_branch($1,_2op::jg,false,$3); }
	| expr GE expr		{ $$ = new expr_binary_branch($1,_2op::jl,true,$3); }
	| expr EQ expr		{ $$ = new expr_binary_branch($1,_2op::je,false,$3); }
	| expr NE expr		{ $$ = new expr_binary_branch($1,_2op::je,true,$3); }
	| NOT expr			{ $$ = new expr_logical_not($2); }
	| expr AND expr		{ $$ = new expr_logical_and($1,$3); }
	| expr OR expr		{ $$ = new expr_logical_or($1,$3); }
	| expr HAS aname	{ $$ = new expr_binary_branch($1,_2op::test_attr,false,$3); }
	| expr HASNT aname	{ $$ = new expr_binary_branch($1,_2op::test_attr,true,$3); }
	| SAVE				{ $$ = new expr_save(); }
	| RESTORE			{ $$ = new expr_restore(); }
	| '{' expr ',' expr '}'				{ $$ = new expr_grouped($2,$4); }
	| '{' expr ',' expr ',' expr '}'	{ $$ = new expr_grouped($2,$4,$6); }
	;

primary
	: aname			{ $$ = $1; }
	| PNAME			{ $$ = new expr_literal($1); }
	;

aname
	: ANAME			{ $$ = new expr_literal($1); }
	| vname			{ $$ = new expr_variable($1); }
	;

vname
	: LNAME			{ $$ = $1 + 1; }
	| GNAME			{ $$ = $1 + 16; }
	;

%%

std::map<const char*,int16_t> rw;
std::map<const char*,_0op> f_0op;
std::map<const char*,_1op> f_1op;


void init() {
	rw["attribute"] = ATTRIBUTE;
	rw["property"] = PROPERTY;
	rw["direction"] = DIRECTION;
	rw["global"] = GLOBAL;
	rw["object"] = OBJECT;
	rw["location"] = LOCATION;
	rw["routine"] = ROUTINE;
	rw["article"] = ARTICLE;
	rw["placeholder"] = PLACEHOLDER;
	rw["action"] = ACTION;
	rw["has"] = HAS;
	rw["hasnt"] = HASNT;
	rw["byte_array"] = BYTE_ARRAY;
	rw["word_array"] = WORD_ARRAY;
	rw["call"] = CALL;
	rw["while"] = WHILE;
	rw["repeat"] = REPEAT;
	rw["if"] = IF;
	rw["else"] = ELSE;
	rw["return"] = RETURN;
	rw["or"] = OR;
	rw["and"] = AND;
	rw["not"] = NOT;
	rw["save"] = SAVE;
	rw["restore"] = RESTORE;
	rw["get_sibling"] = GET_SIBLING;
	rw["get_child"] = GET_CHILD;

	f_0op["rfalse"] = _0op::rfalse;
	f_0op["rtrue"] = _0op::rtrue;
	f_0op["restart"] = _0op::restart;
	f_0op["quit"] = _0op::quit;
	f_0op["crlf"] = _0op::new_line;
	f_0op["show_status"] = _0op::show_status;

	f_1op["get_parent"] = _1op::get_parent;
	f_1op["print_addr"] = _1op::print_addr;
	f_1op["print_paddr"] = _1op::print_paddr;
	f_1op["remove_obj"] = _1op::remove_obj;
	f_1op["print_obj"] = _1op::print_obj;
}

void set_version(int version) {
	if (version != 8 && (version < 3 || version > 5))
		yyerror("only versions 3,4,5,8 supported");
	attribute_next[0] = version>3? 47 : 31;
	property_next[0] = version>3? 63 : 31;
	story_shift = version==8? 3 : version==3? 1 : 2;
	the_header.version = version;
}

void emit1op(_1op opcode,operand uval,int dest) {
	if (uval.type==optype::large_constant)
		emitByte(0x80 + (uint8_t)opcode);
	else if (uval.type==optype::small_constant)
		emitByte(0x90 + (uint8_t)opcode);
	else
		emitByte(0xA0 + (uint8_t)opcode);
	emitOperand(uval);
	if (dest != -1)
		emitByte(dest);
}

void emit2op(operand lval,_2op opcode,operand rval,int dest) {
	if (lval.type==optype::small_constant && rval.type==optype::small_constant)
		emitByte((uint8_t)opcode + 0x00);
	else if (lval.type==optype::small_constant && rval.type==optype::variable)
		emitByte((uint8_t)opcode + 0x20);
	else if (lval.type==optype::variable && rval.type==optype::small_constant)
		emitByte((uint8_t)opcode + 0x40);
	else if (lval.type==optype::variable && rval.type==optype::variable)
		emitByte((uint8_t)opcode + 0x60);
	else {
		emitByte((uint8_t)opcode + 0xC0);
		emitByte(((uint8_t)lval.type << 6) | ((uint8_t)rval.type << 4) | 0xF);
	}
	emitOperand(lval);
	emitOperand(rval);
	if (dest != -1)
		emitByte(dest);
}

int yylex() {
	return INTLIT;
}

void yyerror(const char *fmt,...) {
	va_list args;
	va_start(args,fmt);
	vfprintf(stderr,fmt,args);
	putc('\n',stderr);
	va_end(args);
	exit(1);
}