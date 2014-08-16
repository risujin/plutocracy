/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Spawning, updating, and rendering ships */

#include "g_common.h"

/* Maximum health value */
#define HEALTH_MAX 100

/* Maximum crew value */
#define CREW_MAX (G_SHIP_OPTIMAL_CREW * 400)

/* Ships dict */
PyObject *g_ship_dict;

/* Current ship id */
g_ship_id ship_id = 0;

/* The ship the mouse is hovering over and the currently selected ship */
g_ship_t *g_hover_ship, *g_selected_ship;

static int focus_stamp;

/******************************************************************************\
 Cleanup all ships.
\******************************************************************************/
void G_cleanup_ships(void)
{
        ship_id = 0;
        PyDict_Clear(g_ship_dict);
}

/******************************************************************************\
 Send a ship's spawn information.
\******************************************************************************/
void G_ship_send_spawn(g_ship_t *ship, n_client_id_t client)
{
        if (!ship->in_use)
                return;
        N_send(client, "12121", G_SM_SHIP_SPAWN, ship->id,
               ship->client, ship->tile, ship->class->class_id);
}

/******************************************************************************\
 Send a ship's name.
\******************************************************************************/
void G_ship_send_name(g_ship_t *ship, n_client_id_t client)
{
        if (!ship->in_use)
                return;
        N_send(client, "12s", G_SM_SHIP_NAME, ship->id, ship->name);
}

/******************************************************************************\
 Find an available tile around [tile] (including [tile]) and spawn a new ship
 of the given class there. If [tile] is negative, the ship will be placed on
 a random open tile. If [id] is negative, ship_id will be used instead and
 ship_id will be incremented.
\******************************************************************************/
g_ship_t *G_ship_spawn(g_ship_id id, n_client_id_t client, int tile,
                g_ship_type_t type)
{
        g_ship_t *ship;
        ShipClass *sc;

        if (!N_client_valid(client) || tile >= r_tiles_max ||
            type < 0 || type >= PyList_GET_SIZE(g_ship_class_list)) {
                C_warning("Invalid parameters (%d, %d, %d, %d)",
                          id, client, tile, type);
                return NULL;
        }

        /* Get new id if [id] is not given */
        if (id < 0) {
                if(ship_id == C_SHORT_MAX && n_client_id == N_HOST_CLIENT_ID) {
                        N_send(client, "12ss", G_SM_POPUP, tile, "g-ship-max",
                               "Wow you have reached the maximum limit of 32767"
                               " ships");
                        return NULL;
                }
                id = ship_id++;
        }

        /* Ship limits */
        if(n_client_id == N_HOST_CLIENT_ID) {
                int ships, limit;
                g_ship_t *ship;
                PyObject *key;
                Py_ssize_t pos = 0;
                ships = 0;
                limit = g_player_ship_limit.value.n;
                /* Count ships */
                while (PyDict_Next(g_ship_dict, &pos, &key, (PyObject**)&ship)){
                        if (!ship->in_use || ship->health <= 0 ||
                            client != ship->client)
                                continue;
                        ships++;
                }
                if (ships >= limit) {
                        N_send(client, "12ss", G_SM_POPUP, tile, "g-ship-limit",
                               "You have reached the maximum number of ships");
                        return NULL;
                }
        }

        /* If we are to spawn the ship anywhere, pick a random tile */
        if (tile < 0)
                tile = G_random_open_tile();
        if (tile < 0)
                return NULL;

        /* Find an available tile to start the ship on */
        if (!G_tile_open(tile, NULL)) {
                int i, neighbors[12], len;

                /* Not being able to fit a ship in is common, so don't complain
                   if this happens */
                len = R_tile_region(tile, neighbors);
                for (i = 0; i < len && !G_tile_open(neighbors[i], NULL); i++)
                        if (i >= len)
                                return NULL;
                if (i >= len)
                        return NULL;
                tile = neighbors[i];
        }
        sc = (ShipClass*)PyList_GET_ITEM(g_ship_class_list, type);
        /* Initialize ship structure */
        ship = (g_ship_t*)Ship_new(&ShipType, NULL, NULL);
        ship->in_use = TRUE;
        ship->id = id;
        ship->tile = ship->target = tile;
        ship->target_ship = NULL;
        ship->rear_tile = -1;
        ship->progress = 1.f;
        ship->client = client;
        ship->health = sc->health;
        ship->forward = r_tiles[tile].forward;
        ship->trade_tile = -1;
        ship->focus_stamp = -1;
        ship->boarding_ship = NULL;
        Py_INCREF(sc);
        ship->class = sc;

        /* Start out unnamed */
        C_strncpy_buf(ship->name, C_va("Unnamed id: %d", ship->id));

        /* Initialize store */
        ship->store = G_store_init(sc->cargo);

        /* Place the ship on the tile */
        if(!(ship->model = R_model_init(sc->model_path, TRUE)))
        {
                C_warning("Ship model: %s failed to load", sc->model_path);
                Py_DECREF(ship);
                return NULL;
        }
        G_tile_position_model(tile, ship->model);
        Py_INCREF(ship);
        g_tiles[tile].ship = ship;

        G_set_ship(id, ship);

        /* If we are the server, tell other clients */
        if (n_client_id == N_HOST_CLIENT_ID)
                G_ship_send_spawn(ship, N_BROADCAST_ID);

        /* If this is one of ours, name it */
        if (client == n_client_id) {
                G_get_name_buf(G_NT_SHIP, ship->name);
                N_send(N_SERVER_ID, "12s", G_CM_SHIP_NAME, id, ship->name);
        }

        /* If we spawned on a gib, collect it */
        G_ship_collect_gib(ship);

        return ship;
}

