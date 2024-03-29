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
#include "command_func.h"
#include "table/strings.h"
#include "window_func.h"
#include "company_func.h"
#include "cmd_helper.h"
#include <assert.h>

ProgramList _signal_programs;

SignalProgram::SignalProgram(TileIndex tile, Track track, bool raw)
{
	this->tile  = tile;
	this->track = track;
	if (!raw) {
		this->first_instruction = new SignalSpecial(this, PSO_FIRST);
		this->last_instruction  = new SignalSpecial(this, PSO_LAST);
		SignalSpecial::link(this->first_instruction, this->last_instruction);
	}
}

SignalProgram::~SignalProgram()
{
	this->DebugPrintProgram();
	this->first_instruction->Remove();
	delete this->first_instruction;
	delete this->last_instruction;
}

struct SignalVM {
	// Initial information
	uint num_exits;                 ///< Number of exits from block
	uint num_green;                 ///< Number of green exits from block
	SignalProgram *program;         ///< The program being run
	
	// Current state
	SignalInstruction *instruction; ///< Instruction to execute next
	
	// Output state
	SignalState state;
	
	void Execute()
	{
		DEBUG(misc, 6, "Begining execution of programmable signal on tile %x, track %d", 
					this->program->tile, this->program->track);
		do {
			DEBUG(misc, 10, "  Executing instruction %d, opcode %d", this->instruction->Id(), this->instruction->Opcode());
			this->instruction->Evaluate(*this);
		} while (this->instruction);
		
		DEBUG(misc, 6, "Completed");
	}
};

// -- Conditions

SignalCondition::~SignalCondition()
{}

SignalSimpleCondition::SignalSimpleCondition(SignalConditionCode code)
	: SignalCondition(code)
{}

/* virtual */ bool SignalSimpleCondition::Evaluate(SignalVM &vm)
{
	switch (this->cond_code) {
		case PSC_ALWAYS:    return true;
		case PSC_NEVER:     return false;
		default: NOT_REACHED();
	}
}

SignalVariableCondition::SignalVariableCondition(SignalConditionCode code)
	: SignalCondition(code)
{
	switch (this->cond_code) {
		case PSC_NUM_GREEN: comparator = SGC_NOT_EQUALS; break;
		case PSC_NUM_RED:   comparator = SGC_EQUALS; break;
		default: NOT_REACHED();
	}
	value = 0;
}

/*virtual*/ bool SignalVariableCondition::Evaluate(SignalVM &vm)
{
	uint32 var_val;
	switch (this->cond_code) {
		case PSC_NUM_GREEN:  var_val = vm.num_green; break;
		case PSC_NUM_RED:    var_val = vm.num_exits - vm.num_green; break;
		default: NOT_REACHED();
	}
	
	switch (this->comparator) {
		case SGC_EQUALS:            return var_val == this->value;
		case SGC_NOT_EQUALS:        return var_val != this->value;
		case SGC_LESS_THAN:         return var_val <  this->value;
		case SGC_LESS_THAN_EQUALS:  return var_val <= this->value;
		case SGC_MORE_THAN:         return var_val >  this->value;
		case SGC_MORE_THAN_EQUALS:  return var_val >= this->value;
		case SGC_IS_TRUE:           return var_val != 0;
		case SGC_IS_FALSE:          return !var_val;
		default: NOT_REACHED();
	}
}

SignalStateCondition::SignalStateCondition(SignalReference this_sig, 
															TileIndex sig_tile, Trackdir sig_track)
	: SignalCondition(PSC_SIGNAL_STATE), this_sig(this_sig), sig_tile(sig_tile)
	, sig_track(sig_track)
{
	if (this->IsSignalValid())
		AddSignalDependency(SignalReference(this->sig_tile, TrackdirToTrack(sig_track)),
																				this->this_sig);
}

bool SignalStateCondition::IsSignalValid()
{
	if (IsValidTile(this->sig_tile)) {
		if (HasSignalOnTrackdir(this->sig_tile, this->sig_track)) {
			return true;
		} else {
			Invalidate();
		}
	}
	return false;
}

