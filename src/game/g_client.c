/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Deals messages received from the server */

#include "g_common.h"

/* Array of connected clients */
g_client_t g_clients[N_CLIENTS_MAX + 1];

bool g_initilized;

/******************************************************************************\
 The server has sent an echo request
\******************************************************************************/
static void sm_echo_back(void)
{
        int echo_data;

        echo_data = N_receive_int();
        N_send(N_SERVER_ID, "14", G_CM_ECHO_BACK, echo_data);
}
/******************************************************************************\
 Client is receving a popup message from the server.
\******************************************************************************/
static void sm_popup(void)
{
        c_vec3_t *goto_pos;
        int focus_tile;
        char token[32], message[256];

        focus_tile = N_receive_short();
        if (focus_tile >= r_tiles_max) {
                G_corrupt_disconnect();
                return;
        }
        goto_pos = focus_tile >= 0 ? &r_tiles[focus_tile].origin : NULL;
        N_receive_string(token, sizeof (token));
        N_receive_string(message, sizeof (message));
        I_popup(goto_pos, C_str(token, message));
}

/******************************************************************************\
 Some client changed their national affiliation.
\******************************************************************************/
static void sm_affiliate(void)
{
        c_vec3_t *goto_pos;
        const char *fmt;
        int client, nation, tile;

        client = N_receive_char();
        nation = N_receive_char();
        tile = N_receive_short();
        if (nation < 0 || !N_client_valid(client)) {
                G_corrupt_disconnect();
                return;
        }
        g_clients[client].nation = nation;
        if (client == n_client_id && nation != G_NN_PIRATE)
                I_select_nation(nation);

        /* Reselect the ship if its client's nation changed */
        G_ship_reselect(NULL, client);

        /* Update players window */
        I_configure_player(client, g_clients[client].name,
                           G_nation_to_color(g_clients[client].nation),
                           n_client_id == N_HOST_CLIENT_ID);

        /* Affiliation message might have a goto tile attached */
        goto_pos = NULL;
        if (tile >= 0 && tile < r_tiles_max)
                goto_pos = &r_tiles[tile].origin;

        /* Joining the "none" nation means defeat */
        if (nation == G_NN_NONE) {
                if (client == n_client_id) {
                        I_popup(NULL, C_str("g-defeated-you",
                                            "You were defeated!"));
                        return;
                }
                I_popup(NULL, C_va(C_str("g-defeated-you", "%s was defeated!"),
                                   g_clients[client].name));
                return;
        }

        /* Special case for pirates */
        if (nation == G_NN_PIRATE) {
                if (client == n_client_id) {
                        I_popup(goto_pos, C_str("g-join-pirate",
                                                "You became a pirate."));
                        I_select_nation(-1);
                        return;
                }
                fmt = C_str("g-join-pirate-client", "%s became a pirate.");
                I_popup(goto_pos, C_va(fmt, g_clients[client].name));
                return;
        }

        /* Message for nations */
        if (client == n_client_id) {
                int i;

                fmt = C_str("g-join-nation-you", "You joined the %s nation.");
                I_popup(goto_pos, C_va(fmt, g_nations[nation].long_name));

                /* You may only go pirate now */
                for (i = 0; i < G_NATION_NAMES; i++)
                        if (i != G_NN_PIRATE)
                                I_enable_nation(i, FALSE);
        } else {
                fmt = C_str("g-join-nation-client", "%s joined the %s nation.");
                I_popup(goto_pos, C_va(fmt, g_clients[client].name,
                                       g_nations[nation].long_name));
        }
}

/******************************************************************************\
 When a client connects, it gets information about current clients using this
 message type.
\******************************************************************************/
static void sm_client(void)
{
        n_client_id_t client;

        client = N_receive_char();
        if (client < 0 || client >= N_CLIENTS_MAX) {
                G_corrupt_disconnect();
                return;
        }
        n_clients[client].connected = TRUE;
        g_clients[client].nation = N_receive_char();
        N_receive_string_buf(g_clients[client].name);
        C_debug("Client %d is '%s'", client, g_clients[client].name);

        /* Update players window */
        I_configure_player(client, g_clients[client].name,
                           G_nation_to_color(g_clients[client].nation),
                           n_client_id == N_HOST_CLIENT_ID);
}

