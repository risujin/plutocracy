/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Primarily handles client messages for the server. Remember that the client
   is not trusted and all input must be checked for validity! */

#include "g_common.h"

/* Maximum number of crates to spawn */
#define CRATES_MAX 32

/* Milliseconds between master server heartbeats */
#define PUBLISH_INTERVAL 300000

/* This game's client limit */
int g_clients_max;

/* TRUE if the server has finished initializing */
bool g_host_inited;

/* Time at which game ends */
int g_time_limit_msec;

/******************************************************************************\
 Client has sent back our echo
\******************************************************************************/
static void cm_echo_back(int client)
{
        int echo_data, round_trip_time;
        echo_data = N_receive_int();
        if (g_clients[client].echo_data != echo_data) {
                C_debug("Client '%s' (%d) sent non-matching echo data, "
                        "wanted: %d got: %d", g_clients[client].name, client,
                        g_clients[client].echo_data, echo_data);
                /* Drop them? */
        }
        round_trip_time = c_time_msec - g_clients[client].echo_time;
        g_clients[client].ping_time = round_trip_time;
        C_debug("Reply from Client '%s' (%d) ping time: %d msec",
                g_clients[client].name, client, round_trip_time);

}

/******************************************************************************\
 Handles clients changing nations.
\******************************************************************************/
static void cm_affiliate(int client)
{
        g_ship_t *ship;
        int nation, old, tile;

        if ((nation = G_receive_nation(client)) < 0 ||
            nation == g_clients[client].nation)
                return;
        old = g_clients[client].nation;

        /* Nation changing rules */
        if (old == G_NN_PIRATE ||
            (old != G_NN_NONE && nation != G_NN_PIRATE))
                return;

        g_clients[client].nation = nation;

        /* If this client just joined a nation for the first time,
           try to give them a starter ship */
        tile = -1;
        if (old == G_NN_NONE &&
            (ship = G_ship_spawn(-1, client, -1, G_ST_SPIDER))) {
                tile = ship->tile;
                G_store_add(ship->store, G_CT_GOLD, 500);
                G_store_add(ship->store, G_CT_CREW, 25);
                G_store_add(ship->store, G_CT_RATIONS, 25);
        }

        N_broadcast("1112", G_SM_AFFILIATE, client, nation, tile);
}

/******************************************************************************\
 Handles orders to move ships.
\******************************************************************************/
static void cm_ship_move(int client)
{
        int tile;
        g_ship_t *ship;

        if (!(ship = G_receive_ship(client)) ||
            (tile = G_receive_tile(client)) < 0 ||
            !G_ship_controlled_by(ship, client))
                return;
        Py_CLEAR(ship->target_ship);
        G_ship_path(ship, tile);
}

/******************************************************************************\
 Called when a client wants to change their name.
\******************************************************************************/
static void cm_name(int client)
{
        int i, suffixes, name_len;
        char name_buf[G_NAME_MAX];

        N_receive_string_buf(name_buf);

        /* Must have a valid name */
        C_sanitize(name_buf);
        if (!name_buf[0])
                C_strncpy_buf(name_buf, "Newbie");

        /* Didn't actually change names */
        if (!strcmp(name_buf, g_clients[client].name))
                return;

        /* See if this name is taken */
        name_len = C_strlen(name_buf);
        for (suffixes = 2, i = 0; i < N_CLIENTS_MAX; i++) {
                if (i == client || strcasecmp(name_buf, g_clients[i].name))
                        continue;

                /* Its taken, add a suffix and try again */
                name_buf[name_len] = NUL;
                name_len = C_suffix_buf(name_buf, C_va(" %d", suffixes));

                /* Start over */
                i = -1;
                suffixes++;
        }

        /* Didn't actually change names (after suffix attachment) */
        if (!strcmp(name_buf, g_clients[client].name))
                return;

        C_debug("Client '%s' (%d) renamed to '%s'",
                g_clients[client].name, client, name_buf);
        N_broadcast("11s", G_SM_NAME, client, name_buf);
}