void SignalStateCondition::Invalidate()
{
	this->sig_tile = INVALID_TILE;
}


void SignalStateCondition::SetSignal(TileIndex tile, Trackdir track)
{
	if (this->IsSignalValid())
		RemoveSignalDependency(SignalReference(this->sig_tile, TrackdirToTrack(sig_track)),
																					 this->this_sig);
	this->sig_tile = tile;
	this->sig_track = track;
	if (this->IsSignalValid())
		AddSignalDependency(SignalReference(this->sig_tile, TrackdirToTrack(sig_track)),
																				this->this_sig);
}

/*virtual*/ SignalStateCondition::~SignalStateCondition()
{
	if (this->IsSignalValid())
		RemoveSignalDependency(SignalReference(this->sig_tile, TrackdirToTrack(sig_track)),
																					 this->this_sig);
}

/*virtual*/ bool SignalStateCondition::Evaluate(SignalVM& vm)
{
	if (!this->IsSignalValid()) {
		DEBUG(misc, 1, "Signal (%x, %d) has an invalid condition", this->this_sig.tile, this->this_sig.track);
		return false;
	}
	
	return GetSignalStateByTrackdir(this->sig_tile, this->sig_track) == SIGNAL_STATE_GREEN;
}

// -- Instructions
SignalInstruction::SignalInstruction(SignalProgram *prog, SignalOpcode op)
	: opcode(op), previous(NULL), program(prog)
{
	*program->instructions.Append() = this;
}

SignalInstruction::~SignalInstruction()
{
	SignalInstruction** pthis = program->instructions.Find(this);
	assert(pthis != program->instructions.End());
	program->instructions.Erase(pthis);
}

void SignalInstruction::Insert(SignalInstruction *before_insn)
{
	this->previous = before_insn->Previous();
	before_insn->Previous()->SetNext(this);
	before_insn->SetPrevious(this);
	this->SetNext(before_insn);
}

SignalSpecial::SignalSpecial(SignalProgram *prog, SignalOpcode op)
	: SignalInstruction(prog, op)
{
	assert(op == PSO_FIRST || op == PSO_LAST);
	this->next = NULL;
}

/*virtual*/ void SignalSpecial::Remove()
{
	if (opcode == PSO_FIRST) {
		while (this->next->Opcode() != PSO_LAST) this->next->Remove();
	} else if (opcode == PSO_LAST) {
	} else NOT_REACHED();
}

/*static*/ void SignalSpecial::link(SignalSpecial *first, SignalSpecial *last)
{
	assert(first->opcode == PSO_FIRST && last->opcode == PSO_LAST);
	first->next    = last;
	last->previous = first;
}

void SignalSpecial::Evaluate(SignalVM &vm)
{
	if (this->opcode == PSO_FIRST) {
		DEBUG(misc, 7, "  Executing First");
		vm.instruction = this->next;
	} else {
		DEBUG(misc, 7, "  Executing Last");
		vm.instruction = NULL;
	}
}
/*virtual*/ void SignalSpecial::SetNext(SignalInstruction *next_insn)
{
	this->next = next_insn;
}

SignalIf::PseudoInstruction::PseudoInstruction(SignalProgram *prog, SignalOpcode op) 
	: SignalInstruction(prog, op)
	{}

SignalIf::PseudoInstruction::PseudoInstruction(SignalProgram *prog, SignalIf *block, SignalOpcode op) 
	: SignalInstruction(prog, op)
{
	this->block = block;
	if (op == PSO_IF_ELSE) {
		previous = block;
	} else if (op == PSO_IF_ENDIF) {
		previous = block->if_true;
	} else NOT_REACHED();
}

/*virtual*/ void SignalIf::PseudoInstruction::Remove()
{
	if (opcode == PSO_IF_ELSE) {
		this->block->if_true = NULL;
		while(this->block->if_false) this->block->if_false->Remove();
	} else if (opcode == PSO_IF_ENDIF) {
		this->block->if_false = NULL;
	} else NOT_REACHED();
	delete this;
}

