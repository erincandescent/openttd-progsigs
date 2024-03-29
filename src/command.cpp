/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file command.cpp Handling of commands. */

#include "stdafx.h"
#include "openttd.h"
#include "landscape.h"
#include "gui.h"
#include "command_func.h"
#include "network/network.h"
#include "genworld.h"
#include "newgrf_storage.h"
#include "strings_func.h"
#include "gfx_func.h"
#include "functions.h"
#include "town.h"
#include "date_func.h"
#include "debug.h"
#include "company_func.h"
#include "company_base.h"
#include "signal_func.h"

#include "table/strings.h"

CommandProc CmdBuildRailroadTrack;
CommandProc CmdRemoveRailroadTrack;
CommandProc CmdBuildSingleRail;
CommandProc CmdRemoveSingleRail;

CommandProc CmdLandscapeClear;

CommandProc CmdBuildBridge;

CommandProc CmdBuildRailStation;
CommandProc CmdRemoveFromRailStation;
CommandProc CmdConvertRail;

CommandProc CmdBuildSingleSignal;
CommandProc CmdRemoveSingleSignal;

CommandProc CmdTerraformLand;

CommandProc CmdPurchaseLandArea;
CommandProc CmdSellLandArea;

CommandProc CmdBuildTunnel;

CommandProc CmdBuildTrainDepot;
CommandProc CmdBuildRailWaypoint;
CommandProc CmdRenameWaypoint;
CommandProc CmdRemoveFromRailWaypoint;

CommandProc CmdBuildRoadStop;
CommandProc CmdRemoveRoadStop;

CommandProc CmdBuildLongRoad;
CommandProc CmdRemoveLongRoad;
CommandProc CmdBuildRoad;

CommandProc CmdBuildRoadDepot;

CommandProc CmdBuildAirport;

CommandProc CmdBuildDock;

CommandProc CmdBuildShipDepot;

CommandProc CmdBuildBuoy;

CommandProc CmdPlantTree;

CommandProc CmdBuildRailVehicle;
CommandProc CmdMoveRailVehicle;

CommandProc CmdSellRailWagon;

CommandProc CmdSendTrainToDepot;
CommandProc CmdForceTrainProceed;
CommandProc CmdReverseTrainDirection;

CommandProc CmdModifyOrder;
CommandProc CmdSkipToOrder;
CommandProc CmdDeleteOrder;
CommandProc CmdInsertOrder;
CommandProc CmdChangeServiceInt;
CommandProc CmdRestoreOrderIndex;

CommandProc CmdBuildIndustry;

CommandProc CmdBuildCompanyHQ;
CommandProc CmdSetCompanyManagerFace;
CommandProc CmdSetCompanyColour;

CommandProc CmdIncreaseLoan;
CommandProc CmdDecreaseLoan;

CommandProc CmdWantEnginePreview;

CommandProc CmdRenameVehicle;
CommandProc CmdRenameEngine;

CommandProc CmdRenameCompany;
CommandProc CmdRenamePresident;

CommandProc CmdRenameStation;

CommandProc CmdSellAircraft;
CommandProc CmdBuildAircraft;
CommandProc CmdSendAircraftToHangar;
CommandProc CmdRefitAircraft;

CommandProc CmdPlaceSign;
CommandProc CmdRenameSign;

CommandProc CmdBuildRoadVeh;
CommandProc CmdSellRoadVeh;
CommandProc CmdSendRoadVehToDepot;
CommandProc CmdTurnRoadVeh;
CommandProc CmdRefitRoadVeh;

CommandProc CmdPause;

CommandProc CmdBuyShareInCompany;
CommandProc CmdSellShareInCompany;
CommandProc CmdBuyCompany;

CommandProc CmdFoundTown;

CommandProc CmdRenameTown;
CommandProc CmdDoTownAction;

CommandProc CmdChangeSetting;
CommandProc CmdChangeCompanySetting;

CommandProc CmdSellShip;
CommandProc CmdBuildShip;
CommandProc CmdSendShipToDepot;
CommandProc CmdRefitShip;

CommandProc CmdOrderRefit;
CommandProc CmdCloneOrder;

CommandProc CmdClearArea;

CommandProc CmdGiveMoney;
CommandProc CmdMoneyCheat;
CommandProc CmdBuildCanal;
CommandProc CmdBuildLock;

