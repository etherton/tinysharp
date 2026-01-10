/* tinyz.y */
/* bison tinyz.y -o tinyz.tab.cpp && clang++ -std=c++17 tinyz.tab.cpp */

%{
	#include "opcodes.h"
	#include "header.h"
	#include <set>
	#include <map>
	#include <cassert>

	int yylex();
	void yyerror(const char*,...);
	uint16_t encode_string(uint8_t *dest,size_t destSize,const char *src,size_t srcSize);
	int encode_string(const char*);
	uint8_t next_global, next_local, story_shift = 1, dict_entry_size = 4;
	storyHeader the_header;
	struct operand { // 0=long, 1=small, variable=2, omitted=3
		int value:30;
		optype type:2;
	};

	static_assert(sizeof(operand)==4);
	static const uint8_t opsizes[3] = { 2,1,1 };
	const uint8_t LONG_JUMP = 0x8C;			// +/-32767
	const uint8_t SHORT_JUMP = 0x9C;		// 0-255
	// first byte is the total size of the property
	// second byte encodes the property index and size depending on version.
	static uint8_t *property_blob(uint8_t size) {
		if (!size)
			yyerror("cannot have zero-sized property");
		if (the_header.version == 3) {
			if (size > 8)
				yyerror("property too large for v3");
			uint8_t *pval = new uint8_t[size+2];
			pval[0] = size+1;
			pval[1] = (size-1) << 5;
			return pval;
		}
		else {
			if (size > 64)
				yyerror("property too large for v4+");
			if (size <= 2) {
				uint8_t *pval = new uint8_t[size+2];
				pval[0] = size+1;
				pval[1] = size==1? 0 : 64;
				return pval;
			}
			else {
				uint8_t *pval = new uint8_t[size+3];
				pval[0] = size+1;
				pval[1] = 0x80;
				pval[2] = 0x80 | (size & 63);
				return pval;
			}
		}
	}
	void property_set_index(uint8_t *p,uint8_t idx) {
		assert(idx && idx < (the_header.version==3? 32 : 64));
		p[1] |= idx;
	}
	uint8_t property_get_index(uint8_t *p) {
		return the_header.version==3? p[1] & 31 : p[1] & 63;
	}
	static uint8_t *property_int(int v) {
		if (v>=0 && v<=255) {
			uint8_t *p = property_blob(1);
			p[2] = v;
			return p;
		}
		else {
			uint8_t *p = property_blob(2);
			p[2] = v >> 8;
			p[3] = v;
			return p;
		}
	}
	uint8_t *currentRoutine;
	uint16_t pc;
	void emitByte(uint8_t b) {
		currentRoutine[pc++] = b;
	}

	void emitOperand(operand o) {
		if (o.type==optype::large_constant)
			emitByte(o.value >> 8);
		emitByte(o.value);
	}
	// void emitBranch(uint16_t target);
	void emit2op(operand l,_2op op,operand r);
	void emit1op(_1op op,operand un);
	const uint8_t TOS = 0, SCRATCH = 4;

	template <typename T> struct list_node {
		list_node<T>(T a,list_node<T> *b) : car(a), cdr(b) { }
		T car;
		list_node<T> *cdr;
		size_t size() const {
			return 1 + cdr->size();
		}
	};


	typedef struct label_info {
		uint16_t offset;
		list_node<uint16_t> *references;
	} *label;
	label createLabel() {
		label result = new label_info;
		result->offset = 0xFFFF;
		result->references = nullptr;
		return result;
	}
	void placeLabel(label l) {
		l->offset = pc;
		for (auto i=l->references; i; i=i->cdr) {
			int16_t delta = (i->car - pc + 2);
			if (currentRoutine[i->car])
				currentRoutine[i->car] |= (delta >> 8) & 63;
			else
				currentRoutine[i->car] = (delta >> 8);
			currentRoutine[i->car + 1] = delta;
		}
	}
	void emitJump(label l) {
		emitByte(LONG_JUMP);
		if (l->offset != 0xFFFF) {
			int16_t delta = l->offset - pc + 2;
			emitByte(delta >> 8);
			emitByte(delta);
		}
		else {
			l->references = new list_node<uint16_t>(pc,l->references);
			emitByte(0);
			emitByte(0);
		}
	}
	label createLabelHere() {
		auto l = createLabel();
		placeLabel(l);
		return l;
	}

	struct symbol {
		int16_t token;	// if zero, it's a NEWSYM
		union {
			int16_t ival;
			uint16_t uval;
		};
	};
	std::map<std::string,symbol> the_globals;
	std::vector<std::map<std::string,uint8_t>> the_locals;
	void open_scope() { the_locals.emplace_back(); }
	void close_scope() { the_locals.pop_back(); }

	struct object {
		int16_t child, sibling, parent;
		uint8_t attributes[6];
		uint16_t descrLen;
		uint8_t *descr;
		uint8_t **properties;
	} *cdef;
	std::vector<object*> the_object_table;
	struct dict_entry {
		uint8_t encoded[6];
		bool operator <(const dict_entry &rhs) const {
			return memcmp(encoded,rhs.encoded,sizeof(encoded)) < 0;
		}
	};
	std::map<dict_entry,uint16_t> the_dictionary; // maps a dictionary word to its index
	uint8_t *z_dict;		// 5/7 bytes per entry
	uint8_t& z_dict_payload(uint16_t i) { return z_dict[i * (dict_entry_size+1) + dict_entry_size]; }

	struct expr {
		virtual void emit(uint8_t dest) { }
		virtual void eval(operand &o) {
			o.value = TOS;
			o.type = optype::variable;
			emit(TOS);
		}
		virtual bool isLogical() const { return false; }
		virtual bool isConstant(int &c) const { return false; }
		virtual unsigned size() const { return 0; } // TODO = 0
		static expr *fold_constant(expr* e);
	};

	struct expr_binary: public expr {
		expr_binary(expr *l,_2op op,expr *r) : left(l), opcode(op), right(r) { }
		expr *left, *right;
		_2op opcode;
		void emit(uint8_t dest) {
			// we defer eval call because there may be unsigned forward references
			operand lval, rval;
			right->eval(rval);
			left->eval(lval);
			emit2op(lval,opcode,rval);
			emitByte(dest);
		}
		void eval(operand &o) {
			o.value = TOS;
			o.type = optype::variable;
			emit(TOS);
		}
		unsigned size() const {
			// TODO: need to rewind pc after eval
			operand lval, rval;
			right->eval(rval);
			left->eval(lval);
			// if either is large_constant then it uses VAR form and needs a type byte
			if (rval.type==optype::large_constant||lval.type==optype::large_constant)
				return 2 + opsizes[(uint8_t)rval.type] + opsizes[(uint8_t)lval.type];
			else	// if neither is large_constant then size must be 3
				return 3;
		}
	};
	#define IMPL_EXPR_BINARY(zop,cop) struct expr_binary_##zop: public expr_binary { \
		expr_binary_##zop(expr *l,expr *r) : expr_binary(l,_2op::zop,r) { } \
		bool isConstant(int &v) const { int l,r; \
		if (left->isConstant(l)&&right->isConstant(r)) { v = l cop r; return true; } else return false; } }
	IMPL_EXPR_BINARY(add,+);
	IMPL_EXPR_BINARY(sub,-);
	IMPL_EXPR_BINARY(mul,*);
	IMPL_EXPR_BINARY(div,/);
	IMPL_EXPR_BINARY(mod,%);
	IMPL_EXPR_BINARY(and_,&);
	IMPL_EXPR_BINARY(or_,|);
	struct expr_unary: public expr {
		expr_unary(_1op op,expr *e) : opcode(op), unary(e) { } 
		expr *unary;
		_1op opcode;
		void emit(uint8_t dest) {
			operand uval;
			unary->eval(uval);
			emit1op(opcode,uval);
			emitByte(dest);
		}
		unsigned size() const {
			operand uval;
			unary->eval(uval);
			return 1 + opsizes[(uint8_t)uval.type];
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
		void emit() {
			// shouldn't be called.
		}
		virtual void emitBranch(label target,bool n) {
			if (negated)
				n = !n;
			if (target->offset != 0xFFFF) {
				int16_t delta = target->offset - pc + 2;
				emitByte((n? 0xC0 : 0x040) | ((delta >> 8) & 63));
				emitByte(delta);
			}
			else {
				emitByte(n? 0xC0 : 0x40);
				emitByte(0);
				target->references = new list_node<uint16_t>(pc,target->references);
			}
		}
		bool negated;
		bool isLogical() { return true; }
	};
	struct expr_binary_branch: public expr_branch {
		expr_binary_branch(expr *l,_2op op,bool negated,expr *r) : left(l), opcode(op), right(r), expr_branch(negated) { }
		_2op opcode;
		expr *left, *right;
		void emitBranch(label target,bool negated) {
			operand lval, rval;
			right->eval(rval);
			left->eval(lval);
			emit2op(lval,opcode,rval);
			expr_branch::emitBranch(target,negated);
		}
	};
	struct expr_unary_branch: public expr_branch {
		expr_unary_branch(_1op op,bool negated,expr *e) : opcode(op), unary(e), expr_branch(negated) { }
		_1op opcode;
		expr *unary;
		void emitBranch(label target,bool negated) {
			operand un;
			unary->eval(un);
			emit1op(opcode,un);
			expr_branch::emitBranch(target,negated);
		}
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
		bool isConstant(int &v) const { v = op.value; return true; }
	};
	expr* expr::fold_constant(expr *e) {
			int c;
			if (e->isConstant(c)) {
				delete e;
				return new expr_literal(c);
			}
			else
				return e;
	}
	struct expr_variable: public expr_operand {
		expr_variable(uint8_t v) {
			op.type = optype::variable;
			op.value = v;
		}
	};
	struct expr_logical_not: public expr_branch {
		expr_logical_not(expr *e) : unary(to_branch(e)), expr_branch(!to_branch(e)->negated) { }
		expr_branch *unary;
		void emitBranch(label target,bool negated) {
			unary->emitBranch(target,negated);
		}
	};
	// (a and b) or (c and d) { trueStuff; } else { falseStuff; }
	// jz a,label1
	// jnz b,ifTrue
	// label1: jz c,ifFalse
	// jz d,ifFalse
	// ifTrue: trueStuff
	// jump after
	// ifFalse: falseStuff
	// after:
	// not (a and b) -> (not a) OR (not b)
	struct expr_logical_and: public expr_branch {
		expr_logical_and(expr *l,expr *r) : left(to_branch(l)), right(to_branch(r)), expr_branch(false) { }
		expr_branch *left, *right;
		void emitBranch(label target,bool negated) {
			// (negated=true) if (a and b) means jz a,target; jz b,target
			// (negated=true) while (a and b) means jz a,target; jz b,target
			// (negated=false) repeat ... while (a and b) means jz skip; jnz b,target; skip:
			if (negated) {	// not (a and b) -> (not a) or (not b)
				label trueBranch = createLabel();
				left->emitBranch(trueBranch,true);
				right->emitBranch(target,false);
				placeLabel(trueBranch);
			}
			else {
				left->emitBranch(target,false);
				right->emitBranch(target,false);
			}
		}
	};
	struct expr_logical_or: public expr_branch {
		expr_logical_or(expr *l,expr *r) : left(to_branch(l)), right(to_branch(r)), expr_branch(false) { }
		expr_branch *left, *right;
		void emitBranch(label target,bool negated) {
			// if (a or b) means jnz a,skip; jz b,target; skip:
			if (negated) { // not (a or b) -> (not a) and (not b)
				left->emitBranch(target,true);
				right->emitBranch(target,true);
			}
			else {
				label trueBranch = createLabel();
				left->emitBranch(trueBranch,false);
				right->emitBranch(target,false);
				placeLabel(trueBranch);
			}
		}
	};
	struct expr_grouped: public expr {
		expr_grouped(expr*a,expr *b,expr *c = nullptr) { exprs[0] = a; exprs[1] = b; exprs[2] = c; }
		expr *exprs[3];
	};
	struct expr_call: public expr { // first arg is func address
		expr_call(list_node<expr*> *a) : args(a) { }
		list_node<expr*> *args;
		// TODO: v3 only supports VAR call (3 params). v4 supports 1/2 operand with result and 7 params.
		// v5 supports implicit pop versions of all calls
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
		virtual void emit() = 0;
		virtual unsigned size() const = 0;
		virtual bool isReturn() const { return false; }
	};
	struct stmts: public stmt {
		stmts(list_node<stmt*> *s): slist(s) { 
			tsize = 0;
			for (auto i=slist; i; i=i->cdr)
				tsize += i->car->size();
		}
		list_node<stmt*> *slist;
		unsigned tsize;
		void emit() {
			for (auto i=slist; i; i=i->cdr)
				i->car->emit();
		}
		unsigned size() const { return tsize; }
		bool isReturn() const {
			for (auto i=slist; i; i=i->cdr)
				if (i->car->isReturn())
					return true;
			return false;
		}
	};
	struct stmt_flow: public stmt {
	};
	struct stmt_if: public stmt_flow {
		stmt_if(expr *e,stmt *t,stmt *f): cond(expr_binary_branch::to_branch(e)), ifTrue(t), ifFalse(f) { }
		expr_branch *cond;
		stmt *ifTrue, *ifFalse;
		// TODO: if ifTrue is rfalse/rtrue, we just need the non-negated branch to 0/1
		// TODO: else if ifFalse is rfalse/rtrue, we just need the negated branch to 0/1
		// TODO: If ifTrue ends in a return, we don't need the jump past false block
		void emit() {
			label falseBranch = createLabel();
			cond->emitBranch(falseBranch,true);
			ifTrue->emit();
			if (ifFalse) {
				label skipFalse = createLabel();
				emitJump(skipFalse);
				placeLabel(falseBranch);
				ifFalse->emit();
				placeLabel(skipFalse);
			}
			else
				placeLabel(falseBranch);
		}
		unsigned size() const {
			bool shortBranch = (ifTrue->size() < (ifFalse || ifTrue->isReturn()? 58 : 61));
			return cond->size() + (shortBranch? 1 : 2) + ifTrue->size() + (ifFalse? 3 + ifFalse->size() : 0);
		}
	};
	struct stmt_while: public stmt_flow {
		stmt_while(expr *e,stmt *b): cond(new expr_logical_not(expr_branch::to_branch(e))), body(b) { }
		expr_branch *cond;
		stmt *body;
		void emit() {
			label falseBranch = createLabel(), top = createLabelHere();
			cond->emitBranch(falseBranch,true);
			// TODO: continue and break via a stack
			body->emit();
			emitJump(top);
			placeLabel(falseBranch);
		}
		unsigned size() const { 
			return cond->size() + body->size() + 3; 
		}
	};
	struct stmt_repeat: public stmt_flow {
		stmt_repeat(stmt *b,expr *e): body(b), cond(expr_branch::to_branch(e)) { }
		stmt *body;
		expr_branch *cond;
		void emit() {
			auto trueBranch = createLabelHere();
			body->emit();
			cond->emitBranch(trueBranch,false);
		}
		unsigned size() const { 
			return cond->size() + body->size(); 
		}
	};
	struct stmt_return: public stmt {
		stmt_return(expr *e) : value(e) { }
		expr *value;
		bool isReturn() const { return true; }
		void emit() {
			int c;
			if (value->isConstant(c) && (c==0||c==1))
				emitByte((uint8_t)(c==0? _0op::rfalse : _0op::rtrue));
			else {
				operand o;
				value->eval(o);
				if (o.type == optype::variable && o.value == TOS)
					emitByte((uint8_t)_0op::ret_popped);
				else
					emit1op(_1op::ret,o);
			}
		}
		unsigned size() const { 
			int c;
			if (value->isConstant(c)) {
				if (c==0||c==1)
					return 1;
				else if (c > 1 && c <= 255)
					return 2;
			}
			return 3;
		}
	};
	struct stmt_1op: public stmt {
		stmt_1op(_1op op,expr *e) : opcode(op), value(e) { }
		_1op opcode;
		expr *value;
		void emit() {
			operand o;
			value->eval(o);
			emit1op(opcode,o);
		}
		unsigned size() const {
			return value->size() + 1;
		}
	};	
	struct stmt_0op: public stmt {
		stmt_0op(_0op op) : opcode(op) { }
		_0op opcode;
		void emit() {
			emitByte((uint8_t)opcode);
		}
		unsigned size() const {
			return 1;
		}
	};
	struct stmt_assign: public stmt {
		stmt_assign(uint8_t d,expr *e) : dest(d), value(e) { }
		uint8_t dest;
		expr* value;
		void emit() {
			value->emit(dest);
		}
		unsigned size() const {
			return value->size() + 1;
		}
	};
	struct stmt_call: public stmt {
		stmt_call(list_node<expr*> *a) : call(a) { }
		void emit() {
			// Call as a statement dumps result to a global
			// (alternative is dump to TOS and emit a pop, but this is shorter)
			call.emit(SCRATCH);
		}
		unsigned size() const {
			return call.size();
		}
		expr_call call;
	};
	struct stmt_print_ret: public stmt {
		stmt_print_ret(const char *s) : string(s) { }
		const char *string;
		void emit() {
			emitByte((uint8_t)_0op::print_ret);
			pc += encode_string(currentRoutine + pc,1024-pc,string,strlen(string));
		}
		unsigned size() const {
			return 1 + encode_string(nullptr,1024-pc,string,strlen(string));
		}
	};
	int16_t emit_routine(int numLocals,stmt *body) {
		currentRoutine = new uint8_t[1024];
		pc = 0;
		currentRoutine[0] = numLocals;
		if (the_header.version < 5) {
			while (numLocals--) { 
				emitByte(0); 
				emitByte(0); 
			}
		}
		body->emit();
		return 0;
	}
%}

