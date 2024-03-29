/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file aircraft_cmd.cpp
 * This file deals with aircraft and airport movements functionalities */

#include "stdafx.h"
#include "aircraft.h"
#include "debug.h"
#include "landscape.h"
#include "news_func.h"
#include "vehicle_gui.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"
#include "spritecache.h"
#include "strings_func.h"
#include "command_func.h"
#include "window_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "functions.h"
#include "cheat_type.h"
#include "company_base.h"
#include "autoreplace_gui.h"
#include "ai/ai.hpp"
#include "company_func.h"
#include "effectvehicle_func.h"
#include "station_base.h"
#include "engine_base.h"
#include "engine_func.h"
#include "core/random_func.hpp"

#include "table/strings.h"
#include "table/sprites.h"

void Aircraft::UpdateDeltaXY(Direction direction)
{
	uint32 x;
#define MKIT(a, b, c, d) ((a & 0xFF) << 24) | ((b & 0xFF) << 16) | ((c & 0xFF) << 8) | ((d & 0xFF) << 0)
	switch (this->subtype) {
		default: NOT_REACHED();
		case AIR_AIRCRAFT:
		case AIR_HELICOPTER:
			switch (this->state) {
				case ENDTAKEOFF:
				case LANDING:
				case HELILANDING:
				case FLYING:     x = MKIT(24, 24, -1, -1); break;
				default:         x = MKIT( 2,  2, -1, -1); break;
			}
			this->z_extent = 5;
			break;
		case AIR_SHADOW:     this->z_extent = 1; x = MKIT(2,  2,  0,  0); break;
		case AIR_ROTOR:      this->z_extent = 1; x = MKIT(2,  2, -1, -1); break;
	}
#undef MKIT

	this->x_offs        = GB(x,  0, 8);
	this->y_offs        = GB(x,  8, 8);
	this->x_extent      = GB(x, 16, 8);
	this->y_extent      = GB(x, 24, 8);
}


/** this maps the terminal to its corresponding state and block flag
 *  currently set for 10 terms, 4 helipads */
static const byte _airport_terminal_state[] = {2, 3, 4, 5, 6, 7, 19, 20, 0, 0, 8, 9, 21, 22};
static const byte _airport_terminal_flag[] =  {0, 1, 2, 3, 4, 5, 22, 23, 0, 0, 6, 7, 24, 25};

static bool AirportMove(Aircraft *v, const AirportFTAClass *apc);
static bool AirportSetBlocks(Aircraft *v, const AirportFTA *current_pos, const AirportFTAClass *apc);
static bool AirportHasBlock(Aircraft *v, const AirportFTA *current_pos, const AirportFTAClass *apc);
static bool AirportFindFreeTerminal(Aircraft *v, const AirportFTAClass *apc);
static bool AirportFindFreeHelipad(Aircraft *v, const AirportFTAClass *apc);
static void CrashAirplane(Aircraft *v);

static const SpriteID _aircraft_sprite[] = {
	0x0EB5, 0x0EBD, 0x0EC5, 0x0ECD,
	0x0ED5, 0x0EDD, 0x0E9D, 0x0EA5,
	0x0EAD, 0x0EE5, 0x0F05, 0x0F0D,
	0x0F15, 0x0F1D, 0x0F25, 0x0F2D,
	0x0EED, 0x0EF5, 0x0EFD, 0x0F35,
	0x0E9D, 0x0EA5, 0x0EAD, 0x0EB5,
	0x0EBD, 0x0EC5
};

/** Helicopter rotor animation states */
enum HelicopterRotorStates {
	HRS_ROTOR_STOPPED,
	HRS_ROTOR_MOVING_1,
	HRS_ROTOR_MOVING_2,
	HRS_ROTOR_MOVING_3,
};

/** Find the nearest hangar to v
 * INVALID_STATION is returned, if the company does not have any suitable
 * airports (like helipads only)
 * @param v vehicle looking for a hangar
 * @return the StationID if one is found, otherwise, INVALID_STATION
 */
static StationID FindNearestHangar(const Aircraft *v)
{
	const Station *st;
	uint best = 0;
	StationID index = INVALID_STATION;
	TileIndex vtile = TileVirtXY(v->x_pos, v->y_pos);
	const AircraftVehicleInfo *avi = AircraftVehInfo(v->engine_type);

	FOR_ALL_STATIONS(st) {
		if (st->owner != v->owner || !(st->facilities & FACIL_AIRPORT)) continue;

		const AirportFTAClass *afc = st->airport.GetFTA();
		if (!st->airport.HasHangar() || (
					/* don't crash the plane if we know it can't land at the airport */
					(afc->flags & AirportFTAClass::SHORT_STRIP) &&
					(avi->subtype & AIR_FAST) &&
					!_cheats.no_jetcrash.value)) {
			continue;
		}

		/* v->tile can't be used here, when aircraft is flying v->tile is set to 0 */
		uint distance = DistanceSquare(vtile, st->airport.tile);
		if (distance < best || index == INVALID_STATION) {
			best = distance;
			index = st->index;
		}
	}
	return index;
}

#if 0
/** Check if given vehicle has a goto hangar in his orders
 * @param v vehicle to inquiry
 * @return true if vehicle v has an airport in the schedule, that has a hangar */
static bool HaveHangarInOrderList(Aircraft *v)
{
	const Order *order;

	FOR_VEHICLE_ORDERS(v, order) {
		const Station *st = Station::Get(order->station);
		if (st->owner == v->owner && (st->facilities & FACIL_AIRPORT)) {
			/* If an airport doesn't have a hangar, skip it */
			if (st->Airport()->nof_depots != 0)
				return true;
		}
	}

	return false;
}
#endif

SpriteID Aircraft::GetImage(Direction direction) const
{
	uint8 spritenum = this->spritenum;

	if (is_custom_sprite(spritenum)) {
		SpriteID sprite = GetCustomVehicleSprite(this, direction);
		if (sprite != 0) return sprite;

		spritenum = Engine::Get(this->engine_type)->original_image_index;
	}

	return direction + _aircraft_sprite[spritenum];
}

SpriteID GetRotorImage(const Aircraft *v)
{
	assert(v->subtype == AIR_HELICOPTER);

	const Aircraft *w = v->Next()->Next();
	if (is_custom_sprite(v->spritenum)) {
		SpriteID sprite = GetCustomRotorSprite(v, false);
		if (sprite != 0) return sprite;
	}

	/* Return standard rotor sprites if there are no custom sprites for this helicopter */
	return SPR_ROTOR_STOPPED + w->state;
}

static SpriteID GetAircraftIcon(EngineID engine)
{
	const Engine *e = Engine::Get(engine);
	uint8 spritenum = e->u.air.image_index;

	if (is_custom_sprite(spritenum)) {
		SpriteID sprite = GetCustomVehicleIcon(engine, DIR_W);
		if (sprite != 0) return sprite;

		spritenum = e->original_image_index;
	}

	return DIR_W + _aircraft_sprite[spritenum];
}

void DrawAircraftEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal)
{
	SpriteID sprite = GetAircraftIcon(engine);
	const Sprite *real_sprite = GetSprite(sprite, ST_NORMAL);
	preferred_x = Clamp(preferred_x, left - real_sprite->x_offs, right - real_sprite->width - real_sprite->x_offs);
	DrawSprite(sprite, pal, preferred_x, y);

	if (!(AircraftVehInfo(engine)->subtype & AIR_CTOL)) {
		SpriteID rotor_sprite = GetCustomRotorIcon(engine);
		if (rotor_sprite == 0) rotor_sprite = SPR_ROTOR_STOPPED;
		DrawSprite(rotor_sprite, PAL_NONE, preferred_x, y - 5);
	}
}

/** Get the size of the sprite of an aircraft sprite heading west (used for lists)
 * @param engine The engine to get the sprite from
 * @param width The width of the sprite
 * @param height The height of the sprite
 */
void GetAircraftSpriteSize(EngineID engine, uint &width, uint &height)
{
	const Sprite *spr = GetSprite(GetAircraftIcon(engine), ST_NORMAL);

	width  = spr->width;
	height = spr->height;
}

