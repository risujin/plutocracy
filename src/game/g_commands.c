/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Receives all user commands from the interface */

#include "g_common.h"

/* The ship that was right-clicked on */
static g_ship_id ring_ship;

/******************************************************************************\
 Interface wants us to leave the current game.
\******************************************************************************/
void G_leave_game(void)
{
        if (n_client_id == N_INVALID_ID)
                return;
        N_disconnect();
        I_popup(NULL, "Left the current game.");
}

/******************************************************************************\
 Change the player's nation.
\******************************************************************************/
void G_change_nation(int index)
{
        if (g_game_over)
                return;
        N_send(N_SERVER_ID, "11", G_CM_AFFILIATE, index);
}

/******************************************************************************\
 Callback for ring opened when right-clicking a tile.
\******************************************************************************/
void G_tile_ring_callback(int icon)
{
        if (g_selected_tile < 0 || g_game_over)
                return;
        N_send(N_SERVER_ID, "121", G_CM_TILE_RING, g_selected_tile, icon);
}

/******************************************************************************\
 Callback for ring opened when right-clicking a ship.
\******************************************************************************/
static void ship_ring(i_ring_icon_t icon)
{
        if (!g_selected_ship || ring_ship < 0 || g_game_over)
                return;
        N_send(N_SERVER_ID, "1212", G_CM_SHIP_RING, g_selected_ship->id,
               icon, ring_ship);
}

/******************************************************************************\
 Called when the interface root window receives a click. Returns FALSE is
 no action was taken.
\******************************************************************************/
bool G_process_click(int button)
{
        /* Grabbing has no other effect */
        if (button == SDL_BUTTON_MIDDLE)
                return FALSE;

        /* Left-clicking selects only */
        if (button == SDL_BUTTON_LEFT) {

                /* Selecting a ship */
                if (g_hover_ship) {
                        G_tile_select(-1);
                        G_ship_select(g_hover_ship != g_selected_ship ?
                                      g_hover_ship : NULL);
                        return TRUE;
                }

                /* Selecting a tile */
                if (g_hover_tile >= 0) {
                        G_ship_select(NULL);
                        G_tile_select(g_hover_tile != g_selected_tile ?
                                      g_hover_tile : -1);
                        return TRUE;
                }

                /* Failed to select anything */
                G_ship_select(NULL);
                G_tile_select(-1);
                return FALSE;
        }

        /* Only process right-clicks from here on */
        if (button != SDL_BUTTON_RIGHT)
                return FALSE;

        /* Controlling a ship */
        if (G_ship_controlled_by(g_selected_ship, n_client_id)) {
                if (g_game_over)
                        return TRUE;

                /* Ordered an ocean move */
                if (g_hover_tile >= 0 && G_tile_open(g_hover_tile, NULL))
                        N_send(N_SERVER_ID, "122", G_CM_SHIP_MOVE,
                               g_selected_ship->id, g_hover_tile);

                /* Right-clicked on another ship */
                if (g_hover_ship && g_hover_ship != g_selected_ship) {
                        ring_ship = g_hover_ship->id;
                        I_reset_ring();
                        I_add_to_ring(i_ri_board,
                                      G_ship_hostile(g_hover_ship, n_client_id),
                                      C_str("g-board", "Board"), NULL);
                        I_add_to_ring(i_ri_follow, TRUE,
                                      C_str("g-follow", "Follow"), NULL);
                        I_show_ring((i_ring_f)ship_ring);
                }

                return TRUE;
        }

        /* Opening the selected tile's ring menu */
        if (g_selected_tile >= 0 && g_selected_tile == g_hover_tile) {
                BuildingClass *bc;
                g_building_t *building;
                PyObject *callback, *res, *args;

                if (g_game_over)
                        return TRUE;
                building = g_tiles[g_selected_tile].building;
                if (!building) {
                        /* Get the callback for an empty tile */
                        bc = (BuildingClass*)PyList_GET_ITEM(
                                                     g_building_class_list, 0);
                        callback = PyDict_GetItemString(bc->callbacks,
                                                                 "tile-click");
                        if(!callback)
                        {
                                /* TODO: This is a fatal error and should be
                                 *  checked at somepoint in initilization */
                                return FALSE;
                        }
                }
                else
                {
                        /* Get the callback for the building class */
                        /* TODO: Pass a real client object to the callback and
                         * let the callback handle who can do what */
                        if(building->client != n_client_id)
                                return FALSE;
                        bc = building->class;
                        callback = PyDict_GetItemString(bc->callbacks,
                                                        "tile-click");
                        if(!callback)
                        {
                                return FALSE;
                        }
                }
                args = Py_BuildValue("Oii", bc, n_client_id, g_selected_tile);
                res = PyObject_CallObject(callback, args);
                Py_DECREF(args);
                if(!res)
                {
                        PyErr_Print();
                        return FALSE;
                }
                if (PyObject_IsTrue(res))
                {
                        Py_DECREF(res);
                        return TRUE;
                }
                else
                {
                        Py_DECREF(res);
                        return FALSE;
                }
                return TRUE;
        }

        return FALSE;
}

