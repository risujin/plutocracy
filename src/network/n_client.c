/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "n_common.h"

/* ID of this client in the game */
n_client_id_t n_client_id;

static int connect_time;

/******************************************************************************\
 Initializes the network namespace.
\******************************************************************************/
void N_init(void)
{
#ifdef WINDOWS
        WSADATA wsaData;

        if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
                C_error("Failed to initialize WinSock");
#endif
        n_client_id = N_INVALID_ID;
        n_clients[N_SERVER_ID].socket = INVALID_SOCKET;
}

/******************************************************************************\
 Cleans up the network namespace.
\******************************************************************************/
void N_cleanup(void)
{
#ifdef WINDOWS
        WSACleanup();
#endif
        N_stop_server();
}

/******************************************************************************\
 Connect the client to the given [address] (ip or hostname) and [port].
\******************************************************************************/
void N_connect(const char *address, n_callback_f client_func)
{
        int port;
        char ip[32];

        n_client_func = client_func;

        /* Resolve the hostname */
        C_var_unlatch(&n_port);
        port = n_port.value.n;
        N_resolve_buf(ip, &port, address);

        n_clients[N_SERVER_ID].socket = N_connect_socket(ip, port);
        connect_time = c_time_msec;
}

/******************************************************************************\
 Close the client's connection to the server.
\******************************************************************************/
void N_disconnect(void)
{
        if (n_client_id == N_INVALID_ID)
                return;
        if (n_client_func)
                n_client_func(N_SERVER_ID, n_client_id == N_INVALID_ID ?
                                           N_EV_CONNECT_FAILED :
                                           N_EV_DISCONNECTED);
        if (n_client_id == N_HOST_CLIENT_ID)
                N_stop_server();
        if (n_clients[N_SERVER_ID].socket != INVALID_SOCKET) {
                closesocket(n_clients[N_SERVER_ID].socket);
                n_clients[N_SERVER_ID].socket = INVALID_SOCKET;
        }
        n_clients[N_SERVER_ID].connected = FALSE;
        n_client_id = N_INVALID_ID;
        C_debug("Disconnected from server");
}

/******************************************************************************\
 Receive events from the server.
\******************************************************************************/
void N_poll_client(void)
{
        /* See if we have connected yet */
        if (n_client_id == N_INVALID_ID) {
                if (n_clients[N_SERVER_ID].socket == INVALID_SOCKET ||
                    !N_socket_select(n_clients[N_SERVER_ID].socket, 0)) {
                        if (connect_time + CONNECT_TIMEOUT < c_time_msec)
                                N_disconnect();
                        return;
                }
                n_clients[N_SERVER_ID].connected = TRUE;
                n_clients[N_SERVER_ID].buffer_len = 0;
                n_client_id = N_UNASSIGNED_ID;
                n_client_func(N_SERVER_ID, N_EV_CONNECTED);
                return;
        }

        /* Send and receive data */
        if (!N_send_buffer(N_SERVER_ID) || !N_receive(N_SERVER_ID))
                N_disconnect();
}

/******************************************************************************\
 Returns a string representation of a client ID.
\******************************************************************************/
const char *N_client_to_string(n_client_id_t client)
{
        if (client == N_HOST_CLIENT_ID)
                return "host client";
        else if (client == N_SERVER_ID)
                return "server";
        else if (client == N_UNASSIGNED_ID)
                return "unassigned";
        else if (client == N_BROADCAST_ID)
                return "broadcast";
        else if (client == N_INVALID_ID)
                return "invalid";
        return C_va("client %d", client);
}

/******************************************************************************\
 Returns TRUE if a client index is valid.
\******************************************************************************/
bool N_client_valid(n_client_id_t client)
{
        return client == N_SERVER_ID ||
               (client >= 0 && client < N_CLIENTS_MAX &&
                n_clients[client].connected);
}

