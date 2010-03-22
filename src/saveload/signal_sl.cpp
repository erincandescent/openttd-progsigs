/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file signal_sl.cpp Code handling saving and loading of signals */

#include "../stdafx.h"
#include "../programmable_signals.h"
#include "../core/alloc_type.hpp"
#include "../core/bitmath_func.hpp"
#include <vector>
#include "saveload.h"

typedef std::vector<byte> Buffer;

static void WriteVLI(Buffer& b, uint i)
{
    uint lsmask =  0x7F;
    uint msmask = ~0x7F;
    while(i & msmask) {
        byte part = (i & lsmask) | 0x80;
        b.push_back(part);
        i >>= 7;
    }
    b.push_back((byte) i);
}

static uint ReadVLI()
{
    uint shift = 0;
		uint val = 0;
    byte b;

    b = SlReadByte();
    while(b & 0x80) {
        val |= uint(b & 0x7F) << shift;
        shift += 7;
        b = SlReadByte();
    }
    val |= uint(b) << shift;
    return val;
}

static void WriteCondition(Buffer& b, SignalCondition* c)
{
	WriteVLI(b, c->ConditionCode());
	switch(c->ConditionCode()) {
		case PSC_NUM_GREEN:
		case PSC_NUM_RED: {
			SignalVariableCondition* vc = static_cast<SignalVariableCondition*>(c);
			WriteVLI(b, vc->comparator);
			WriteVLI(b, vc->value);
			break;
		}
		
		default:
			break;
	}
}

static SignalCondition* ReadCondition()
{
	SignalConditionCode code = (SignalConditionCode) ReadVLI();
	switch(code) {
		case PSC_NUM_GREEN:
		case PSC_NUM_RED: {
			SignalVariableCondition* c = new SignalVariableCondition(code);
			c->comparator = (SignalComparator) ReadVLI();
			if(c->comparator > SGC_LAST) NOT_REACHED();
			c->value = ReadVLI();
			return c;
		}
		default:
			return new SignalSimpleCondition(code);
	}
}

static void Save_SPRG()
{
	// Check for, and dispose of, any signal information on a tile which doesn't have signals.
	// This indicates that someone removed the signals from the tile but didn't clean them up.
	for(ProgramList::iterator i = _signal_programs.begin(), e = _signal_programs.end();
			i != e; ++i) {
		uint id = i->first;
		if(!HasProgrammableSignals(id)) {
			DEBUG(sl, 0, "Programmable signal information for signal with id %x has been leaked", id);
			++i;
			FreeSignalProgram(id);
			if(i == e) break;
		}
	}
	
	// OK, we can now write out our programs
	Buffer b;
	WriteVLI(b, _signal_programs.size());
	for(ProgramList::iterator i = _signal_programs.begin(), e = _signal_programs.end();
			i != e; ++i) {
		uint id = i->first;
		SignalProgram* prog = i->second;
	
		prog->DebugPrintProgram();
	
		WriteVLI(b, id);
		WriteVLI(b, prog->instructions.Length());
		for(SignalInstruction **j = prog->instructions.Begin(), **je = prog->instructions.End();
				j != je; ++j) {
			SignalInstruction* insn = *j;
			WriteVLI(b, insn->Opcode());
			if(insn->Opcode() != PSO_FIRST)
				WriteVLI(b, insn->Previous()->Id());
			switch(insn->Opcode()) {
				case PSO_FIRST: {
					SignalSpecial* s = static_cast<SignalSpecial*>(insn);
					WriteVLI(b, s->next->Id());
					break;
				}
				
				case PSO_LAST: break;
				
				case PSO_IF: {
					SignalIf* i = static_cast<SignalIf*>(insn);
					WriteCondition(b, i->condition);
					WriteVLI(b, i->if_true->Id()); 
					WriteVLI(b, i->if_false->Id());
					WriteVLI(b, i->after->Id());
					break;
				}
				
				case PSO_IF_ELSE:
				case PSO_IF_ENDIF: {
					SignalIf::PseudoInstruction* p = static_cast<SignalIf::PseudoInstruction*>(insn);
					WriteVLI(b, p->block->Id());
					break;
				}
				
				case PSO_SET_SIGNAL: {
					SignalSet* s = static_cast<SignalSet*>(insn);
					WriteVLI(b, s->to_state ? 1 : 0);
					break;
				}
				
				default: NOT_REACHED();
			}
		}
	}
	
	uint size = b.size();
	SlSetLength(size);
	for(uint i = 0; i < size; i++)
		SlWriteByte(b[i]); // TODO Gotta be a better way
}

