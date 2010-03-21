/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file programmable_signals.cpp Programmable Signals */

#include "stdafx.h"
#include "programmable_signals.h"
#include "debug.h"
#include "command_type.h"
#include "table/strings.h"
#include <map>

ProgramList _signal_programs;

SignalProgram::SignalProgram(bool raw)
{
	if(!raw) {
		this->first_instruction = new SignalSpecial(this, PSO_FIRST);
		this->last_instruction  = new SignalSpecial(this, PSO_LAST);
		SignalSpecial::link(this->first_instruction, this->last_instruction);
	}
}

SignalProgram::~SignalProgram()
{
	first_instruction->Remove();
}

struct SignalVM {
	// Initial information
	uint num_exits;                 ///< Number of exits from block
	uint num_green;                 ///< Number of green exits from block
	SignalProgram* program;         ///< The program being run
	TileIndex tile;                 ///< Our tile
	Track track;                    ///< Our track
	
	// Current state
	SignalInstruction* instruction; ///< Instruction to execute next
	
	// Output state
	bool state;                     ///< Output state (true = green)
};

// -- Conditions

SignalCondition::~SignalCondition()
{}

#define COND_DEBUG(cond, val) \
	{ DEBUG(misc, 7, "  Condition " #cond " is %s", (val ? "true" : "false")); \
	  return val; }

SignalSimpleCondition::SignalSimpleCondition(SignalConditionCode code)
	: SignalCondition(code)
{}

/* virtual */ bool SignalSimpleCondition::Evaluate(SignalVM& vm)
{
	switch (this->cond_code) {
		case PSC_ANYGREEN:  COND_DEBUG(AnyGreen, vm.num_green);
		case PSC_ANYRED:    COND_DEBUG(AnyRed, vm.num_exits - vm.num_green);
		case PSC_ALWAYS:    COND_DEBUG(Always, true);
		case PSC_NEVER:     COND_DEBUG(Never, false);
		default: NOT_REACHED();
	}
}

// -- Instructions
SignalInstruction::SignalInstruction(SignalProgram* prog, SignalOpcode op)
	: opcode(op), previous(NULL), program(prog)
{
	*program->instructions.Append() = this;
}

SignalInstruction::~SignalInstruction()
{
	program->instructions.Erase(program->instructions.Find(this));
}

void SignalInstruction::Insert(SignalInstruction* before_insn)
{
	this->previous = before_insn->Previous();
	SetOtherNext(before_insn->Previous(), this);
	SetOtherPrevious(before_insn, this);
	this->SetNext(before_insn);
}

SignalSpecial::SignalSpecial(SignalProgram* prog, SignalOpcode op)
	: SignalInstruction(prog, op)
{
	assert(op == PSO_FIRST || op == PSO_LAST);
	this->next = NULL;
}

/*virtual*/ void SignalSpecial::Remove()
{
	if(opcode == PSO_FIRST) {
		while(this->next) this->next->Remove();
	} else if(opcode == PSO_LAST) {
		SetOtherNext(this->previous, NULL);
	} else NOT_REACHED();
	delete this;
}

/*static*/ void SignalSpecial::link(SignalSpecial* first, SignalSpecial* last)
{
	assert(first->opcode == PSO_FIRST && last->opcode == PSO_LAST);
	first->next    = last;
	last->previous = first;
}

void SignalSpecial::Evaluate(SignalVM& vm)
{
	if(this->opcode == PSO_FIRST) {
		DEBUG(misc, 7, "  Executing First");
		vm.instruction = this->next;
	} else {
		DEBUG(misc, 7, "  Executing Last");
		vm.instruction = NULL;
	}
}
/*virtual*/ void SignalSpecial::SetNext(SignalInstruction* next_insn)
{
	this->next = next_insn;
}

SignalIf::PseudoInstruction::PseudoInstruction(SignalProgram* prog, SignalOpcode op) 
	: SignalInstruction(prog, op)
	{}

SignalIf::PseudoInstruction::PseudoInstruction(SignalProgram* prog, SignalIf* block, SignalOpcode op) 
	: SignalInstruction(prog, op)
{
	this->block = block;
	if(op == PSO_IF_ELSE) {
		previous = block;
	} else if(op == PSO_IF_ENDIF) {
		previous = block->if_true;
	} else NOT_REACHED();
}

/*virtual*/ void SignalIf::PseudoInstruction::Remove()
{
	if(opcode == PSO_IF_ELSE) {
		this->block->if_true = NULL;
	} else if(opcode == PSO_IF_ENDIF) {
		this->block->if_false = NULL;
	} else NOT_REACHED();
}

/*virtual*/ void SignalIf::PseudoInstruction::Evaluate(SignalVM& vm)
{
	DEBUG(misc, 7, "  Executing If Pseudo Instruction %s", opcode == PSO_IF_ELSE ? "Else" : "Endif");
	vm.instruction = this->block->after;
}

/*virtual*/ void SignalIf::PseudoInstruction::SetNext(SignalInstruction* next_insn)
{
	if(this->opcode == PSO_IF_ELSE) {
		this->block->if_false = next_insn;
	} else if(this->opcode == PSO_IF_ENDIF) {
		this->block->after = next_insn;
	} else NOT_REACHED();
}

SignalIf::SignalIf(SignalProgram* prog, bool raw)
	: SignalInstruction(prog, PSO_IF)
{
	if(!raw) {
		this->condition = new SignalSimpleCondition(PSC_ALWAYS);
		this->if_true   = new PseudoInstruction(prog, this, PSO_IF_ELSE);
		this->if_false  = new PseudoInstruction(prog, this, PSO_IF_ENDIF);
		this->after     = NULL;
	}
}

/*virtual*/ void SignalIf::Remove()
{
	delete this->condition;
	while(this->if_true)  this->if_true->Remove();
	while(this->if_false) this->if_false->Remove();
	
	SetOtherNext(this->previous, this->after);
	SetOtherPrevious(this->after, this->previous);
	delete this;
}

/*virtual*/ void SignalIf::Insert(SignalInstruction* before_insn)
{
	this->previous = before_insn->Previous();
	SetOtherNext(before_insn->Previous(), this);
	SetOtherPrevious(before_insn, this->if_false);
	this->after    = before_insn;
}

void SignalIf::SetCondition(SignalCondition* cond)
{
	assert(cond != this->condition);
	delete this->condition;
	this->condition = cond;
}

/*virtual*/ void SignalIf::Evaluate(SignalVM& vm)
{
	bool is_true = this->condition->Evaluate(vm);
	DEBUG(misc, 7, "  Executing If, taking %s branch", is_true ? "then" : "else");
	if(is_true) {
		vm.instruction = this->if_true;
	} else {
		vm.instruction = this->if_false;
	}
}

/*virtual*/ void SignalIf::SetNext(SignalInstruction* next_insn)
{
	this->if_true = next_insn;
}



SignalSet::SignalSet(SignalProgram* prog, bool state)
	: SignalInstruction(prog, PSO_SET_SIGNAL)
{
	this->to_state = state;
}

/*virtual*/ void SignalSet::Remove()
{
	SetOtherPrevious(this->next, this->previous);
	SetOtherNext(this->previous, this->next);
	delete this;
}

/*virtual*/ void SignalSet::Evaluate(SignalVM& vm)
{
	DEBUG(misc, 7, "  Executing SetSignal, making %s", this->to_state? "green" : "red");
	vm.state       = this->to_state;
	vm.instruction = NULL;
}


/*virtual*/ void SignalSet::SetNext(SignalInstruction* next_insn)
{
	this->next = next_insn;
}

SignalProgram* GetSignalProgram(TileIndex t, Track track)
{
	return GetSignalProgram(GetSignalId(t, track));
}

SignalProgram* GetSignalProgram(uint32 signal_id)
{	
	ProgramList::iterator i = _signal_programs.find(signal_id);
	if(i != _signal_programs.end()) {
		return i->second;
	} else {
		// Converted NAND signal
		DEBUG(misc, 4, "Programmable signal at tile %x is old NAND, converting", signal_id >> 1);
		SignalProgram* pr = new SignalProgram();
		pr->DebugPrintProgram();
		SignalIf* cond = new SignalIf(pr);
		cond->SetCondition(new SignalSimpleCondition(PSC_ANYRED));
		cond->Insert(pr->last_instruction);
		
		SignalSet* make_green = new SignalSet(pr, true);
		SignalSet* make_red   = new SignalSet(pr, false);
		
		make_green->Insert(cond->if_true);
		pr->DebugPrintProgram();
		make_red->Insert(cond->if_false);
		pr->DebugPrintProgram();
		
		_signal_programs[signal_id] = pr;

		ShowSignalProgramWindow(signal_id >> 1, signal_id & 1 ? TRACK_LOWER : TRACK_X);
		return pr;
	}
}

void FreeSignalProgram(TileIndex t, Track track)
{
	FreeSignalProgram(GetSignalId(t, track));
}

void FreeSignalProgram(uint signal_id)
{
	ProgramList::iterator i = _signal_programs.find(signal_id);
	if(i != _signal_programs.end()) {
		delete i->second;
		_signal_programs.erase(i);
	}
}
bool RunSignalProgram(TileIndex t, Track track, uint num_exits, uint num_green)
{
	SignalProgram* program = GetSignalProgram(t, track);
	SignalVM vm;
	vm.program = program;
	vm.num_exits = num_exits;
	vm.num_green = num_green;
	vm.tile = t;
	vm.track = track;
	
	vm.instruction = program->first_instruction;
	vm.state = false;
	
	DEBUG(misc, 6, "Begining execution of programmable signal on tile %x, track %d", t, track);
	DEBUG(misc, 7, "%d exits, of which %d green", num_exits, num_green);
	do {
		DEBUG(misc, 10, "  Executing instruction %d, opcode %d", vm.instruction->Id(), vm.instruction->Opcode());
		vm.instruction->Evaluate(vm);
	} while(vm.instruction);
	DEBUG(misc, 6, "Completed, returning %s", vm.state ? "green" : "red");
	
	return vm.state;
}

void SignalProgram::DebugPrintProgram()
{
	DEBUG(misc, 5, "Program %x listing", this);
	for(SignalInstruction **b = this->instructions.Begin(), **i = b, **e = this->instructions.End();
			i != e; i++)
	{
		SignalInstruction* insn = *i;
		DEBUG(misc, 5, " %d: Opcode %d, prev %d", i - b, insn->Opcode(), 
					insn->Previous() ? insn->Previous()->Id() : -1);
	}
}

/** Insert a signal instruction into the signal program.
 *
 * @param tile The Tile on which to perform the operation
 * @param p1 Flags and information
 *   - Bit  0      Which signal on tile to perform operation on (Corresponds to bit 0 of signal IDs)
 *   - Bits 1-16   ID of instruction to insert before
 *   - Bits 17-25  Which opcode to create
 *   - Bits 26-31  Reserved
 * @param p2 Depends upon instruction
 *   - PSO_SET_SIGNAL:
 *       - Colour to set the signal to
 * @param text unused
 */
CommandCost CmdInsertSignalInstruction(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	uint32 signal_id    = tile << 1 | GB(p1, 0, 1);
	uint instruction_id = GB(p1, 1, 16);
	SignalOpcode op     = (SignalOpcode) GB(p1, 17, 8);
	
	SignalProgram *prog = GetSignalProgram(signal_id);
	if(instruction_id > prog->instructions.Length())
		return CommandCost(STR_ERR_PROGSIG_INVALID_INSTRUCTION);

	if(!flags & DC_EXEC)
		return CommandCost();
	
	SignalInstruction *insert_before = prog->instructions[instruction_id];
	switch(op) {
		case PSO_IF: {
			SignalIf *if_ins = new SignalIf(prog);
			if_ins->Insert(insert_before);
			break;
		}
		
		case PSO_SET_SIGNAL: {
			SignalSet *set = new SignalSet(prog, p2);
			set->Insert(insert_before);
			break;
		}
		
		case PSO_FIRST:
		case PSO_LAST:
		case PSO_IF_ELSE:
		case PSO_IF_ENDIF:
		default:
			return CommandCost(STR_ERR_PROGSIG_INVALID_OPCODE);
	}
	
	return CommandCost();
}