/******************************************************************************\
 Client wants to rename a ship.
\******************************************************************************/
static void cm_ship_name(int client)
{
        g_ship_t *ship;
        char new_name[G_NAME_MAX];

        if (!(ship = G_receive_ship(client)) ||
            !G_ship_controlled_by(ship, client))
                return;
        N_receive_string_buf(new_name);
        C_sanitize(new_name);
        if (!new_name[0])
                return;
        C_strncpy_buf(ship->name, new_name);
        N_broadcast_except(client, "12s", G_SM_SHIP_NAME, ship->id,
                           ship->name);
        C_debug("'%s' named ship %d '%s'", g_clients[client].name,
                ship->id, new_name);
}

/******************************************************************************\
 Client changed cargo prices.
\******************************************************************************/
static void cm_ship_prices(int client)
{
        g_ship_t *ship;
        int cargo, buy_price, sell_price, minimum, maximum;

        if (!(ship = G_receive_ship(client)) ||
            !G_ship_controlled_by(ship, client))
                return;
        cargo = N_receive_char();
        if (cargo < 0 || cargo >= G_CARGO_TYPES) {
                G_corrupt_drop(client);
                return;
        }

        /* Prices */
        buy_price = N_receive_short();
        sell_price = N_receive_short();
        if (buy_price > 999)
                buy_price = 999;
        if (sell_price > 999)
                sell_price = 999;

        /* Quantities */
        minimum = N_receive_short();
        maximum = N_receive_short();

        /* Select clients that can see this store */
        G_store_select_clients(ship->store);

        /* The host needs to see this message to process the update */
        n_clients[N_HOST_CLIENT_ID].selected = TRUE;

        /* Originating client already knows what the prices are */
        n_clients[client].selected = FALSE;

        N_send_selected("1212222", G_SM_SHIP_PRICES, ship->id, cargo,
                        buy_price, sell_price, minimum, maximum);
}

/******************************************************************************\
 Client wants to buy something.
\******************************************************************************/
static void cm_ship_buy(int client)
{
        g_ship_t *ship, *partner;
        g_store_t *buyer, *seller;
        int trade_tile, cargo, amount, gold;
        bool free;

        if (!(ship = G_receive_ship(client)) ||
            (trade_tile = G_receive_tile(client)) < 0 ||
            (cargo = G_receive_cargo(client)) < 0 ||
            !G_ship_controlled_by(ship, client) ||
            !G_ship_can_trade_with(ship, trade_tile))
                return;
        amount = N_receive_short();
        partner = g_tiles[trade_tile].ship;
        free = ship->client == partner->client;
        buyer = ship->store;
        seller = partner->store;
        amount = G_limit_purchase(buyer, seller, cargo, amount, free);
        if (amount == 0)
                return;

        /* Do the transfer. Cargo must be subtracted before it is added or
           it could overflow and get clamped! */
        if (amount > 0) {
                gold = (free) ? 0 : seller->cargo[cargo].sell_price * amount;
                G_store_add(buyer, G_CT_GOLD, -gold);
                G_store_add(seller, cargo, -amount);
                G_store_add(seller, G_CT_GOLD, gold);
                G_store_add(buyer, cargo, amount);
        } else {
                amount = -amount;
                gold = (free) ? 0 : seller->cargo[cargo].buy_price * amount;
                G_store_add(seller, G_CT_GOLD, -gold);
                G_store_add(buyer, cargo, -amount);
                G_store_add(buyer, G_CT_GOLD, gold);
                G_store_add(seller, cargo, amount);
        }
}