/*virtual*/ void SignalIf::PseudoInstruction::Evaluate(SignalVM &vm)
{
	DEBUG(misc, 7, "  Executing If Pseudo Instruction %s", opcode == PSO_IF_ELSE ? "Else" : "Endif");
	vm.instruction = this->block->after;
}

/*virtual*/ void SignalIf::PseudoInstruction::SetNext(SignalInstruction *next_insn)
{
	if (this->opcode == PSO_IF_ELSE) {
		this->block->if_false = next_insn;
	} else if (this->opcode == PSO_IF_ENDIF) {
		this->block->after = next_insn;
	} else NOT_REACHED();
}

SignalIf::SignalIf(SignalProgram *prog, bool raw)
	: SignalInstruction(prog, PSO_IF)
{
	if (!raw) {
		this->condition = new SignalSimpleCondition(PSC_ALWAYS);
		this->if_true   = new PseudoInstruction(prog, this, PSO_IF_ELSE);
		this->if_false  = new PseudoInstruction(prog, this, PSO_IF_ENDIF);
		this->after     = NULL;
	}
}

/*virtual*/ void SignalIf::Remove()
{
	delete this->condition;
	while (this->if_true)  this->if_true->Remove();
	
	this->previous->SetNext(this->after);
	this->after->SetPrevious(this->previous);
	delete this;
}

/*virtual*/ void SignalIf::Insert(SignalInstruction *before_insn)
{
	this->previous = before_insn->Previous();
	before_insn->Previous()->SetNext(this);
	before_insn->SetPrevious(this->if_false);
	this->after    = before_insn;
}

void SignalIf::SetCondition(SignalCondition *cond)
{
	assert(cond != this->condition);
	delete this->condition;
	this->condition = cond;
}

/*virtual*/ void SignalIf::Evaluate(SignalVM &vm)
{
	bool is_true = this->condition->Evaluate(vm);
	DEBUG(misc, 7, "  Executing If, taking %s branch", is_true ? "then" : "else");
	if (is_true) {
		vm.instruction = this->if_true;
	} else {
		vm.instruction = this->if_false;
	}
}

/*virtual*/ void SignalIf::SetNext(SignalInstruction *next_insn)
{
	this->if_true = next_insn;
}



SignalSet::SignalSet(SignalProgram *prog, SignalState state)
	: SignalInstruction(prog, PSO_SET_SIGNAL)
{
	this->to_state = state;
}

/*virtual*/ void SignalSet::Remove()
{
	this->next->SetPrevious(this->previous);
	this->previous->SetNext(this->next);
	delete this;
}

/*virtual*/ void SignalSet::Evaluate(SignalVM &vm)
{
	DEBUG(misc, 7, "  Executing SetSignal, making %s", this->to_state? "green" : "red");
	vm.state       = this->to_state;
	vm.instruction = NULL;
}


/*virtual*/ void SignalSet::SetNext(SignalInstruction *next_insn)
{
	this->next = next_insn;
}

static SignalProgram *GetExistingSignalProgram(SignalReference ref)
{
	ProgramList::iterator i = _signal_programs.find(ref);
	if (i != _signal_programs.end()) {
		assert(i->first == ref);
		return i->second;
	} else {
		return NULL;
	}
}


SignalProgram *GetSignalProgram(SignalReference ref)
{
	SignalProgram *pr = GetExistingSignalProgram(ref);
	if (!pr) {
		pr = new SignalProgram(ref.tile, ref.track);	
		_signal_programs[ref] = pr;
	} else assert(pr->tile == ref.tile && pr->track == ref.track);
	return pr;
}

void FreeSignalProgram(SignalReference ref)
{
	DeleteWindowById(WC_SIGNAL_PROGRAM, (ref.tile << 3) | ref.track);
	ProgramList::iterator i = _signal_programs.find(ref);
	if (i != _signal_programs.end()) {
		delete i->second;
		_signal_programs.erase(i);
	}
}

void FreeSignalPrograms()
{
	ProgramList::iterator i, e;
	for (i = _signal_programs.begin(), e = _signal_programs.end(); i != e;) {
		delete i->second;
		// Must postincrement here to avoid iterator invalidation
		_signal_programs.erase(i++);
	}
}

