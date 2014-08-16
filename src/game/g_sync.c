/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Convenience functions for client and servers to automate error checking */

#include "g_common.h"

/******************************************************************************\
 Disconnect if the server has sent corrupted data.
\******************************************************************************/
void G_corrupt_drop_full(const char *file, int line, const char *func,
                         n_client_id_t client)
{
        C_warning_full(file, line, func, "%s sent corrupt data",
                       N_client_to_string(client));
        if (client < 0 || client == N_SERVER_ID) {
                I_popup(NULL, "Server sent invalid data.");
                N_disconnect();
        } else {
                N_send(client, "12ss", G_SM_POPUP, -1,
                       "g-host-invalid", "Your client sent invalid data.");
                N_drop_client(client);
        }
}

/******************************************************************************\
 Convenience function to receive a client index and disconnect if it is invalid.
 Returns a negative index if the received index was not valid.
\******************************************************************************/
int G_receive_client_full(const char *file, int line, const char *func,
                          n_client_id_t client)
{
        int index;

        index = N_receive_char();
        if (!N_client_valid(index)) {
                G_corrupt_drop_full(file, line, func, client);
                return -1;
        }
        return index;
}

/******************************************************************************\
 Convenience function to receive a one-byte index and disconnect if it is out
 of range. Returns a negative index if the received index was not valid.
 Note that [min] is inclusive but [max] is exclusive.
\******************************************************************************/
int G_receive_range_full(const char *file, int line, const char *func,
                         n_client_id_t client, int min, int max)
{
        int index;

        index = N_receive_char();
        if (index < min || index >= max) {
                G_corrupt_drop_full(file, line, func, client);
                return -1;
        }
        return index;
}

/******************************************************************************\
 Convenience function to receive a ship id and check if it belongs to
 [client]. Pass a negative value for [client] to ignore this check. Returns a
 negative index if the received id was not valid.
\******************************************************************************/
g_ship_t *G_receive_ship_full(const char *file, int line, const char *func,
                        int client)
{
        g_ship_t *ship;
        short id;

        id = N_receive_short();
        ship = G_get_ship(id);
        if (!ship || !ship->in_use)
                return NULL;
        return ship;
}

/******************************************************************************\
 Convenience function to receive a tile index. Returns a negative index if the
 received index was not valid.
\******************************************************************************/
int G_receive_tile_full(const char *file, int line, const char *func,
                        n_client_id_t client)
{
        int index;

        index = N_receive_short();
        if (index < 0 || index >= r_tiles_max) {
                G_corrupt_drop_full(file, line, func, client);
                return -1;
        }
        return index;
}

/******************************************************************************\
 Convenience function to receive a building id and check if it belongs to
 [client]. Pass a negative value for [client] to ignore this check. Returns a
 negative index if the received id was not valid.
\******************************************************************************/
g_building_t *G_receive_building_full(const char *file, int line,
                                      const char *func, int client)
{
        g_building_t *building;
        int id;

        id = N_receive_int();
        building = G_get_building(id);
        if (!building)
                return NULL;
        return building;
}
