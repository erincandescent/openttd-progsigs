/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file vehiclelist.cpp Lists of vehicles. */

#include "stdafx.h"
#include "vehicle_gui.h"
#include "train.h"
#include "vehiclelist.h"

/**
 * Generate a list of vehicles inside a depot.
 * @param type    Type of vehicle
 * @param tile    The tile the depot is located on
 * @param engines Pointer to list to add vehicles to
 * @param wagons  Pointer to list to add wagons to (can be NULL)
 * @param individual_wagons If true add every wagon to #wagons which is not attached to an engine. If false only add the first wagon of every row.
 */
void BuildDepotVehicleList(VehicleType type, TileIndex tile, VehicleList *engines, VehicleList *wagons, bool individual_wagons)
{
	engines->Clear();
	if (wagons != NULL && wagons != engines) wagons->Clear();

	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		/* General tests for all vehicle types */
		if (v->type != type) continue;
		if (v->tile != tile) continue;

		switch (type) {
			case VEH_TRAIN: {
				const Train *t = Train::From(v);
				if (t->IsArticulatedPart() || t->IsRearDualheaded()) continue;
				if (t->track != TRACK_BIT_DEPOT) continue;
				if (wagons != NULL && t->First()->IsFreeWagon()) {
					if (individual_wagons || t->IsFreeWagon()) *wagons->Append() = t;
					continue;
				}
				break;
			}

			default:
				if (!v->IsInDepot()) continue;
				break;
		}

		if (!v->IsPrimaryVehicle()) continue;

		*engines->Append() = v;
	}

	/* Ensure the lists are not wasting too much space. If the lists are fresh
	 * (i.e. built within a command) then this will actually do nothing. */
	engines->Compact();
	if (wagons != NULL && wagons != engines) wagons->Compact();
}

/**
 * Generate a list of vehicles based on window type.
 * @param list        Pointer to list to add vehicles to
 * @param type        Type of vehicle
 * @param owner       Company to generate list for
 * @param index       This parameter has different meanings depending on window_type
 *    <ul>
 *      <li>VLW_STATION_LIST:  index of station to generate a list for</li>
 *      <li>VLW_SHARED_ORDERS: index of order to generate a list for<li>
 *      <li>VLW_STANDARD: not used<li>
 *      <li>VLW_DEPOT_LIST: TileIndex of the depot/hangar to make the list for</li>
 *      <li>VLW_GROUP_LIST: index of group to generate a list for</li>
 *      <li>VLW_WAYPOINT_LIST: index of waypoint to generate a list for</li>
 *    </ul>
 * @param window_type The type of window the list is for, using the VLW_ flags in vehicle_gui.h
 * @return false if invalid list is requested
 */
bool GenerateVehicleSortList(VehicleList *list, VehicleType type, Owner owner, uint32 index, uint16 window_type)
{
	list->Clear();

	const Vehicle *v;

	switch (window_type) {
		case VLW_STATION_LIST:
			FOR_ALL_VEHICLES(v) {
				if (v->type == type && v->IsPrimaryVehicle()) {
					const Order *order;

					FOR_VEHICLE_ORDERS(v, order) {
						if (order->IsType(OT_GOTO_STATION) && order->GetDestination() == index) {
							*list->Append() = v;
							break;
						}
					}
				}
			}
			break;

		case VLW_SHARED_ORDERS:
			/* Add all vehicles from this vehicle's shared order list */
			v = Vehicle::GetIfValid(index);
			if (v == NULL || v->type != type || !v->IsPrimaryVehicle()) return false;

			for (; v != NULL; v = v->NextShared()) {
				*list->Append() = v;
			}
			break;

		case VLW_STANDARD:
			FOR_ALL_VEHICLES(v) {
				if (v->type == type && v->owner == owner && v->IsPrimaryVehicle()) {
					*list->Append() = v;
				}
			}
			break;

		case VLW_DEPOT_LIST:
			FOR_ALL_VEHICLES(v) {
				if (v->type == type && v->IsPrimaryVehicle()) {
					const Order *order;

					FOR_VEHICLE_ORDERS(v, order) {
						if (order->IsType(OT_GOTO_DEPOT) && !(order->GetDepotActionType() & ODATFB_NEAREST_DEPOT) && order->GetDestination() == index) {
							*list->Append() = v;
							break;
						}
					}
				}
			}
			break;

		case VLW_WAYPOINT_LIST:
			FOR_ALL_VEHICLES(v) {
				if (v->type == type && v->IsPrimaryVehicle()) {
					const Order *order;

					FOR_VEHICLE_ORDERS(v, order) {
						if (order->IsType(OT_GOTO_WAYPOINT) && order->GetDestination() == index) {
							*list->Append() = v;
							break;
						}
					}
				}
			}
			break;

		case VLW_GROUP_LIST:
			FOR_ALL_VEHICLES(v) {
				if (v->type == type && v->IsPrimaryVehicle() &&
						v->owner == owner && v->group_id == index) {
					*list->Append() = v;
				}
			}
			break;

		default: return false;
	}

	list->Compact();
	return true;
}