SignalState RunSignalProgram(SignalReference ref, uint num_exits, uint num_green)
{
	SignalProgram *program = GetSignalProgram(ref);
	SignalVM vm;
	vm.program = program;
	vm.num_exits = num_exits;
	vm.num_green = num_green;
	
	vm.instruction = program->first_instruction;
	vm.state = SIGNAL_STATE_RED;
	
	DEBUG(misc, 7, "%d exits, of which %d green", vm.num_exits, vm.num_green);
	vm.Execute();
	DEBUG(misc, 7, "Returning %s", vm.state == SIGNAL_STATE_GREEN ? "green" : "red");	
	return vm.state;
}

void RemoveProgramDependencies(SignalReference by, SignalReference on)
{
	SignalProgram *prog = GetSignalProgram(by);
	for (SignalInstruction **b = prog->instructions.Begin(), **i = b, **e = prog->instructions.End();
			i != e; i++) {
		SignalInstruction *insn = *i;
		if (insn->Opcode() == PSO_IF) {
			SignalIf* ifi = static_cast<SignalIf*>(insn);
			if (ifi->condition->ConditionCode() == PSC_SIGNAL_STATE) {
				SignalStateCondition* c = static_cast<SignalStateCondition*>(ifi->condition);
				if(c->sig_tile == by.tile && TrackdirToTrack(c->sig_track) == by.track)
					c->Invalidate();
				}
			}
		}
		
		AddTrackToSignalBuffer(by.tile, by.track, GetTileOwner(by.tile));
	UpdateSignalsInBuffer();
}

void SignalProgram::DebugPrintProgram()
{
	DEBUG(misc, 5, "Program %p listing", this);
	for (SignalInstruction **b = this->instructions.Begin(), **i = b, **e = this->instructions.End();
			i != e; i++)
	{
		SignalInstruction *insn = *i;
		DEBUG(misc, 5, " %ld: Opcode %d, prev %d", long(i - b), int(insn->Opcode()), 
					int(insn->Previous() ? insn->Previous()->Id() : -1));
	}
}

/** Insert a signal instruction into the signal program.
 *
 * @param tile The Tile on which to perform the operation
 * @param p1 Flags and information
 *   - Bits 0-2    Which track the signal sits on
 *   - Bits 3-18   ID of instruction to insert before
 *   - Bits 19-26  Which opcode to create
 *   - Bits 27-31  Reserved
 * @param p2 Depends upon instruction
 *   - PSO_SET_SIGNAL:
 *       - Colour to set the signal to
 * @param text unused
 */
CommandCost CmdInsertSignalInstruction(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Track track         = Extract<Track,        0,   3>(p1);
	uint instruction_id = GB(p1, 3, 16);
	SignalOpcode op     = Extract<SignalOpcode, 19,  8>(p1);
	
	if (!IsValidTrack(track) || !IsPlainRailTile(tile) || !HasTrack(tile, track)) {
		return CMD_ERROR;
	}
	
	if (!IsTileOwner(tile, _current_company)) 
		return_cmd_error(STR_ERROR_AREA_IS_OWNED_BY_ANOTHER);
	
	SignalProgram *prog = GetExistingSignalProgram(SignalReference(tile, track));
	if (!prog) 
		return_cmd_error(STR_ERR_PROGSIG_NOT_THERE);
	if (instruction_id > prog->instructions.Length())
		return_cmd_error(STR_ERR_PROGSIG_INVALID_INSTRUCTION);

	bool exec = (flags & DC_EXEC) != 0;
	
	SignalInstruction *insert_before = prog->instructions[instruction_id];
	switch (op) {
		case PSO_IF: {
			if (!exec) return CommandCost();
			SignalIf *if_ins = new SignalIf(prog);
			if_ins->Insert(insert_before);
			break;
		}
		
		case PSO_SET_SIGNAL: {
			SignalState ss = (SignalState) p2;
			if (ss > SIGNAL_STATE_MAX) return_cmd_error(STR_ERR_PROGSIG_INVALID_OPCODE);
			if (!exec) return CommandCost();
			
			SignalSet *set = new SignalSet(prog, ss);
			set->Insert(insert_before);
			break;
		}
		
		case PSO_FIRST:
		case PSO_LAST:
		case PSO_IF_ELSE:
		case PSO_IF_ENDIF:
		default:
			return_cmd_error(STR_ERR_PROGSIG_INVALID_OPCODE);
	}
	
	if (!exec) return CommandCost();
	AddTrackToSignalBuffer(tile, track, GetTileOwner(tile));
	UpdateSignalsInBuffer();
	InvalidateWindowData(WC_SIGNAL_PROGRAM, (tile << 3) | track);
	return CommandCost();
}