/******************************************************************************\
 Client wants to buy something.
\******************************************************************************/
static void cm_building_buy(int client)
{
        g_ship_t *ship;
        g_building_t *partner;
        g_store_t *buyer, *seller;
        int trade_tile, cargo, amount;

        if (!(ship = G_receive_ship(client)) ||
            (trade_tile = G_receive_tile(client)) < 0 ||
            (cargo = G_receive_cargo(client)) < 0 ||
            !G_ship_controlled_by(ship, client) ||
            !G_ship_can_trade_with(ship, trade_tile))
                return;
        amount = N_receive_short();
        partner = g_tiles[trade_tile].building;
        buyer = ship->store;
        seller = partner->store;
        amount = G_limit_purchase(buyer, seller, cargo, amount, TRUE);
        if (amount == 0)
                return;

        /* Do the transfer. Cargo must be subtracted before it is added or
           it could overflow and get clamped! */
        if (amount > 0) {
                G_store_add(seller, cargo, -amount);
                G_store_add(buyer, cargo, amount);
        } else {
                amount = -amount;
                G_store_add(buyer, cargo, -amount);
                G_store_add(seller, cargo, amount);
        }
}

/******************************************************************************\
 Client wants to drop some cargo.
\******************************************************************************/
static void cm_ship_drop(int client)
{
        g_ship_t *ship;
        int cargo, amount;

        if (!(ship = G_receive_ship(client)) ||
            (cargo = G_receive_cargo(client)) < 0 ||
            !G_ship_controlled_by(ship, client) ||
            (amount = N_receive_short()) < 0)
                return;
        G_ship_drop_cargo(ship, cargo, amount);
}

/******************************************************************************\
 Client typed something into chat.
\******************************************************************************/
static void cm_chat(int client)
{
        char chat_buffer[N_SYNC_MAX];

        N_receive_string_buf(chat_buffer);

        C_sanitize(chat_buffer);
        if (!chat_buffer[0])
                return;
        N_broadcast_except(client, "11s", G_SM_CHAT, client, chat_buffer);
}

/******************************************************************************\
 Client typed something into private chat.
\******************************************************************************/
static void cm_privmsg(int client)
{
        char chat_buffer[N_SYNC_MAX];
        int i ,clients;

        clients = N_receive_int();

        N_receive_string_buf(chat_buffer);
        C_sanitize(chat_buffer);
        if (!chat_buffer[0])
                return;
        for (i = 0; i < N_CLIENTS_MAX; i++)
                n_clients[i].selected = (clients & (1 << i)) ? TRUE : FALSE;
        n_clients[client].selected = FALSE;
        N_send_selected("114s", G_SM_PRIVMSG, client, clients, chat_buffer);
}

/******************************************************************************\
 Client wants to do something to a tile via a ring command.
\******************************************************************************/
static void cm_tile_ring(int client)
{
//        g_building_class_t *bc;
        BuildingClass *bc;
        i_ring_icon_t icon;
        int tile;

        tile = G_receive_tile(client);
        icon = (i_ring_icon_t)G_receive_range(client, 0, I_RING_ICONS);
        if (tile < 0 || icon < 0)
                return;

        /* Client wants to build a shipyard (tech preview) */
        bc = G_building_class_from_ring_id(icon);
        if (bc && bc->buildable) {
                g_cost_t cost;
                int building_id;

                building_id = G_building_class_index_from_ring_id(icon);
                G_list_to_cost(bc->cost, &cost);

                /* Can you do it? */
                if (!G_pay(client, tile, &cost, FALSE))
                        return;
                /* Building limits */
                if(n_client_id == N_HOST_CLIENT_ID) {
                        int buildings, limit;
                        g_building_t *building;
                        PyObject *key;
                        Py_ssize_t pos = 0;
                        buildings = 0;
                        limit = g_player_building_limit.value.n;
                        /* Count ships */
                        while (PyDict_Next(g_building_dict, &pos, &key,
                                           (PyObject**)&building)) {
                                if(building->health <= 0 ||
                                   client != building->client)
                                        continue;
                                buildings++;
                        }
                        if (buildings >= limit) {
                                N_send(client, "12ss", G_SM_POPUP, tile,
                                       "g-building-limit",
                                       "You have reached the maximum number of "
                                       "buildings");
                                return;
                        }
                }

                /* Pay for and build the town hall */
                G_pay(client, tile, &cost, TRUE);
                G_tile_build(tile, building_id, client);
                return;
        }

        /* Call building's tile ring command callback */
        if (g_tiles[tile].building) {
                PyObject *callback;
                /* TODO: Pass a real client object to the callback and let the
                 * callback handle who can do what */
                if(g_tiles[tile].building->client != client)
                        return;
                if( (callback = PyDict_GetItemString(
                                      g_tiles[tile].building->class->callbacks,
                                      "tile-ring-command")) )
                {
                        PyObject *res, *args;
                        C_debug("calling ring click callback");
                        args = Py_BuildValue("Oiii",
                                             g_tiles[tile].building->class,
                                             client, tile, icon);
                        res = PyObject_CallObject(callback, args);
                        Py_DECREF(args);
                        if(!res)
                        {
                                PyErr_Print();
                                return;
                        }
                        Py_DECREF(res);
                        return;
                }
        }
}