/** Build an aircraft.
 * @param tile tile of depot where aircraft is built
 * @param flags for command
 * @param p1 aircraft type being built (engine)
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdBuildAircraft(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	EngineID eid = GB(p1, 0, 16);
	if (!IsEngineBuildable(eid, VEH_AIRCRAFT, _current_company)) return_cmd_error(STR_ERROR_AIRCRAFT_NOT_AVAILABLE);

	const Engine *e = Engine::Get(eid);
	const AircraftVehicleInfo *avi = &e->u.air;
	CommandCost value(EXPENSES_NEW_VEHICLES, e->GetCost());

	/* Engines without valid cargo should not be available */
	if (e->GetDefaultCargoType() == CT_INVALID) return CMD_ERROR;

	/* to just query the cost, it is not neccessary to have a valid tile (automation/AI) */
	if (flags & DC_QUERY_COST) return value;

	if (!IsHangarTile(tile) || !IsTileOwner(tile, _current_company)) return CMD_ERROR;

	/* Prevent building aircraft types at places which can't handle them */
	if (!CanVehicleUseStation(eid, Station::GetByTile(tile))) return CMD_ERROR;

	/* We will need to allocate 2 or 3 vehicle structs, depending on type */
	if (!Vehicle::CanAllocateItem(avi->subtype & AIR_CTOL ? 2 : 3)) {
		return_cmd_error(STR_ERROR_TOO_MANY_VEHICLES_IN_GAME);
	}

	UnitID unit_num = (flags & DC_AUTOREPLACE) ? 0 : GetFreeUnitNumber(VEH_AIRCRAFT);
	if (unit_num > _settings_game.vehicle.max_aircraft)
		return_cmd_error(STR_ERROR_TOO_MANY_VEHICLES_IN_GAME);

	if (flags & DC_EXEC) {
		Aircraft *v = new Aircraft(); // aircraft
		Aircraft *u = new Aircraft(); // shadow

		v->unitnumber = unit_num;
		v->direction = DIR_SE;

		v->owner = u->owner = _current_company;

		v->tile = tile;

		uint x = TileX(tile) * TILE_SIZE + 5;
		uint y = TileY(tile) * TILE_SIZE + 3;

		v->x_pos = u->x_pos = x;
		v->y_pos = u->y_pos = y;

		u->z_pos = GetSlopeZ(x, y);
		v->z_pos = u->z_pos + 1;

		v->vehstatus = VS_HIDDEN | VS_STOPPED | VS_DEFPAL;
		u->vehstatus = VS_HIDDEN | VS_UNCLICKABLE | VS_SHADOW;

		v->spritenum = avi->image_index;

		v->cargo_cap = avi->passenger_capacity;
		u->cargo_cap = avi->mail_capacity;

		v->cargo_type = e->GetDefaultCargoType();
		u->cargo_type = CT_MAIL;

		v->name = NULL;
		v->last_station_visited = INVALID_STATION;

		v->max_speed = avi->max_speed;
		v->acceleration = avi->acceleration;
		v->engine_type = eid;
		u->engine_type = eid;

		v->subtype = (avi->subtype & AIR_CTOL ? AIR_AIRCRAFT : AIR_HELICOPTER);
		v->UpdateDeltaXY(INVALID_DIR);
		v->value = value.GetCost();

		u->subtype = AIR_SHADOW;
		u->UpdateDeltaXY(INVALID_DIR);

		v->reliability = e->reliability;
		v->reliability_spd_dec = e->reliability_spd_dec;
		v->max_age = e->GetLifeLengthInDays();

		_new_vehicle_id = v->index;

		v->pos = GetVehiclePosOnBuild(tile);

		v->state = HANGAR;
		v->previous_pos = v->pos;
		v->targetairport = GetStationIndex(tile);
		v->SetNext(u);

		v->service_interval = Company::Get(_current_company)->settings.vehicle.servint_aircraft;

		v->date_of_last_service = _date;
		v->build_year = u->build_year = _cur_year;

		v->cur_image = u->cur_image = SPR_IMG_QUERY;

		v->random_bits = VehicleRandomBits();
		u->random_bits = VehicleRandomBits();

		v->vehicle_flags = 0;
		if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) SetBit(v->vehicle_flags, VF_BUILT_AS_PROTOTYPE);

		v->InvalidateNewGRFCacheOfChain();

		v->cargo_cap = GetVehicleCapacity(v, &u->cargo_cap);

		v->InvalidateNewGRFCacheOfChain();

		UpdateAircraftCache(v);

		VehicleMove(v, false);
		VehicleMove(u, false);

		/* Aircraft with 3 vehicles (chopper)? */
		if (v->subtype == AIR_HELICOPTER) {
			Aircraft *w = new Aircraft();
			w->engine_type = eid;
			w->direction = DIR_N;
			w->owner = _current_company;
			w->x_pos = v->x_pos;
			w->y_pos = v->y_pos;
			w->z_pos = v->z_pos + 5;
			w->vehstatus = VS_HIDDEN | VS_UNCLICKABLE;
			w->spritenum = 0xFF;
			w->subtype = AIR_ROTOR;
			w->cur_image = SPR_ROTOR_STOPPED;
			w->random_bits = VehicleRandomBits();
			/* Use rotor's air.state to store the rotor animation frame */
			w->state = HRS_ROTOR_STOPPED;
			w->UpdateDeltaXY(INVALID_DIR);

			u->SetNext(w);
			VehicleMove(w, false);
		}

		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
		InvalidateWindowClassesData(WC_AIRCRAFT_LIST, 0);
		SetWindowDirty(WC_COMPANY, v->owner);
		if (IsLocalCompany())
			InvalidateAutoreplaceWindow(v->engine_type, v->group_id); // updates the replace Aircraft window

		Company::Get(_current_company)->num_engines[eid]++;
	}

	return value;
}


/** Sell an aircraft.
 * @param tile unused
 * @param flags for command type
 * @param p1 vehicle ID to be sold
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdSellAircraft(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Aircraft *v = Aircraft::GetIfValid(p1);
	if (v == NULL) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if (!v->IsStoppedInDepot()) return_cmd_error(STR_ERROR_AIRCRAFT_MUST_BE_STOPPED);

	if (v->vehstatus & VS_CRASHED) return_cmd_error(STR_ERROR_CAN_T_SELL_DESTROYED_VEHICLE);

	ret = CommandCost(EXPENSES_NEW_VEHICLES, -v->value);

	if (flags & DC_EXEC) {
		delete v;
	}

	return ret;
}

bool Aircraft::FindClosestDepot(TileIndex *location, DestinationID *destination, bool *reverse)
{
	const Station *st = GetTargetAirportIfValid(this);
	/* If the station is not a valid airport or if it has no hangars */
	if (st == NULL || !st->airport.HasHangar()) {
		/* the aircraft has to search for a hangar on its own */
		StationID station = FindNearestHangar(this);

		if (station == INVALID_STATION) return false;

		st = Station::Get(station);
	}

	if (location    != NULL) *location    = st->xy;
	if (destination != NULL) *destination = st->index;

	return true;
}

/** Send an aircraft to the hangar.
 * @param tile unused
 * @param flags for command type
 * @param p1 vehicle ID to send to the hangar
 * @param p2 various bitmasked elements
 * - p2 bit 0-3 - DEPOT_ flags (see vehicle.h)
 * - p2 bit 8-10 - VLW flag (for mass goto depot)
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdSendAircraftToHangar(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (p2 & DEPOT_MASS_SEND) {
		/* Mass goto depot requested */
		if (!ValidVLWFlags(p2 & VLW_MASK)) return CMD_ERROR;
		return SendAllVehiclesToDepot(VEH_AIRCRAFT, flags, p2 & DEPOT_SERVICE, _current_company, (p2 & VLW_MASK), p1);
	}

	Aircraft *v = Aircraft::GetIfValid(p1);
	if (v == NULL) return CMD_ERROR;

	return v->SendToDepot(flags, (DepotCommand)(p2 & DEPOT_COMMAND_MASK));
}


/** Refits an aircraft to the specified cargo type.
 * @param tile unused
 * @param flags for command type
 * @param p1 vehicle ID of the aircraft to refit
 * @param p2 various bitstuffed elements
 * - p2 = (bit 0-7) - the new cargo type to refit to
 * - p2 = (bit 8-15) - the new cargo subtype to refit to
 * - p2 = (bit 16) - refit only this vehicle (ignored)
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdRefitAircraft(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	byte new_subtype = GB(p2, 8, 8);

	Aircraft *v = Aircraft::GetIfValid(p1);
	if (v == NULL) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if (!v->IsStoppedInDepot()) return_cmd_error(STR_ERROR_AIRCRAFT_MUST_BE_STOPPED);
	if (v->vehstatus & VS_CRASHED) return_cmd_error(STR_ERROR_CAN_T_REFIT_DESTROYED_VEHICLE);

	/* Check cargo */
	CargoID new_cid = GB(p2, 0, 8);
	if (new_cid >= NUM_CARGO) return CMD_ERROR;

	CommandCost cost = RefitVehicle(v, true, new_cid, new_subtype, flags);

	if (flags & DC_EXEC) {
		v->colourmap = PAL_NONE; // invalidate vehicle colour map
		SetWindowDirty(WC_VEHICLE_DETAILS, v->index);
		SetWindowDirty(WC_VEHICLE_DEPOT, v->tile);
		InvalidateWindowClassesData(WC_AIRCRAFT_LIST, 0);
	}
	v->InvalidateNewGRFCacheOfChain(); // always invalidate; querycost might have filled it

	return cost;
}


static void CheckIfAircraftNeedsService(Aircraft *v)
{
	if (Company::Get(v->owner)->settings.vehicle.servint_aircraft == 0 || !v->NeedsAutomaticServicing()) return;
	if (v->IsInDepot()) {
		VehicleServiceInDepot(v);
		return;
	}

	/* When we're parsing conditional orders and the like
	 * we don't want to consider going to a depot too. */
	if (!v->current_order.IsType(OT_GOTO_DEPOT) && !v->current_order.IsType(OT_GOTO_STATION)) return;

	const Station *st = Station::Get(v->current_order.GetDestination());

	assert(st != NULL);

	/* only goto depot if the target airport has a depot */
	if (st->airport.HasHangar() && CanVehicleUseStation(v, st)) {
		v->current_order.MakeGoToDepot(st->index, ODTFB_SERVICE);
		SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	} else if (v->current_order.IsType(OT_GOTO_DEPOT)) {
		v->current_order.MakeDummy();
		SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	}
}

