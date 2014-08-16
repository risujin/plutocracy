/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Handles ship combat mechanics */

#include "g_common.h"

/* Milliseconds between boarding attack rolls */
#define BOARD_INTERVAL 2000

/* Number of dice to roll for a boarding attack */
#define BOARD_DICE 5

/******************************************************************************\
 Check if we want to start boarding.
\******************************************************************************/
static void start_boarding(g_ship_t *ship)
{
        int i, neighbors[3];
        g_ship_t *target_ship;

        if (!ship->target_board)
                return;
        target_ship = ship->target_ship;

        /* The target may have turned friendly in the mean time */
        if (!G_ship_hostile(ship, target_ship->client)) {
                ship->target_board = FALSE;
                Py_CLEAR(ship->target_ship);
                return;
        }

        /* The ship must be adjacent to begin boarding */
        R_tile_neighbors(ship->tile, neighbors);
        for (i = 0; g_tiles[neighbors[i]].ship != target_ship; i++)
                if (i >= 2)
                        return;

        /* Start a boarding attack */
        Py_INCREF(target_ship);
        ship->boarding_ship = target_ship;
        ship->boarding++;
        ship->modified = TRUE;
        target_ship->boarding++;

        /* Host boarding announcements */
        if (G_ship_controlled_by(ship, n_client_id))
                I_popup(&ship->model->origin,
                        C_va(C_str("g-boarding", "%s boarding the %s!"),
                             target_ship->name, ship->name));
        else if (G_ship_controlled_by(target_ship, n_client_id))
                I_popup(&ship->model->origin,
                        C_va(C_str("g-boarded", "%s is being boarded!"),
                             ship->name));
}

/******************************************************************************\
 Perform a boarding attack/defend roll. Returns TRUE on victory.
\******************************************************************************/
static bool ship_board_attack(g_ship_t *ship, g_ship_t *defender, int power)
{
        int crew;

        /* How much crew did attacker kill? */
        crew = C_roll_dice(BOARD_DICE, power) / BOARD_DICE - 1;
        if (crew < 1)
                return FALSE;
        G_store_add(defender->store, G_CT_CREW, -crew);

        /* Did we win? */
        if (defender->store->cargo[G_CT_CREW].amount >= 1)
                return FALSE;
        ship->boarding--;
        defender->boarding--;
        C_assert(ship->boarding >= 0);
        C_assert(defender->boarding >= 0);

        /* Reset orders for defeated ship */
        Py_CLEAR(defender->target_ship);
        defender->target = -1;

        /* Not enough crew to capture */
        if (ship->store->cargo[G_CT_CREW].amount < 2)
                G_ship_change_client(defender, N_SERVER_ID);

        /* Capture the ship and transfer a crew unit */
        else {
                G_store_add(ship->store, G_CT_CREW, -1);
                G_store_add(defender->store, G_CT_CREW, 1);
                G_ship_change_client(defender, ship->client);
        }

        /* Send a full status update */
        ship->modified = TRUE;
        defender->modified = TRUE;

        return TRUE;
}

/******************************************************************************\
 Update boarding mechanics.
\******************************************************************************/
static void ship_update_board(g_ship_t *ship)
{
        if (c_time_msec < ship->combat_time ||
                        ship->boarding_ship == NULL)
                return;
        if (ship_board_attack(ship, ship->boarding_ship, 4) ||
            ship_board_attack(ship->boarding_ship, ship, 6)) {
                Py_CLEAR(ship->boarding_ship);
                return;
        }
        ship->combat_time = c_time_msec + BOARD_INTERVAL;
}

/******************************************************************************\
 Pump combat mechanics. The ship may have lost its crew or health after this
 call!
\******************************************************************************/
void G_ship_update_combat(g_ship_t *ship)
{
        if (n_client_id != N_HOST_CLIENT_ID)
                return;

        /* In a boarding fight */
        if (ship->boarding > 0) {
                ship_update_board(ship);
                return;
        }

        /* Can't start a fight or don't want to */
        if (ship->rear_tile >= 0 || !ship->target_ship ||
            ship->store->cargo[G_CT_CREW].amount < 2)
                return;

        start_boarding(ship);
}