struct Fixup {
	Fixup(SignalInstruction** p, SignalOpcode type)
		: type(type), ptr(p)
	{}
	
	SignalOpcode type;
	SignalInstruction** ptr;
};

typedef SmallVector<Fixup, 4> FixupList;

template<typename T>
static void MakeFixup(FixupList& l, T*& ir, uint id, SignalOpcode op = PSO_INVALID)
{
	ir = reinterpret_cast<T*>(id);
	new(l.Append()) Fixup(reinterpret_cast<SignalInstruction**>(&ir), op);
}

static void DoFixups(FixupList& l, InstructionList& il)
{
	for(Fixup *i = l.Begin(), *e = l.End(); i != e; ++i) {
		uint id = reinterpret_cast<size_t>(*i->ptr);
		if(id >= il.Length())
			NOT_REACHED();
			
		*i->ptr = il[id];
		
		if(((*i->ptr)->Opcode() & i->type) != (*i->ptr)->Opcode()) {
			DEBUG(sl, 0, "Expected Id %d to be %d, but was in fact %d", id, i->type, (*i->ptr)->Opcode());
			NOT_REACHED();
		}
	}
}

static void Load_SPRG()
{
	_signal_programs.clear();
	uint count = ReadVLI();
	for(uint i = 0; i < count; i++) {
		FixupList l;
		uint signalId     = ReadVLI();
		uint instructions = ReadVLI();
		
		SignalProgram* sp = new SignalProgram(true);
		_signal_programs[signalId] = sp;
		
		for(uint j = 0; j < instructions; j++) {
			SignalOpcode op = (SignalOpcode) ReadVLI();
			switch(op) {
				case PSO_FIRST: {
					sp->first_instruction = new SignalSpecial(sp, PSO_FIRST);
					sp->first_instruction->GetPrevHandle() = NULL;
					MakeFixup(l, sp->first_instruction->next, ReadVLI());
					break;
				}
				
				case PSO_LAST: {
					sp->last_instruction = new SignalSpecial(sp, PSO_LAST);
					sp->last_instruction->next = NULL;
					MakeFixup(l, sp->last_instruction->GetPrevHandle(), ReadVLI());
					break;
				}
				
				case PSO_IF: {
					SignalIf* i = new SignalIf(sp, true);
					MakeFixup(l, i->GetPrevHandle(), ReadVLI());
					i->condition = ReadCondition();
					MakeFixup(l, i->if_true,  ReadVLI()); 
					MakeFixup(l, i->if_false, ReadVLI()); 
					MakeFixup(l, i->after, ReadVLI()); 
					break;
				}
				
				case PSO_IF_ELSE:
				case PSO_IF_ENDIF: {
					SignalIf::PseudoInstruction* p = new SignalIf::PseudoInstruction(sp, op);
					MakeFixup(l, p->GetPrevHandle(), ReadVLI());
					MakeFixup(l, p->block, ReadVLI(), PSO_IF);
					break;
				}
				
				case PSO_SET_SIGNAL: {
					SignalSet* s = new SignalSet(sp);
					MakeFixup(l, s->GetPrevHandle(), ReadVLI());
					s->to_state = ReadVLI();
					break;
				}
				
				default: NOT_REACHED();
			}
		}
		
		DoFixups(l, sp->instructions);
		sp->DebugPrintProgram();
	}
}

extern const ChunkHandler _signal_chunk_handlers[] = {
	{ 'SPRG', Save_SPRG, Load_SPRG, NULL, CH_RIFF | CH_LAST},
};