/******************************************************************************\
 Client wants to do something with a ship using the ring.
\******************************************************************************/
static void cm_ship_ring(int client)
{
        i_ring_icon_t icon;
        g_ship_t *ship, *target_ship;

        if (!(ship = G_receive_ship(client)))
                return;
        icon = (i_ring_icon_t)G_receive_range(client, 0, I_RING_ICONS);
        if (!(target_ship = G_receive_ship(client)))
                return;

        /* Follow the target */
        if (icon == I_RI_FOLLOW)
                ship->target_board = FALSE;

        /* Board the target */
        else if (icon == i_ri_board) {

                /* Don't attack friendlies! */
                if (!G_ship_hostile(ship, target_ship->client))
                        return;

               ship->target_board = TRUE;
        }

        Py_CLEAR(ship->target_ship);
        Py_INCREF(target_ship);
        ship->target_ship = target_ship;
        G_ship_path(ship, target_ship->tile);
}

/******************************************************************************\
 Initialize a new client.
\******************************************************************************/
static void init_client(int client)
{
        g_ship_t *ship;
        PyObject *key;
        Py_ssize_t pos = 0;
        int i;

        /* The server already has all of the information */
        if (client == N_HOST_CLIENT_ID)
                return;

        /* This client has already been counted toward the total, kick them
           if this is more players than we want */
        if (n_clients_num > g_clients_max) {
                N_send(client, "12ss", G_SM_POPUP,
                       "g-host-full", "Server is full.");
                N_drop_client(client);
                return;
        }

        C_debug("Initializing client %d", client);
        C_zero(g_clients + client);

        /* Communicate the globe info */
        N_send(client, "12111422ff4", G_SM_INIT, G_PROTOCOL, client,
               g_clients_max, g_globe_subdiv4.value.n, g_globe_seed.value.n,
               g_island_num.value.n, g_island_size.value.n,
               g_island_variance.value.f, r_solar_angle,
               g_time_limit_msec - c_time_msec);

        /* Tell them about everyone already here */
        for (i = 0; i < N_CLIENTS_MAX; i++)
                if (n_clients[i].connected && g_clients[i].name[0])
                        N_send(client, "111s", G_SM_CLIENT, i,
                               g_clients[i].nation, g_clients[i].name);

        /* Tell them about the buildings and gibs on them globe */
        for (i = 0; i < r_tiles_max; i++) {
                if (g_tiles[i].building)
                        G_tile_send_building(i, client);
                if (g_tiles[i].gib)
                        G_tile_send_gib(i, client);
        }

        /* Tell them about all the ships on the globe */
        while (PyDict_Next(g_ship_dict, &pos, &key, (PyObject**)&ship)) {
                if (!ship->in_use)
                        continue;
                G_ship_send_spawn(ship, client);
                G_ship_send_name(ship, client);
                if (!ship->modified)
                        G_ship_send_state(ship, client);
                G_ship_send_path(ship, client);
        }
}

/******************************************************************************\
 Publish callback function.
\******************************************************************************/
static void publish_callback(n_event_t event, const char *text, int len)
{
        if (event == N_EV_SEND_COMPLETE) {
                C_debug("Sent heartbeat to master server");
                N_disconnect_http();
        }
}