/******************************************************************************\
 Message received when the game is over.
\******************************************************************************/
static void sm_game_over(void)
{
        g_nation_name_t nation;
        const char *fmt;

        if ((nation = G_receive_nation(-1)) < 0)
                return;

        /* Tie */
        if (nation == G_NN_NONE)
                I_popup(NULL, C_str("g-victory-tie", "Game tied!"));

        /* Pirate won */
        else if (nation == G_NN_PIRATE) {
                n_client_id_t client;

                if ((client = G_receive_client(-1)) < 0)
                        return;
                fmt = C_str("g-victory-pirate", "%s won the game!");
                I_popup(NULL, C_va(fmt, g_clients[client].name));
        }

        /* Nation won */
        else {
                fmt = C_str("g-victory-team", "%s team won the game!");
                I_popup(NULL, C_va(fmt, g_nations[nation].long_name));
        }

        g_game_over = TRUE;
        G_ship_reselect(NULL, -1);
        I_select_nation(-1);
}

/******************************************************************************\
 Client just connected to the server or the server has re-hosted.
\******************************************************************************/
static void sm_init(void)
{
        float variance;
        int protocol, subdiv4, islands, island_size;

        C_assert(n_client_id != N_HOST_CLIENT_ID);
        G_reset_elements();

        /* Start off nation-less */
        I_select_nation(G_NN_NONE);

        /* Check the server's protocol */
        if ((protocol = N_receive_short()) != G_PROTOCOL) {
                C_warning("Server protocol (%d) not equal to client (%d)",
                          protocol, G_PROTOCOL);
                I_popup(NULL, C_str("g-incompatible",
                                    "Server protocol is not compatible"));
                N_disconnect();
                return;
        }

        /* Get the client ID */
        if ((n_client_id = G_receive_range(-1, 0, N_CLIENTS_MAX)) < 0)
                return;
        n_clients[n_client_id].connected = TRUE;

        /* Maximum number of clients */
        if ((g_clients_max = G_receive_range(-1, 1, N_CLIENTS_MAX)) < 0)
                return;
        I_configure_player_num(g_clients_max);
        C_debug("Client ID %d of %d", n_client_id, g_clients_max);

        /* Generate matching globe */
        subdiv4 = N_receive_char();
        g_globe_seed.value.n = N_receive_int();
        islands = N_receive_short();
        island_size = N_receive_short();
        variance = N_receive_float();
        G_generate_globe(subdiv4, islands, island_size, variance);

        /* Get solar angle */
        r_solar_angle = N_receive_float();

        /* Get time limit */
        g_time_limit_msec = c_time_msec + N_receive_int();

        I_leave_limbo();
}

/******************************************************************************\
 A client has changed their name.
\******************************************************************************/
static void sm_name(void)
{
        int client;
        char old_name[G_NAME_MAX];

        if ((client = G_receive_cargo(-1)) < 0)
                return;
        C_strncpy_buf(old_name, g_clients[client].name);
        N_receive_string_buf(g_clients[client].name);

        /* Update players window */
        I_configure_player(client, g_clients[client].name,
                           G_nation_to_color(g_clients[client].nation),
                           n_client_id == N_HOST_CLIENT_ID);

        /* The first name-change on connect */
        if (!old_name[0])
                I_print_chat(C_va("%s joined the game.",
                                  g_clients[client].name), I_COLOR, NULL);
        else
                I_print_chat(C_va("%s renamed to %s.", old_name,
                                  g_clients[client].name), I_COLOR, NULL);
}

/******************************************************************************\
 Another client sent out a chat message.
\******************************************************************************/
static void sm_chat(void)
{
        i_color_t color;
        int client;
        char buffer[N_SYNC_MAX], *name;

        client = N_receive_char();
        N_receive_string_buf(buffer);
        if (!buffer[0])
                return;

        /* Normal chat message */
        if (client >= 0 && client < N_CLIENTS_MAX) {
                color = G_nation_to_color(g_clients[client].nation);
                name = g_clients[client].name;
                I_print_chat(name, color, buffer);
                return;
        }

        /* No-text message */
        I_print_chat(buffer, I_COLOR, NULL);
}

