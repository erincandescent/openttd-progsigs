/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file programmable_signals.h Programmable Signals */

#ifndef PROGRAMMABLE_SIGNALS_H
#define PROGRAMMABLE_SIGNALS_H
#include "strings.h"
#include "rail_map.h"

/** The Programmable Signal virtual machine.
 *
 * This structure contains the state of the currently executing signal program.
 */
struct SignalVM;

/** Programmable Signal opcode.
 *
 * Opcode types are discriminated by this enumeration. It is primarily used for
 * code which must be able to inspect the type of a signal operation, rather than
 * evaluate it (such as the programming GUI)
 */
enum SignalOpcode {
	PSO_FIRST      = 0,     ///< Start pseudo instruction
	PSO_LAST       = 1,     ///< End pseudo instruction
	PSO_IF         = 2,     ///< If instruction
	PSO_IF_ELSE    = 3,     ///< If Else pseudo instruction
	PSO_IF_ENDIF   = 4,     ///< If Endif pseudo instruction
	PSO_SET_SIGNAL = 5,     ///< Set signal instruction
};

/** Signal instruction base class. All instructions must derive from this. */
class SignalInstruction {
public:
	/// Get the instruction's opcode
	inline SignalOpcode Opcode() const { return this->opcode; }
	
	/// Get the previous instruction. If this is NULL, then this is the first
	/// instruction.
	inline SignalInstruction* Previous() const { return this->previous; }
	
	/// Insert this instruction, placing it before @p before_insn
	virtual void Insert(SignalInstruction* before_insn);
	
	/// Evaluate the instruction. The instruction should update the VM state.
	virtual void Evaluate(SignalVM& vm) = 0;
	
	/// Remove the instruction. When removing itself, an instruction should
	/// <ul>
	///   <li>Set next->previous to previous
	///   <li>Set previous->next to next
	///   <li>Destroy any other children
	/// </ul>
	virtual void Remove() = 0;
	
protected:
	inline SignalInstruction(SignalOpcode op) : opcode(op), previous(NULL) {}
	inline ~SignalInstruction() {}
	
	/// Set the next instruction. This should only be called by instructions
	/// manipulating their own position.
	virtual void SetNext(SignalInstruction* next_insn) = 0;
	
	/// Set another instruction's next instruction
	inline void SetOtherNext(SignalInstruction* other, SignalInstruction* next)
	{ other->SetNext(next); }

	/// Set another instruction's next instruction
	inline void SetOtherPrevious(SignalInstruction* other, SignalInstruction* prev)
	{ assert(prev); other->previous = prev; }

	const SignalOpcode opcode;
	SignalInstruction* previous;
};

/** Programmable Signal condition code.
 *
 * These discriminate conditions in much the same way that SignalOpcode 
 * discriminates instructions.
 */
enum SignalConditionCode {
	PSC_ANYGREEN = 0,	///< If there are any green signals behind this signal
	PSC_ANYRED = 1,   ///< If there are any red signals behind this signal
	PSC_ALWAYS = 2,   ///< Always true
	PSC_NEVER = 3,    ///< Always false
};

class SignalCondition {
public:
	/// Get the condition's code
	inline SignalConditionCode ConditionCode() const { return this->cond_code; }
	
	/// Evaluate the condition
	virtual bool Evaluate(SignalVM& vm) = 0;
	
	/// Destroy the condition. Any children should also be destroyed
	virtual ~SignalCondition();
	
protected:
	SignalCondition(SignalConditionCode code) : cond_code(code) {}
	
	const SignalConditionCode cond_code;
};

// -- Condition codes --
/** Simple condition code. These conditions have no complex inputs, and can be
 *  evaluated directly from VM state and their condition code.
 */
class SignalSimpleCondition: public SignalCondition {
	public:
		SignalSimpleCondition(SignalConditionCode code);
		virtual bool Evaluate(SignalVM& vm);
};

// -- Instructions

/** The special start and end pseudo instructions */
class SignalSpecial: public SignalInstruction {
public:
	SignalSpecial(SignalOpcode op);
	
	virtual void Evaluate(SignalVM& vm);
	
	static void link(SignalSpecial* first, SignalSpecial* last);
	
	virtual void Remove();
	
protected:
	virtual void SetNext(SignalInstruction* next_insn);
	SignalInstruction* next;
};

/** If signal instruction. This is perhaps the most important, as without it,
 *  programmable signals are pretty useless.
 *
 * It's also the most complex!
 */
class SignalIf: public SignalInstruction {
public:
	class PseudoInstruction: public SignalInstruction {
		friend class SignalIf;
	private:
		/** The If-Else and If-Endif pseudo instructions. The Else instruction 
		 * follows the Then block, and the Endif instruction follows the Else block.
		 *
		 * These serve two purposes:
		 * <ul>
		 *  <li>They correctly vector the execution to after the if block 
		 *      (if needed)
		 *  <li>They provide an instruction for the GUI to insert other instructions
		 *      before.
		 * </ul>
		 */
		PseudoInstruction(SignalIf* block, SignalOpcode op);
		virtual void Remove();
		
	public:
		virtual void Evaluate(SignalVM& vm);
		
	protected:
		virtual void SetNext(SignalInstruction* next_insn);
		
	private:
		SignalIf* block;
	};
	
public:
	SignalIf();
	void SetCondition(SignalCondition* cond);
	virtual void Evaluate(SignalVM& vm);
	virtual void Insert(SignalInstruction* before_insn);
	virtual void Remove();
	
	SignalCondition* condition;
	SignalInstruction* if_true;
	SignalInstruction* if_false;
	SignalInstruction* after;
	
protected:
	virtual void SetNext(SignalInstruction* next_insn);
};

/** Set signal instruction. This sets the state of the signal and terminates execution */
class SignalSet: public SignalInstruction {
public:
	SignalSet(bool state = false);
	virtual void Evaluate(SignalVM& vm);
	virtual void Remove();
	
	bool to_state;
	
	SignalInstruction* next;
	
protected:
	virtual void SetNext(SignalInstruction* next_insn);
};

/** The actual programmable signal information */
struct SignalProgram {
	SignalProgram();
	~SignalProgram();
	
	SignalSpecial* first_instruction;
	SignalSpecial* last_instruction;
};

SignalProgram* GetSignalProgram(TileIndex t, Track track);
void FreeSignalProgram(TileIndex t, Track track);
bool RunSignalProgram(TileIndex t, Track track, uint num_exits, uint num_green);

#endif