Money Aircraft::GetRunningCost() const
{
	const Engine *e = Engine::Get(this->engine_type);
	uint cost_factor = GetVehicleProperty(this, PROP_AIRCRAFT_RUNNING_COST_FACTOR, e->u.air.running_cost);
	return GetPrice(PR_RUNNING_AIRCRAFT, cost_factor, e->grffile);
}

void Aircraft::OnNewDay()
{
	if (!this->IsNormalAircraft()) return;

	if ((++this->day_counter & 7) == 0) DecreaseVehicleValue(this);

	CheckOrders(this);

	CheckVehicleBreakdown(this);
	AgeVehicle(this);
	CheckIfAircraftNeedsService(this);

	if (this->running_ticks == 0) return;

	CommandCost cost(EXPENSES_AIRCRAFT_RUN, this->GetRunningCost() * this->running_ticks / (DAYS_IN_YEAR * DAY_TICKS));

	this->profit_this_year -= cost.GetCost();
	this->running_ticks = 0;

	SubtractMoneyFromCompanyFract(this->owner, cost);

	SetWindowDirty(WC_VEHICLE_DETAILS, this->index);
	SetWindowClassesDirty(WC_AIRCRAFT_LIST);
}

static void HelicopterTickHandler(Aircraft *v)
{
	Aircraft *u = v->Next()->Next();

	if (u->vehstatus & VS_HIDDEN) return;

	/* if true, helicopter rotors do not rotate. This should only be the case if a helicopter is
	 * loading/unloading at a terminal or stopped */
	if (v->current_order.IsType(OT_LOADING) || (v->vehstatus & VS_STOPPED)) {
		if (u->cur_speed != 0) {
			u->cur_speed++;
			if (u->cur_speed >= 0x80 && u->state == HRS_ROTOR_MOVING_3) {
				u->cur_speed = 0;
			}
		}
	} else {
		if (u->cur_speed == 0)
			u->cur_speed = 0x70;

		if (u->cur_speed >= 0x50)
			u->cur_speed--;
	}

	int tick = ++u->tick_counter;
	int spd = u->cur_speed >> 4;

	SpriteID img;
	if (spd == 0) {
		u->state = HRS_ROTOR_STOPPED;
		img = GetRotorImage(v);
		if (u->cur_image == img) return;
	} else if (tick >= spd) {
		u->tick_counter = 0;
		u->state++;
		if (u->state > HRS_ROTOR_MOVING_3) u->state = HRS_ROTOR_MOVING_1;
		img = GetRotorImage(v);
	} else {
		return;
	}

	u->cur_image = img;

	VehicleMove(u, true);
}

void SetAircraftPosition(Aircraft *v, int x, int y, int z)
{
	v->x_pos = x;
	v->y_pos = y;
	v->z_pos = z;

	v->UpdateViewport(true, false);
	if (v->subtype == AIR_HELICOPTER) v->Next()->Next()->cur_image = GetRotorImage(v);

	Aircraft *u = v->Next();

	int safe_x = Clamp(x, 0, MapMaxX() * TILE_SIZE);
	int safe_y = Clamp(y - 1, 0, MapMaxY() * TILE_SIZE);
	u->x_pos = x;
	u->y_pos = y - ((v->z_pos - GetSlopeZ(safe_x, safe_y)) >> 3);

	safe_y = Clamp(u->y_pos, 0, MapMaxY() * TILE_SIZE);
	u->z_pos = GetSlopeZ(safe_x, safe_y);
	u->cur_image = v->cur_image;

	VehicleMove(u, true);

	u = u->Next();
	if (u != NULL) {
		u->x_pos = x;
		u->y_pos = y;
		u->z_pos = z + 5;

		VehicleMove(u, true);
	}
}

/** Handle Aircraft specific tasks when a an Aircraft enters a hangar
 * @param *v Vehicle that enters the hangar
 */
void HandleAircraftEnterHangar(Aircraft *v)
{
	v->subspeed = 0;
	v->progress = 0;

	Aircraft *u = v->Next();
	u->vehstatus |= VS_HIDDEN;
	u = u->Next();
	if (u != NULL) {
		u->vehstatus |= VS_HIDDEN;
		u->cur_speed = 0;
	}

	SetAircraftPosition(v, v->x_pos, v->y_pos, v->z_pos);
}

static void PlayAircraftSound(const Vehicle *v)
{
	if (!PlayVehicleSound(v, VSE_START)) {
		SndPlayVehicleFx(AircraftVehInfo(v->engine_type)->sfx, v);
	}
}


void UpdateAircraftCache(Aircraft *v)
{
	uint max_speed = GetVehicleProperty(v, PROP_AIRCRAFT_SPEED, 0);
	if (max_speed != 0) {
		/* Convert from original units to (approx) km/h */
		max_speed = (max_speed * 129) / 10;

		v->acache.cached_max_speed = max_speed;
	} else {
		v->acache.cached_max_speed = 0xFFFF;
	}
}


/**
 * Special velocities for aircraft
 */
enum AircraftSpeedLimits {
	SPEED_LIMIT_TAXI     =     50,  ///< Maximum speed of an aircraft while taxiing
	SPEED_LIMIT_APPROACH =    230,  ///< Maximum speed of an aircraft on finals
	SPEED_LIMIT_BROKEN   =    320,  ///< Maximum speed of an aircraft that is broken
	SPEED_LIMIT_HOLD     =    425,  ///< Maximum speed of an aircraft that flies the holding pattern
	SPEED_LIMIT_NONE     = 0xFFFF   ///< No environmental speed limit. Speed limit is type dependent
};

/**
 * Sets the new speed for an aircraft
 * @param v The vehicle for which the speed should be obtained
 * @param speed_limit The maximum speed the vehicle may have.
 * @param hard_limit If true, the limit is directly enforced, otherwise the plane is slowed down gradually
 * @return The number of position updates needed within the tick
 */
static int UpdateAircraftSpeed(Aircraft *v, uint speed_limit = SPEED_LIMIT_NONE, bool hard_limit = true)
{
	uint spd = v->acceleration * 16;
	byte t;

	/* Adjust speed limits by plane speed factor to prevent taxiing
	 * and take-off speeds being too low. */
	speed_limit *= _settings_game.vehicle.plane_speed;

	if (v->acache.cached_max_speed < speed_limit) {
		if (v->cur_speed < speed_limit) hard_limit = false;
		speed_limit = v->acache.cached_max_speed;
	}

	speed_limit = min(speed_limit, v->max_speed);

	v->subspeed = (t = v->subspeed) + (byte)spd;

	/* Aircraft's current speed is used twice so that very fast planes are
	 * forced to slow down rapidly in the short distance needed. The magic
	 * value 16384 was determined to give similar results to the old speed/48
	 * method at slower speeds. This also results in less reduction at slow
	 * speeds to that aircraft do not get to taxi speed straight after
	 * touchdown. */
	if (!hard_limit && v->cur_speed > speed_limit) {
		speed_limit = v->cur_speed - max(1, ((v->cur_speed * v->cur_speed) / 16384) / _settings_game.vehicle.plane_speed);
	}

	spd = min(v->cur_speed + (spd >> 8) + (v->subspeed < t), speed_limit);

	/* adjust speed for broken vehicles */
	if (v->vehstatus & VS_AIRCRAFT_BROKEN) spd = min(spd, SPEED_LIMIT_BROKEN);

	/* updates statusbar only if speed have changed to save CPU time */
	if (spd != v->cur_speed) {
		v->cur_speed = spd;
		if (_settings_client.gui.vehicle_speed)
			SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	}

	/* Adjust distance moved by plane speed setting */
	if (_settings_game.vehicle.plane_speed > 1) spd /= _settings_game.vehicle.plane_speed;

	if (!(v->direction & 1)) spd = spd * 3 / 4;

	spd += v->progress;
	v->progress = (byte)spd;
	return spd >> 8;
}

/**
 * Gets the cruise altitude of an aircraft.
 * The cruise altitude is determined by the velocity of the vehicle
 * and the direction it is moving
 * @param v The vehicle. Should be an aircraft
 * @returns Altitude in pixel units
 */
byte GetAircraftFlyingAltitude(const Aircraft *v)
{
	/* Make sure Aircraft fly no lower so that they don't conduct
	 * CFITs (controlled flight into terrain)
	 */
	byte base_altitude = 150;

	/* Make sure eastbound and westbound planes do not "crash" into each
	 * other by providing them with vertical seperation
	 */
	switch (v->direction) {
		case DIR_N:
		case DIR_NE:
		case DIR_E:
		case DIR_SE:
			base_altitude += 10;
			break;

		default: break;
	}

	/* Make faster planes fly higher so that they can overtake slower ones */
	base_altitude += min(20 * (v->max_speed / 200), 90);

	return base_altitude;
}