/******************************************************************************\
 Another client sent out a private chat message.
\******************************************************************************/
static void sm_privmsg(void)
{
        i_color_t color;
        int client, clients;
        char buffer[N_SYNC_MAX], *name;

        client = N_receive_char();
        clients = N_receive_int();
        N_receive_string_buf(buffer);
        if (!buffer[0])
                return;

        /* Private chat message */
        if (client >= 0 && client < N_CLIENTS_MAX) {
                color = G_nation_to_color(g_clients[client].nation);
                name = C_va("%s -> %s", g_clients[client].name,
                                        g_clients[n_client_id].name);
                I_print_chat(name, I_COLOR, buffer);
                return;
        }

        /* No-text message */
        I_print_chat(buffer, I_COLOR, NULL);
}

/******************************************************************************\
 A ship spawned on the server.
\******************************************************************************/
static void sm_ship_spawn(void)
{
        g_ship_type_t type;
        n_client_id_t client;
        int tile, id;

        if (n_client_id == N_HOST_CLIENT_ID)
                return;
        id = N_receive_short();
        client = N_receive_char();
        tile = N_receive_short();
        type = N_receive_char();
        if (!G_ship_spawn(id, client, tile, type))
                G_corrupt_disconnect();
}

/******************************************************************************\
 Receive a ship path.
\******************************************************************************/
static void sm_ship_state(void)
{
        g_ship_id id, boarding_ship_id;
        int boarding;
        g_ship_t *ship, *boarding_ship;

        if (!(ship = G_receive_ship(-1)))
                return;

        id = ship->id;
        ship->health = N_receive_char();
        ship->store->cargo[G_CT_CREW].amount = N_receive_short();

        /* Remote client boarding announcements */
        boarding = N_receive_char();
        boarding_ship_id = N_receive_short();
        if(boarding_ship_id >= 0)
                boarding_ship = G_get_ship(boarding_ship_id);
        else
                boarding_ship = NULL;
        if (boarding_ship_id >= 0 && ship->boarding_ship == NULL &&
            G_ship_controlled_by(ship, n_client_id))
                I_popup(&ship->model->origin,
                        C_va(C_str("g-boarding", "%s boarding the %s."),
                             ship->name, boarding_ship->name));
        else if (boarding && !ship->boarding &&
                 G_ship_controlled_by(ship, n_client_id))
                I_popup(&ship->model->origin,
                        C_va(C_str("g-boarded", "%s is being boarded!"),
                             ship->name));
        ship->boarding = boarding;
        Py_CLEAR(ship->boarding_ship);
        Py_XINCREF(boarding_ship);
        ship->boarding_ship = boarding_ship;
}

/******************************************************************************\
 The buy/sell prices of a cargo item have changed on some ship.
\******************************************************************************/
static void sm_ship_prices(void)
{
        g_cargo_t *cargo;
        g_ship_t  *ship;
        int cargo_i, buy_price, sell_price;

        ship = G_receive_ship(-1);
        cargo_i = G_receive_cargo(-1);
        if (!ship || cargo_i < 0)
                return;

        /* Save prices */
        buy_price = N_receive_short();
        sell_price = N_receive_short();
        cargo = ship->store->cargo + cargo_i;
        if ((cargo->auto_buy = buy_price >= 0))
                cargo->buy_price = buy_price;
        if ((cargo->auto_sell = sell_price >= 0))
                cargo->sell_price = sell_price;

        /* Save quantities */
        cargo->minimum = N_receive_short();
        cargo->maximum = N_receive_short();

        /* Update trade window */
        G_ship_reselect(ship, -1);
        if (g_selected_ship &&
            g_tiles[g_selected_ship->trade_tile].ship == ship)
                G_ship_reselect(NULL, -1);
}

/******************************************************************************\
 Receive ping and gold info
\******************************************************************************/
static void sm_client_update(void)
{
        int i, client_array;
        client_array = N_receive_int();
        for (i = 0; i < N_CLIENTS_MAX; i++) {
                if (!(client_array & (1 << i)))
                        continue;
                g_clients[i].gold = N_receive_int();
                g_clients[i].ping_time = N_receive_short();
                I_update_player(i, g_clients[i].gold, g_clients[i].ping_time);
        }
}