%union {
	int ival;
	const char *sval;
	uint8_t *pval;
	expr *eval;
	symbol *sym;
	scope_enum scopeval;
	list_node<uint16_t> *dlist;
	list_node<expr*> *elist;
	list_node<int> *ilist;
	list_node<stmt*> *stlist;
	stmt *stval;
	_0op zeroOp;
	_1op oneOp;
}

%token ATTRIBUTE PROPERTY DIRECTION GLOBAL OBJECT LOCATION ROUTINE ARTICLE PLACEHOLDER ACTION HAS HASNT
%token BYTE_ARRAY WORD_ARRAY CALL
%token <ival> DICT ANAME PNAME LNAME GNAME RNAME INTLIT ONAME
%token <sval> STRLIT
%token <sym> NEWSYM
%token WHILE REPEAT IF ELSE
%token LE "<=" GE ">=" EQ "==" NE "!="
// %token DEC_CHK "--<" INC_CHK "++>"
%token GET_SIBLING GET_CHILD GET_PARENT SAVE RESTORE
%token LSH "<<" RSH ">>"
%token ARROW "->"
%token RFALSE RTRUE RETURN
%token OR AND NOT
%token <zeroOp> STMT_0OP
%token <oneOp> STMT_1OP

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
%type <ival> init routine_body vname opt_parent
%type <scopeval> scope
%type <dlist> dict_list;
%type <ilist> init_list;
%type <elist> opt_call_args arg_list
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
		for (auto it=$3; it; it=it->cdr) {
			uint8_t &payload = z_dict_payload($3->car);
			if (payload & 15) 
				yyerror("dictionary word already has direction bits set");
			else
				payload |= ($2 & 15);
		}
	}
	;