/** Modify a singal instruction
 *
 * @param tile The Tile on which to perform the operation
 * @param p1 Flags and information
 *   - Bits 0-2    Which track the signal sits on
 *   - Bits 3-18   ID of instruction to insert before
 * @param p2 Depends upon instruction
 *   - PSO_SET_SIGNAL:
 *       - Colour to set the signal to
 *   - PSO_IF:
 *       - Bit 0 If 0, set the condidion code:
 *         - Bit 1-8:  Conditon code to change to
 *       - Otherwise, if SignalVariableCondition:
 *        - Bits 1-2:  Which field to change (ConditionField)
 *        - Bits 3-31: Value to set field to
 *       - Otherwise, if SignalStateCondition:
 *        - Bits 1-4:  Trackdir on which signal is located
 *        - Bits 5-31: Tile on which signal is located
 * @param text unused
 */
CommandCost CmdModifySignalInstruction(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Track track         = Extract<Track, 0, 3 >(p1);
	uint instruction_id = GB(p1, 3, 16);
	
	if (!IsValidTrack(track) || !IsPlainRailTile(tile) || !HasTrack(tile, track)) {
		return CMD_ERROR;
	}
	
	if (!IsTileOwner(tile, _current_company)) 
		return_cmd_error(STR_ERROR_AREA_IS_OWNED_BY_ANOTHER);
	
	SignalProgram *prog = GetExistingSignalProgram(SignalReference(tile, track));
	if (!prog) 
		return_cmd_error(STR_ERR_PROGSIG_NOT_THERE);
	
	if (instruction_id > prog->instructions.Length())
		return_cmd_error(STR_ERR_PROGSIG_INVALID_INSTRUCTION);

	bool exec = (flags & DC_EXEC) != 0;
	
	SignalInstruction *insn = prog->instructions[instruction_id];
	switch (insn->Opcode()) {
		case PSO_SET_SIGNAL: {
			SignalState state = (SignalState) p2;
			if (state > SIGNAL_STATE_MAX) 
				return_cmd_error(STR_ERR_PROGSIG_INVALID_SIGNAL_STATE);
			if (!exec) 
				return CommandCost();
			SignalSet *ss = static_cast<SignalSet*>(insn);
			ss->to_state = state;
		} break;
		
		case PSO_IF: {
			SignalIf *si = static_cast<SignalIf*>(insn);
			byte act = GB(p2, 0, 1);
			if (act == 0) { // Set code
				SignalConditionCode code = (SignalConditionCode) GB(p2, 1, 8);
				if (code > PSC_MAX)
					return_cmd_error(STR_ERR_PROGSIG_INVALID_CONDITION);
				if (!exec) return CommandCost();
				
				SignalCondition *cond;
				switch (code) {
					case PSC_ALWAYS:
					case PSC_NEVER:
						cond = new SignalSimpleCondition(code);
						break;
						
					case PSC_NUM_GREEN:
					case PSC_NUM_RED:
						cond = new SignalVariableCondition(code);
						break;
						
					case PSC_SIGNAL_STATE:
						cond = new SignalStateCondition(SignalReference(tile, track), INVALID_TILE, INVALID_TRACKDIR);
						break;
						
					default: NOT_REACHED();
				}
				si->SetCondition(cond);
			} else { // modify condition
				switch (si->condition->ConditionCode()) {
					case PSC_ALWAYS:
					case PSC_NEVER:
						return CommandCost(STR_ERR_PROGSIG_INVALID_CONDITION_FIELD);
						
					case PSC_NUM_GREEN:
					case PSC_NUM_RED: {
						SignalVariableCondition *vc = static_cast<SignalVariableCondition*>(si->condition);
						SignalConditionField f = (SignalConditionField) GB(p2, 1, 2);
						uint32 val = GB(p2, 3, 27);
						if (f == SCF_COMPARATOR) {
							if (val > SGC_LAST) return_cmd_error(STR_ERR_PROGSIG_INVALID_COMPARATOR);
							if (!exec) return CommandCost();						
							vc->comparator = (SignalComparator) val;
						} else if (f == SCF_VALUE) {
							if (!exec) return CommandCost();
							vc->value = val;
						} else CommandCost(STR_ERR_PROGSIG_INVALID_CONDITION_FIELD);
					} break;
						
					case PSC_SIGNAL_STATE: {
						SignalStateCondition *sc = static_cast<SignalStateCondition*>(si->condition);
						Trackdir  td = (Trackdir)  GB(p2, 1, 4);
						TileIndex ti = (TileIndex) GB(p2, 5, 27);
						
						if (!IsValidTile(ti) || !IsValidTrackdir(td) || !HasSignalOnTrackdir(ti, td)
								|| GetTileOwner(ti) != _current_company)
							return_cmd_error(STR_ERR_PROGSIG_INVALID_SIGNAL);
						if (!exec) return CommandCost();
						sc->SetSignal(ti, td);
					} break;
				}
			}
		} break;
		
		case PSO_FIRST:
		case PSO_LAST:
		case PSO_IF_ELSE:
		case PSO_IF_ENDIF:
		default:
			return CommandCost(STR_ERR_PROGSIG_INVALID_OPCODE);
	}
	
	if (!exec) return CommandCost();
	
	AddTrackToSignalBuffer(tile, track, GetTileOwner(tile));
	UpdateSignalsInBuffer();
	InvalidateWindowData(WC_SIGNAL_PROGRAM, (tile << 3) | track);
	return CommandCost();
}