/******************************************************************************\
 Send the server an update if our name changes.
\******************************************************************************/
static int name_update(c_var_t *var, c_var_value_t value)
{
        if (!value.s[0])
                return FALSE;
        C_sanitize(value.s);
        N_send(N_SERVER_ID, "1s", G_CM_NAME, value.s);
        return TRUE;
}

/******************************************************************************\
 Client network event callback function.
\******************************************************************************/
void G_client_callback(int client, n_event_t event)
{
        g_server_msg_t token;
        g_ship_t *ship;
        g_building_t *building;
        int i, j, k;

        C_assert(client == N_SERVER_ID);

        /* Connected */
        if (event == N_EV_CONNECTED) {
                if (n_client_id == N_HOST_CLIENT_ID)
                        return;
                I_popup(NULL, C_str("g-connect-ok", "Connected to server."));

                /* Send out our name */
                name_update(&g_name, g_name.value);
                return;
        }

        /* Connection failed */
        if (event == N_EV_CONNECT_FAILED) {
                I_popup(NULL, C_str("g-connect-fail",
                                    "Failed to connect to server."));
                return;
        }

        /* Disconnected */
        if (event == N_EV_DISCONNECTED) {
                I_enter_limbo();
                if (n_client_id != N_HOST_CLIENT_ID)
                        I_popup(NULL, "Disconnected from server.");
                return;
        }

        /* Process messages */
        if (event != N_EV_MESSAGE)
                return;
        token = N_receive_char();
        switch (token) {
        case G_SM_ECHO_REQUEST:
                sm_echo_back();
                break;
        case G_SM_POPUP:
                sm_popup();
                break;
        case G_SM_AFFILIATE:
                sm_affiliate();
                break;
        case G_SM_INIT:
                sm_init();
                break;
        case G_SM_NAME:
                sm_name();
                break;
        case G_SM_CHAT:
                sm_chat();
                break;
        case G_SM_PRIVMSG:
                sm_privmsg();
                break;
        case G_SM_CLIENT:
                sm_client();
                break;
        case G_SM_GAME_OVER:
                sm_game_over();
                break;
        case G_SM_CLIENT_UPDATE:
                sm_client_update();
                break;
        case G_SM_SHIP_SPAWN:
                sm_ship_spawn();
                break;
        case G_SM_SHIP_STATE:
                sm_ship_state();
                break;
        case G_SM_SHIP_PRICES:
                sm_ship_prices();
                break;

        /* A ship's cargo manifest changed */
        case G_SM_SHIP_CARGO:
                if (!(ship = G_receive_ship(-1)))
                        return;
                if (n_client_id != N_HOST_CLIENT_ID) {
                        bool own;

                        own = G_ship_controlled_by(ship, n_client_id);
                        G_store_receive(ship->store, own);
                }
                G_ship_reselect(ship, -1);
                G_ship_update_trade_ui(ship);
                break;

        /* A gib spawned or vanished */
        case G_SM_GIB:
                if (n_client_id == N_HOST_CLIENT_ID ||
                    (i = G_receive_tile(-1)) < 0)
                        return;
                G_tile_gib(i, N_receive_char());
                break;

        /* Ship path changed */
        case G_SM_SHIP_PATH:
                if (n_client_id == N_HOST_CLIENT_ID ||
                    !(ship = G_receive_ship(-1)))
                        return;
                G_ship_move_to(ship, N_receive_short());
                ship->progress = N_receive_float();
                N_receive_string_buf(ship->path);
                if (g_selected_ship == ship && ship->client == n_client_id)
                        R_select_path(ship->tile, ship->path);
                break;

        /* Ship changed names */
        case G_SM_SHIP_NAME:
                if (!(ship = G_receive_ship(-1)))
                        return;
                N_receive_string_buf(ship->name);
                G_count_name(G_NT_SHIP, ship->name);
                G_ship_reselect(ship, -1);
                break;

        /* Ship changed owners */
        case G_SM_SHIP_OWNER:
                if (!(ship = G_receive_ship(-1)) ||
                    (j = G_receive_client(-1)) < 0 ||
                    ship->client == j)
                        return;
                if (ship->client == n_client_id)
                        I_popup(&ship->model->origin,
                                C_va(C_str("g-ship-lost", "Lost the %s!"),
                                     ship->name));
                else if (j == n_client_id)
                        I_popup(&ship->model->origin,
                                C_va(C_str("g-ship-captured",
                                           "Captured the %s."),
                                     ship->name));
                ship->client = j;
                G_ship_reselect(ship, -1);
                break;

        /* Building changed */
        case G_SM_BUILDING:
                if (n_client_id == N_HOST_CLIENT_ID ||
                    (i = G_receive_tile(-1)) < 0 ||
                    (j = G_receive_range(-1, 0, G_BUILDING_TYPES)) < 0)
                        return;
                k = N_receive_char();
                if (k != -2 && !N_client_valid(k)) {
                        G_corrupt_drop(-1);
                        return;
                }
                G_tile_build(i, j, k);
                break;

        /* A building's cargo manifest changed */
        case G_SM_BUILDING_CARGO:
                if (!(building = G_receive_building(-1)))
                        return;
                if (n_client_id != N_HOST_CLIENT_ID) {
                        bool own;

                        own = G_building_controlled_by(building, n_client_id);
                        G_store_receive(building->store, own);
                }
                if (g_selected_tile == building->tile) {
                        G_tile_select(building->tile);
                }

                break;

        /* Somebody connected but we don't have their name yet */
        case G_SM_CONNECTED:
                if ((i = G_receive_range(-1, 0, N_CLIENTS_MAX)) < 0)
                        return;
                n_clients[i].connected = TRUE;
                C_zero(g_clients + i);
                C_debug("Client %d connected", i);
                break;

        /* Somebody disconnected */
        case G_SM_DISCONNECTED:
                i = N_receive_char();
                if (i < 0 || i >= N_CLIENTS_MAX) {
                        G_corrupt_disconnect();
                        return;
                }
                k = N_receive_char();
                n_clients[i].connected = FALSE;
                I_print_chat(C_va(k ? C_str("g-kicked", "%s was kicked.") :
                                      C_str("g-left", "%s left the game."),
                                  g_clients[i].name), I_COLOR, NULL);
                I_configure_player(i, NULL, I_COLOR, FALSE);
                C_debug("Client %d disconnected", i);
                break;

        default:
                break;
        }
}