dict_list
	: DICT dict_list	{ $$ = new list_node<uint16_t>($1,$2); }
	| DICT				{ $$ = new list_node<uint16_t>($1,nullptr); }
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
	: 
	ONAME STRLIT opt_parent '{' {
		cdef = the_object_table[$1];
		cdef->child = cdef->sibling = 0;
		cdef->parent = $3;
		cdef->descrLen = encode_string(nullptr,0,$2,strlen($2));
		cdef->descr = new uint8_t[cdef->descrLen];
		encode_string(cdef->descr,cdef->descrLen,$2,strlen($2));
		memset(cdef->attributes,0,sizeof(cdef->attributes));
		unsigned propCount = the_header.version==3? 32 : 64;
		cdef->properties = new uint8_t*[propCount];
		memset(cdef->properties,0,propCount * sizeof(uint8_t*));
	} property_or_attribute_list '}' ';'
	;

opt_parent
	: 						{ $$ = 0; }
	| '(' ONAME ')'			{ $$ = $2; }
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
				if (cdef->properties[thisIndex])
					yyerror("already have this property set");
				cdef->properties[thisIndex] = $3;
				property_set_index($3,thisIndex);
			}
	| ANAME ';'
			{ 
				if (!($1 & expected_scope)) 
					yyerror("wrong type of attribute"); 
				if (cdef->attributes[$1>>3] & (0x80 >> ($1 & 7)))
					yyerror("already have this attribute set");
				cdef->attributes[$1>>3] |= (0x80 >> ($1 & 7));
			}
	;