/******************************************************************************\
 The user has typed chat into the box and hit enter.
\******************************************************************************/
void G_input_chat(char *message)
{
        i_color_t color;

        if (!message || !message[0])
                return;
        color = G_nation_to_color(g_clients[n_client_id].nation);
        C_sanitize(message);
        I_print_chat(g_clients[n_client_id].name, color, message);
        N_send(N_SERVER_ID, "1s", G_CM_CHAT, message);
}

/******************************************************************************\
 The user has typed chat into the box and pressed the PM button
\******************************************************************************/
void G_input_privmsg(char *message, int clients)
{
        int i, recipients;
        char *name;
        i_color_t color;

        if (!message || !message[0])
                return;
        color = G_nation_to_color(g_clients[n_client_id].nation);
        C_sanitize(message);

        recipients = 0;
        for (i = 0; i < N_CLIENTS_MAX; i++) {
                if (!(clients & (1 << i)))
                        continue;
                recipients++;
        }

        name = C_va("%s -> %d recipient%s", g_clients[n_client_id].name,
                    recipients, (recipients>1) ? "s" : "");

        I_print_chat(name, I_COLOR, message);

        N_send(N_SERVER_ID, "14s", G_CM_PRIVMSG, clients, message);
}

/******************************************************************************\
 Interface wants us to join a game hosted at [address].
\******************************************************************************/
void G_join_game(const char *address)
{
        G_leave_game();
        N_connect(address, (n_callback_f)G_client_callback);
}

/******************************************************************************\
 Received updated trade parameters from the interface.
\******************************************************************************/
void G_trade_params(g_cargo_type_t index, int buy_price, int sell_price,
                    int minimum, int maximum)
{
        g_cargo_t old_cargo, *cargo;

        if (!g_selected_ship || g_game_over)
                return;
        C_assert(g_selected_ship->client == n_client_id);
        cargo = g_selected_ship->store->cargo + index;
        old_cargo = *cargo;

        /* Update cargo */
        if ((cargo->auto_buy = buy_price >= 0))
                cargo->buy_price = buy_price;
        if ((cargo->auto_sell = sell_price >= 0))
                cargo->sell_price = sell_price;
        cargo->minimum = minimum;
        cargo->maximum = maximum;

        /* Did anything actually change? */
        if (G_cargo_equal(&old_cargo, cargo))
                return;

        /* Tell the server */
        N_send(N_SERVER_ID, "1212222", G_CM_SHIP_PRICES, g_selected_ship->id,
               index, buy_price, sell_price, minimum, maximum);
}

/******************************************************************************\
 Transmit purchase/sale request for the currently selected ship from the
 interface to the server. Use a negative amount to indicate a sale.
\******************************************************************************/
void G_buy_cargo(g_cargo_type_t cargo, int amount)
{
        g_ship_t *ship;
        g_store_t *store;
        bool building;
        bool free;

        if (g_game_over)
                return;
        C_assert(cargo >= 0 && cargo < G_CARGO_TYPES);
        ship = g_selected_ship;
        if (!g_selected_ship || ship->trade_tile < 0 ||
            !G_ship_can_trade_with(g_selected_ship, ship->trade_tile))
                return;
        if(g_tiles[ship->trade_tile].ship) {
                g_ship_t *partner;
                partner = g_tiles[ship->trade_tile].ship;
                store = partner->store;
                free = partner->client == ship->client;
                building = FALSE;
        } else if(g_tiles[ship->trade_tile].building) {
                g_building_t *partner;
                partner = g_tiles[ship->trade_tile].building;
                store = partner->store;
                building = TRUE;
                free = partner->client == ship->client;
        } else {
                return;
        }

        /* Clamp the amount to what we can actually transfer */
        if (!(amount = G_limit_purchase(ship->store, store,
                                        cargo, amount, free)))
                return;
        N_send(N_SERVER_ID, "12212", (building) ? G_CM_BUILDING_BUY: G_CM_SHIP_BUY,
               g_selected_ship->id, ship->trade_tile, cargo, amount);
}

/******************************************************************************\
 Transmit drop cargo request for the currently selected ship from the
 interface to the server.
\******************************************************************************/
void G_drop_cargo(g_cargo_type_t cargo, int amount)
{
        g_ship_t *ship;

        if (g_game_over)
                return;
        C_assert(cargo >= 0 && cargo < G_CARGO_TYPES);
        ship = g_selected_ship;
        if (!g_selected_ship)
                return;
        N_send(N_SERVER_ID, "1212", G_CM_SHIP_DROP, g_selected_ship->id,
               cargo, amount);
}

/******************************************************************************\
 A keypress event was forwarded from the interface.
\******************************************************************************/
void G_process_key(int key, bool shift, bool ctrl, bool alt)
{
        /* Focus on selected tile or ship */
        if (key == SDLK_SPACE) {
                if (g_selected_tile >= 0) {
                        R_rotate_cam_to(r_tiles[g_selected_tile].origin);
                        return;
                }
                G_focus_next_ship();
        }

        /* Deselect */
        else if (key == SDLK_ESCAPE) {
                G_tile_select(-1);
                G_ship_select(NULL);
        }
}