/******************************************************************************\
 Render ship 2D effects.
\******************************************************************************/
void G_render_ships(void)
{
        ShipClass *ship_class;
        g_ship_t *ship;
        PyObject *key;
        Py_ssize_t pos = 0;
        const g_tile_t *tile;
        c_color_t color;
        float crew, crew_max, health, health_max;

        if (i_limbo)
                return;
        while (PyDict_Next(g_ship_dict, &pos, &key, (PyObject**)&ship)) {
                if (!ship->in_use)
                        continue;
                C_assert(ship->tile >= 0 && ship->tile < r_tiles_max);
                tile = g_tiles + ship->tile;

                /* Don't bother rendering if the ship isn't visible */
                if (!tile->visible)
                        continue;

                /* Draw the status display */
                ship_class = ship->class;
                crew_max = ship->store->capacity * G_SHIP_OPTIMAL_CREW /
                           CREW_MAX;
                crew = (float)ship->store->cargo[G_CT_CREW].amount / CREW_MAX;
                health = (float)ship->health / HEALTH_MAX;
                health_max = (float)ship_class->health / HEALTH_MAX;
                color = g_nations[g_clients[ship->client].nation].color;
                R_render_ship_status(ship->model, health, health_max,
                                     crew, crew_max, color,
                                     g_selected_ship == ship,
                                     ship->client == n_client_id);

                /* Draw boarding status indicator */
                if (ship->boarding_ship) {
                        c_vec3_t origin_b;

                        origin_b = ship->boarding_ship->model->origin;
                        R_render_ship_boarding(ship->model->origin, origin_b,
                                               color);
                }
        }
}

