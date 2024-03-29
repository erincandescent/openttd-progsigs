/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file group_sl.cpp Code handling saving and loading of economy data */

#include "../stdafx.h"
#include "../group.h"

#include "saveload.h"

static const SaveLoad _group_desc[] = {
  SLE_CONDVAR(Group, name,           SLE_NAME,    0, 83),
  SLE_CONDSTR(Group, name,           SLE_STR, 0, 84, SL_MAX_VERSION),
  SLE_VAR(Group, num_vehicle,        SLE_UINT16),
  SLE_VAR(Group, owner,              SLE_UINT8),
  SLE_VAR(Group, vehicle_type,       SLE_UINT8),
  SLE_VAR(Group, replace_protection, SLE_BOOL),
  SLE_END()
};

static void Save_GRPS()
{
	Group *g;

	FOR_ALL_GROUPS(g) {
		SlSetArrayIndex(g->index);
		SlObject(g, _group_desc);
	}
}


static void Load_GRPS()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		Group *g = new (index) Group();
		SlObject(g, _group_desc);
	}
}

extern const ChunkHandler _group_chunk_handlers[] = {
	{ 'GRPS', Save_GRPS, Load_GRPS, NULL, CH_ARRAY | CH_LAST},
};
