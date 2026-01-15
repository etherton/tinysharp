/* tinyz.y */
/* bison --debug --token-table --verbose tinyz.y -o tinyz.tab.cpp && clang++ -g -std=c++17 tinyz.tab.cpp */

%expect 1
%{
	#include "opcodes.h"
	#include "header.h"
	#include <set>
	#include <map>
	#include <cassert>

	int yylex();
	void yyerror(const char*,...);
	uint16_t encode_string(uint8_t *dest,size_t destSize,const char *src,size_t srcSize,bool forDict = false);
	int encode_string(const char*);
	void print_encoded_string(const uint8_t *src,void (*pr)(char ch));
	uint8_t next_global, next_local, story_shift = 1, dict_entry_size = 4;
	storyHeader the_header = { 3 };

	template <typename T> struct list_node {
		list_node<T>(T a,list_node<T> *b) : car(a), cdr(b) { }
		~list_node() {
			delete cdr;
		}
		T car;
		list_node<T> *cdr;
		size_t size() const {
			return cdr? 1 + cdr->size() : 1;
		}
	};

	struct operand { // 0=long, 1=small, variable=2, omitted=3
		int value:30;
		optype type:2;
	};
	static_assert(sizeof(operand)==4);

	// a relocatable blob can itself be relocated, and can contain
	// zero or more references to other relocatable blobs.
	// all recloations are 16 bits and can represent either a
	// direct address, or a packed story-shifted address.
	// there can be up to 32768 relocations.
	std::vector<struct relocatableBlob*> the_relocations;
	struct relocatableBlob {
		static uint16_t firstFree;
		static relocatableBlob* create(uint16_t totalSize) {
			relocatableBlob* result = (relocatableBlob*) new uint8_t[totalSize + sizeof(relocatableBlob)];
			result->size = totalSize;
			result->offset = 0;
			result->relocations = nullptr;
			result->userData = 0;
			if (firstFree != 0xFFFF) {
				result->index = firstFree;
				firstFree = (uint16_t)(size_t)the_relocations[firstFree];
				the_relocations[result->index] = result;
			}
			else {
				result->index = the_relocations.size();
				the_relocations.push_back(result);
			}
			return result;
		}
		static uint16_t createInt(int16_t t) {
			auto r = create(2);
			r->storeInt(t);
			return r->index;
		}
		static relocatableBlob* createProperty(uint16_t size,uint8_t propertyIndex) {
			if (!size)
				yyerror("cannot have zero-sized proeprty");
			if (the_header.version == 3) {
				if (size > 8)
					yyerror("property too large for v3");
				auto r = create(size+1);
				r->storeByte(((size-1)<<5) | propertyIndex);
				return r;
			}
			else {
				if (size > 64)
					yyerror("property too large for v4+");
				if (size <= 2) {
					auto r = create(size+1);
					r->storeByte(size==1? propertyIndex : 64 | propertyIndex);
					return r;
				}
				else {
					auto r = create(size+2);
					r->storeByte(0x80 | propertyIndex);
					r->storeByte(0x80 | (size & 63));
					return r;
				}
			}
		}
		void destroy() {
			uint16_t indexSave = index;
			delete the_relocations[index]->relocations;
			delete [] (uint16_t*) the_relocations[index];
			the_relocations[indexSave] = (relocatableBlob*)(size_t)firstFree;
			firstFree = indexSave;
		}
		void seal() {
			assert(offset <= size);
			relocatableBlob *newResult = (relocatableBlob*) new uint8_t[offset + sizeof(relocatableBlob)];
			newResult->size = newResult->offset = offset;
			newResult->relocations = relocations;
			newResult->userData = userData;
			delete [] (uint8_t*) the_relocations[index];
			the_relocations[index] = newResult;;
		}
		void storeByte(uint8_t b) {
			assert(offset < size);
			contents[offset++] = b;
		}
		void copy(const uint8_t *src,size_t srcLen) {
			while (srcLen--)
				storeByte(*src++);
		}
		void storeWord(uint16_t w) {
			storeByte(w >> 8);
			storeByte(w);
		}
		void storeInt(int16_t w) {
			storeByte(w >> 8);
			storeByte(w);
		}
		void addRelocation(uint16_t ri,uint16_t o) {
			relocations = new list_node<std::pair<uint16_t,uint16_t>>(std::pair<uint16_t,uint16_t>(ri,o),relocations);
		}
		void append(relocatableBlob *other) {
			for (auto i=other->relocations; i; i=i->cdr)
				addRelocation(i->car.first,i->car.second + offset);
			copy(other->contents,other->size);
			other->destroy();
		}
		uint16_t size, offset, index, userData;
		list_node<std::pair<uint16_t,uint16_t>> *relocations;
		uint8_t contents[0];
	};
	uint16_t relocatableBlob::firstFree=0xFFFF;
	relocatableBlob *header_blob, *dictionary_blob, *object_blob, *properties_blob;

	static const uint8_t opsizes[3] = { 2,1,1 };
	const uint8_t LONG_JUMP = 0x8C;			// +/-32767
	const uint8_t SHORT_JUMP = 0x9C;		// 0-255

	relocatableBlob * currentRoutine;
	uint8_t currentProperty;
	void emitByte(uint8_t b) {
		currentRoutine->storeByte(b);
	}
	void emitOperand(operand o) {
		if (o.type==optype::large_constant)
			emitByte(o.value >> 8);
		emitByte(o.value);
	}
	// void emitBranch(uint16_t target);
	void emitvarop(operand l,_2op op,operand r1,operand r2);
	void emitvarop(operand l,_2op op,operand r1,operand r2,operand r3);
	void emit2op(operand l,_2op op,operand r);
	void emit1op(_1op op,operand un);
	const uint8_t TOS = 0, SCRATCH = 4;

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
		l->offset = currentRoutine->offset;
		for (auto i=l->references; i; i=i->cdr) {
			int16_t delta = (i->car - currentRoutine->offset + 2);
			if (currentRoutine->contents[i->car])
				currentRoutine->contents[i->car] |= (delta >> 8) & 63;
			else
				currentRoutine->contents[i->car] = (delta >> 8);
			currentRoutine->contents[i->car + 1] = delta;
		}
	}
	void emitJump(label l,bool isLong) {
		emitByte(isLong? LONG_JUMP : SHORT_JUMP);
		if (l->offset != 0xFFFF) {
			int16_t delta = l->offset - currentRoutine->offset + 2;
			if (isLong)
				emitByte(delta >> 8);
			emitByte(delta);
		}
		else {
			l->references = new list_node<uint16_t>(currentRoutine->offset,l->references);
			if (isLong)
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
	std::vector<std::map<std::string,symbol>*> the_locals;
	std::vector<relocatableBlob*> routine_stack;
	void open_scope() { 
		the_locals.push_back(new std::map<std::string,symbol>()); 
		routine_stack.push_back(currentRoutine);
		currentRoutine = nullptr;
	}
	void close_scope() { 
		currentRoutine = routine_stack.back();
		routine_stack.pop_back();
		delete the_locals.back(); 
		the_locals.pop_back();
	 }

	struct object {
		int16_t child, sibling, parent;
		uint8_t attributes[6];
		uint16_t descrLen, propertySize;
		uint8_t *descr;
		union {
			relocatableBlob **properties;
			relocatableBlob *finalProps;
		};
	} *cdef;
	uint16_t self_value;
	std::vector<object*> the_object_table;
	struct action {
	};
	std::vector<action*> the_action_table;

	struct dict_entry {
		uint8_t encoded[6];
		bool operator <(const dict_entry &rhs) const {
			return memcmp(encoded,rhs.encoded,sizeof(encoded)) < 0;
		}
	};
	std::map<dict_entry,uint16_t> the_dictionary; // maps a dictionary word to its index
	uint8_t& z_dict_payload(uint16_t i) { return dictionary_blob->contents[7 + i * (dict_entry_size+1) + dict_entry_size]; }

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
		void emit() {
			assert(false); // shouldn't be called.
		}
		virtual void emitBranch(label target,bool n,bool isLong) {
			if (negated)
				n = !n;
			if (target->offset != 0xFFFF) {
				int16_t delta = target->offset - currentRoutine->offset + 2;
				if (isLong) {
					emitByte((n? 0xC0 : 0x040) | ((delta >> 8) & 63));
					emitByte(delta);
				}
				else
					emitByte((n? 0x80 : 0x00) | delta & 63);
			}
			else {
				emitByte(n? 0xC0 : 0x40);
				if (isLong)
					emitByte(0);
				target->references = new list_node<uint16_t>(currentRoutine->offset,target->references);
			}
		}
		bool negated;
		bool isLogical() const { return true; }
	};
	struct expr_binary_branch: public expr_branch {
		expr_binary_branch(expr *l,_2op op,bool negated,expr *r) : left(l), opcode(op), right(r), expr_branch(negated) { }
		_2op opcode;
		expr *left, *right;
		void emitBranch(label target,bool negated,bool isLong) {
			operand lval, rval;
			right->eval(rval);
			left->eval(lval);
			emit2op(lval,opcode,rval);
			expr_branch::emitBranch(target,negated,isLong);
		}
	};
	struct expr_in: public expr_branch {
		expr_in(expr *l,expr *r1,expr *r2=nullptr,expr *r3=nullptr) : left(l), right1(r1), right2(r2), right3(r3), expr_branch(false) { }
		expr *left,*right1,*right2,*right3;
		void emitBranch(label target,bool negated,bool isLong) {
			operand lval, rval1, rval2, rval3;
			if (right3)
				right3->eval(rval3);
			if (right2)
				right2->eval(rval2);
			left->eval(lval);
			if (right3)
				emitvarop(lval,_2op::je,rval1,rval2,rval3);
			else if (right2)
				emitvarop(lval,_2op::je,rval1,rval2);
			else
				emit2op(lval,_2op::je,rval1);
			expr_branch::emitBranch(target,negated,isLong);
		}
	};
	struct expr_unary_branch: public expr_branch {
		expr_unary_branch(_1op op,bool negated,expr *e) : opcode(op), unary(e), expr_branch(negated) { }
		_1op opcode;
		expr *unary;
		void emitBranch(label target,bool negated,bool isLong) {
			operand un;
			unary->eval(un);
			emit1op(opcode,un);
			expr_branch::emitBranch(target,negated,isLong);
		}
	};
	struct expr_unary_branch_store: public expr_branch {
		expr_unary_branch_store(_1op op,expr *e,uint8_t d) : opcode(op), unary(e), dest(d), expr_branch(false) { }
		_1op opcode;
		uint8_t dest;
		expr *unary;
		void emitBranch(label target,bool negated,bool isLong) {
			operand un;
			unary->eval(un);
			emit1op(opcode,un);
			emitByte(dest);
			expr_branch::emitBranch(target,negated,isLong);
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
		expr_logical_not(expr_branch *e) : unary(), expr_branch(!e->negated) { }
		expr_branch *unary;
		void emitBranch(label target,bool negated,bool isLong) {
			unary->emitBranch(target,negated,isLong);
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
		expr_logical_and(expr_branch *l,expr_branch *r) : left(l), right(r), expr_branch(false) { }
		expr_branch *left, *right;
		void emitBranch(label target,bool negated,bool isLong) {
			// (negated=true) if (a and b) means jz a,target; jz b,target
			// (negated=true) while (a and b) means jz a,target; jz b,target
			// (negated=false) repeat ... while (a and b) means jz skip; jnz b,target; skip:
			if (negated) {	// not (a and b) -> (not a) or (not b)
				label trueBranch = createLabel();
				left->emitBranch(trueBranch,true,isLong);
				right->emitBranch(target,false,isLong);
				placeLabel(trueBranch);
			}
			else {
				left->emitBranch(target,false,isLong);
				right->emitBranch(target,false,isLong);
			}
		}
	};
	struct expr_logical_or: public expr_branch {
		expr_logical_or(expr_branch*l,expr_branch *r) : left(l), right(r), expr_branch(false) { }
		expr_branch *left, *right;
		void emitBranch(label target,bool negated,bool isLong) {
			// if (a or b) means jnz a,skip; jz b,target; skip:
			if (negated) { // not (a or b) -> (not a) and (not b)
				left->emitBranch(target,true,isLong);
				right->emitBranch(target,true,isLong);
			}
			else {
				label trueBranch = createLabel();
				left->emitBranch(trueBranch,false,isLong);
				right->emitBranch(target,false,isLong);
				placeLabel(trueBranch);
			}
		}
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
	size_t jumpPastSize(stmt*s) {
		return s? (s->size() > 61? 3 : 2) : 0;
	}
	size_t includingBranchPast(size_t s) {
		return s > 61? 2 : 1;
	}
	size_t includingJumpPast(size_t s) {
		return s > 61? 3 : 2;
	}
	struct stmt_if: public stmt_flow {
		stmt_if(expr_branch *e,stmt *t,stmt *f): cond(e), ifTrue(t), ifFalse(f) { }
		expr_branch *cond;
		stmt *ifTrue, *ifFalse;
		// TODO: if ifTrue is rfalse/rtrue, we just need the non-negated branch to 0/1
		// TODO: else if ifFalse is rfalse/rtrue, we just need the negated branch to 0/1
		// TODO: If ifTrue ends in a return, we don't need the jump past false block
		void emit() {
			label falseBranch = createLabel();
			cond->emitBranch(falseBranch,true,ifTrue->size() > (ifFalse? 57 : 59));
			ifTrue->emit();
			if (ifFalse) {
				label skipFalse = createLabel();
				emitJump(skipFalse,ifFalse->size() > 59);
				placeLabel(falseBranch);
				ifFalse->emit();
				placeLabel(skipFalse);
			}
			else
				placeLabel(falseBranch);
		}
		unsigned size() const {
			return cond->size() + includingBranchPast(ifTrue->size() + jumpPastSize(ifFalse)) +
				(ifFalse? ifFalse->size() : 0);
		}
	};
	struct stmt_while: public stmt_flow {
		stmt_while(expr_branch *e,stmt *b): cond(e), body(b) { }
		expr_branch *cond;
		stmt *body;
		void emit() {
			label falseBranch = createLabel(), top = createLabelHere();
			cond->emitBranch(falseBranch,true,body->size() > 58);
			// TODO: continue and break via a stack
			body->emit();
			emitJump(top,true);
			placeLabel(falseBranch);
		}
		unsigned size() const { 
			return cond->size() + includingJumpPast(body->size()) + 3; 
		}
	};
	struct stmt_repeat: public stmt_flow {
		stmt_repeat(stmt *b,expr_branch *e): body(b), cond(e) { }
		stmt *body;
		expr_branch *cond;
		void emit() {
			auto trueBranch = createLabelHere();
			body->emit();
			cond->emitBranch(trueBranch,false,true);
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
	struct stmt_2op: public stmt {
		stmt_2op(_2op op,expr *l,expr *r) : opcode(op), left(l), right(r) { }
		_2op opcode;
		expr *left, *right;
		void emit() {
			operand lop, rop;
			right->eval(rop);
			left->eval(lop);
			emit2op(lop,opcode,rop);
		}
		unsigned size() const {
			return left->size() + right->size() + 1;
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
	struct stmt_print: public stmt {
		stmt_print(_0op o,const char *s) : opcode(o), string(s) { }
		const char *string;
		_0op opcode;
		void emit() {
			emitByte((uint8_t)opcode);
			currentRoutine->offset += encode_string(currentRoutine->contents + currentRoutine->offset,
				(currentRoutine->size - currentRoutine->offset) & ~1,string,strlen(string));
		}
		unsigned size() const {
			return 1 + encode_string(nullptr,0,string,strlen(string));
		}
	};
	uint16_t emit_routine(int numLocals,stmt *body) {
		currentRoutine = relocatableBlob::create(1024);
		emitByte(numLocals);
		if (the_header.version < 5) {
			while (numLocals--) { 
				emitByte(0); 
				emitByte(0); 
			}
		}
		body->emit();
		return currentRoutine->index;
	}

	uint16_t property_defaults[63];
%}

%union {
	int ival;
	uint16_t rval;
	const char *sval;
	expr *eval;
	expr_branch *brval;
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

%token ATTRIBUTE PROPERTY DIRECTION GLOBAL OBJECT LOCATION ROUTINE ARTICLE PLACEHOLDER ACTION HAS HASNT IN HOLDS
%token BYTE_ARRAY WORD_ARRAY CALL PRINT PRINT_RET SELF
%token <ival> DICT ANAME PNAME LNAME GNAME INTLIT ONAME
%token <sval> STRLIT
%token <rval> RNAME
%token <sym> NEWSYM
%token WHILE REPEAT IF ELSE
%token LE "<=" GE ">=" EQ "==" NE "!="
// %token DEC_CHK "--<" INC_CHK "++>"
%token GET_SIBLING GET_CHILD SAVE RESTORE
%token LSH "<<" RSH ">>"
%token ARROW "->" INCR "++" DECR "--"
%token RFALSE RTRUE RETURN
%token OR AND NOT
%token <zeroOp> STMT_0OP
%token <oneOp> STMT_1OP
%token GAINS LOSES

%right '~' NOT
%left '*' '/' '%'
%left '+' '-'
%left HAS HASNT GET_CHILD GET_SIBLING
%left '<' LE '>' GE
%left EQ NE
// %left LSH RSH
%left '&'
%left '|'
%left AND
%left OR

%type <eval> expr primary aname arg
%type <brval> bool_expr cond_expr
%type <ival> init vname opt_parent opt_default
%type <rval> routine_body pvalue
%type <scopeval> scope
%type <dlist> dict_list;
%type <ilist> init_list;
%type <elist> opt_call_args arg_list
%type <stval> stmt
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
	: PROPERTY scope NEWSYM opt_default ';' 
		{ 
			$3->token = PNAME; 
			$3->ival = next_value_in_scope($2,property_next); 
			auto i = $3->ival & 63;
			if (property_defaults[i] && property_defaults[i] != $4)
				yyerror("inconsistent valuep for default property (index %d) %d <> %d",
					i,property_defaults[i],$4);
			property_defaults[i] = $4;
		}
	;

opt_default
	: 			{ $$ = 0; }
	| INTLIT	{ $$ = $1; }
	;

direction_def
	: DIRECTION PNAME dict_list ';' { 
		if (($2 & SCOPE_LOCATION_MASK) == 0)
			yyerror("direction property must be type location");
		if (($2 & 63) == 0 || ($2 & 63) > 14) 
			yyerror("direction property index must be between 1 and 14");
		for (auto it=$3; it; it=it->cdr) {
			uint8_t &payload = z_dict_payload(it->car);
			if (payload & 15) 
				yyerror("dictionary word already has direction bits set (payload %d)",payload);
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
	// | STRLIT			{ $$ = encode_string($1); }
	;

object_def
	: OBJECT { expected_scope = SCOPE_OBJECT_MASK; } object_or_location_def
	;

location_def
	: LOCATION { expected_scope = SCOPE_LOCATION_MASK; } object_or_location_def
	;

object_or_location_def
	: ONAME STRLIT opt_parent '{' {
		self_value = $1;
		cdef = the_object_table[$1];
		cdef->child = 0;
		cdef->parent = $3;
		if ($3) {
			cdef->sibling = the_object_table[$3]->child;
			the_object_table[$3]->child = $1;
		}
		else
			cdef->sibling = 0;
		cdef->descrLen = encode_string(nullptr,0,$2,strlen($2));
		cdef->descr = new uint8_t[cdef->descrLen];
		encode_string(cdef->descr,cdef->descrLen,$2,strlen($2));
		delete[] $2;
		memset(cdef->attributes,0,sizeof(cdef->attributes));
		unsigned propCount = the_header.version==3? 32 : 64;
		cdef->properties = new relocatableBlob*[propCount];
		cdef->propertySize = 0;
		memset(cdef->properties,0,propCount * sizeof(relocatableBlob*));
	} opt_property_or_attribute_list '}' {
		unsigned finalSize = 1 + cdef->descrLen + cdef->propertySize + 1;
		auto finalProps = relocatableBlob::create(finalSize);
		finalProps->storeByte(cdef->descrLen>>1);
		finalProps->copy(cdef->descr,cdef->descrLen);
		unsigned propCount = the_header.version==3? 32 : 64;
		while (--propCount) {
			auto p = cdef->properties[propCount];
			if (p)
				finalProps->append(p);
		}
		finalProps->storeByte(0);
		delete [] cdef->properties;
		cdef->finalProps = finalProps;
		cdef->propertySize = finalSize;
		self_value = 0;
	}
	;

opt_parent
	: 						{ $$ = 0; }
	| '(' ONAME ')'			{ $$ = $2; }
	;

opt_property_or_attribute_list
	:
	| property_or_attribute_list
	;

property_or_attribute_list
	: property_or_attribute_list property_or_attribute
	| property_or_attribute
	;

property_or_attribute
	: PNAME ':' { currentProperty = $1; } pvalue		
			{ 
				if (!($1 & expected_scope))
					yyerror("wrong type of property"); 
				uint8_t thisIndex = $1 & 63;
				if (cdef->properties[thisIndex])
					yyerror("already have property %d set",thisIndex);
				cdef->properties[thisIndex] = the_relocations[$4];
				cdef->propertySize += the_relocations[$4]->size;
			}
	| ANAME ';'
			{ 
				if (!($1 & expected_scope)) 
					yyerror("wrong type of attribute"); 
				uint8_t thisIndex = $1 & 63;
				if (cdef->attributes[thisIndex>>3] & (0x80 >> (thisIndex & 7)))
					yyerror("already have attribute %d set",thisIndex);
				cdef->attributes[thisIndex>>3] |= (0x80 >> (thisIndex & 7));
			}
	;

pvalue
	: ONAME ';' { $$ = relocatableBlob::createInt($1); }
	| STRLIT ';'
		{
			// string literal is just a shorthand for the address of a routine that calls print_ret with that string
			$$ = emit_routine(0,new stmt_print(_0op::print_ret,$1));
		}
	| INTLIT ';' { $$ = relocatableBlob::createInt($1); }
	| routine_body { $$ = $1; }
	| dict_list ';' { 
		auto p = relocatableBlob::createProperty($1->size() * 2,currentProperty);
		$$ = p->index;
		auto s = $1;
		while (s) {
			p->storeWord(s->car);
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
	: '[' { open_scope(); next_local=0; } opt_params_list opt_locals_list ']' stmt
		{ 
			$$ = emit_routine(next_local,$6);
			close_scope();
		}
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
			if (the_header.version==3 && next_local==3) 
				yyerror("too many params (limit is 3 for v3)"); 
			else if (the_header.version>3 && next_local==7)
				yyerror("too many params (limit is 7 for v4+)");
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
	: IF cond_expr stmt  				{ $$ = new stmt_if($2,$3,nullptr); }
	| IF cond_expr stmt ELSE stmt 	%prec IF	{ $$ = new stmt_if($2,$3,$5); }
	| REPEAT stmt WHILE cond_expr ';'	{ $$ = new stmt_repeat($2,$4); }
	| WHILE cond_expr stmt				{ $$ = new stmt_while($2,$3); }
	| '{' stmts '}'			{ $$ = new stmts($2); }
	| vname '=' expr ';'	{ $$ = new stmt_assign($1,expr::fold_constant($3)); }
	| RETURN expr ';'		{ $$ = new stmt_return(expr::fold_constant($2)); }
	| RFALSE ';'			{ $$ = new stmt_return(new expr_literal(0)); }
	| RTRUE ';'				{ $$ = new stmt_return(new expr_literal(1)); }
	| CALL expr opt_call_args ';'	{ $$ = new stmt_call(new list_node<expr*>($2,$3));  }
	| RNAME opt_call_args ';'		{ $$ = new stmt_call(new list_node<expr*>(new expr_literal($1),$2)); }
	| STMT_0OP ';'					{ $$ = new stmt_0op($1); }
	| STMT_1OP  expr  ';'			{ $$ = new stmt_1op($1,$2); }
	| PRINT STRLIT ';'				{ $$ = new stmt_print(_0op::print,$2); }
	| PRINT_RET STRLIT ';'			{ $$ = new stmt_print(_0op::print_ret,$2); }
	| INCR vname ';'				{ $$ = new stmt_assign($2,new expr_binary_add(new expr_variable($2),new expr_literal(1))); }
	| DECR vname ';'				{ $$ = new stmt_assign($2,new expr_binary_sub(new expr_variable($2),new expr_literal(1))); }
	| primary GAINS aname ';' 		{ $$ = new stmt_2op(_2op::set_attr,$1,$3); }
	| primary LOSES aname ';'		{ $$ = new stmt_2op(_2op::clear_attr,$1,$3); }
	; 
	
cond_expr
	: '(' bool_expr ')' { $$ = $2; }
	;

opt_call_args
	:					{ $$ = nullptr; }
	| STRLIT			{ $$ = new list_node<expr*>(new expr_literal(encode_string($1)),nullptr); delete[] $1; }
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
	;

bool_expr
	: expr '<' expr		{ $$ = new expr_binary_branch($1,_2op::jl,false,$3); }
	| expr LE expr		{ $$ = new expr_binary_branch($1,_2op::jg,true,$3); }
	| expr '>' expr		{ $$ = new expr_binary_branch($1,_2op::jg,false,$3); }
	| expr GE expr		{ $$ = new expr_binary_branch($1,_2op::jl,true,$3); }
	| expr EQ expr		{ $$ = new expr_binary_branch($1,_2op::je,false,$3); }
	| expr NE expr		{ $$ = new expr_binary_branch($1,_2op::je,true,$3); }
	| expr IN '{' expr '}'	{ $$ = new expr_in($1,$4); }
	| expr IN '{' expr ',' expr '}' { $$ = new expr_in($1,$4,$6); }
	| expr IN '{' expr ',' expr ',' expr '}' { $$ = new expr_in($1,$4,$6,$8); }
	| NOT bool_expr		{ $$ = new expr_logical_not($2); }
	| bool_expr AND bool_expr		{ $$ = new expr_logical_and($1,$3); }
	| bool_expr OR bool_expr		{ $$ = new expr_logical_or($1,$3); }
	| primary HAS aname	{ $$ = new expr_binary_branch($1,_2op::test_attr,false,$3); }
	| primary HASNT aname	{ $$ = new expr_binary_branch($1,_2op::test_attr,true,$3); }
	| GET_CHILD '(' expr ')' ARROW vname { $$ = new expr_unary_branch_store(_1op::get_child,$3,$6); }
	| GET_SIBLING '(' expr ')' ARROW vname { $$ = new expr_unary_branch_store(_1op::get_sibling,$3,$6); }
	| expr HOLDS expr 	{ $$ = new expr_binary_branch($1,_2op::jin,false,$3); }
	| SAVE				{ $$ = new expr_save(); }
	| RESTORE			{ $$ = new expr_restore(); }
	| '(' bool_expr ')' { $$ = $2; }
	;

primary
	: aname			{ $$ = $1; }
	| PNAME			{ $$ = new expr_literal($1); }
	| ONAME			{ $$ = new expr_literal($1); }
	| SELF			{ 
						if (!self_value) 
							yyerror("'self' can only appear within objects"); 
						$$ = new expr_literal(self_value); 
					}
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

std::map<std::string,int16_t> rw;
std::map<std::string,_0op> f_0op;
std::map<std::string,_1op> f_1op;

static uint8_t s_EncodedCharacters[256];

void print_encoded_string(const uint8_t *src,void (*pr)(char ch)) {
	uint8_t step = 0, end = 0;
	auto readCode = [&]() {
		if (step==0) {
			step = 1;
			return (src[0] >> 2) & 31;
		}
		else if (step==1) {
			step = 2;
			return ((src[0] & 3) << 3) | (src[1] >> 5);
		}
		else {
			step = 0;
			end = !!(src[0] & 0x80);
			src += 2;
			return src[-1] & 31;
		}
	};
	const char *alphabet = DEFAULT_ZSCII_ALPHABET;
	uint8_t shift = 0;
	while (!end) {
		uint8_t ch = readCode();
		if (!ch)
			pr(32);
		else if (ch==4)
			shift = 26;
		else if (ch==5)
			shift = 52;
		else if (ch>=6) {
			pr(alphabet[(ch-6)+shift]);
			shift = 0;
		}
	}
}

uint16_t encode_string(uint8_t *dest,size_t destSize,const char *src,size_t srcSize,bool forDict) {
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
	while (forDict && offset < destSize) {
		storeCode(5);
		storeCode(5);
		storeCode(5);
	}
	if (dest)
		dest[offset-2] |= 0x80; // mark end of string
	return offset;
}

const int TOPLEVEL = 16384;

void init() {
	rw["attribute"] = ATTRIBUTE | TOPLEVEL;
	rw["property"] = PROPERTY | TOPLEVEL;
	rw["direction"] = DIRECTION | TOPLEVEL;
	rw["global"] = GLOBAL | TOPLEVEL;
	rw["object"] = OBJECT | TOPLEVEL;
	rw["location"] = LOCATION | TOPLEVEL;
	rw["routine"] = ROUTINE | TOPLEVEL;
	rw["article"] = ARTICLE | TOPLEVEL;
	rw["placeholder"] = PLACEHOLDER | TOPLEVEL;
	rw["action"] = ACTION | TOPLEVEL;
	rw["in"] = IN;
	rw["is"] = EQ;
	rw["isnt"] = NE;
	rw["has"] = HAS;
	rw["hasnt"] = HASNT;
	rw["gains"] = GAINS;
	rw["loses"] = LOSES;
	rw["byte_array"] = BYTE_ARRAY;
	rw["word_array"] = WORD_ARRAY;
	rw["call"] = CALL;
	rw["while"] = WHILE;
	rw["repeat"] = REPEAT;
	rw["rtrue"] = RTRUE;
	rw["rfalse"] = RFALSE;
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
	rw["print"] = PRINT;
	rw["print_ret"] = PRINT_RET;
	rw["self"] = SELF;

	f_0op["restart"] = _0op::restart;
	f_0op["quit"] = _0op::quit;
	f_0op["crlf"] = _0op::new_line;
	f_0op["show_status"] = _0op::show_status;

	f_1op["get_parent"] = _1op::get_parent; // unlike others, get_parent isn't a branch
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
	the_action_table.push_back(nullptr);
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

void emitvarop(operand lval,_2op opcode,operand rval1,operand rval2) {
	emitByte((uint8_t)opcode + 0xC0);
	emitByte((uint8_t(lval.type) << 6) | (uint8_t(rval1.type) << 4) | (uint8_t(rval2.type) << 2) | 0x3);
	emitOperand(lval);
	emitOperand(rval1);
	emitOperand(rval2);
}

void emitvarop(operand lval,_2op opcode,operand rval1,operand rval2,operand rval3) {
	emitByte((uint8_t)opcode + 0xC0);
	emitByte((uint8_t(lval.type) << 6) | (uint8_t(rval1.type) << 4) | (uint8_t(rval2.type) << 2) | uint8_t(rval3.type));
	emitOperand(lval);
	emitOperand(rval1);
	emitOperand(rval2);
	emitOperand(rval3);
}

int yych, yylen, yypass, yyline, yyscope;
char yytoken[32];
FILE *yyinput;
inline int yynext() { yych = getc(yyinput); if (yych == 10) ++yyline; return yych; }

int yylex_() {
	yylen = 0;
	while (isspace(yych))
		yynext();

	if (isalpha(yych)||yych=='#'||yych=='_') {
		do {
			if (yylen==sizeof(yytoken)-1)
				yyerror("token too long");
			yytoken[yylen++] = yych;
			yynext();
		} while (isalnum(yych)||yych=='_');
		yytoken[yylen] = 0;
		// reserved words and builtin funcs first
		auto r = rw.find(yytoken);
		if (r != rw.end() && (!yyscope || !(r->second & TOPLEVEL)))
			return r->second & (TOPLEVEL-1);
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
		// check locals, which take precedence over other symbols
		if (the_locals.size()) {
			auto l = the_locals.back()->find(yytoken);
			if (l != the_locals.back()->end()) {
				yylval.ival = l->second.ival;
				return LNAME;
			}
		}
		// finally search globals
		auto s = the_globals.find(yytoken);
		if (s != the_globals.end()) {
			yylval.ival = s->second.ival;
			return s->second.token;
		}
		// otherwise it's a new symbol (do no actual work on first pass)
		if (yypass==1)
			return NEWSYM;
		else {
			if (the_locals.size())
				yylval.sym = &the_locals.back()->operator[](yytoken);
			else
				yylval.sym = &the_globals.operator[](yytoken);
			return NEWSYM;
		}
	}
	else switch(yych) {
		case '-':
			yytoken[yylen++] = '-';
			yynext();
			if (yych<'0'||yych>'9') {
				if (yych=='>') {
					yynext();
					return ARROW;
				}
				else if (yych=='-') {
					yynext();
					return DECR;
				}
				else	
					return '-';
			}
			[[fallthrough]];
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			do {
				yytoken[yylen++] = yych;
				yynext();
			} while (yych>='0'&&yych<='9');
			yytoken[yylen] = 0;
			yylval.ival = atoi(yytoken);
			return INTLIT;
		case '+': 
			if (yynext()=='+') {
				yynext();
				return INCR;
			}
			else
				return '+';
		case '(': case ')':
		case '~': case '*': case ':': case '.': case '%':
		case '&': case '|': case ';':
		case ',': case '!':
			yytoken[0] = yych;
			yynext();
			return yytoken[0];
		case '[':
			++yyscope;
			yynext();
			return '[';
		case ']':
			--yyscope;
			yynext();
			return ']';
		case '{':
			++yyscope;
			yynext();
			return '{';
		case '}':
			--yyscope;
			yynext();
			return '}';		case '=':
			yynext();
			if (yych=='=') {
				yynext();
				return EQ;
			}
			else
				return '='; 
		case '<':
			yynext();
			if (yych=='<') {
				yynext();
				return LSH;
			}
			else if (yych=='=') {
				yynext();
				return LE;
			}
			else if (yych=='>') {
				yynext();
				return NE;
			}
			else
				return '<';
		case '>':
			yynext();
			if (yych=='>') {
				yynext();
				return RSH;
			}
			else if (yych=='=') {
				yynext();
				return GE;
			}
			else
				return '>';
		case '/':
			yych = yynext();
			if (yych == '/') {
				while (yynext() != EOF && yych != 10)
					;
				return yylex_();	// silly, should just goto top, hopefully compiler spots tail recursion :)
			}
			else
				return '/';
		case '\'': {
			yych = yynext();
			while (yych != '\'' && yych != EOF && yych != 32) {
				if (yylen+1==sizeof(yytoken))
					yyerror("dictionary word way too long");
				yytoken[yylen++] = tolower(yych);
				yynext();
			}
			// turn a space into a new dict word
			if (yych==32)
				yych = '\'';
			else
				yynext();
			yytoken[yylen] = 0;
			dict_entry de = {};
			encode_string(de.encoded,dict_entry_size,yytoken,yylen,true);
			if (yypass==1) {
				the_dictionary[de] = -1;
				yylval.ival = -1;
			}
			else {
				yylval.ival = the_dictionary[de];
			}
			return DICT;
		}
		case '"': {
			const unsigned maxString = 512;
			char *sval = new char[maxString];
			unsigned offset = 0;
			while (yynext()!=EOF && yych!='"') {
				if (offset < maxString-1)
					sval[offset++] = yych;
			}
			yynext();
			sval[offset] = 0;
			yylval.sval = sval;
			return STRLIT;
		}
		default:
			yyerror("unknown character %c in input",yych);
			[[fallthrough]];
		case EOF:
			return EOF;
	}
}

int yylex() {
	int token = yylex_();
	if (yydebug) {
		printf("(%d)",yyscope);
		if (token==EOF)
			printf("[[EOF]]\n");
		else if (token < 255)
			printf("%u:[%c][%d]\n",yyline,token,token);
		else
			printf("%u:[%s][%s][%d]\n",yyline,yytoken,yytname[token - 255],token);
	}
	return token;
}

void yyerror(const char *fmt,...) {
	va_list args;
	va_start(args,fmt);
	fprintf(stderr,"line %d: ",yyline);
	vfprintf(stderr,fmt,args);
	putc('\n',stderr);
	va_end(args);
	exit(1);
}

int main(int argc,char **argv) {
	init();

	/* uint8_t dest[6];
	encode_string(dest,6,"Test",4);
	printf("%x %x %x %x %x %x\n",dest[0],dest[1],dest[2],dest[3],dest[4],dest[5]);
	print_encoded_string(dest,[](char ch){putchar(ch);});
	putchar('\n');
	return 1; */

	yydebug = 0;
	for (yypass=1; yypass<=2; yypass++) {
		yyinput = fopen(argv[1],"r");
		int nextObject = 1;
		yych = 32;
		yyline = 1;
		yyscope = 0;
		if (yypass==1) {
			int t;
			while ((t = yylex()) != EOF) {
				if (yyscope == 0) {
					if (t == ATTRIBUTE || t == PROPERTY)
						yylex();	// skip LOCATION/OBJECT/GLOBAL
					else if (t == OBJECT || t == LOCATION) {
						if (yylex() == NEWSYM) {
							// declare the object and assign its value
							the_globals[yytoken] = { ONAME,(int16_t)the_object_table.size() };
							the_object_table.push_back(new object {});
						}
					}
					else if (t == ACTION) {
						if (yylex() == NEWSYM) {
							if (yytoken[0]!='#')
								yyerror("action symbols must start with #");
							the_globals[yytoken] = { INTLIT,(int16_t)the_action_table.size() };
							the_action_table.push_back(new action {});
						}
					}
				}
			}
			printf("%zu words in dictionary\n",the_dictionary.size());
			// build the final dictionary, assigning word indices.
			dictionary_blob = relocatableBlob::create(7 + the_dictionary.size() * (dict_entry_size+1));
			dictionary_blob->storeByte(3);
			dictionary_blob->storeByte('.');
			dictionary_blob->storeByte(',');
			dictionary_blob->storeByte('"');
			dictionary_blob->storeByte(dict_entry_size+1);
			dictionary_blob->storeWord(the_dictionary.size());
			uint16_t idx = 0;
			for (auto &d: the_dictionary) {
				if (yydebug) {
					printf("word %u: [",idx);
					print_encoded_string(d.first.encoded,[](char ch){putchar(ch);});
					printf("]\n");
				}
				d.second = idx++;
				dictionary_blob->copy(d.first.encoded,dict_entry_size);
				dictionary_blob->storeByte(0);
			}
			printf("%zu objects\n",the_object_table.size()-1);
			printf("%zu actions\n",the_action_table.size()-1);
			the_globals["object_count"] = { INTLIT, int16_t(the_object_table.size() - 1) };
		}
		else
			yyparse();
		fclose(yyinput);
	}
	header_blob = relocatableBlob::create(64);
	header_blob->storeByte(the_header.version);
	// typical order
	// header (64 bytes)
	// +0 version
	// +1 misc flags
	// +4 byte address of high memory
	// +6 initial program counter
	// +8 byte address of dictionary
	// +10 byte address of object table
	// +12 byte address of globals
	// +14 byte address of static memory
	// +16 more flags
	// +18 serial number (ascii)
	// +24 byte address of abbreviation table
	// +26 length of file (>> story_shift)
	// +28 checksum
	// +46 (v5+) byte address of terminating characters table
	// +52 (v5+) byte address of alphabet table
	// property defaults (31 words in v3, else 63)
	// objects
	// object properties (dynamic) (first object must be here)
	// global variables
	// arrays
	// -- end of dynamic memory --
	// object properties (static)
	// abbreviations (one word per up to 96 abbreviations, word address of that abbreviation)
	// grammar table
	// actions table
	// dictionary
	// -- high memory --
	// routines
	// static strings
	// -- end of file --
}