/******************************************************************************\
 Inform the master server that our game is over.
\******************************************************************************/
static void publish_game_dead(void)
{
        /* Disable if blank master server name */
        C_var_unlatch(&g_master);
        if (!*g_master.value.s)
                return;
        C_var_unlatch(&g_master_url);

        /* Wait on this connection so we make sure we get the dead hearbeat
           out if the game quit */
        if (!N_connect_http_wait(g_master.value.s,
                                 (n_callback_http_f)publish_callback)) {
                C_debug("Failed to connect to master server");
                return;
        }

        /* Since we are no longer alive, send invalid values */
        N_send_post(g_master_url.value.s,
                    "port", C_va("%d", n_port.value.n));
        N_poll_http();
}

/******************************************************************************\
 Inform the master server about our game. If [force] is not TRUE, this will
 only be done periodically.
\******************************************************************************/
static void publish_game_alive(bool force)
{
        static int publish_time;

        if ((c_time_msec < publish_time && !force) || g_game_over)
                return;
        publish_time = c_time_msec + PUBLISH_INTERVAL;

        /* Disable if blank master server name */
        C_var_unlatch(&g_master);
        if (!*g_master.value.s)
                return;
        C_var_unlatch(&g_master_url);

        /* Send game info key/value pairs, the server can figure out our
           ip adress on its own */
        N_connect_http(g_master.value.s, (n_callback_http_f)publish_callback);
        N_send_post(g_master_url.value.s,
                    "protocol", C_va("%d", G_PROTOCOL),
                    "name", g_name.value.s,
                    "info", C_va("%d/%d, %d min", n_clients_num, g_clients_max,
                                 (g_time_limit_msec - c_time_msec) / 60000),
                    "port", C_va("%d", n_port.value.n));
}

/******************************************************************************\
 A client has left the game and we need to clean up.
\******************************************************************************/
static void client_disconnected(int client)
{
        g_ship_t *ship;
        PyObject *key;
        Py_ssize_t pos = 0;

        C_debug("Client %d disconnected", client);

        /* If the host has quit, tell the master server we are done */
        if (n_client_id == N_HOST_CLIENT_ID && client == N_HOST_CLIENT_ID) {
                publish_game_dead();
                return;
        }

        /* Let everyone know about it */
        if (g_clients[client].name[0])
                N_broadcast("111", G_SM_DISCONNECTED, client,
                            g_clients[client].kicked);

        /* Disown their ships */
        while (PyDict_Next(g_ship_dict, &pos, &key, (PyObject**)&ship))
                if (ship->client == client)
                        G_ship_change_client(ship, N_SERVER_ID);
}

/******************************************************************************\
 Converts a client message to a string.
\******************************************************************************/
static const char *cm_to_string(g_client_msg_t msg)
{
        switch (msg) {
        case G_CM_NONE:
                return "G_CM_NONE";
        case G_CM_AFFILIATE:
                return "G_CM_AFFILIATE";
        case G_CM_NAME:
                return "G_CM_NAME";
        case G_CM_CHAT:
                return "G_CM_CHAT";
        case G_CM_SHIP_BUY:
                return "G_CM_SHIP_BUY";
        case G_CM_BUILDING_BUY:
                return "G_CM_BUILDING_BUY";
        case G_CM_SHIP_DROP:
                return "G_CM_SHIP_DROP";
        case G_CM_SHIP_MOVE:
                return "G_CM_SHIP_MOVE";
        case G_CM_SHIP_NAME:
                return "G_CM_SHIP_NAME";
        case G_CM_SHIP_PRICES:
                return "G_CM_SHIP_PRICES";
        case G_CM_SHIP_RING:
                return "G_CM_SHIP_RING";
        case G_CM_TILE_RING:
                return "G_CM_TILE_RING";
        default:
                return C_va("%d", msg);
        }
}