CommandProc CmdCompanyCtrl;

CommandProc CmdLevelLand;

CommandProc CmdRefitRailVehicle;

CommandProc CmdBuildSignalTrack;
CommandProc CmdRemoveSignalTrack;

CommandProc CmdSetAutoReplace;

CommandProc CmdCloneVehicle;
CommandProc CmdStartStopVehicle;
CommandProc CmdMassStartStopVehicle;
CommandProc CmdAutoreplaceVehicle;
CommandProc CmdDepotSellAllVehicles;
CommandProc CmdDepotMassAutoReplace;

CommandProc CmdCreateGroup;
CommandProc CmdRenameGroup;
CommandProc CmdDeleteGroup;
CommandProc CmdAddVehicleGroup;
CommandProc CmdAddSharedVehicleGroup;
CommandProc CmdRemoveAllVehiclesGroup;
CommandProc CmdSetGroupReplaceProtection;

CommandProc CmdMoveOrder;
CommandProc CmdChangeTimetable;
CommandProc CmdSetVehicleOnTime;
CommandProc CmdAutofillTimetable;
CommandProc CmdSetTimetableStart;

CommandProc CmdInsertSignalInstruction;
CommandProc CmdModifySignalInstruction;
CommandProc CmdRemoveSignalInstruction;

#define DEF_CMD(proc, flags) {proc, #proc, flags}

/**
 * The master command table
 *
 * This table contains all possible CommandProc functions with
 * the flags which belongs to it. The indices are the same
 * as the value from the CMD_* enums.
 */