pvalue
	: ONAME { $$ = property_int($1); }
	| STRLIT
		{
			// string literal is just a shorthand for the address of a routine that calls print_ret with that string
			$$ = property_int(emit_routine(0,new stmt_print_ret($1)));
		}
	| INTLIT { $$ = property_int($1); }
	| routine_body { $$ = property_int($1); }
	| dict_list { 
		uint8_t *p = ($$ = property_blob($1->size() * 2)) + 2;
		auto s = $1;
		while (s) {
			*p++ = s->car >> 8;
			*p++ = s->car;
			s = s->cdr;
		}
	}
	;

routine_def
	: ROUTINE NEWSYM routine_body { $2->token = RNAME; $2->ival = $3; }
	;

article_def
	: ARTICLE dict_list ';'
		{
			for (auto it=$2; it; it = it->cdr) {
				auto &payload = z_dict_payload(it->car);
				if (payload & 15)
					yyerror("already an article or direction");
				else
					payload |= 15;
			}
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
	: stmt stmts	{ if ($1->isReturn() && $2) yyerror("unreachable code"); $$ = new list_node<stmt*>($1,$2); }
	| stmt			{ $$ = new list_node<stmt*>($1,nullptr); }
	;

stmt
	: IF cond_expr stmt opt_else ';'	{ $$ = new stmt_if($2,$3,$4); }
	| REPEAT stmt WHILE cond_expr ';'	{ $$ = new stmt_repeat($2,$4); }
	| WHILE cond_expr stmt ';'			{ $$ = new stmt_while($2,$3); }
	| '{' stmts '}'			{ $$ = new stmts($2); }
	| vname '=' expr ';'	{ $$ = new stmt_assign($1,expr::fold_constant($3)); }
	| RETURN expr ';'		{ $$ = new stmt_return(expr::fold_constant($2)); }
	| RFALSE ';'			{ $$ = new stmt_return(new expr_literal(0)); }
	| RTRUE ';'				{ $$ = new stmt_return(new expr_literal(1)); }
	| CALL expr opt_call_args ';'	{ $$ = new stmt_call(new list_node<expr*>($2,$3));  }
	| RNAME opt_call_args ';'		{ $$ = new stmt_call(new list_node<expr*>(new expr_literal($1),$2)); }
	| STMT_0OP ';'					{ $$ = new stmt_0op($1); }
	| STMT_1OP '(' expr ')' ';'		{ $$ = new stmt_1op($1,$3); }
	; 
	
opt_else
	:			{ $$ = nullptr; }
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
	: expr '+' expr 	{ $$ = new expr_binary_add($1,$3); }
	| expr '-' expr 	{ $$ = new expr_binary_sub($1,$3); }
	| expr '*' expr 	{ $$ = new expr_binary_mul($1,$3); }
	| expr '/' expr 	{ $$ = new expr_binary_div($1,$3); }
	| expr '%' expr 	{ $$ = new expr_binary_mod($1,$3); }
	| '~' expr      	{ $$ = new expr_unary(_1op::not_,$2); }
	| expr '&' expr 	{ $$ = new expr_binary_and_($1,$3); }
	| expr '|' expr 	{ $$ = new expr_binary_or_($1,$3); }
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

static uint8_t s_EncodedCharacters[256];

uint16_t encode_string(uint8_t *dest,size_t destSize,const char *src,size_t srcSize) {
	uint16_t offset = 0, step = 0;
	assert((destSize & 1) == 0);
	auto storeCode = [&](uint8_t code) {
		assert(code<32);
		if (step==0) {
			step = 1;
			if (dest)
				 dest[offset] = code << 2;
		}
		else if (step==1) {
			step = 2;
			if (dest) {
				dest[offset] |= code >> 3;
				dest[offset+1] = code << 5;
			}
		}
		else {	/* step==2 */
			step = 0;
			if (dest)
				dest[offset+1] |= code;
			offset += 2;
		}
	};
	while (srcSize-- && (!destSize || offset < destSize)) {
		uint8_t code = s_EncodedCharacters[*src++];
		if (code == 255) {
			storeCode(5);
			storeCode(6);
			storeCode(src[-1]>>5);
			storeCode(src[-1]&31);
		}
		else {
			if (code > 31)
				storeCode(code >> 5);
			storeCode(code & 31);
		}
	}
	// pad with shift characters
	if (step) {
		storeCode(5);
		if (step)
			storeCode(5);
	}
	if (dest)
		dest[offset-2] |= 0x80; // mark end of string
	return offset;
}

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
	rw["get_parent"] = GET_PARENT;

	f_0op["restart"] = _0op::restart;
	f_0op["quit"] = _0op::quit;
	f_0op["crlf"] = _0op::new_line;
	f_0op["show_status"] = _0op::show_status;

	// f_1op["get_parent"] = _1op::get_parent;
	f_1op["print_addr"] = _1op::print_addr;
	f_1op["print_paddr"] = _1op::print_paddr;
	f_1op["remove_obj"] = _1op::remove_obj;
	f_1op["print_obj"] = _1op::print_obj;

	// build the forward mapping
	const char *alphabet = DEFAULT_ZSCII_ALPHABET;
	memset(s_EncodedCharacters,0xFF,sizeof(s_EncodedCharacters));
	for (uint32_t i=0; i<26; i++) {
		s_EncodedCharacters[alphabet[i]] = (i + 6);
		s_EncodedCharacters[alphabet[i+26]] = (4<<5) | (i + 6);
	}
	for (uint32_t i=2; i<26; i++)
		s_EncodedCharacters[alphabet[i+52]] = (5<<5) | (i + 6);
	s_EncodedCharacters[32] = 0;
	s_EncodedCharacters[10] = (5 << 5) | 7;
	// 1,2,3=abbreviations, 4=shift1, 5=shift2

	the_object_table.push_back(nullptr);	// object zero doesn't exist
}