/**
 * Find the entry point to an airport depending on direction which
 * the airport is being approached from. Each airport can have up to
 * four entry points for its approach system so that approaching
 * aircraft do not fly through each other or are forced to do 180
 * degree turns during the approach. The arrivals are grouped into
 * four sectors dependent on the DiagDirection from which the airport
 * is approached.
 *
 * @param v   The vehicle that is approaching the airport
 * @param apc The Airport Class being approached.
 * @returns   The index of the entry point
 */
static byte AircraftGetEntryPoint(const Aircraft *v, const AirportFTAClass *apc)
{
	assert(v != NULL);
	assert(apc != NULL);

	/* In the case the station doesn't exit anymore, set target tile 0.
	 * It doesn't hurt much, aircraft will go to next order, nearest hangar
	 * or it will simply crash in next tick */
	TileIndex tile = 0;

	const Station *st = Station::GetIfValid(v->targetairport);
	if (st != NULL) {
		/* Make sure we don't go to INVALID_TILE if the airport has been removed. */
		tile = (st->airport.tile != INVALID_TILE) ? st->airport.tile : st->xy;
	}

	int delta_x = v->x_pos - TileX(tile) * TILE_SIZE;
	int delta_y = v->y_pos - TileY(tile) * TILE_SIZE;

	DiagDirection dir;
	if (abs(delta_y) < abs(delta_x)) {
		/* We are northeast or southwest of the airport */
		dir = delta_x < 0 ? DIAGDIR_NE : DIAGDIR_SW;
	} else {
		/* We are northwest or southeast of the airport */
		dir = delta_y < 0 ? DIAGDIR_NW : DIAGDIR_SE;
	}
	return apc->entry_points[dir];
}


static void MaybeCrashAirplane(Aircraft *v);

/**
 * Controls the movement of an aircraft. This function actually moves the vehicle
 * on the map and takes care of minor things like sound playback.
 * @todo    De-mystify the cur_speed values for helicopter rotors.
 * @param v The vehicle that is moved. Must be the first vehicle of the chain
 * @return  Whether the position requested by the State Machine has been reached
 */
static bool AircraftController(Aircraft *v)
{
	int count;

	/* NULL if station is invalid */
	const Station *st = Station::GetIfValid(v->targetairport);
	/* INVALID_TILE if there is no station */
	TileIndex tile = INVALID_TILE;
	if (st != NULL) {
		tile = (st->airport.tile != INVALID_TILE) ? st->airport.tile : st->xy;
	}
	/* DUMMY if there is no station or no airport */
	const AirportFTAClass *afc = tile == INVALID_TILE ? GetAirport(AT_DUMMY) : st->airport.GetFTA();

	/* prevent going to INVALID_TILE if airport is deleted. */
	if (st == NULL || st->airport.tile == INVALID_TILE) {
		/* Jump into our "holding pattern" state machine if possible */
		if (v->pos >= afc->nofelements) {
			v->pos = v->previous_pos = AircraftGetEntryPoint(v, afc);
		} else if (v->targetairport != v->current_order.GetDestination()) {
			/* If not possible, just get out of here fast */
			v->state = FLYING;
			UpdateAircraftCache(v);
			AircraftNextAirportPos_and_Order(v);
			/* get aircraft back on running altitude */
			SetAircraftPosition(v, v->x_pos, v->y_pos, GetAircraftFlyingAltitude(v));
			return false;
		}
	}

	/*  get airport moving data */
	const AirportMovingData *amd = afc->MovingData(v->pos);

	int x = TileX(tile) * TILE_SIZE;
	int y = TileY(tile) * TILE_SIZE;

	/* Helicopter raise */
	if (amd->flag & AMED_HELI_RAISE) {
		Aircraft *u = v->Next()->Next();

		/* Make sure the rotors don't rotate too fast */
		if (u->cur_speed > 32) {
			v->cur_speed = 0;
			if (--u->cur_speed == 32) {
				if (!PlayVehicleSound(v, VSE_START)) {
					SndPlayVehicleFx(SND_18_HELICOPTER, v);
				}
			}
		} else {
			u->cur_speed = 32;
			count = UpdateAircraftSpeed(v);
			if (count > 0) {
				v->tile = 0;

				/* Reached altitude? */
				if (v->z_pos >= 184) {
					v->cur_speed = 0;
					return true;
				}
				SetAircraftPosition(v, v->x_pos, v->y_pos, min(v->z_pos + count, 184));
			}
		}
		return false;
	}

	/* Helicopter landing. */
	if (amd->flag & AMED_HELI_LOWER) {
		if (st == NULL) {
			/* FIXME - AircraftController -> if station no longer exists, do not land
			 * helicopter will circle until sign disappears, then go to next order
			 * what to do when it is the only order left, right now it just stays in 1 place */
			v->state = FLYING;
			UpdateAircraftCache(v);
			AircraftNextAirportPos_and_Order(v);
			return false;
		}

		/* Vehicle is now at the airport. */
		v->tile = tile;

		/* Find altitude of landing position. */
		int z = GetSlopeZ(x, y) + 1 + afc->delta_z;

		if (z == v->z_pos) {
			Vehicle *u = v->Next()->Next();

			/*  Increase speed of rotors. When speed is 80, we've landed. */
			if (u->cur_speed >= 80) return true;
			u->cur_speed += 4;
		} else {
			count = UpdateAircraftSpeed(v);
			if (count > 0) {
				if (v->z_pos > z) {
					SetAircraftPosition(v, v->x_pos, v->y_pos, max(v->z_pos - count, z));
				} else {
					SetAircraftPosition(v, v->x_pos, v->y_pos, min(v->z_pos + count, z));
				}
			}
		}
		return false;
	}

	/* Get distance from destination pos to current pos. */
	uint dist = abs(x + amd->x - v->x_pos) +  abs(y + amd->y - v->y_pos);

	/* Need exact position? */
	if (!(amd->flag & AMED_EXACTPOS) && dist <= (amd->flag & AMED_SLOWTURN ? 8U : 4U)) return true;

	/* At final pos? */
	if (dist == 0) {
		/* Change direction smoothly to final direction. */
		DirDiff dirdiff = DirDifference(amd->direction, v->direction);
		/* if distance is 0, and plane points in right direction, no point in calling
		 * UpdateAircraftSpeed(). So do it only afterwards */
		if (dirdiff == DIRDIFF_SAME) {
			v->cur_speed = 0;
			return true;
		}

		if (!UpdateAircraftSpeed(v, SPEED_LIMIT_TAXI)) return false;

		v->direction = ChangeDir(v->direction, dirdiff > DIRDIFF_REVERSE ? DIRDIFF_45LEFT : DIRDIFF_45RIGHT);
		v->cur_speed >>= 1;

		SetAircraftPosition(v, v->x_pos, v->y_pos, v->z_pos);
		return false;
	}

	if (amd->flag & AMED_BRAKE && v->cur_speed > SPEED_LIMIT_TAXI * _settings_game.vehicle.plane_speed) {
		MaybeCrashAirplane(v);
		if ((v->vehstatus & VS_CRASHED) != 0) return false;
	}

	uint speed_limit = SPEED_LIMIT_TAXI;
	bool hard_limit = true;

	if (amd->flag & AMED_NOSPDCLAMP)   speed_limit = SPEED_LIMIT_NONE;
	if (amd->flag & AMED_HOLD)       { speed_limit = SPEED_LIMIT_HOLD;     hard_limit = false; }
	if (amd->flag & AMED_LAND)       { speed_limit = SPEED_LIMIT_APPROACH; hard_limit = false; }
	if (amd->flag & AMED_BRAKE)      { speed_limit = SPEED_LIMIT_TAXI;     hard_limit = false; }

	count = UpdateAircraftSpeed(v, speed_limit, hard_limit);
	if (count == 0) return false;

	if (v->turn_counter != 0) v->turn_counter--;

	do {

		GetNewVehiclePosResult gp;

		if (dist < 4 || (amd->flag & AMED_LAND)) {
			/* move vehicle one pixel towards target */
			gp.x = (v->x_pos != (x + amd->x)) ?
					v->x_pos + ((x + amd->x > v->x_pos) ? 1 : -1) :
					v->x_pos;
			gp.y = (v->y_pos != (y + amd->y)) ?
					v->y_pos + ((y + amd->y > v->y_pos) ? 1 : -1) :
					v->y_pos;

			/* Oilrigs must keep v->tile as st->airport.tile, since the landing pad is in a non-airport tile */
			gp.new_tile = (st->airport.type == AT_OILRIG) ? st->airport.tile : TileVirtXY(gp.x, gp.y);

		} else {

			/* Turn. Do it slowly if in the air. */
			Direction newdir = GetDirectionTowards(v, x + amd->x, y + amd->y);
			if (newdir != v->direction) {
				if (amd->flag & AMED_SLOWTURN && v->number_consecutive_turns < 8 && v->subtype == AIR_AIRCRAFT) {
					if (v->turn_counter == 0 || newdir == v->last_direction) {
						if (newdir == v->last_direction) {
							v->number_consecutive_turns = 0;
						} else {
							v->number_consecutive_turns++;
						}
						v->turn_counter = 2 * _settings_game.vehicle.plane_speed;
						v->last_direction = v->direction;
						v->direction = newdir;
					}

					/* Move vehicle. */
					gp = GetNewVehiclePos(v);
				} else {
					v->cur_speed >>= 1;
					v->direction = newdir;

					/* When leaving a terminal an aircraft often goes to a position
					 * directly in front of it. If it would move while turning it
					 * would need an two extra turns to end up at the correct position.
					 * To make it easier just disallow all moving while turning as
					 * long as an aircraft is on the ground. */
					gp.x = v->x_pos;
					gp.y = v->y_pos;
					gp.new_tile = gp.old_tile = v->tile;
				}
			} else {
				v->number_consecutive_turns = 0;
				/* Move vehicle. */
				gp = GetNewVehiclePos(v);
			}
		}

		v->tile = gp.new_tile;
		/* If vehicle is in the air, use tile coordinate 0. */
		if (amd->flag & (AMED_TAKEOFF | AMED_SLOWTURN | AMED_LAND)) v->tile = 0;

		/* Adjust Z for land or takeoff? */
		uint z = v->z_pos;

		if (amd->flag & AMED_TAKEOFF) {
			z = min(z + 2, GetAircraftFlyingAltitude(v));
		}

		if ((amd->flag & AMED_HOLD) && (z > 150)) z--;

		if (amd->flag & AMED_LAND) {
			if (st->airport.tile == INVALID_TILE) {
				/* Airport has been removed, abort the landing procedure */
				v->state = FLYING;
				UpdateAircraftCache(v);
				AircraftNextAirportPos_and_Order(v);
				/* get aircraft back on running altitude */
				SetAircraftPosition(v, gp.x, gp.y, GetAircraftFlyingAltitude(v));
				continue;
			}

			uint curz = GetSlopeZ(x + amd->x, y + amd->y) + 1;

			/* We're not flying below our destination, right? */
			assert(curz <= z);
			int t = max(1U, dist - 4);
			int delta = z - curz;

			/* Only start lowering when we're sufficiently close for a 1:1 glide */
			if (delta >= t) {
				z -= CeilDiv(z - curz, t);
			}
			if (z < curz) z = curz;
		}

		/* We've landed. Decrease speed when we're reaching end of runway. */
		if (amd->flag & AMED_BRAKE) {
			uint curz = GetSlopeZ(x, y) + 1;

			if (z > curz) {
				z--;
			} else if (z < curz) {
				z++;
			}

		}

		SetAircraftPosition(v, gp.x, gp.y, z);
	} while (--count != 0);
	return false;
}