/******************************************************************************\
 Called from within the network namespace when a network event arrives for the
 server from one of the clients (including the server's client).
\******************************************************************************/
static void server_callback(int client, n_event_t event)
{
        g_client_msg_t token;

        /* Special client events */
        if (event == N_EV_CONNECTED) {
                N_broadcast_except(N_HOST_CLIENT_ID, "11",
                                   G_SM_CONNECTED, client);
                init_client(client);
                return;
        } else if (event == N_EV_DISCONNECTED) {
                client_disconnected(client);
                return;
        }

        /* Handle messages */
        if (event != N_EV_MESSAGE)
                return;
        token = (g_client_msg_t)N_receive_char();

        /* Debug messages */
        if (g_debug_net.value.n)
                C_trace("%s from client %d", cm_to_string(token), client);

        /* Only certain messages allowed when the game is over */
        if (g_game_over)
                switch (token) {
                case G_CM_CHAT:
                case G_CM_NAME:
                        break;
                default:
                        return;
                }

        switch (token) {
        case G_CM_ECHO_BACK:
                cm_echo_back(client);
                break;
        case G_CM_AFFILIATE:
                cm_affiliate(client);
                break;
        case G_CM_CHAT:
                cm_chat(client);
                break;
        case G_CM_PRIVMSG:
                cm_privmsg(client);
                break;
        case G_CM_NAME:
                cm_name(client);
                break;
        case G_CM_SHIP_BUY:
                cm_ship_buy(client);
                break;
        case G_CM_BUILDING_BUY:
                cm_building_buy(client);
                break;
        case G_CM_SHIP_DROP:
                cm_ship_drop(client);
                break;
        case G_CM_SHIP_NAME:
                cm_ship_name(client);
                break;
        case G_CM_SHIP_MOVE:
                cm_ship_move(client);
                break;
        case G_CM_SHIP_PRICES:
                cm_ship_prices(client);
                break;
        case G_CM_SHIP_RING:
                cm_ship_ring(client);
                break;
        case G_CM_TILE_RING:
                cm_tile_ring(client);
                break;
        default:
                break;
        }
}

/******************************************************************************\
 Place starter buildings.
\******************************************************************************/
static void initial_buildings(void)
{
        int i;

        for (i = 0; i < r_tiles_max; i++) {
                if (R_terrain_base(r_tiles[i].terrain) != R_T_GROUND)
                        continue;
                if (C_rand_real() < g_forest.value.f)
                        G_tile_build(i, G_BT_TREE, -2);
        }
}

/******************************************************************************\
 Kick a client via the interface.
\******************************************************************************/
void G_kick_client(n_client_id_t client)
{
        /* Mark the client as kicked, this will be communicated via normal
           client disconnection channels */
        g_clients[client].kicked = TRUE;

        N_send(client, "12ss", G_SM_POPUP, -1,
               "g-host-kicked", "Kicked by host.");
        N_drop_client(client);
}

/******************************************************************************\
 Host a new game.
\******************************************************************************/
void G_host_game(void)
{
        int i;

        if (n_client_id != N_HOST_CLIENT_ID)
                G_leave_game();
        C_var_unlatch(&g_victory_gold);
        G_reset_elements();

        /* Reset time limit */
        C_var_unlatch(&g_time_limit);
        g_time_limit_msec = c_time_msec + g_time_limit.value.n * 60000;

        /* Start off nation-less */
        I_select_nation(G_NN_NONE);

        /* Maximum number of clients */
        C_var_unlatch(&g_players);
        if (g_players.value.n < 1)
                g_players.value.n = 1;
        if (g_players.value.n > N_CLIENTS_MAX)
                g_players.value.n = N_CLIENTS_MAX;
        I_configure_player_num(g_clients_max = g_players.value.n);

        /* Start the network server */
        if (!N_start_server((n_callback_f)server_callback,
                            (n_callback_f)G_client_callback)) {
                I_popup(NULL, "Failed to start server.");
                I_enter_limbo();
                return;
        }

        /* Generate a new globe */
        C_var_unlatch(&g_globe_subdiv4);
        C_var_unlatch(&g_island_num);
        C_var_unlatch(&g_island_size);
        C_var_unlatch(&g_island_variance);
        C_var_unlatch(&g_name);
        if (g_globe_subdiv4.value.n < 3)
                g_globe_subdiv4.value.n = 3;
        if (g_globe_subdiv4.value.n > 5)
                g_globe_subdiv4.value.n = 5;
        if (g_island_variance.value.f > 1.f)
                g_island_variance.value.f = 1.f;
        if (!C_var_unlatch(&g_globe_seed))
                g_globe_seed.value.n = (int)time(NULL);
        G_generate_globe(g_globe_subdiv4.value.n, g_island_num.value.n,
                         g_island_size.value.n, g_island_variance.value.f);
        initial_buildings();

        /* Set our name */
        C_var_unlatch(&g_name);
        C_sanitize(g_name.value.s);
        C_strncpy_buf(g_clients[N_HOST_CLIENT_ID].name, g_name.value.s);

        /* Reinitialize any connected clients */
        for (i = 0; i < N_CLIENTS_MAX; i++) {
                if (!n_clients[i].connected)
                        continue;
                init_client(i);
                I_configure_player(i, g_clients[i].name,
                                   G_nation_to_color(g_clients[i].nation),
                                   TRUE);
        }

        /* Tell remote clients that we rehosted */
        N_broadcast_except(N_HOST_CLIENT_ID, "12ss", G_SM_POPUP, -1,
                           "g-host-rehost", "Host started a new game.");

        I_leave_limbo();
        I_popup(NULL, "Hosted a new game.");

        /* Finished initialization */
        g_host_inited = TRUE;
        publish_game_alive(TRUE);
}