/** Remove an instruction from a signal program
 *
 * @param tile The Tile on which to perform the operation
 * @param p1 Flags and information
 *   - Bits 0-2    Which track the signal sits on
 *   - Bits 3-18   ID of instruction to insert before
 * @param p2 unused
 * @param text unused
 */
CommandCost CmdRemoveSignalInstruction(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Track track         = Extract<Track, 0, 3 >(p1);
	uint instruction_id = GB(p1, 3, 16);
	
	if (!IsValidTrack(track) || !IsPlainRailTile(tile) || !HasTrack(tile, track)) {
		return CMD_ERROR;
	}
	
	if (!IsTileOwner(tile, _current_company)) 
		return_cmd_error(STR_ERROR_AREA_IS_OWNED_BY_ANOTHER);
	
	SignalProgram *prog = GetExistingSignalProgram(SignalReference(tile, track));
	if (!prog) 
		return_cmd_error(STR_ERR_PROGSIG_NOT_THERE);
	
	if (instruction_id > prog->instructions.Length())
		return_cmd_error(STR_ERR_PROGSIG_INVALID_INSTRUCTION);

	bool exec = (flags & DC_EXEC) != 0;
	
	SignalInstruction *insn = prog->instructions[instruction_id];
	switch (insn->Opcode()) {
		case PSO_SET_SIGNAL:
		case PSO_IF:
			if (!exec) return CommandCost();
			insn->Remove();
			break;
		
		case PSO_FIRST:
		case PSO_LAST:
		case PSO_IF_ELSE:
		case PSO_IF_ENDIF:
		default:
			return_cmd_error(STR_ERR_PROGSIG_INVALID_OPCODE);
	}
	
	if (!exec) return CommandCost();
	AddTrackToSignalBuffer(tile, track, GetTileOwner(tile));
	UpdateSignalsInBuffer();
	InvalidateWindowData(WC_SIGNAL_PROGRAM, (tile << 3) | track);
	return CommandCost();
}