static bool HandleCrashedAircraft(Aircraft *v)
{
	v->crashed_counter += 3;

	Station *st = GetTargetAirportIfValid(v);

	/* make aircraft crash down to the ground */
	if (v->crashed_counter < 500 && st == NULL && ((v->crashed_counter % 3) == 0) ) {
		uint z = GetSlopeZ(v->x_pos, v->y_pos);
		v->z_pos -= 1;
		if (v->z_pos == z) {
			v->crashed_counter = 500;
			v->z_pos++;
		}
	}

	if (v->crashed_counter < 650) {
		uint32 r;
		if (Chance16R(1, 32, r)) {
			static const DirDiff delta[] = {
				DIRDIFF_45LEFT, DIRDIFF_SAME, DIRDIFF_SAME, DIRDIFF_45RIGHT
			};

			v->direction = ChangeDir(v->direction, delta[GB(r, 16, 2)]);
			SetAircraftPosition(v, v->x_pos, v->y_pos, v->z_pos);
			r = Random();
			CreateEffectVehicleRel(v,
				GB(r, 0, 4) - 4,
				GB(r, 4, 4) - 4,
				GB(r, 8, 4),
				EV_EXPLOSION_SMALL);
		}
	} else if (v->crashed_counter >= 10000) {
		/*  remove rubble of crashed airplane */

		/* clear runway-in on all airports, set by crashing plane
		 * small airports use AIRPORT_BUSY, city airports use RUNWAY_IN_OUT_block, etc.
		 * but they all share the same number */
		if (st != NULL) {
			CLRBITS(st->airport.flags, RUNWAY_IN_block);
			CLRBITS(st->airport.flags, RUNWAY_IN_OUT_block); // commuter airport
			CLRBITS(st->airport.flags, RUNWAY_IN2_block);    // intercontinental
		}

		delete v;

		return false;
	}

	return true;
}

static void HandleBrokenAircraft(Aircraft *v)
{
	if (v->breakdown_ctr != 1) {
		v->breakdown_ctr = 1;
		v->vehstatus |= VS_AIRCRAFT_BROKEN;

		if (v->breakdowns_since_last_service != 255)
			v->breakdowns_since_last_service++;
		SetWindowDirty(WC_VEHICLE_VIEW, v->index);
		SetWindowDirty(WC_VEHICLE_DETAILS, v->index);
	}
}


static void HandleAircraftSmoke(Aircraft *v)
{
	static const struct {
		int8 x;
		int8 y;
	} smoke_pos[] = {
		{  5,  5 },
		{  6,  0 },
		{  5, -5 },
		{  0, -6 },
		{ -5, -5 },
		{ -6,  0 },
		{ -5,  5 },
		{  0,  6 }
	};

	if (!(v->vehstatus & VS_AIRCRAFT_BROKEN)) return;

	if (v->cur_speed < 10) {
		v->vehstatus &= ~VS_AIRCRAFT_BROKEN;
		v->breakdown_ctr = 0;
		return;
	}

	if ((v->tick_counter & 0x1F) == 0) {
		CreateEffectVehicleRel(v,
			smoke_pos[v->direction].x,
			smoke_pos[v->direction].y,
			2,
			EV_SMOKE
		);
	}
}

void HandleMissingAircraftOrders(Aircraft *v)
{
	/*
	 * We do not have an order. This can be divided into two cases:
	 * 1) we are heading to an invalid station. In this case we must
	 *    find another airport to go to. If there is nowhere to go,
	 *    we will destroy the aircraft as it otherwise will enter
	 *    the holding pattern for the first airport, which can cause
	 *    the plane to go into an undefined state when building an
	 *    airport with the same StationID.
	 * 2) we are (still) heading to a (still) valid airport, then we
	 *    can continue going there. This can happen when you are
	 *    changing the aircraft's orders while in-flight or in for
	 *    example a depot. However, when we have a current order to
	 *    go to a depot, we have to keep that order so the aircraft
	 *    actually stops.
	 */
	const Station *st = GetTargetAirportIfValid(v);
	if (st == NULL) {
		CommandCost ret;
		CompanyID old_company = _current_company;

		_current_company = v->owner;
		ret = DoCommand(v->tile, v->index, 0, DC_EXEC, CMD_SEND_AIRCRAFT_TO_HANGAR);
		_current_company = old_company;

		if (ret.Failed()) CrashAirplane(v);
	} else if (!v->current_order.IsType(OT_GOTO_DEPOT)) {
		v->current_order.Free();
	}
}


TileIndex Aircraft::GetOrderStationLocation(StationID station)
{
	/* Orders are changed in flight, ensure going to the right station. */
	if (this->state == FLYING) {
		AircraftNextAirportPos_and_Order(this);
	}

	/* Aircraft do not use dest-tile */
	return 0;
}

void Aircraft::MarkDirty()
{
	this->UpdateViewport(false, false);
	if (this->subtype == AIR_HELICOPTER) this->Next()->Next()->cur_image = GetRotorImage(this);
}


uint Aircraft::Crash(bool flooded)
{
	uint pass = Vehicle::Crash(flooded) + 2; // pilots
	this->crashed_counter = flooded ? 9000 : 0; // max 10000, disappear pretty fast when flooded

	return pass;
}

static void CrashAirplane(Aircraft *v)
{
	CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE);

	uint pass = v->Crash();
	SetDParam(0, pass);

	v->cargo.Truncate(0);
	v->Next()->cargo.Truncate(0);
	const Station *st = GetTargetAirportIfValid(v);
	StringID newsitem;
	if (st == NULL) {
		newsitem = STR_NEWS_PLANE_CRASH_OUT_OF_FUEL;
	} else {
		SetDParam(1, st->index);
		newsitem = STR_NEWS_AIRCRAFT_CRASH;
	}

	AI::NewEvent(v->owner, new AIEventVehicleCrashed(v->index, v->tile, st == NULL ? AIEventVehicleCrashed::CRASH_AIRCRAFT_NO_AIRPORT : AIEventVehicleCrashed::CRASH_PLANE_LANDING));

	AddVehicleNewsItem(newsitem,
		NS_ACCIDENT,
		v->index,
		st != NULL ? st->index : INVALID_STATION);

	SndPlayVehicleFx(SND_12_EXPLOSION, v);
}

static void MaybeCrashAirplane(Aircraft *v)
{
	if (_settings_game.vehicle.plane_crashes == 0) return;

	Station *st = Station::Get(v->targetairport);

	/* FIXME -- MaybeCrashAirplane -> increase crashing chances of very modern airplanes on smaller than AT_METROPOLITAN airports */
	uint32 prob = (0x4000 << _settings_game.vehicle.plane_crashes);
	if ((st->airport.GetFTA()->flags & AirportFTAClass::SHORT_STRIP) &&
			(AircraftVehInfo(v->engine_type)->subtype & AIR_FAST) &&
			!_cheats.no_jetcrash.value) {
		prob /= 20;
	} else {
		prob /= 1500;
	}

	if (GB(Random(), 0, 22) > prob) return;

	/* Crash the airplane. Remove all goods stored at the station. */
	for (CargoID i = 0; i < NUM_CARGO; i++) {
		st->goods[i].rating = 1;
		st->goods[i].cargo.Truncate(0);
	}

	CrashAirplane(v);
}