/******************************************************************************\
 Configure the interface to reflect the ship's cargo and trade status.
\******************************************************************************/
static void ship_configure_trade(g_ship_t *ship)
{
        n_client_id_t trading_client;
        g_store_t *store;
        int i;

        /* Our client can't actually see this cargo -- we probably don't have
           the right data for it anyway! */
        if (!ship || !ship->store->visible[n_client_id]) {
                I_disable_trade();
                return;
        }

        /* Do we have a trading partner? */
        G_store_space(ship->store);
        if (ship->trade_tile > 0 && g_tiles[ship->trade_tile].building) {
                g_building_t *partner;
                partner = g_tiles[ship->trade_tile].building;
                I_enable_trade(TRUE, TRUE,
                               partner->class->name, ship->store->space_used,
                               ship->store->capacity);
                store = partner->store;
                trading_client = partner->client;
        } else if (ship->trade_tile > 0 && g_tiles[ship->trade_tile].ship) {
                g_ship_t *partner;
                partner = g_tiles[ship->trade_tile].ship;
                I_enable_trade(ship->client == n_client_id,
                               partner->client == n_client_id,
                               partner->name, ship->store->space_used,
                               ship->store->capacity);
                store = partner->store;
                trading_client = partner->client;
        } else {
                store = NULL;
                I_enable_trade(ship->client == n_client_id, FALSE, NULL,
                               ship->store->space_used, ship->store->capacity);
                trading_client = -1;
        }

        /* Configure cargo items */
        for (i = 0; i < G_CARGO_TYPES; i++) {
                i_cargo_info_t info;
                g_cargo_t *cargo;

                /* Our side */
                cargo = ship->store->cargo + i;
                info.amount = cargo->amount;
                info.auto_buy = cargo->auto_buy;
                info.auto_sell = cargo->auto_sell;
                info.buy_price = cargo->buy_price;
                info.sell_price = cargo->sell_price;
                info.minimum = cargo->minimum;
                info.maximum = cargo->maximum;

                /* No trade partner */
                info.p_buy_price = -1;
                info.p_sell_price = -1;
                if (!store) {
                        info.p_amount = -1;
                        I_configure_cargo(i, &info);
                        continue;
                }

                /* Trade partner's side */
                cargo = store->cargo + i;
                if (cargo->auto_sell) {
                        info.p_buy_price = cargo->sell_price;
                        info.p_maximum = cargo->maximum;
                }
                if (cargo->auto_buy) {
                        info.p_sell_price = cargo->buy_price;
                        info.p_minimum = cargo->minimum;
                }
                /* Allow free trading between ships of the same nation */
                if(ship->client == trading_client){
                        info.auto_buy = TRUE;
                        info.auto_sell = TRUE;
                        info.p_buy_price = 0;
                        info.p_sell_price = 0;
                        info.p_maximum = cargo->maximum;
                        info.p_minimum = cargo->minimum;
                }
                info.p_amount = cargo->amount;
                I_configure_cargo(i, &info);
        }
}

/******************************************************************************\
 Returns TRUE if a ship is capable of trading.
\******************************************************************************/
static bool ship_can_trade(g_ship_t *ship)
{
        return ship  && ship->in_use && ship->rear_tile < 0 &&
               ship->health > 0;
}

/******************************************************************************\
 Returns TRUE if a building is capable of trading.
\******************************************************************************/
static bool building_can_trade(g_building_t *building)
{
        return building && building->health > 0 && building->class->cargo > 0;
}

/******************************************************************************\
 Returns TRUE if a ship can trade with the given tile.
\******************************************************************************/
bool G_ship_can_trade_with(g_ship_t *ship, int tile)
{
        int i, neighbors[3];

        R_tile_neighbors(ship->tile, neighbors);
        for (i = 0; i < 3; i++) {
                if (neighbors[i] != tile)
                        continue;
                if(ship_can_trade(g_tiles[tile].ship) &&
                   g_tiles[tile].ship->boarding_ship != ship &&
                   ship->boarding_ship != g_tiles[tile].ship)
                        return TRUE;
                else if (building_can_trade(g_tiles[tile].building) &&
                         g_tiles[tile].building->nation ==
                                                g_clients[ship->client].nation)
                        return TRUE;
        }
        return FALSE;
}

/******************************************************************************\
 Check if the ship needs a new trading partner.
\******************************************************************************/
static void ship_update_trade(g_ship_t *ship)
{
        int i, trade_tile, neighbors[3];

        /* Only need to do this for our own ships */
        if (ship->client != n_client_id)
                return;

        /* Find a trading partner */
        trade_tile = -1;
        if (ship->rear_tile < 0) {
                R_tile_neighbors(ship->tile, neighbors);
                for (i = 0; i < 3; i++) {
                        g_ship_t *other_ship;
                        g_building_t *building;

                        other_ship = g_tiles[neighbors[i]].ship;
                        building = g_tiles[neighbors[i]].building;
                        if (ship_can_trade(other_ship) &&
                            other_ship->boarding_ship != ship &&
                            ship->boarding_ship != other_ship)
                                trade_tile = neighbors[i];
                        else if (building_can_trade(building) &&
                                 building->nation ==
                                              g_clients[ship->client].nation) {
                                trade_tile = neighbors[i];
                        }
                        else
                                continue;
                        /* Found our old trading partner */
                        if (trade_tile == ship->trade_tile)
                                return;
                }
        }

        /* Still no partner */
        if (trade_tile < 0 && ship->trade_tile < 0)
                return;

        /* Update to reflect our new trading partner */
        ship->trade_tile = trade_tile;
        if (g_selected_ship == ship)
                ship_configure_trade(ship);
}