int encode_string(const char *src) {
	size_t srcLen = strlen(src);
	uint16_t bytes = encode_string(nullptr,0,src,srcLen);
	uint8_t *dest = new uint8_t[bytes];
	encode_string(dest,0,src,srcLen);
	return 0; // TODO
}

void set_version(int version) {
	if (version != 8 && (version < 3 || version > 5))
		yyerror("only versions 3,4,5,8 supported");
	attribute_next[0] = version>3? 47 : 31;
	property_next[0] = version>3? 63 : 31;
	story_shift = version==8? 3 : version==3? 1 : 2;
	dict_entry_size = version>3? 6 : 4;
	the_header.version = version;
}

void emit1op(_1op opcode,operand uval) {
	if (uval.type==optype::large_constant)
		emitByte(0x80 + (uint8_t)opcode);
	else if (uval.type==optype::small_constant)
		emitByte(0x90 + (uint8_t)opcode);
	else
		emitByte(0xA0 + (uint8_t)opcode);
	emitOperand(uval);
}

void emit2op(operand lval,_2op opcode,operand rval) {
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
}

int yych, yylen, yypass;
char yytoken[32];
FILE *yyinput;
inline int yynext() { return getc(yyinput); }

int yylex() {
	yylen = 0;
	while (isspace(yych))
		yych=yynext();

	if (isalpha(yych)||yych=='#'||yych=='_') {
		do {
			if (yylen==sizeof(yytoken)-1)
				yyerror("token too long");
			yytoken[yylen++] = yych;
			yych = yynext();
		} while (isalnum(yych)||yych=='_');
		yytoken[yylen] = 0;
		auto r = rw.find(yytoken);
		if (r != rw.end())
			return r->second;
		auto z = f_0op.find(yytoken);
		if (z != f_0op.end()) {
			yylval.zeroOp = z->second;
			return STMT_0OP;
		}
		auto o = f_1op.find(yytoken);
		if (o != f_1op.end()) {
			yylval.oneOp = o->second;
			return STMT_1OP;
		}
		// otherwise NEWSYM or existing symbol of specific type
		if (yypass==1)
			return NEWSYM;
		else
			return NEWSYM;
	}
	else switch(yych) {
		case '-':
			yytoken[yylen++] = '-';
			yych = yynext();
			if (yych<'0'||yych>'9') {
				return '-';
			}
			[[fallthrough]];
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			do {
				yytoken[yylen++] = yych;
				yych = yynext();
			} while (yych>='0'&&yych<='9');
			yytoken[yylen] = 0;
			yylval.ival = atoi(yytoken);
			return INTLIT;
		case '+': case '(': case '[': case ')': case ']':
		case '~': case '*': case ':': case '.': case '%':
		case '&': case '|':
			yytoken[0] = yych;
			yych = yynext();
			return yytoken[0];
		case '=':
			yych = yynext();
			if (yych=='=') {
				yych = yynext();
				return EQ;
			}
			else
				return '='; 
		case '<':
			yych = yynext();
			if (yych=='<') {
				yych = yynext();
				return LSH;
			}
			else if (yych=='=') {
				yych = yynext();
				return LE;
			}
			else if (yych=='>') {
				yych = yynext();
				return NE;
			}
			else
				return '<';
		case '>':
			yych = yynext();
			if (yych=='>') {
				yych = yynext();
				return RSH;
			}
			else if (yych=='=') {
				yych = yynext();
				return GE;
			}
			else
				return '>';
		case '\'': {
			yych = yynext();
			while (yych != '\'' && yych != EOF) {
				if (yylen+1==sizeof(yytoken))
					yyerror("dictionary word way too long");
				yytoken[yylen++] = tolower(yych);
				yych = yynext();
			}
			yych = yynext();
			yytoken[yylen] = 0;
			dict_entry de = {};
			encode_string(de.encoded,dict_entry_size,yytoken,yylen);
			if (yypass==1) {
				the_dictionary[de] = -1;
				yylval.ival = -1;
			}
			else {
				yylval.ival = the_dictionary[de];
			}
			return DICT;
		}
		default:
			yyerror("unknown character %c in input",yych);
			[[fallthrough]];
		case EOF:
			return EOF;
	}
}