/** we've landed and just arrived at a terminal */
static void AircraftEntersTerminal(Aircraft *v)
{
	if (v->current_order.IsType(OT_GOTO_DEPOT)) return;

	Station *st = Station::Get(v->targetairport);
	v->last_station_visited = v->targetairport;

	/* Check if station was ever visited before */
	if (!(st->had_vehicle_of_type & HVOT_AIRCRAFT)) {
		st->had_vehicle_of_type |= HVOT_AIRCRAFT;
		SetDParam(0, st->index);
		/* show newsitem of celebrating citizens */
		AddVehicleNewsItem(
			STR_NEWS_FIRST_AIRCRAFT_ARRIVAL,
			(v->owner == _local_company) ? NS_ARRIVAL_COMPANY : NS_ARRIVAL_OTHER,
			v->index,
			st->index
		);
		AI::NewEvent(v->owner, new AIEventStationFirstVehicle(st->index, v->index));
	}

	v->BeginLoading();
}

static void AircraftLandAirplane(Aircraft *v)
{
	v->UpdateDeltaXY(INVALID_DIR);

	if (!PlayVehicleSound(v, VSE_TOUCHDOWN)) {
		SndPlayVehicleFx(SND_17_SKID_PLANE, v);
	}
}


/** set the right pos when heading to other airports after takeoff */
void AircraftNextAirportPos_and_Order(Aircraft *v)
{
	if (v->current_order.IsType(OT_GOTO_STATION) || v->current_order.IsType(OT_GOTO_DEPOT)) {
		v->targetairport = v->current_order.GetDestination();
	}

	const Station *st = GetTargetAirportIfValid(v);
	const AirportFTAClass *apc = st == NULL ? GetAirport(AT_DUMMY) : st->airport.GetFTA();
	v->pos = v->previous_pos = AircraftGetEntryPoint(v, apc);
}

void AircraftLeaveHangar(Aircraft *v)
{
	v->cur_speed = 0;
	v->subspeed = 0;
	v->progress = 0;
	v->direction = DIR_SE;
	v->vehstatus &= ~VS_HIDDEN;
	{
		Vehicle *u = v->Next();
		u->vehstatus &= ~VS_HIDDEN;

		/* Rotor blades */
		u = u->Next();
		if (u != NULL) {
			u->vehstatus &= ~VS_HIDDEN;
			u->cur_speed = 80;
		}
	}

	VehicleServiceInDepot(v);
	SetAircraftPosition(v, v->x_pos, v->y_pos, v->z_pos);
	InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	SetWindowClassesDirty(WC_AIRCRAFT_LIST);
}

////////////////////////////////////////////////////////////////////////////////
///////////////////   AIRCRAFT MOVEMENT SCHEME  ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
static void AircraftEventHandler_EnterTerminal(Aircraft *v, const AirportFTAClass *apc)
{
	AircraftEntersTerminal(v);
	v->state = apc->layout[v->pos].heading;
}

static void AircraftEventHandler_EnterHangar(Aircraft *v, const AirportFTAClass *apc)
{
	VehicleEnterDepot(v);
	v->state = apc->layout[v->pos].heading;
}

/** In an Airport Hangar */
static void AircraftEventHandler_InHangar(Aircraft *v, const AirportFTAClass *apc)
{
	/* if we just arrived, execute EnterHangar first */
	if (v->previous_pos != v->pos) {
		AircraftEventHandler_EnterHangar(v, apc);
		return;
	}

	/* if we were sent to the depot, stay there */
	if (v->current_order.IsType(OT_GOTO_DEPOT) && (v->vehstatus & VS_STOPPED)) {
		v->current_order.Free();
		return;
	}

	if (!v->current_order.IsType(OT_GOTO_STATION) &&
			!v->current_order.IsType(OT_GOTO_DEPOT))
		return;

	/* if the block of the next position is busy, stay put */
	if (AirportHasBlock(v, &apc->layout[v->pos], apc)) return;

	/* We are already at the target airport, we need to find a terminal */
	if (v->current_order.GetDestination() == v->targetairport) {
		/* FindFreeTerminal:
		 * 1. Find a free terminal, 2. Occupy it, 3. Set the vehicle's state to that terminal */
		if (v->subtype == AIR_HELICOPTER) {
			if (!AirportFindFreeHelipad(v, apc)) return; // helicopter
		} else {
			if (!AirportFindFreeTerminal(v, apc)) return; // airplane
		}
	} else { // Else prepare for launch.
		/* airplane goto state takeoff, helicopter to helitakeoff */
		v->state = (v->subtype == AIR_HELICOPTER) ? HELITAKEOFF : TAKEOFF;
	}
	AircraftLeaveHangar(v);
	AirportMove(v, apc);
}

/** At one of the Airport's Terminals */
static void AircraftEventHandler_AtTerminal(Aircraft *v, const AirportFTAClass *apc)
{
	/* if we just arrived, execute EnterTerminal first */
	if (v->previous_pos != v->pos) {
		AircraftEventHandler_EnterTerminal(v, apc);
		/* on an airport with helipads, a helicopter will always land there
		 * and get serviced at the same time - setting */
		if (_settings_game.order.serviceathelipad) {
			if (v->subtype == AIR_HELICOPTER && apc->helipads != NULL) {
				/* an exerpt of ServiceAircraft, without the invisibility stuff */
				v->date_of_last_service = _date;
				v->breakdowns_since_last_service = 0;
				v->reliability = Engine::Get(v->engine_type)->reliability;
				SetWindowDirty(WC_VEHICLE_DETAILS, v->index);
			}
		}
		return;
	}

	if (v->current_order.IsType(OT_NOTHING)) return;

	/* if the block of the next position is busy, stay put */
	if (AirportHasBlock(v, &apc->layout[v->pos], apc)) return;

	/* airport-road is free. We either have to go to another airport, or to the hangar
	 * ---> start moving */

	bool go_to_hangar = false;
	switch (v->current_order.GetType()) {
		case OT_GOTO_STATION: // ready to fly to another airport
			break;
		case OT_GOTO_DEPOT:   // visit hangar for serivicing, sale, etc.
			go_to_hangar = v->current_order.GetDestination() == v->targetairport;
			break;
		case OT_CONDITIONAL:
			/* In case of a conditional order we just have to wait a tick
			 * longer, so the conditional order can actually be processed;
			 * we should not clear the order as that makes us go nowhere. */
			return;
		default:  // orders have been deleted (no orders), goto depot and don't bother us
			v->current_order.Free();
			go_to_hangar = Station::Get(v->targetairport)->airport.HasHangar();
	}

	if (go_to_hangar) {
		v->state = HANGAR;
	} else {
		/* airplane goto state takeoff, helicopter to helitakeoff */
		v->state = (v->subtype == AIR_HELICOPTER) ? HELITAKEOFF : TAKEOFF;
	}
	AirportMove(v, apc);
}

static void AircraftEventHandler_General(Aircraft *v, const AirportFTAClass *apc)
{
	error("OK, you shouldn't be here, check your Airport Scheme!");
}

static void AircraftEventHandler_TakeOff(Aircraft *v, const AirportFTAClass *apc)
{
	PlayAircraftSound(v); // play takeoffsound for airplanes
	v->state = STARTTAKEOFF;
}

static void AircraftEventHandler_StartTakeOff(Aircraft *v, const AirportFTAClass *apc)
{
	v->state = ENDTAKEOFF;
	v->UpdateDeltaXY(INVALID_DIR);
}

static void AircraftEventHandler_EndTakeOff(Aircraft *v, const AirportFTAClass *apc)
{
	v->state = FLYING;
	/* get the next position to go to, differs per airport */
	AircraftNextAirportPos_and_Order(v);
}

static void AircraftEventHandler_HeliTakeOff(Aircraft *v, const AirportFTAClass *apc)
{
	v->state = FLYING;
	v->UpdateDeltaXY(INVALID_DIR);

	/* get the next position to go to, differs per airport */
	AircraftNextAirportPos_and_Order(v);

	/* Send the helicopter to a hangar if needed for replacement */
	if (v->NeedsAutomaticServicing()) {
		_current_company = v->owner;
		DoCommand(v->tile, v->index, DEPOT_SERVICE | DEPOT_LOCATE_HANGAR, DC_EXEC, CMD_SEND_AIRCRAFT_TO_HANGAR);
		_current_company = OWNER_NONE;
	}
}