/******************************************************************************\
 Send updated cargo information to clients. If [client] is negative, all
 clients that can see the ship's cargo are updated and only modified cargo
 entries are communicated.
\******************************************************************************/
void G_ship_send_cargo(g_ship_t *ship, n_client_id_t client)
{
        g_ship_id id;
        int i;
        bool broadcast;

        /* Host already knows everything */
        if (client == N_HOST_CLIENT_ID || n_client_id != N_HOST_CLIENT_ID)
                return;

        /* Nothing is sent if broadcasting an unmodified store */
        if ((broadcast = client < 0 || client == N_BROADCAST_ID) &&
            !ship->store->modified)
                return;

        id = ship->id;

        /* Pack all the cargo information */
        N_send_start();
        N_send_char(G_SM_SHIP_CARGO);
        N_send_short(id);
        G_store_send(ship->store,
                     !broadcast || client == N_SELECTED_ID);

        /* Already selected the target clients */
        if (client == N_SELECTED_ID) {
                N_send_selected(NULL);
                return;
        }

        /* Send to clients that can see complete cargo information */
        if (broadcast) {
                for (i = 0; i < N_CLIENTS_MAX; i++)
                        n_clients[i].selected = ship->store->visible[i];
                N_send_selected(NULL);
                return;
        }

        /* Send to a single client */
        if (ship->store->visible[client])
                N_send(client, NULL);
}

/******************************************************************************\
 Sends out the ship's current state.
\******************************************************************************/
void G_ship_send_state(g_ship_t *ship, n_client_id_t client)
{
        if (n_client_id != N_HOST_CLIENT_ID)
                return;
        N_broadcast_except(N_HOST_CLIENT_ID, "121212", G_SM_SHIP_STATE,
                           ship->id, ship->health,
                           ship->store->cargo[G_CT_CREW].amount,
                           ship->boarding, (ship->boarding_ship) ?
                                                 ship->boarding_ship->id : -1);
}

/******************************************************************************\
 Update which clients can see [ship]'s cargo. Also updates the ship's
 neighbors' visibility due to the [ship] having moved into its tile.
\******************************************************************************/
static void ship_update_visible(g_ship_t *ship)
{
        int i;
        bool old_visible[N_CLIENTS_MAX], selected;

        memcpy(old_visible, ship->store->visible, sizeof (old_visible));

        /* Cargo is always visable */
        C_one_buf(ship->store->visible);

        /* Reselect if our client's visibility toward this ship changed and
           we have it selected */
        if (old_visible[n_client_id] != ship->store->visible[n_client_id])
                G_ship_reselect(ship, -1);

        /* No need to send updates here if we aren't hosting */
        if (n_client_id != N_HOST_CLIENT_ID)
                return;

        /* Only send the update to clients that can see the store now but
           couldn't see it before */
        for (selected = FALSE, i = 0; i < N_CLIENTS_MAX; i++) {
                n_clients[i].selected = FALSE;
                if (i == N_HOST_CLIENT_ID || old_visible[i] ||
                    !ship->store->visible[i])
                        continue;
                n_clients[i].selected = TRUE;
                selected = TRUE;
        }
        if (selected)
                G_ship_send_cargo(ship, N_SELECTED_ID);
}

