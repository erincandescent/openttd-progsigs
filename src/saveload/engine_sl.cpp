/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file engine_sl.cpp Code handling saving and loading of engines */

#include "../stdafx.h"
#include "saveload_internal.h"
#include "../engine_base.h"
#include <map>

static const SaveLoad _engine_desc[] = {
	 SLE_CONDVAR(Engine, intro_date,          SLE_FILE_U16 | SLE_VAR_I32,  0,  30),
	 SLE_CONDVAR(Engine, intro_date,          SLE_INT32,                  31, SL_MAX_VERSION),
	 SLE_CONDVAR(Engine, age,                 SLE_FILE_U16 | SLE_VAR_I32,  0,  30),
	 SLE_CONDVAR(Engine, age,                 SLE_INT32,                  31, SL_MAX_VERSION),
	     SLE_VAR(Engine, reliability,         SLE_UINT16),
	     SLE_VAR(Engine, reliability_spd_dec, SLE_UINT16),
	     SLE_VAR(Engine, reliability_start,   SLE_UINT16),
	     SLE_VAR(Engine, reliability_max,     SLE_UINT16),
	     SLE_VAR(Engine, reliability_final,   SLE_UINT16),
	     SLE_VAR(Engine, duration_phase_1,    SLE_UINT16),
	     SLE_VAR(Engine, duration_phase_2,    SLE_UINT16),
	     SLE_VAR(Engine, duration_phase_3,    SLE_UINT16),

	SLE_CONDNULL(1,                                                        0, 120),
	     SLE_VAR(Engine, flags,               SLE_UINT8),
	     SLE_VAR(Engine, preview_company_rank,SLE_UINT8),
	     SLE_VAR(Engine, preview_wait,        SLE_UINT8),
	SLE_CONDNULL(1,                                                        0,  44),
	 SLE_CONDVAR(Engine, company_avail,       SLE_FILE_U8  | SLE_VAR_U16,  0, 103),
	 SLE_CONDVAR(Engine, company_avail,       SLE_UINT16,                104, SL_MAX_VERSION),
	 SLE_CONDSTR(Engine, name,                SLE_STR, 0,                 84, SL_MAX_VERSION),

	/* reserve extra space in savegame here. (currently 16 bytes) */
	SLE_CONDNULL(16,                                                       2, SL_MAX_VERSION),

	SLE_END()
};

static std::map<EngineID, Engine> _temp_engine;

Engine *GetTempDataEngine(EngineID index)
{
	return &_temp_engine[index];
}

static void Save_ENGN()
{
	Engine *e;
	FOR_ALL_ENGINES(e) {
		SlSetArrayIndex(e->index);
		SlObject(e, _engine_desc);
	}
}

static void Load_ENGN()
{
	/* As engine data is loaded before engines are initialized we need to load
	 * this information into a temporary array. This is then copied into the
	 * engine pool after processing NewGRFs by CopyTempEngineData(). */
	int index;
	while ((index = SlIterateArray()) != -1) {
		Engine *e = GetTempDataEngine(index);
		SlObject(e, _engine_desc);
	}
}

/**
 * Copy data from temporary engine array into the real engine pool.
 */
void CopyTempEngineData()
{
	Engine *e;
	FOR_ALL_ENGINES(e) {
		if (e->index >= _temp_engine.size()) break;

		const Engine *se = GetTempDataEngine(e->index);
		e->intro_date          = se->intro_date;
		e->age                 = se->age;
		e->reliability         = se->reliability;
		e->reliability_spd_dec = se->reliability_spd_dec;
		e->reliability_start   = se->reliability_start;
		e->reliability_max     = se->reliability_max;
		e->reliability_final   = se->reliability_final;
		e->duration_phase_1    = se->duration_phase_1;
		e->duration_phase_2    = se->duration_phase_2;
		e->duration_phase_3    = se->duration_phase_3;
		e->flags               = se->flags;
		e->preview_company_rank= se->preview_company_rank;
		e->preview_wait        = se->preview_wait;
		e->company_avail       = se->company_avail;
		if (se->name != NULL) e->name = strdup(se->name);
	}

	/* Get rid of temporary data */
	_temp_engine.clear();
}

static void Load_ENGS()
{
	/* Load old separate String ID list into a temporary array. This
	 * was always 256 entries. */
	StringID names[256];

	SlArray(names, lengthof(names), SLE_STRINGID);

	/* Copy each string into the temporary engine array. */
	for (EngineID engine = 0; engine < lengthof(names); engine++) {
		Engine *e = GetTempDataEngine(engine);
		e->name = CopyFromOldName(names[engine]);
	}
}

/** Save and load the mapping between the engine id in the pool, and the grf file it came from. */
static const SaveLoad _engine_id_mapping_desc[] = {
	SLE_VAR(EngineIDMapping, grfid,         SLE_UINT32),
	SLE_VAR(EngineIDMapping, internal_id,   SLE_UINT16),
	SLE_VAR(EngineIDMapping, type,          SLE_UINT8),
	SLE_VAR(EngineIDMapping, substitute_id, SLE_UINT8),
	SLE_END()
};

static void Save_EIDS()
{
	const EngineIDMapping *end = _engine_mngr.End();
	uint index = 0;
	for (EngineIDMapping *eid = _engine_mngr.Begin(); eid != end; eid++, index++) {
		SlSetArrayIndex(index);
		SlObject(eid, _engine_id_mapping_desc);
	}
}

static void Load_EIDS()
{
	int index;

	_engine_mngr.Clear();

	while ((index = SlIterateArray()) != -1) {
		EngineIDMapping *eid = _engine_mngr.Append();
		SlObject(eid, _engine_id_mapping_desc);
	}
}

extern const ChunkHandler _engine_chunk_handlers[] = {
	{ 'EIDS', Save_EIDS, Load_EIDS, NULL, CH_ARRAY          },
	{ 'ENGN', Save_ENGN, Load_ENGN, NULL, CH_ARRAY          },
	{ 'ENGS', NULL,      Load_ENGS, NULL, CH_RIFF | CH_LAST },
};