static const Command _command_proc_table[] = {
	DEF_CMD(CmdBuildRailroadTrack,       CMD_NO_WATER | CMD_AUTO), // CMD_BUILD_RAILROAD_TRACK
	DEF_CMD(CmdRemoveRailroadTrack,                     CMD_AUTO), // CMD_REMOVE_RAILROAD_TRACK
	DEF_CMD(CmdBuildSingleRail,          CMD_NO_WATER | CMD_AUTO), // CMD_BUILD_SINGLE_RAIL
	DEF_CMD(CmdRemoveSingleRail,                        CMD_AUTO), // CMD_REMOVE_SINGLE_RAIL
	DEF_CMD(CmdLandscapeClear,                                 0), // CMD_LANDSCAPE_CLEAR
	DEF_CMD(CmdBuildBridge,                             CMD_AUTO), // CMD_BUILD_BRIDGE
	DEF_CMD(CmdBuildRailStation,         CMD_NO_WATER | CMD_AUTO), // CMD_BUILD_RAIL_STATION
	DEF_CMD(CmdBuildTrainDepot,          CMD_NO_WATER | CMD_AUTO), // CMD_BUILD_TRAIN_DEPOT
	DEF_CMD(CmdBuildSingleSignal,                       CMD_AUTO), // CMD_BUILD_SIGNALS
	DEF_CMD(CmdRemoveSingleSignal,                      CMD_AUTO), // CMD_REMOVE_SIGNALS
	DEF_CMD(CmdTerraformLand,           CMD_ALL_TILES | CMD_AUTO), // CMD_TERRAFORM_LAND
	DEF_CMD(CmdPurchaseLandArea,         CMD_NO_WATER | CMD_AUTO), // CMD_PURCHASE_LAND_AREA
	DEF_CMD(CmdSellLandArea,                                   0), // CMD_SELL_LAND_AREA
	DEF_CMD(CmdBuildTunnel,                             CMD_AUTO), // CMD_BUILD_TUNNEL
	DEF_CMD(CmdRemoveFromRailStation,                          0), // CMD_REMOVE_FROM_RAIL_STATION
	DEF_CMD(CmdConvertRail,                                    0), // CMD_CONVERT_RAILD
	DEF_CMD(CmdBuildRailWaypoint,                              0), // CMD_BUILD_RAIL_WAYPOINT
	DEF_CMD(CmdRenameWaypoint,                                 0), // CMD_RENAME_WAYPOINT
	DEF_CMD(CmdRemoveFromRailWaypoint,                         0), // CMD_REMOVE_FROM_RAIL_WAYPOINT

	DEF_CMD(CmdBuildRoadStop,            CMD_NO_WATER | CMD_AUTO), // CMD_BUILD_ROAD_STOP
	DEF_CMD(CmdRemoveRoadStop,                                 0), // CMD_REMOVE_ROAD_STOP
	DEF_CMD(CmdBuildLongRoad,            CMD_NO_WATER | CMD_AUTO), // CMD_BUILD_LONG_ROAD
	DEF_CMD(CmdRemoveLongRoad,            CMD_NO_TEST | CMD_AUTO), // CMD_REMOVE_LONG_ROAD; towns may disallow removing road bits (as they are connected) in test, but in exec they're removed and thus removing is allowed.
	DEF_CMD(CmdBuildRoad,                CMD_NO_WATER | CMD_AUTO), // CMD_BUILD_ROAD
	DEF_CMD(CmdBuildRoadDepot,           CMD_NO_WATER | CMD_AUTO), // CMD_BUILD_ROAD_DEPOT

	DEF_CMD(CmdBuildAirport,             CMD_NO_WATER | CMD_AUTO), // CMD_BUILD_AIRPORT
	DEF_CMD(CmdBuildDock,                               CMD_AUTO), // CMD_BUILD_DOCK
	DEF_CMD(CmdBuildShipDepot,                          CMD_AUTO), // CMD_BUILD_SHIP_DEPOT
	DEF_CMD(CmdBuildBuoy,                               CMD_AUTO), // CMD_BUILD_BUOY
	DEF_CMD(CmdPlantTree,                               CMD_AUTO), // CMD_PLANT_TREE
	DEF_CMD(CmdBuildRailVehicle,                               0), // CMD_BUILD_RAIL_VEHICLE
	DEF_CMD(CmdMoveRailVehicle,                                0), // CMD_MOVE_RAIL_VEHICLE

	DEF_CMD(CmdSellRailWagon,                                  0), // CMD_SELL_RAIL_WAGON
	DEF_CMD(CmdSendTrainToDepot,                               0), // CMD_SEND_TRAIN_TO_DEPOT
	DEF_CMD(CmdForceTrainProceed,                              0), // CMD_FORCE_TRAIN_PROCEED
	DEF_CMD(CmdReverseTrainDirection,                          0), // CMD_REVERSE_TRAIN_DIRECTION

	DEF_CMD(CmdModifyOrder,                                    0), // CMD_MODIFY_ORDER
	DEF_CMD(CmdSkipToOrder,                                    0), // CMD_SKIP_TO_ORDER
	DEF_CMD(CmdDeleteOrder,                                    0), // CMD_DELETE_ORDER
	DEF_CMD(CmdInsertOrder,                                    0), // CMD_INSERT_ORDER

	DEF_CMD(CmdChangeServiceInt,                               0), // CMD_CHANGE_SERVICE_INT

	DEF_CMD(CmdBuildIndustry,                                  0), // CMD_BUILD_INDUSTRY
	DEF_CMD(CmdBuildCompanyHQ,           CMD_NO_WATER | CMD_AUTO), // CMD_BUILD_COMPANY_HQ
	DEF_CMD(CmdSetCompanyManagerFace,                          0), // CMD_SET_COMPANY_MANAGER_FACE
	DEF_CMD(CmdSetCompanyColour,                               0), // CMD_SET_COMPANY_COLOUR

	DEF_CMD(CmdIncreaseLoan,                                   0), // CMD_INCREASE_LOAN
	DEF_CMD(CmdDecreaseLoan,                                   0), // CMD_DECREASE_LOAN

	DEF_CMD(CmdWantEnginePreview,                              0), // CMD_WANT_ENGINE_PREVIEW

	DEF_CMD(CmdRenameVehicle,                                  0), // CMD_RENAME_VEHICLE
	DEF_CMD(CmdRenameEngine,                                   0), // CMD_RENAME_ENGINE

	DEF_CMD(CmdRenameCompany,                                  0), // CMD_RENAME_COMPANY
	DEF_CMD(CmdRenamePresident,                                0), // CMD_RENAME_PRESIDENT

	DEF_CMD(CmdRenameStation,                                  0), // CMD_RENAME_STATION

	DEF_CMD(CmdSellAircraft,                                   0), // CMD_SELL_AIRCRAFT

	DEF_CMD(CmdBuildAircraft,                                  0), // CMD_BUILD_AIRCRAFT
	DEF_CMD(CmdSendAircraftToHangar,                           0), // CMD_SEND_AIRCRAFT_TO_HANGAR
	DEF_CMD(CmdRefitAircraft,                                  0), // CMD_REFIT_AIRCRAFT

	DEF_CMD(CmdPlaceSign,                                      0), // CMD_PLACE_SIGN
	DEF_CMD(CmdRenameSign,                                     0), // CMD_RENAME_SIGN

	DEF_CMD(CmdBuildRoadVeh,                                   0), // CMD_BUILD_ROAD_VEH
	DEF_CMD(CmdSellRoadVeh,                                    0), // CMD_SELL_ROAD_VEH
	DEF_CMD(CmdSendRoadVehToDepot,                             0), // CMD_SEND_ROADVEH_TO_DEPOT
	DEF_CMD(CmdTurnRoadVeh,                                    0), // CMD_TURN_ROADVEH
	DEF_CMD(CmdRefitRoadVeh,                                   0), // CMD_REFIT_ROAD_VEH

	DEF_CMD(CmdPause,                                 CMD_SERVER), // CMD_PAUSE

	DEF_CMD(CmdBuyShareInCompany,                              0), // CMD_BUY_SHARE_IN_COMPANY
	DEF_CMD(CmdSellShareInCompany,                             0), // CMD_SELL_SHARE_IN_COMPANY
	DEF_CMD(CmdBuyCompany,                                     0), // CMD_BUY_COMANY

	DEF_CMD(CmdFoundTown,                            CMD_NO_TEST), // CMD_FOUND_TOWN; founding random town can fail only in exec run
	DEF_CMD(CmdRenameTown,                            CMD_SERVER), // CMD_RENAME_TOWN
	DEF_CMD(CmdDoTownAction,                                   0), // CMD_DO_TOWN_ACTION

	DEF_CMD(CmdSellShip,                                       0), // CMD_SELL_SHIP
	DEF_CMD(CmdBuildShip,                                      0), // CMD_BUILD_SHIP
	DEF_CMD(CmdSendShipToDepot,                                0), // CMD_SEND_SHIP_TO_DEPOT
	DEF_CMD(CmdRefitShip,                                      0), // CMD_REFIT_SHIP

	DEF_CMD(CmdOrderRefit,                                     0), // CMD_ORDER_REFIT
	DEF_CMD(CmdCloneOrder,                                     0), // CMD_CLONE_ORDER

	DEF_CMD(CmdClearArea,                            CMD_NO_TEST), // CMD_CLEAR_AREA; destroying multi-tile houses makes town rating differ between test and execution

	DEF_CMD(CmdMoneyCheat,                           CMD_OFFLINE), // CMD_MONEY_CHEAT
	DEF_CMD(CmdBuildCanal,                              CMD_AUTO), // CMD_BUILD_CANAL
	DEF_CMD(CmdCompanyCtrl,                        CMD_SPECTATOR), // CMD_COMPANY_CTRL

	DEF_CMD(CmdLevelLand, CMD_ALL_TILES | CMD_NO_TEST | CMD_AUTO), // CMD_LEVEL_LAND; test run might clear tiles multiple times, in execution that only happens once

	DEF_CMD(CmdRefitRailVehicle,                               0), // CMD_REFIT_RAIL_VEHICLE
	DEF_CMD(CmdRestoreOrderIndex,                              0), // CMD_RESTORE_ORDER_INDEX
	DEF_CMD(CmdBuildLock,                               CMD_AUTO), // CMD_BUILD_LOCK

	DEF_CMD(CmdBuildSignalTrack,                        CMD_AUTO), // CMD_BUILD_SIGNAL_TRACK
	DEF_CMD(CmdRemoveSignalTrack,                       CMD_AUTO), // CMD_REMOVE_SIGNAL_TRACK

	DEF_CMD(CmdGiveMoney,                                      0), // CMD_GIVE_MONEY
	DEF_CMD(CmdChangeSetting,                         CMD_SERVER), // CMD_CHANGE_SETTING
	DEF_CMD(CmdChangeCompanySetting,                           0), // CMD_CHANGE_COMPANY_SETTING
	DEF_CMD(CmdSetAutoReplace,                                 0), // CMD_SET_AUTOREPLACE
	DEF_CMD(CmdCloneVehicle,                         CMD_NO_TEST), // CMD_CLONE_VEHICLE; NewGRF callbacks influence building and refitting making it impossible to correctly estimate the cost
	DEF_CMD(CmdStartStopVehicle,                               0), // CMD_START_STOP_VEHICLE
	DEF_CMD(CmdMassStartStopVehicle,                           0), // CMD_MASS_START_STOP
	DEF_CMD(CmdAutoreplaceVehicle,                             0), // CMD_AUTOREPLACE_VEHICLE
	DEF_CMD(CmdDepotSellAllVehicles,                           0), // CMD_DEPOT_SELL_ALL_VEHICLES
	DEF_CMD(CmdDepotMassAutoReplace,                           0), // CMD_DEPOT_MASS_AUTOREPLACE
	DEF_CMD(CmdCreateGroup,                                    0), // CMD_CREATE_GROUP
	DEF_CMD(CmdDeleteGroup,                                    0), // CMD_DELETE_GROUP
	DEF_CMD(CmdRenameGroup,                                    0), // CMD_RENAME_GROUP
	DEF_CMD(CmdAddVehicleGroup,                                0), // CMD_ADD_VEHICLE_GROUP
	DEF_CMD(CmdAddSharedVehicleGroup,                          0), // CMD_ADD_SHARE_VEHICLE_GROUP
	DEF_CMD(CmdRemoveAllVehiclesGroup,                         0), // CMD_REMOVE_ALL_VEHICLES_GROUP
	DEF_CMD(CmdSetGroupReplaceProtection,                      0), // CMD_SET_GROUP_REPLACE_PROTECTION
	DEF_CMD(CmdMoveOrder,                                      0), // CMD_MOVE_ORDER
	DEF_CMD(CmdChangeTimetable,                                0), // CMD_CHANGE_TIMETABLE
	DEF_CMD(CmdSetVehicleOnTime,                               0), // CMD_SET_VEHICLE_ON_TIME
	DEF_CMD(CmdAutofillTimetable,                              0), // CMD_AUTOFILL_TIMETABLE
	DEF_CMD(CmdSetTimetableStart,                              0), // CMD_SET_TIMETABLE_START
	DEF_CMD(CmdInsertSignalInstruction,                        0), // CMD_INSERT_SIGNAL_INSTRUCTION
	DEF_CMD(CmdModifySignalInstruction,                        0), // CMD_MODIFY_SIGNAL_INSTRUCTION
	DEF_CMD(CmdRemoveSignalInstruction,                        0), // CMD_REMOVE_SIGNAL_INSTRUCTION
};