static void AircraftEventHandler_Flying(Aircraft *v, const AirportFTAClass *apc)
{
	Station *st = Station::Get(v->targetairport);

	/* runway busy or not allowed to use this airstation, circle */
	if ((apc->flags & (v->subtype == AIR_HELICOPTER ? AirportFTAClass::HELICOPTERS : AirportFTAClass::AIRPLANES)) &&
			st->airport.tile != INVALID_TILE &&
			(st->owner == OWNER_NONE || st->owner == v->owner)) {
		/* {32,FLYING,NOTHING_block,37}, {32,LANDING,N,33}, {32,HELILANDING,N,41},
		 * if it is an airplane, look for LANDING, for helicopter HELILANDING
		 * it is possible to choose from multiple landing runways, so loop until a free one is found */
		byte landingtype = (v->subtype == AIR_HELICOPTER) ? HELILANDING : LANDING;
		const AirportFTA *current = apc->layout[v->pos].next;
		while (current != NULL) {
			if (current->heading == landingtype) {
				/* save speed before, since if AirportHasBlock is false, it resets them to 0
				 * we don't want that for plane in air
				 * hack for speed thingie */
				uint16 tcur_speed = v->cur_speed;
				uint16 tsubspeed = v->subspeed;
				if (!AirportHasBlock(v, current, apc)) {
					v->state = landingtype; // LANDING / HELILANDING
					/* it's a bit dirty, but I need to set position to next position, otherwise
					 * if there are multiple runways, plane won't know which one it took (because
					 * they all have heading LANDING). And also occupy that block! */
					v->pos = current->next_position;
					SETBITS(st->airport.flags, apc->layout[v->pos].block);
					return;
				}
				v->cur_speed = tcur_speed;
				v->subspeed = tsubspeed;
			}
			current = current->next;
		}
	}
	v->state = FLYING;
	v->pos = apc->layout[v->pos].next_position;
}

static void AircraftEventHandler_Landing(Aircraft *v, const AirportFTAClass *apc)
{
	v->state = ENDLANDING;
	AircraftLandAirplane(v);  // maybe crash airplane

	/* check if the aircraft needs to be replaced or renewed and send it to a hangar if needed */
	if (v->NeedsAutomaticServicing()) {
		_current_company = v->owner;
		DoCommand(v->tile, v->index, DEPOT_SERVICE, DC_EXEC, CMD_SEND_AIRCRAFT_TO_HANGAR);
		_current_company = OWNER_NONE;
	}
}

static void AircraftEventHandler_HeliLanding(Aircraft *v, const AirportFTAClass *apc)
{
	v->state = HELIENDLANDING;
	v->UpdateDeltaXY(INVALID_DIR);
}

static void AircraftEventHandler_EndLanding(Aircraft *v, const AirportFTAClass *apc)
{
	/* next block busy, don't do a thing, just wait */
	if (AirportHasBlock(v, &apc->layout[v->pos], apc)) return;

	/* if going to terminal (OT_GOTO_STATION) choose one
	 * 1. in case all terminals are busy AirportFindFreeTerminal() returns false or
	 * 2. not going for terminal (but depot, no order),
	 * --> get out of the way to the hangar. */
	if (v->current_order.IsType(OT_GOTO_STATION)) {
		if (AirportFindFreeTerminal(v, apc)) return;
	}
	v->state = HANGAR;

}

static void AircraftEventHandler_HeliEndLanding(Aircraft *v, const AirportFTAClass *apc)
{
	/*  next block busy, don't do a thing, just wait */
	if (AirportHasBlock(v, &apc->layout[v->pos], apc)) return;

	/* if going to helipad (OT_GOTO_STATION) choose one. If airport doesn't have helipads, choose terminal
	 * 1. in case all terminals/helipads are busy (AirportFindFreeHelipad() returns false) or
	 * 2. not going for terminal (but depot, no order),
	 * --> get out of the way to the hangar IF there are terminals on the airport.
	 * --> else TAKEOFF
	 * the reason behind this is that if an airport has a terminal, it also has a hangar. Airplanes
	 * must go to a hangar. */
	if (v->current_order.IsType(OT_GOTO_STATION)) {
		if (AirportFindFreeHelipad(v, apc)) return;
	}
	v->state = Station::Get(v->targetairport)->airport.HasHangar() ? HANGAR : HELITAKEOFF;
}

typedef void AircraftStateHandler(Aircraft *v, const AirportFTAClass *apc);
static AircraftStateHandler * const _aircraft_state_handlers[] = {
	AircraftEventHandler_General,        // TO_ALL         =  0
	AircraftEventHandler_InHangar,       // HANGAR         =  1
	AircraftEventHandler_AtTerminal,     // TERM1          =  2
	AircraftEventHandler_AtTerminal,     // TERM2          =  3
	AircraftEventHandler_AtTerminal,     // TERM3          =  4
	AircraftEventHandler_AtTerminal,     // TERM4          =  5
	AircraftEventHandler_AtTerminal,     // TERM5          =  6
	AircraftEventHandler_AtTerminal,     // TERM6          =  7
	AircraftEventHandler_AtTerminal,     // HELIPAD1       =  8
	AircraftEventHandler_AtTerminal,     // HELIPAD2       =  9
	AircraftEventHandler_TakeOff,        // TAKEOFF        = 10
	AircraftEventHandler_StartTakeOff,   // STARTTAKEOFF   = 11
	AircraftEventHandler_EndTakeOff,     // ENDTAKEOFF     = 12
	AircraftEventHandler_HeliTakeOff,    // HELITAKEOFF    = 13
	AircraftEventHandler_Flying,         // FLYING         = 14
	AircraftEventHandler_Landing,        // LANDING        = 15
	AircraftEventHandler_EndLanding,     // ENDLANDING     = 16
	AircraftEventHandler_HeliLanding,    // HELILANDING    = 17
	AircraftEventHandler_HeliEndLanding, // HELIENDLANDING = 18
	AircraftEventHandler_AtTerminal,     // TERM7          = 19
	AircraftEventHandler_AtTerminal,     // TERM8          = 20
	AircraftEventHandler_AtTerminal,     // HELIPAD3       = 21
	AircraftEventHandler_AtTerminal,     // HELIPAD4       = 22
};

static void AirportClearBlock(const Aircraft *v, const AirportFTAClass *apc)
{
	/* we have left the previous block, and entered the new one. Free the previous block */
	if (apc->layout[v->previous_pos].block != apc->layout[v->pos].block) {
		Station *st = Station::Get(v->targetairport);

		CLRBITS(st->airport.flags, apc->layout[v->previous_pos].block);
	}
}

static void AirportGoToNextPosition(Aircraft *v)
{
	/* if aircraft is not in position, wait until it is */
	if (!AircraftController(v)) return;

	const AirportFTAClass *apc = Station::Get(v->targetairport)->airport.GetFTA();

	AirportClearBlock(v, apc);
	AirportMove(v, apc); // move aircraft to next position
}

/* gets pos from vehicle and next orders */
static bool AirportMove(Aircraft *v, const AirportFTAClass *apc)
{
	/* error handling */
	if (v->pos >= apc->nofelements) {
		DEBUG(misc, 0, "[Ap] position %d is not valid for current airport. Max position is %d", v->pos, apc->nofelements-1);
		assert(v->pos < apc->nofelements);
	}

	const AirportFTA *current = &apc->layout[v->pos];
	/* we have arrived in an important state (eg terminal, hangar, etc.) */
	if (current->heading == v->state) {
		byte prev_pos = v->pos; // location could be changed in state, so save it before-hand
		byte prev_state = v->state;
		_aircraft_state_handlers[v->state](v, apc);
		if (v->state != FLYING) v->previous_pos = prev_pos;
		if (v->state != prev_state || v->pos != prev_pos) UpdateAircraftCache(v);
		return true;
	}

	v->previous_pos = v->pos; // save previous location

	/* there is only one choice to move to */
	if (current->next == NULL) {
		if (AirportSetBlocks(v, current, apc)) {
			v->pos = current->next_position;
			UpdateAircraftCache(v);
		} // move to next position
		return false;
	}

	/* there are more choices to choose from, choose the one that
	 * matches our heading */
	do {
		if (v->state == current->heading || current->heading == TO_ALL) {
			if (AirportSetBlocks(v, current, apc)) {
				v->pos = current->next_position;
				UpdateAircraftCache(v);
			} // move to next position
			return false;
		}
		current = current->next;
	} while (current != NULL);

	DEBUG(misc, 0, "[Ap] cannot move further on Airport! (pos %d state %d) for vehicle %d", v->pos, v->state, v->index);
	NOT_REACHED();
}

/*  returns true if the road ahead is busy, eg. you must wait before proceeding */
static bool AirportHasBlock(Aircraft *v, const AirportFTA *current_pos, const AirportFTAClass *apc)
{
	const AirportFTA *reference = &apc->layout[v->pos];
	const AirportFTA *next = &apc->layout[current_pos->next_position];

	/* same block, then of course we can move */
	if (apc->layout[current_pos->position].block != next->block) {
		const Station *st = Station::Get(v->targetairport);
		uint64 airport_flags = next->block;

		/* check additional possible extra blocks */
		if (current_pos != reference && current_pos->block != NOTHING_block) {
			airport_flags |= current_pos->block;
		}

		if (st->airport.flags & airport_flags) {
			v->cur_speed = 0;
			v->subspeed = 0;
			return true;
		}
	}
	return false;
}

/**
 * "reserve" a block for the plane
 * @param v airplane that requires the operation
 * @param current_pos of the vehicle in the list of blocks
 * @param apc airport on which block is requsted to be set
 * @returns true on success. Eg, next block was free and we have occupied it
 */