/******************************************************************************\
 Feed the ship's crew at regular interval.
\******************************************************************************/
static void ship_update_food(g_ship_t *ship)
{
        int crew, available;

        /* Only the host updates food */
        if (n_client_id != N_HOST_CLIENT_ID)
                return;

        /* Not time to eat yet */
        if (c_time_msec < ship->lunch_time ||
            ship->store->cargo[G_CT_CREW].amount <= 0)
                return;
        crew = ship->store->cargo[G_CT_CREW].amount;

        /* Consume rations first */
        if (ship->store->cargo[G_CT_RATIONS].amount > 0) {
                available = G_cargo_nutritional_value(G_CT_RATIONS);
                G_store_add(ship->store, G_CT_RATIONS, -1);
        }

        /* Out of food, it's cannibalism time */
        else {
                available = G_cargo_nutritional_value(G_CT_CREW);
                G_store_add(ship->store, G_CT_CREW, -1);
        }

        ship->lunch_time = c_time_msec + available / crew;

        /* Did the crew just starve to death? */
        if (ship->store->cargo[G_CT_CREW].amount <= 0)
                G_ship_change_client(ship, N_SERVER_ID);
}

/******************************************************************************\
 Updates ship positions and actions.
\******************************************************************************/
void G_update_ships(void)
{
        g_ship_t *ship;
        PyObject *key;
        Py_ssize_t pos = 0;

        while (PyDict_Next(g_ship_dict, &pos, &key, (PyObject**)&ship)) {
                if (!ship->in_use)
                        continue;
                /* Reset modified status */
                ship->modified = FALSE;

                if (!g_game_over) {
                        G_ship_update_move(ship);
                        G_ship_update_combat(ship);
                        ship_update_trade(ship);
                        ship_update_food(ship);
                }
                ship_update_visible(ship);

                /* If the cargo manfiest changed, send updates once per frame */
                if (ship->store->modified)
                        G_ship_send_cargo(ship, -1);

                /* If the ship state changed, send an update */
                if (ship->modified)
                        G_ship_send_state(ship, -1);
        }
}

/******************************************************************************\
 Update which ship the mouse is hovering over and setup the hover window.
 Pass NULL to deselect.
\******************************************************************************/
void G_ship_hover(g_ship_t *ship)
{
        r_model_t *model = NULL;

        if (g_hover_ship)
                model = g_hover_ship->model;
        if (g_hover_ship == ship) {
                if (!ship)
                        return;

                /* Ship can get unhighlighted for whatever reason */
                if (!model->selected)
                        model->selected = R_MS_HOVER;

                return;
        }

        /* Deselect last hovered-over ship */
        if (g_hover_ship && model->selected == R_MS_HOVER)
        {
                model->selected = R_MS_NONE;
        }
        Py_CLEAR(g_hover_ship);
        /* Highlight the new ship */
        Py_XINCREF(ship);
        if(!(g_hover_ship = ship))
                return;
        model = ship->model;
        if (!model->selected)
                model->selected = R_MS_HOVER;
}

/******************************************************************************\
 Setup the quick info window to reflect a ship's information.
\******************************************************************************/
static void ship_quick_info(g_ship_t *ship)
{
        ShipClass *ship_class;
        i_color_t color;
        float prop;
        int i, total;

        if (!ship) {
                I_quick_info_close();
                return;
        }
        ship_class = ship->class;
        I_quick_info_show(ship->name, &ship->model->origin);

        /* Owner */
        color = G_nation_to_color(g_clients[ship->client].nation);
        I_quick_info_add_color("Owner:", g_clients[ship->client].name, color);

        /* Health */
        color = I_COLOR_ALT;
        prop = (float)ship->health / ship_class->health;
        if (prop >= 0.67)
                color = I_COLOR_GOOD;
        if (prop <= 0.33)
                color = I_COLOR_BAD;
        I_quick_info_add_color("Health:", C_va("%d/%d", ship->health,
                                               ship_class->health), color);

        /* Crew */
        color = I_COLOR_ALT;
        i = ship->store->cargo[G_CT_CREW].amount;
        total = (int)(ship->store->capacity * G_SHIP_OPTIMAL_CREW);
        prop = (float)i / total;
        if (prop >= 0.67)
                color = I_COLOR_GOOD;
        if (prop <= 0.33)
                color = I_COLOR_BAD;
        I_quick_info_add_color("Crew:", C_va("%d/%d", i, total), color);
        if (i <= 0)
                return;

        /* Food supply before resorting to cannibalism */
        if (!ship->store->visible[n_client_id])
                return;
        for (total = i = 0; i < G_CARGO_TYPES; i++) {
                if (i == G_CT_CREW)
                        continue;
                total += G_cargo_nutritional_value(i) *
                         ship->store->cargo[i].amount;
        }
        total = (total + ship->store->cargo[G_CT_CREW].amount - 1) /
                ship->store->cargo[G_CT_CREW].amount;
        if (total > 60000)
                I_quick_info_add_color("Food:", C_va("%d min", total / 60000),
                                       I_COLOR_GOOD);
        else if (total > 0)
                I_quick_info_add_color("Food:", C_va("%d sec", total / 1000),
                                       I_COLOR_ALT);
        else
                I_quick_info_add_color("Food:", "STARVING", I_COLOR_BAD);

}