void yyerror(const char *fmt,...) {
	va_list args;
	va_start(args,fmt);
	vfprintf(stderr,fmt,args);
	putc('\n',stderr);
	va_end(args);
	exit(1);
}

int main(int argc,char **argv) {
	init();
	for (yypass=1; yypass<=2; yypass++) {
		yyinput = fopen(argv[1],"r");
		int scope = 0;
		int nextObject = 1;
		yych = 32;
		if (yypass==1) {
			int t = yylex();
			if (t == '{')
				++scope;
			else if (t == '}')
				--scope;
			else if (scope == 0) {
				if (t == ATTRIBUTE || t == PROPERTY)
					yylex();	// skip LOCATION/OBJECT/GLOBAL
				else if (t == OBJECT || t == LOCATION) {
					if (yylex() != NEWSYM)
						yyerror("expected object name after 'object' or location'");
					// declare the object and assign its value
					the_globals[yytoken] = { (int16_t)t,(int16_t)the_object_table.size() };
					the_object_table.push_back(new object);
				}
			}
			while (yylex() != -1)
				;
			printf("%zu words in dictionary\n",the_dictionary.size());
			// build the final dictionary, assigning word indices.
			uint16_t idx = 0;
			z_dict = new uint8_t[the_dictionary.size() * (dict_entry_size+1)];
			uint8_t *zi = z_dict;
			for (auto &d: the_dictionary) {
				d.second = idx++;
				memcpy(zi,d.first.encoded,dict_entry_size);
				zi[dict_entry_size] = 0;
				zi += dict_entry_size + 1;
			}
		}
		else
			yyparse();
		fclose(yyinput);
	}
}