/******************************************************************************\
 Broadcast a game-over message.
\******************************************************************************/
static void game_over(g_nation_name_t nation, n_client_id_t client)
{
        N_broadcast("111", G_SM_GAME_OVER, nation, client);
}

/******************************************************************************\
 Periodically scan through clients and ships to check for winners and losers.
\******************************************************************************/
static void check_game_over(void)
{
        static int check_time;
        g_nation_name_t best_nation, client_nation;
        int i, best_gold;
        n_client_id_t best_client;
        g_ship_t *ship;
        g_building_t *b;
        PyObject *key;
        Py_ssize_t pos = 0;

        /* Check clients periodically */
        if (c_time_msec < check_time || g_game_over)
                return;
        best_nation = G_NN_NONE;
        best_gold = -1;
        best_client = -1;
        check_time = c_time_msec + 1000;

        /* Reset counts */
        for (i = 0; i < N_CLIENTS_MAX; i++) {
                g_clients[i].ships = 0;
                g_clients[i].buildings = 0;
                g_clients[i].gold = 0;
        }
        for (i = 0; i < G_NATION_NAMES; i++)
                g_nations[i].gold = 0;

        /* Count ships */
        while (PyDict_Next(g_ship_dict, &pos, &key, (PyObject**)&ship)) {
                n_client_id_t client;

                if (!ship->in_use || ship->health <= 0)
                        continue;
                client = ship->client;
                g_clients[client].ships++;
                g_clients[client].gold += ship->store->cargo[G_CT_GOLD].amount;
        }

        /* Count buildings */
        pos = 0;
        while (PyDict_Next(g_building_dict, &pos, &key, (PyObject**)&b)) {
                n_client_id_t client;

                if (b->health <= 0)
                        continue;
                client = b->client;
                g_clients[client].buildings++;
                g_clients[client].gold += b->store->cargo[G_CT_GOLD].amount;
        }

        /* Client pass */
        for (i = 0; i < N_CLIENTS_MAX; i++) {
                if (!N_client_valid(i) || g_clients[i].nation == G_NN_NONE)
                        continue;

                /* Count gold toward nation */
                client_nation = g_clients[i].nation;
                g_nations[client_nation].gold += g_clients[i].gold;
                if (client_nation != G_NN_PIRATE) {

                        /* Nation won */
                        if (g_victory_gold.value.n > 0 &&
                            g_nations[client_nation].gold >=
                            g_victory_gold.value.n) {
                                game_over(client_nation, -1);
                                return;
                        }

                        /* Nation in the lead */
                        if (g_nations[client_nation].gold > best_gold) {
                                best_nation = client_nation;
                                best_gold = g_nations[client_nation].gold;
                        }

                        /* Nation tied for the lead */
                        else if (g_nations[client_nation].gold == best_gold)
                                best_nation = G_NN_NONE;
                }

                /* Pirates win individually */
                else if (g_victory_gold.value.n > 0 && g_clients[i].gold >=
                         g_victory_gold.value.n) {
                        game_over(G_NN_PIRATE, i);
                        return;
                }

                /* Pirate in the lead */
                else if (g_clients[i].gold > best_gold) {
                        best_nation = G_NN_PIRATE;
                        best_client = i;
                        best_gold = g_clients[i].gold;
                }

                /* Pirate tied for the lead */
                else if (g_clients[i].gold == best_gold)
                        best_nation = G_NN_NONE;


                /* Check for players that lost their ships */
                if (g_clients[i].ships > 0)
                        continue;
                g_clients[i].nation = G_NN_NONE;
                N_broadcast("1112", G_SM_AFFILIATE, i, G_NN_NONE, -1);
        }

        /* Timelimit ends the game */
        if (g_time_limit.value.n > 0 && g_time_limit_msec <= c_time_msec)
                game_over(best_nation, best_client);
}