static bool AirportSetBlocks(Aircraft *v, const AirportFTA *current_pos, const AirportFTAClass *apc)
{
	const AirportFTA *next = &apc->layout[current_pos->next_position];
	const AirportFTA *reference = &apc->layout[v->pos];

	/* if the next position is in another block, check it and wait until it is free */
	if ((apc->layout[current_pos->position].block & next->block) != next->block) {
		uint64 airport_flags = next->block;
		/* search for all all elements in the list with the same state, and blocks != N
		 * this means more blocks should be checked/set */
		const AirportFTA *current = current_pos;
		if (current == reference) current = current->next;
		while (current != NULL) {
			if (current->heading == current_pos->heading && current->block != 0) {
				airport_flags |= current->block;
				break;
			}
			current = current->next;
		};

		/* if the block to be checked is in the next position, then exclude that from
		 * checking, because it has been set by the airplane before */
		if (current_pos->block == next->block) airport_flags ^= next->block;

		Station *st = Station::Get(v->targetairport);
		if (st->airport.flags & airport_flags) {
			v->cur_speed = 0;
			v->subspeed = 0;
			return false;
		}

		if (next->block != NOTHING_block) {
			SETBITS(st->airport.flags, airport_flags); // occupy next block
		}
	}
	return true;
}

static bool FreeTerminal(Aircraft *v, byte i, byte last_terminal)
{
	Station *st = Station::Get(v->targetairport);
	for (; i < last_terminal; i++) {
		if (!HasBit(st->airport.flags, _airport_terminal_flag[i])) {
			/* TERMINAL# HELIPAD# */
			v->state = _airport_terminal_state[i]; // start moving to that terminal/helipad
			SetBit(st->airport.flags, _airport_terminal_flag[i]); // occupy terminal/helipad
			return true;
		}
	}
	return false;
}

static uint GetNumTerminals(const AirportFTAClass *apc)
{
	uint num = 0;

	for (uint i = apc->terminals[0]; i > 0; i--) num += apc->terminals[i];

	return num;
}

static bool AirportFindFreeTerminal(Aircraft *v, const AirportFTAClass *apc)
{
	/* example of more terminalgroups
	 * {0,HANGAR,NOTHING_block,1}, {0,255,TERM_GROUP1_block,0}, {0,255,TERM_GROUP2_ENTER_block,1}, {0,0,N,1},
	 * Heading 255 denotes a group. We see 2 groups here:
	 * 1. group 0 -- TERM_GROUP1_block (check block)
	 * 2. group 1 -- TERM_GROUP2_ENTER_block (check block)
	 * First in line is checked first, group 0. If the block (TERM_GROUP1_block) is free, it
	 * looks at the corresponding terminals of that group. If no free ones are found, other
	 * possible groups are checked (in this case group 1, since that is after group 0). If that
	 * fails, then attempt fails and plane waits
	 */
	if (apc->terminals[0] > 1) {
		const Station *st = Station::Get(v->targetairport);
		const AirportFTA *temp = apc->layout[v->pos].next;

		while (temp != NULL) {
			if (temp->heading == 255) {
				if (!(st->airport.flags & temp->block)) {
					/* read which group do we want to go to?
					 * (the first free group) */
					uint target_group = temp->next_position + 1;

					/* at what terminal does the group start?
					 * that means, sum up all terminals of
					 * groups with lower number */
					uint group_start = 0;
					for (uint i = 1; i < target_group; i++) {
						group_start += apc->terminals[i];
					}

					uint group_end = group_start + apc->terminals[target_group];
					if (FreeTerminal(v, group_start, group_end)) return true;
				}
			} else {
				/* once the heading isn't 255, we've exhausted the possible blocks.
				 * So we cannot move */
				return false;
			}
			temp = temp->next;
		}
	}

	/* if there is only 1 terminalgroup, all terminals are checked (starting from 0 to max) */
	return FreeTerminal(v, 0, GetNumTerminals(apc));
}

static uint GetNumHelipads(const AirportFTAClass *apc)
{
	uint num = 0;

	for (uint i = apc->helipads[0]; i > 0; i--) num += apc->helipads[i];

	return num;
}


static bool AirportFindFreeHelipad(Aircraft *v, const AirportFTAClass *apc)
{
	/* if an airport doesn't have helipads, use terminals */
	if (apc->helipads == NULL) return AirportFindFreeTerminal(v, apc);

	/* if there are more helicoptergroups, pick one, just as in AirportFindFreeTerminal() */
	if (apc->helipads[0] > 1) {
		const Station *st = Station::Get(v->targetairport);
		const AirportFTA *temp = apc->layout[v->pos].next;

		while (temp != NULL) {
			if (temp->heading == 255) {
				if (!(st->airport.flags & temp->block)) {

					/* read which group do we want to go to?
					 * (the first free group) */
					uint target_group = temp->next_position + 1;

					/* at what terminal does the group start?
					 * that means, sum up all terminals of
					 * groups with lower number */
					uint group_start = 0;
					for (uint i = 1; i < target_group; i++) {
						group_start += apc->helipads[i];
					}

					uint group_end = group_start + apc->helipads[target_group];
					if (FreeTerminal(v, group_start, group_end)) return true;
				}
			} else {
				/* once the heading isn't 255, we've exhausted the possible blocks.
				 * So we cannot move */
				return false;
			}
			temp = temp->next;
		}
	} else {
		/* only 1 helicoptergroup, check all helipads
		 * The blocks for helipads start after the last terminal (MAX_TERMINALS) */
		return FreeTerminal(v, MAX_TERMINALS, GetNumHelipads(apc) + MAX_TERMINALS);
	}
	return false; // it shouldn't get here anytime, but just to be sure
}

static bool AircraftEventHandler(Aircraft *v, int loop)
{
	v->tick_counter++;

	if (v->vehstatus & VS_CRASHED) {
		return HandleCrashedAircraft(v);
	}

	if (v->vehstatus & VS_STOPPED) return true;

	/* aircraft is broken down? */
	if (v->breakdown_ctr != 0) {
		if (v->breakdown_ctr <= 2) {
			HandleBrokenAircraft(v);
		} else {
			if (!v->current_order.IsType(OT_LOADING)) v->breakdown_ctr--;
		}
	}

	HandleAircraftSmoke(v);
	ProcessOrders(v);
	v->HandleLoading(loop != 0);

	if (v->current_order.IsType(OT_LOADING) || v->current_order.IsType(OT_LEAVESTATION)) return true;

	AirportGoToNextPosition(v);

	return true;
}

bool Aircraft::Tick()
{
	if (!this->IsNormalAircraft()) return true;

	if (!(this->vehstatus & VS_STOPPED)) this->running_ticks++;

	if (this->subtype == AIR_HELICOPTER) HelicopterTickHandler(this);

	this->current_order_time++;

	for (uint i = 0; i != 2; i++) {
		/* stop if the aircraft was deleted */
		if (!AircraftEventHandler(this, i)) return false;
	}

	return true;
}


/** Returns aircraft's target station if v->target_airport
 * is a valid station with airport.
 * @param v vehicle to get target airport for
 * @return pointer to target station, NULL if invalid
 */
Station *GetTargetAirportIfValid(const Aircraft *v)
{
	assert(v->type == VEH_AIRCRAFT);

	Station *st = Station::GetIfValid(v->targetairport);
	if (st == NULL) return NULL;

	return st->airport.tile == INVALID_TILE ? NULL : st;
}

/**
 * Updates the status of the Aircraft heading or in the station
 * @param st Station been updated
 */
void UpdateAirplanesOnNewStation(const Station *st)
{
	/* only 1 station is updated per function call, so it is enough to get entry_point once */
	const AirportFTAClass *ap = st->airport.GetFTA();

	Aircraft *v;
	FOR_ALL_AIRCRAFT(v) {
		if (v->IsNormalAircraft()) {
			if (v->targetairport == st->index) { // if heading to this airport
				/* update position of airplane. If plane is not flying, landing, or taking off
				 * you cannot delete airport, so it doesn't matter */
				if (v->state >= FLYING) { // circle around
					v->pos = v->previous_pos = AircraftGetEntryPoint(v, ap);
					v->state = FLYING;
					UpdateAircraftCache(v);
					/* landing plane needs to be reset to flying height (only if in pause mode upgrade,
					 * in normal mode, plane is reset in AircraftController. It doesn't hurt for FLYING */
					GetNewVehiclePosResult gp = GetNewVehiclePos(v);
					/* set new position x,y,z */
					SetAircraftPosition(v, gp.x, gp.y, GetAircraftFlyingAltitude(v));
				} else {
					assert(v->state == ENDTAKEOFF || v->state == HELITAKEOFF);
					byte takeofftype = (v->subtype == AIR_HELICOPTER) ? HELITAKEOFF : ENDTAKEOFF;
					/* search in airportdata for that heading
					 * easiest to do, since this doesn't happen a lot */
					for (uint cnt = 0; cnt < ap->nofelements; cnt++) {
						if (ap->layout[cnt].heading == takeofftype) {
							v->pos = ap->layout[cnt].position;
							UpdateAircraftCache(v);
							break;
						}
					}
				}
			}
		}
	}
}