/******************************************************************************\
 Called to update client-side structures.
\******************************************************************************/
void G_update_client(void)
{
        N_poll_client();
        if (i_limbo)
                return;
        G_update_ships();
        G_update_buildings();
}

/******************************************************************************\
 Initialize game structures.
\******************************************************************************/
void G_init(void)
{
        C_status("Initializing client");
        g_ship_dict = PyDict_New();
        g_building_dict = PyDict_New();

        G_init_elements();
        G_init_globe();

        /* Update the server when our name changes */
        C_var_unlatch(&g_name);
        g_name.update = (c_var_update_f)name_update;
        g_name.edit = C_VE_FUNCTION;

        /* Parse names config */
        G_load_names();
        /* Set initilized var */
        g_initilized = TRUE;
}

/******************************************************************************\
 Cleanup game structures.
\******************************************************************************/
void G_cleanup(void)
{
        G_cleanup_ships();
        G_cleanup_tiles();
        Py_CLEAR(g_ship_dict);
        Py_CLEAR(g_building_dict);
        /* Set initilized var */
        g_initilized = FALSE;
}

/******************************************************************************\
 Connect to the master server and refresh game servers.
\******************************************************************************/
void G_refresh_servers(void)
{
        PyObject *callback, *args, *res;
        C_var_unlatch(&g_master);
        if (!g_master.value.s[0])
                return;
        C_var_unlatch(&g_master_url);
        callback = PyDict_GetItemString(g_callbacks, "refresh-servers");
        if(!callback)
        {
                /* TODO: This is probably a fatal error and should be
                 *  checked at somepoint in initilization */
                return;
        }
        args = Py_BuildValue("ss", g_master.value.s, g_master_url.value.s);
        res = PyObject_CallObject(callback, args);
        if(!res) {
                PyErr_Print();
        }
        Py_XDECREF(args);
        Py_XDECREF(res);
}