/******************************************************************************\
 Send echo requests
\******************************************************************************/
void G_ping_clients(void)
{
        static int check_time;
        int i, echo_data;

        if(g_echo_rate.value.n == 0)
                return;
        /* Limit echo rate */
        if(g_echo_rate.value.n < 100)
                C_var_set(&g_echo_rate, "100");

        if (c_time_msec < check_time)
                return;
        check_time = c_time_msec + g_echo_rate.value.n;

        echo_data = C_rand();

        for (i = 0; i < N_CLIENTS_MAX; i++) {
                if (!N_client_valid(i))
                        continue;
                g_clients[i].echo_time = c_time_msec;
                g_clients[i].echo_data = echo_data;
        }

        N_broadcast("14", G_SM_ECHO_REQUEST, echo_data);
}

/******************************************************************************\
 Send ping time and gold info to clients
\******************************************************************************/
void G_update_clients(void)
{
        static int check_time;
        int i, client_array;

        if (c_time_msec < check_time)
                return;
        check_time = c_time_msec + 1000;

        client_array = 0;

        for (i = 0; i < N_CLIENTS_MAX; i++) {
                if (N_client_valid(i)) {
                        client_array |= 1 << i;
                }
        }

        N_send_start();
        N_send_char(G_SM_CLIENT_UPDATE);
        N_send_int(client_array);

        for (i = 0; i < N_CLIENTS_MAX; i++) {
                if (!N_client_valid(i))
                        continue;
                N_send_int(g_clients[i].gold);
                N_send_short(g_clients[i].ping_time);
        }
        N_broadcast(NULL);

}

/******************************************************************************\
 Called to update server-side structures. Does nothing if not hosting.
\******************************************************************************/
void G_update_host(void)
{
        if (n_client_id != N_HOST_CLIENT_ID || i_limbo)
                return;
        /* Send echo requests */
        G_ping_clients();
        /* Accept new connections and sent/receive data */
        N_poll_server();
        N_poll_http();

        /* Spawn crates for the players */
        while (g_gibs < CRATES_MAX) {
                g_cost_t *loot;
                int tile;

                tile = G_tile_gib(-1, G_GT_CRATES);
                if (tile < 0)
                        break;

                /* Put some loot in the crate */
                loot = &g_tiles[tile].gib->loot;
                loot->cargo[G_CT_GOLD] = 10 * C_roll_dice(5, 15) - 250;
                loot->cargo[G_CT_CREW] = C_roll_dice(3, 3) - 3;
                loot->cargo[G_CT_RATIONS] = C_roll_dice(4, 5) - 4;
                loot->cargo[G_CT_WOOD] = C_roll_dice(5, 10) - 15;
                loot->cargo[G_CT_IRON] = C_roll_dice(5, 10) - 25;
        }

        check_game_over();
        /* Send gold and ping time updates to clients */
        G_update_clients();

        publish_game_alive(FALSE);
}