/*!
 * This function range-checks a cmd, and checks if the cmd is not NULL
 *
 * @param cmd The integervalue of a command
 * @return true if the command is valid (and got a CommandProc function)
 */
bool IsValidCommand(uint32 cmd)
{
	cmd &= CMD_ID_MASK;

	return
		cmd < lengthof(_command_proc_table) &&
		_command_proc_table[cmd].proc != NULL;
}

/*!
 * This function mask the parameter with CMD_ID_MASK and returns
 * the flags which belongs to the given command.
 *
 * @param cmd The integer value of the command
 * @return The flags for this command
 */
byte GetCommandFlags(uint32 cmd)
{
	assert(IsValidCommand(cmd));

	return _command_proc_table[cmd & CMD_ID_MASK].flags;
}

/*!
 * This function mask the parameter with CMD_ID_MASK and returns
 * the name which belongs to the given command.
 *
 * @param cmd The integer value of the command
 * @return The name for this command
 */
const char *GetCommandName(uint32 cmd)
{
	assert(IsValidCommand(cmd));

	return _command_proc_table[cmd & CMD_ID_MASK].name;
}

static int _docommand_recursive = 0;

/**
 * Shorthand for calling the long DoCommand with a container.
 *
 * @param container Container with (almost) all information
 * @param flags Flags for the command and how to execute the command
 * @see CommandProc
 * @return the cost
 */
