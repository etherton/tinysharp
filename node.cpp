#include "node.h"
#include "trap.h"

namespace tinysharp {

uint8_t* node::sm_heap;
label_t node::sm_pc;

stmt::state stmt::sm_current;

stmt::state stmt::beginLoopBody() {
	auto result = sm_current;
	sm_current.b = nullptr;
	sm_current.c = nullptr;
	return result;
}

void stmt::endLoopBody(stmt::state prev,label_t breakAddr,label_t continueAddr) {
	for (auto i=sm_current.b; i; i=i->m_nextBreak) {
		sm_heap[i->m_offset] = breakAddr;
		sm_heap[i->m_offset + 1] = breakAddr >> 8;
	}
	for (auto i=sm_current.c; i; i=i->m_nextContinue) {
		sm_heap[i->m_offset] = continueAddr;
		sm_heap[i->m_offset + 1] = continueAddr >> 8;
	}
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
	auto prev = beginLoopBody();
	m_body->emit();
	emitBackwardJump(OP_J,eval);
	placeForwardJump(past);
	endLoopBody(prev,past,eval);	// break, continue addresses
}

void stmt_do::emit() {
	label_t top = emitLabel();
	auto prev = beginLoopBody();
	m_body->emit();
	emitBackwardJump(OP_JNZ,top);
	label_t bottom = emitLabel();
	endLoopBody(prev,bottom,top);
}

void stmt_for::emit() {
	if (m_initializer)
		m_initializer->emit();
	auto top = emitLabel();
	uint16_t bottom;
	if (m_cond)
		m_cond->emit();
	bottom = emitForwardJump(OP_JZ);
	auto prev = beginLoopBody();
	m_body->emit();
	auto step = m_step? emitLabel() : top;
	if (m_step) {
		m_step->emit();
		emitBackwardJump(OP_J,top);
	}
	emitBackwardJump(OP_J,top);
	placeForwardJump(bottom);
	endLoopBody(prev,bottom,step);
}

void stmt_break::emit() {
	m_nextBreak = sm_current.b;
	emitOp(OP_J);
	m_offset = sm_pc;
	emitHalf(0);
	sm_current.b = this;
}

void stmt_continue::emit() {
	m_nextContinue = sm_current.c;
	emitOp(OP_J);
	m_offset = sm_pc;
	emitHalf(0);	
	sm_current.c = this;
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
			default: trap();
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
            default: trap();
        }
        return true;
    }
    else
        return false;
}

void expr_integer_literal::emit() {
	int i = m_literal < -1 ? -i : i;
	switch (i) {
		case -1: emitOp(OP_LITM1); break;
		case 0: emitOp(OP_LIT0); break;
		case 1: emitOp(OP_LIT1); break;
		case 2: emitOp(OP_LIT2); break;
		case 3: emitOp(OP_LIT3); break;
		default:
			if (m_literal <= 0xFF) {
				emitOp(OP_LIT_B);
				emitByte(m_literal);
			}
			else if (m_literal <= 0xFFFF) {
				emitOp(OP_LIT_H);
				emitHalf(m_literal);
			}
			else if (m_literal <= 0xFFFFFF) {
				emitOp(OP_LIT_3B);
				emitByte(m_literal);
				emitByte(m_literal >> 8);
				emitByte(m_literal >> 16);
			}
			else {
				emitOp(OP_LIT_W);
				emitWord(m_literal);			
			}
			break;
	}
	if (m_literal < -1)
		emitOp(OP_NEGI);
}

bool expr_integer_literal::isConstant(constant_t &outValue) {
	outValue.i = m_literal;
	return true;
}

void expr_float_literal::emit() {
}


bool expr_float_literal::isConstant(constant_t &outValue) {
	outValue.f = m_literal;
	return true;
}

} // namespace tinysharp
