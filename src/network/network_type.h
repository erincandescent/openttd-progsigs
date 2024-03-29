/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_type.h Types used for networking. */

#ifndef NETWORK_TYPE_H
#define NETWORK_TYPE_H

#include "core/game.h"

#ifdef ENABLE_NETWORK

enum {
	/** How many clients can we have */
	MAX_CLIENTS = 255,

	/**
	 * The number of slots; must be at least 1 more than MAX_CLIENTS. It must
	 * furthermore be less than or equal to 256 as client indices (sent over
	 * the network) are 8 bits. It needs 1 more for the dedicated server.
	 */
	MAX_CLIENT_SLOTS = 256,

	/** How many vehicle/station types we put over the network */
	NETWORK_VEHICLE_TYPES = 5,
	NETWORK_STATION_TYPES = 5,
};

/** 'Unique' identifier to be given to clients */
enum ClientID {
	INVALID_CLIENT_ID = 0, ///< Client is not part of anything
	CLIENT_ID_SERVER  = 1, ///< Servers always have this ID
	CLIENT_ID_FIRST   = 2, ///< The first client ID
};

/** Indices into the client tables */
typedef uint8 ClientIndex;

/** Simple calculated statistics of a company */
struct NetworkCompanyStats {
	uint16 num_vehicle[NETWORK_VEHICLE_TYPES];      ///< How many vehicles are there of this type?
	uint16 num_station[NETWORK_STATION_TYPES];      ///< How many stations are there of this type?
	bool ai;                                        ///< Is this company an AI
};

/** Some state information of a company, especially for servers */
struct NetworkCompanyState {
	char password[NETWORK_PASSWORD_LENGTH];         ///< The password for the company
	uint16 months_empty;                            ///< How many months the company is empty
};

struct NetworkClientInfo;

enum NetworkPasswordType {
	NETWORK_GAME_PASSWORD,
	NETWORK_COMPANY_PASSWORD,
};

enum DestType {
	DESTTYPE_BROADCAST, ///< Send message/notice to all clients (All)
	DESTTYPE_TEAM,      ///< Send message/notice to everyone playing the same company (Team)
	DESTTYPE_CLIENT,    ///< Send message/notice to only a certain client (Private)
};

/** Actions that can be used for NetworkTextMessage */
enum NetworkAction {
	NETWORK_ACTION_JOIN,
	NETWORK_ACTION_LEAVE,
	NETWORK_ACTION_SERVER_MESSAGE,
	NETWORK_ACTION_CHAT,
	NETWORK_ACTION_CHAT_COMPANY,
	NETWORK_ACTION_CHAT_CLIENT,
	NETWORK_ACTION_GIVE_MONEY,
	NETWORK_ACTION_NAME_CHANGE,
	NETWORK_ACTION_COMPANY_SPECTATOR,
	NETWORK_ACTION_COMPANY_JOIN,
	NETWORK_ACTION_COMPANY_NEW,
};

enum NetworkErrorCode {
	NETWORK_ERROR_GENERAL, // Try to use this one like never

	/* Signals from clients */
	NETWORK_ERROR_DESYNC,
	NETWORK_ERROR_SAVEGAME_FAILED,
	NETWORK_ERROR_CONNECTION_LOST,
	NETWORK_ERROR_ILLEGAL_PACKET,
	NETWORK_ERROR_NEWGRF_MISMATCH,

	/* Signals from servers */
	NETWORK_ERROR_NOT_AUTHORIZED,
	NETWORK_ERROR_NOT_EXPECTED,
	NETWORK_ERROR_WRONG_REVISION,
	NETWORK_ERROR_NAME_IN_USE,
	NETWORK_ERROR_WRONG_PASSWORD,
	NETWORK_ERROR_COMPANY_MISMATCH, // Happens in CLIENT_COMMAND
	NETWORK_ERROR_KICKED,
	NETWORK_ERROR_CHEATER,
	NETWORK_ERROR_FULL,
};

#endif /* ENABLE_NETWORK */
#endif /* NETWORK_TYPE_H */