CommandCost DoCommand(const CommandContainer *container, DoCommandFlag flags)
{
	return DoCommand(container->tile, container->p1, container->p2, flags, container->cmd & CMD_ID_MASK, container->text);
}

/*!
 * This function executes a given command with the parameters from the #CommandProc parameter list.
 * Depending on the flags parameter it execute or test a command.
 *
 * @param tile The tile to apply the command on (for the #CommandProc)
 * @param p1 Additional data for the command (for the #CommandProc)
 * @param p2 Additional data for the command (for the #CommandProc)
 * @param flags Flags for the command and how to execute the command
 * @param cmd The command-id to execute (a value of the CMD_* enums)
 * @param text The text to pass
 * @see CommandProc
 * @return the cost
 */
CommandCost DoCommand(TileIndex tile, uint32 p1, uint32 p2, DoCommandFlag flags, uint32 cmd, const char *text)
{
	CommandCost res;

	/* Do not even think about executing out-of-bounds tile-commands */
	if (tile != 0 && (tile >= MapSize() || (!IsValidTile(tile) && (flags & DC_ALL_TILES) == 0))) return CMD_ERROR;

	/* Chop of any CMD_MSG or other flags; we don't need those here */
	CommandProc *proc = _command_proc_table[cmd & CMD_ID_MASK].proc;

	_docommand_recursive++;

	/* only execute the test call if it's toplevel, or we're not execing. */
	if (_docommand_recursive == 1 || !(flags & DC_EXEC) ) {
		SetTownRatingTestMode(true);
		res = proc(tile, flags & ~DC_EXEC, p1, p2, text);
		SetTownRatingTestMode(false);
		if (res.Failed()) {
			goto error;
		}

		if (_docommand_recursive == 1 &&
				!(flags & DC_QUERY_COST) &&
				!(flags & DC_BANKRUPT) &&
				!CheckCompanyHasMoney(res)) { // CheckCompanyHasMoney() modifies 'res' to an error if it fails.
			goto error;
		}

		if (!(flags & DC_EXEC)) {
			_docommand_recursive--;
			return res;
		}
	}

	/* Execute the command here. All cost-relevant functions set the expenses type
	 * themselves to the cost object at some point */
	res = proc(tile, flags, p1, p2, text);
	if (res.Failed()) {
error:
		_docommand_recursive--;
		return res;
	}

	/* if toplevel, subtract the money. */
	if (--_docommand_recursive == 0 && !(flags & DC_BANKRUPT)) {
		SubtractMoneyFromCompany(res);
	}

	return res;
}

