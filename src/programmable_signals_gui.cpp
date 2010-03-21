/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file programmable_signals_gui.cpp GUI related to programming signals */

#include "stdafx.h"
#include "programmable_signals.h"
#include "command_func.h"
#include "window_func.h"
#include "strings_func.h"
#include "widgets/dropdown_func.h"
#include "gui.h"
#include "gfx_func.h"

#include "table/sprites.h"
#include "table/strings.h"

enum ProgramWindowWidgets {
	PROGRAM_WIDGET_CAPTION,
	PROGRAM_WIDGET_INSTRUCTION_LIST,
	PROGRAM_WIDGET_SCROLLBAR,
};

/** Get the string for a condition */
static char* GetConditionString(SignalCondition *cond, char* buf, char* buflast)
{
	StringID string = INVALID_STRING_ID;
	switch(cond->ConditionCode()) {
		case PSC_ANYGREEN: string = STR_PROGSIG_COND_ANY_GREEN; break;
		case PSC_ANYRED:   string = STR_PROGSIG_COND_ANY_RED;   break;
		case PSC_ALWAYS:   string = STR_PROGSIG_COND_ALWAYS;    break;
		case PSC_NEVER:    string = STR_PROGSIG_COND_NEVER;    break;
	}
	return GetString(buf, string, buflast);
}

/**
 * Draws an instruction in the programming GUI
 * @param instruction The instruction to draw
 * @param y Y position for drawing
 * @param selected True, if the order is selected
 * @param indent How many levels the instruction is indented
 * @param left Left border for text drawing
 * @param right Right border for text drawing
 */
static void DrawInstructionString(SignalInstruction *instruction, int y, bool selected, int indent, int left, int right)
{
	StringID instruction_string = INVALID_STRING_ID;
	
	char condstr[512];
	
	switch (instruction->Opcode()) {
		case PSO_FIRST:
			instruction_string = STR_PROGSIG_FIRST;
			break;
			
		case PSO_LAST:
			instruction_string = STR_PROGSIG_LAST;
			break;
			
		case PSO_IF: {
			SignalIf *if_ins = static_cast<SignalIf*>(instruction);
			GetConditionString(if_ins->condition, condstr, lastof(condstr));
			SetDParamStr(0, condstr);
			instruction_string = STR_PROGSIG_IF;
			break;
		}
			
		case PSO_IF_ELSE:
			instruction_string = STR_PROGSIG_ELSE;
			break;
			
		case PSO_IF_ENDIF:
			instruction_string = STR_PROGSIG_ENDIF;
			break;
			
		case PSO_SET_SIGNAL: {
			instruction_string = STR_PROGSIG_SET_SIGNAL;
			SignalSet *set = static_cast<SignalSet*>(instruction);
			SetDParam(0, set->to_state ? STR_COLOUR_GREEN : STR_COLOUR_RED);
			break;
		}

		default: NOT_REACHED();
	}

	DrawString(left + indent * 16, right, y, instruction_string, selected ? TC_WHITE : TC_BLACK);
}

struct GuiInstruction {
	SignalInstruction *insn;
	uint indent;
};

typedef SmallVector<GuiInstruction, 4> GuiInstructionList;

class ProgramWindow: public Window {
public:
	ProgramWindow(const WindowDesc* desc, TileIndex tile, Track track)
	{
		this->InitNested(desc, GetSignalId(tile, track));
		this->tile = tile;
		this->track = track;
		program = GetSignalProgram(tile, track);
		RebuildInstructionList();
	}
	