/******************************************************************************\
 Select a ship. Pass a NULL [ship] to deselect.
\******************************************************************************/
void G_ship_select(g_ship_t *ship)
{
        r_model_t *model;

        if (g_selected_ship == ship)
                return;

        /* Deselect previous ship */
        if (g_selected_ship) {
                model = g_selected_ship->model;
                model->selected = R_MS_NONE;
                Py_CLEAR(g_selected_ship);
        }

        /* Select the new ship */
        if (ship) {
                model = ship->model;
                model->selected = R_MS_SELECTED;

                /* Only show the path of our own ships */
                if (ship->client == n_client_id)
                        R_select_path(ship->tile, ship->path);
                else
                        R_select_path(-1, NULL);
        }

        /* Deselect */
        else
                R_select_path(-1, NULL);

        ship_configure_trade(ship);
        ship_quick_info(ship);

        /* Selecting a ship resets the focus order */
        focus_stamp++;

        Py_XINCREF(ship);
        g_selected_ship = ship;
}

/******************************************************************************\
 Re-selects the currently selected ship if it is [ship] and if its client
 is [client]. If either [ship] or [client] is negative or NULL, they are ignored.
\******************************************************************************/
void G_ship_reselect(g_ship_t *ship, n_client_id_t client)
{
        g_ship_t *current;
        if (!g_selected_ship ||
            (ship && g_selected_ship != ship) ||
            (client >= 0 && g_selected_ship->client != client))
                return;
        current = g_selected_ship;
        Py_CLEAR(g_selected_ship);
        G_ship_select(current);
}

/******************************************************************************\
 Returns TRUE if a ship index is valid and can be controlled by the given
 client. Also doubles as a way to check if the index is valid to begin with.
\******************************************************************************/
bool G_ship_controlled_by(g_ship_t *ship, n_client_id_t client)
{
        return ship && ship->in_use &&
                ship->health > 0 && ship->client == client;
}

/******************************************************************************\
 Returns TRUE if the ship is owned by a hostile player.
\******************************************************************************/
bool G_ship_hostile(g_ship_t *ship, n_client_id_t to_client)
{
        n_client_id_t ship_client;

        if ( !ship || !ship->in_use || ship->client == to_client)
                return FALSE;
        ship_client = ship->client;

        /* For the tech preview, all nations are permanently at war */
        return g_clients[ship_client].nation != g_clients[to_client].nation ||
               g_clients[ship_client].nation == G_NN_PIRATE;
}

/******************************************************************************\
 Focus on the next suitable ship.
\******************************************************************************/
void G_focus_next_ship(void)
{
        PyObject *key;
        Py_ssize_t pos = 0;
        g_ship_t *ship, *best_ship;
        float best_dist;
        int tile, available;

        /* If we have a ship selected, just center on that */
        if (g_selected_ship) {
                tile = g_selected_ship->tile;
                R_rotate_cam_to(g_selected_ship->model->origin);
                return;
        }

        /* Find the closest available ship */
        available = 0;
        best_ship = NULL;
        best_dist = C_FLOAT_MAX;

        while (PyDict_Next(g_ship_dict, &pos, &key, (PyObject**)&ship)) {
                c_vec3_t origin;
                float dist;

                if (!G_ship_controlled_by(ship, n_client_id) ||
                    ship->focus_stamp >= focus_stamp)
                        continue;
                available++;
                origin = ship->model->origin;
                dist = C_vec3_len(C_vec3_sub(r_cam_origin, origin));
                if (dist < best_dist) {
                        best_dist = dist;
                        best_ship = ship;
                }
        }
        if (available <= 1)
                focus_stamp++;
        if (!best_ship)
                return;
        best_ship->focus_stamp = focus_stamp;
        tile = best_ship->tile;
        R_rotate_cam_to(best_ship->model->origin);
}