/*!
 * This functions returns the money which can be used to execute a command.
 * This is either the money of the current company or INT64_MAX if there
 * is no such a company "at the moment" like the server itself.
 *
 * @return The available money of a company or INT64_MAX
 */
Money GetAvailableMoneyForCommand()
{
	CompanyID company = _current_company;
	if (!Company::IsValidID(company)) return INT64_MAX;
	return Company::Get(company)->money;
}

/**
 * Shortcut for the long DoCommandP when having a container with the data.
 * @param container the container with information.
 * @param my_cmd indicator if the command is from a company or server (to display error messages for a user)
 * @return true if the command succeeded, else false
 */
bool DoCommandP(const CommandContainer *container, bool my_cmd)
{
	return DoCommandP(container->tile, container->p1, container->p2, container->cmd, container->callback, container->text, my_cmd);
}

/*!
 * Toplevel network safe docommand function for the current company. Must not be called recursively.
 * The callback is called when the command succeeded or failed. The parameters
 * \a tile, \a p1, and \a p2 are from the #CommandProc function. The parameter \a cmd is the command to execute.
 * The parameter \a my_cmd is used to indicate if the command is from a company or the server.
 *
 * @param tile The tile to perform a command on (see #CommandProc)
 * @param p1 Additional data for the command (see #CommandProc)
 * @param p2 Additional data for the command (see #CommandProc)
 * @param cmd The command to execute (a CMD_* value)
 * @param callback A callback function to call after the command is finished
 * @param text The text to pass
 * @param my_cmd indicator if the command is from a company or server (to display error messages for a user)
 * @return \c true if the command succeeded, else \c false.
 */