	virtual void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		switch (widget) {
			case PROGRAM_WIDGET_INSTRUCTION_LIST:
				resize->height = FONT_HEIGHT_NORMAL;
				size->height = 6 * resize->height + WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
				break;
		}
	}
	
	virtual void OnResize()
	{
		/* Update the scroll bar */
		this->vscroll.SetCapacityFromWidget(this, PROGRAM_WIDGET_INSTRUCTION_LIST);
	}
	
	virtual void OnPaint()
	{
		this->DrawWidgets();
	}
	
	virtual void DrawWidget(const Rect &r, int widget) const
	{
		if (widget != PROGRAM_WIDGET_INSTRUCTION_LIST) return;
	
		int y = r.top + WD_FRAMERECT_TOP;
		int line_height = this->GetWidget<NWidgetBase>(PROGRAM_WIDGET_INSTRUCTION_LIST)->resize_y;

		uint no = this->vscroll.GetPosition();

		for(const GuiInstruction *i = instructions.Begin() + no, *e = instructions.End();
				i != e; ++i, no++) {
			/* Don't draw anything if it extends past the end of the window. */
			if (!this->vscroll.IsVisible(no)) break;

			DrawInstructionString(i->insn, y, no == this->selected_instruction, i->indent, r.left + WD_FRAMETEXT_LEFT, r.right - WD_FRAMETEXT_RIGHT);
			y += line_height;
		}
	}
private:
	void RebuildInstructionList()
	{
		this->instructions.Clear();
		SignalInstruction *insn = program->first_instruction;
		uint indent = 0;
		
		do {
			DEBUG(misc, 5, "PSig Gui: Opcode %d", insn->Opcode());
			switch(insn->Opcode()) {
				case PSO_FIRST:
				case PSO_LAST: {
					SignalSpecial *s = static_cast<SignalSpecial*>(insn);
					GuiInstruction* gi = this->instructions.Append();
					gi->insn   = s;
					gi->indent = indent;
					insn = s->next;
					break;
				}
				
				case PSO_IF: {
					SignalIf *i = static_cast<SignalIf*>(insn);
					GuiInstruction* gi = this->instructions.Append();
					gi->insn   = i;
					gi->indent = indent++;
					insn = i->if_true;
					break;
				}
				
				case PSO_IF_ELSE: {
					SignalIf::PseudoInstruction *p = static_cast<SignalIf::PseudoInstruction*>(insn);
					GuiInstruction* gi = this->instructions.Append();
					gi->insn   = p;
					gi->indent = indent - 1;
					insn = p->block->if_false;
					break;
				}
				
				case PSO_IF_ENDIF: {
					SignalIf::PseudoInstruction *p = static_cast<SignalIf::PseudoInstruction*>(insn);
					GuiInstruction* gi = this->instructions.Append();
					gi->insn   = p;
					gi->indent = --indent;
					insn = p->block->after;
					break;
				}
				
				case PSO_SET_SIGNAL: {
					SignalSet *s = static_cast<SignalSet*>(insn);
					GuiInstruction* gi = this->instructions.Append();
					gi->insn   = s;
					gi->indent = indent;
					insn = s->next;
					break;
				}
				
				default: NOT_REACHED();
			}
		} while(insn);
		
		this->vscroll.SetCount(this->instructions.Length());
		this->SetDirty();
	}
	
	TileIndex tile;
	Track track;
	SignalProgram *program;
	GuiInstructionList instructions;
	uint selected_instruction;
};

static const NWidgetPart _nested_program_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, PROGRAM_WIDGET_CAPTION), SetDataTip(STR_PROGSIG_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY, PROGRAM_WIDGET_INSTRUCTION_LIST), SetMinimalSize(372, 62), SetDataTip(0x0, STR_PROGSIG_CAPTION), SetResize(1, 1), EndContainer(),
		NWidget(WWT_SCROLLBAR, COLOUR_GREY, PROGRAM_WIDGET_SCROLLBAR),
	EndContainer(),
};

static const WindowDesc _program_desc(
	WDP_AUTO, 384, 100,
	WC_SIGNAL_PROGRAM, WC_BUILD_SIGNAL,
	WDF_CONSTRUCTION,
	_nested_program_widgets, lengthof(_nested_program_widgets)
);

void ShowSignalProgramWindow(TileIndex tile, Track track)
{
	uint32 signal_id = GetSignalId(tile, track);
	if (BringWindowToFrontById(WC_SIGNAL_PROGRAM, signal_id) != NULL) return;

	new ProgramWindow(&_program_desc, tile, track);
}