/******************************************************************************\
 Change a ship's owner.
\******************************************************************************/
void G_ship_change_client(g_ship_t *ship, n_client_id_t client)
{
        N_broadcast("121", G_SM_SHIP_OWNER, ship->id, client);
}

/******************************************************************************\
 Check if the ship's tile has a crate gib and give its contents to the ship.
\******************************************************************************/
void G_ship_collect_gib(g_ship_t *ship)
{
        int i, tile;

        tile = ship->tile;
        if (n_client_id != N_HOST_CLIENT_ID || !g_tiles[tile].gib)
                return;
        for (i = 0; i < G_CARGO_TYPES; i++)
                if (g_tiles[tile].gib->loot.cargo[i] > 0)
                        G_store_add(ship->store, i,
                                    g_tiles[tile].gib->loot.cargo[i]);
        G_tile_gib(tile, G_GT_NONE);
}

/******************************************************************************\
 Create a crate gib around the ship with the dropped cargo.
\******************************************************************************/
void G_ship_drop_cargo(g_ship_t *ship, g_cargo_type_t type, int amount)
{
        g_gib_t *gib;
        int i, neighbors[3], tile;

        /* Limit amount */
        if (ship->store->cargo[type].amount < amount)
                amount = ship->store->cargo[type].amount;
        if (amount < 1)
                return;

        /* Don't let them drop the last crew member */
        if (type == G_CT_CREW &&
            ship->store->cargo[type].amount - amount < 1)
                amount = ship->store->cargo[type].amount - 1;

        /* Find an existing crate or an open tile */
        R_tile_neighbors(ship->tile, neighbors);
        for (tile = -1, i = 0; i < 3; i++) {
                if (G_tile_open(neighbors[i], NULL))
                        tile = neighbors[i];
                if ((gib = g_tiles[neighbors[i]].gib))
                        break;
        }

        /* Spawn a new gib */
        if (!gib) {
                if (tile < 0)
                        return;
                G_tile_gib(tile, G_GT_CRATES);
                gib = g_tiles[tile].gib;
        }

        gib->loot.cargo[type] += amount;
        G_store_add(ship->store, type, -amount);
}
/******************************************************************************\
 Find the ship's class from its ring id
 Returns: Borrowed Reference
\******************************************************************************/
ShipClass *G_ship_class_from_ring_id(i_ring_icon_t id)
{
        ShipClass *sc;
        int i, classes;

        classes = PyList_GET_SIZE(g_ship_class_list);

        for(i=0; i < classes; i++)
        {
                sc = (ShipClass*)PyList_GET_ITEM(g_ship_class_list, i);
                if(sc->ring_id == id)
                {
                        return sc;
                }
        }
        return NULL;
}
/******************************************************************************\
 Find the ship's class index from its ring id
\******************************************************************************/
int G_ship_class_index_from_ring_id(i_ring_icon_t id)
{
        ShipClass *sc;
        int i, classes;

        classes = PyList_GET_SIZE(g_ship_class_list);

        for(i=0; i < classes; i++)
        {
                sc = (ShipClass*)PyList_GET_ITEM(g_ship_class_list, i);
                if(sc->ring_id == id)
                {
                        return i;
                }
        }
        return -1;
}

/******************************************************************************\
 Update ship trading ui if needed
\******************************************************************************/
void G_ship_update_trade_ui(g_ship_t *ship)
{
        if(!g_selected_ship)
                return;
        if(ship != g_selected_ship &&
           ship != g_tiles[g_selected_ship->trade_tile].ship)
                return;
        ship_configure_trade(g_selected_ship);
}