bool DoCommandP(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback *callback, const char *text, bool my_cmd)
{
	/* Cost estimation is generally only done when the
	 * local user presses shift while doing somthing.
	 * However, in case of incoming network commands,
	 * map generation of the pause button we do want
	 * to execute. */
	bool estimate_only = _shift_pressed && IsLocalCompany() &&
			!IsGeneratingWorld() &&
			!(cmd & CMD_NETWORK_COMMAND) &&
			(cmd & CMD_ID_MASK) != CMD_PAUSE;

	/* We're only sending the command, so don't do
	 * fancy things for 'success'. */
	bool only_sending = _networking && !(cmd & CMD_NETWORK_COMMAND);

	/* Where to show the message? */
	int x = TileX(tile) * TILE_SIZE;
	int y = TileY(tile) * TILE_SIZE;

	CommandCost res = DoCommandPInternal(tile, p1, p2, cmd, callback, text, my_cmd, estimate_only);
	if (res.Failed()) {
		/* Only show the error when it's for us. */
		StringID error_part1 = GB(cmd, 16, 16);
		if (estimate_only || (IsLocalCompany() && error_part1 != 0 && my_cmd)) {
			ShowErrorMessage(error_part1, res.GetErrorMessage(), WL_INFO, x, y);
		}
	} else if (estimate_only) {
		ShowEstimatedCostOrIncome(res.GetCost(), x, y);
	} else if (!only_sending && res.GetCost() != 0 && tile != 0 && IsLocalCompany() && _game_mode != GM_EDITOR) {
		/* Only show the cost animation when we did actually
		 * execute the command, i.e. we're not sending it to
		 * the server, when it has cost the local company
		 * something. Furthermore in the editor there is no
		 * concept of cost, so don't show it there either. */
		ShowCostOrIncomeAnimation(x, y, GetSlopeZ(x, y), res.GetCost());
	}

	if (!estimate_only && !only_sending && callback != NULL) {
		callback(res, tile, p1, p2);
	}

	return res.Succeeded();
}


/**
 * Helper to deduplicate the code for returning.
 * @param cmd   the command cost to return.
 * @param clear whether to keep the storage changes or not.
 */
#define return_dcpi(cmd, clear) { _docommand_recursive = 0; ClearStorageChanges(clear); return cmd; }

/*!
 * Helper function for the toplevel network safe docommand function for the current company.
 *
 * @param tile The tile to perform a command on (see #CommandProc)
 * @param p1 Additional data for the command (see #CommandProc)
 * @param p2 Additional data for the command (see #CommandProc)
 * @param cmd The command to execute (a CMD_* value)
 * @param callback A callback function to call after the command is finished
 * @param text The text to pass
 * @param my_cmd indicator if the command is from a company or server (to display error messages for a user)
 * @param estimate_only whether to give only the estimate or also execute the command
 * @return the command cost of this function.
 */
CommandCost DoCommandPInternal(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback *callback, const char *text, bool my_cmd, bool estimate_only)
{
	/* Prevent recursion; it gives a mess over the network */
	assert(_docommand_recursive == 0);
	_docommand_recursive = 1;

	/* Reset the state. */
	_additional_cash_required = 0;

	/* Get pointer to command handler */
	byte cmd_id = cmd & CMD_ID_MASK;
	assert(cmd_id < lengthof(_command_proc_table));

	CommandProc *proc = _command_proc_table[cmd_id].proc;
	/* Shouldn't happen, but you never know when someone adds
	 * NULLs to the _command_proc_table. */
	assert(proc != NULL);

	/* Command flags are used internally */
	uint cmd_flags = GetCommandFlags(cmd);
	/* Flags get send to the DoCommand */
	DoCommandFlag flags = CommandFlagsToDCFlags(cmd_flags);

	/* Do not even think about executing out-of-bounds tile-commands */
	if (tile != 0 && (tile >= MapSize() || (!IsValidTile(tile) && (cmd_flags & CMD_ALL_TILES) == 0))) return_dcpi(CMD_ERROR, false);

	/* Always execute server and spectator commands as spectator */
	if (cmd_flags & (CMD_SPECTATOR | CMD_SERVER)) _current_company = COMPANY_SPECTATOR;

	CompanyID old_company = _current_company;

	/* If the company isn't valid it may only do server command or start a new company!
	 * The server will ditch any server commands a client sends to it, so effectively
	 * this guards the server from executing functions for an invalid company. */
	if (_game_mode == GM_NORMAL && (cmd_flags & (CMD_SPECTATOR | CMD_SERVER)) == 0 && !Company::IsValidID(_current_company)) {
		return_dcpi(CMD_ERROR, false);
	}

	bool test_and_exec_can_differ = (cmd_flags & CMD_NO_TEST) != 0;
	bool skip_test = _networking && (cmd & CMD_NO_TEST_IF_IN_NETWORK) != 0;

	/* Do we need to do a test run?
	 * Basically we need to always do this, except when
	 * the no-test-in-network flag is giving and we're
	 * in a network game (e.g. restoring orders would
	 * fail this test because the first order does not
	 * exist yet when inserting the second, giving that
	 * a wrong insert location and ignoring the command
	 * and thus breaking restoring). However, when we
	 * just want to do cost estimation we don't care
	 * because it's only done once anyway. */
	CommandCost res;
	if (estimate_only || !skip_test) {
		/* Test the command. */
		SetTownRatingTestMode(true);
		res = proc(tile, flags, p1, p2, text);
		SetTownRatingTestMode(false);

		/* Make sure we're not messing things up here. */
		assert(cmd_id == CMD_COMPANY_CTRL || old_company == _current_company);

		/* If the command fails, we're doing an estimate
		 * or the player does not have enough money
		 * (unless it's a command where the test and
		 * execution phase might return different costs)
		 * we bail out here. */
		if (res.Failed() || estimate_only ||
				(!test_and_exec_can_differ && !CheckCompanyHasMoney(res))) {
			return_dcpi(res, false);
		}
	}

#ifdef ENABLE_NETWORK
	/*
	 * If we are in network, and the command is not from the network
	 * send it to the command-queue and abort execution
	 */
	if (_networking && !(cmd & CMD_NETWORK_COMMAND)) {
		NetworkSend_Command(tile, p1, p2, cmd & ~CMD_FLAGS_MASK, callback, text, _current_company);

		/* Don't return anything special here; no error, no costs.
		 * This way it's not handled by DoCommand and only the
		 * actual execution of the command causes messages. Also
		 * reset the storages as we've not executed the command. */
		return_dcpi(CommandCost(), false);
	}
#endif /* ENABLE_NETWORK */
	DEBUG(desync, 1, "cmd: %08x; %02x; %02x; %06x; %08x; %08x; %08x; \"%s\" (%s)", _date, _date_fract, (int)_current_company, tile, p1, p2, cmd & ~CMD_NETWORK_COMMAND, text, GetCommandName(cmd));

	/* Actually try and execute the command. If no cost-type is given
	 * use the construction one */
	CommandCost res2 = proc(tile, flags | DC_EXEC, p1, p2, text);

	/* Make sure nothing bad happened, like changing the current company. */
	assert(cmd_id == CMD_COMPANY_CTRL || old_company == _current_company);

	/* If the test and execution can differ, or we skipped the test
	 * we have to check the return of the command. Otherwise we can
	 * check whether the test and execution have yielded the same
	 * result, i.e. cost and error state are the same. */
	if (!test_and_exec_can_differ && !skip_test) {
		assert(res.GetCost() == res2.GetCost() && res.Failed() == res2.Failed()); // sanity check
	} else if (res2.Failed()) {
		return_dcpi(res2, false);
	}

	/* If we're needing more money and we haven't done
	 * anything yet, ask for the money! */
	if (_additional_cash_required != 0 && res2.GetCost() == 0) {
		/* It could happen we removed rail, thus gained money, and deleted something else.
		 * So make sure the signal buffer is empty even in this case */
		UpdateSignalsInBuffer();
		SetDParam(0, _additional_cash_required);
		return_dcpi(CommandCost(STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY), false);
	}

	/* update last build coordinate of company. */
	if (tile != 0) {
		Company *c = Company::GetIfValid(_current_company);
		if (c != NULL) c->last_build_coordinate = tile;
	}

	SubtractMoneyFromCompany(res2);

	/* update signals if needed */
	UpdateSignalsInBuffer();

	return_dcpi(res2, true);
}
#undef return_dcpi


/**
 * Adds the cost of the given command return value to this cost.
 * Also takes a possible error message when it is set.
 * @param ret The command to add the cost of.
 */
void CommandCost::AddCost(const CommandCost &ret)
{
	this->AddCost(ret.cost);
	if (this->success && !ret.success) {
		this->message = ret.message;
		this->success = false;
	}
}
