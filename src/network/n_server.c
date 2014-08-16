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

/* Client slots array */
n_client_t n_clients[N_CLIENTS_MAX + 1];
int n_clients_num;

/* Server socket for listening for incoming connections */
static SOCKET listen_socket;

/******************************************************************************\
 Close server sockets and stop accepting connections.
\******************************************************************************/
void N_stop_server(void)
{
        int i;

        if (n_client_id != N_HOST_CLIENT_ID)
                return;
        n_server_func(N_HOST_CLIENT_ID, N_EV_DISCONNECTED);
        n_client_id = N_INVALID_ID;

        /* Close listen server socket */
        if (listen_socket != INVALID_SOCKET)
                closesocket(listen_socket);
        listen_socket = INVALID_SOCKET;

        /* Disconnect any active clients */
        for (i = 0; i < N_CLIENTS_MAX; i++)
                if (n_clients[i].connected) {
                        closesocket(n_clients[i].socket);
                        n_clients[i].connected = FALSE;
                }

        C_debug("Stopped listen server");
}

/******************************************************************************\
 Open server sockets and begin accepting connections. Returns TRUE on success.
\******************************************************************************/
int N_start_server(n_callback_f server_func, n_callback_f client_func)
{
        struct sockaddr_in addr;
#if defined(WINDOWS) || defined(SOLARIS)
        char yes;
#else
        int yes;
#endif

        if (n_client_id == N_HOST_CLIENT_ID)
                return TRUE;
        N_disconnect();

        /* Reinitialize clients table */
        n_client_id = N_HOST_CLIENT_ID;
        n_server_func = server_func;
        n_client_func = client_func;
        C_zero(&n_clients);

        /* Setup the host's client */
        n_clients[N_HOST_CLIENT_ID].connected = TRUE;
        n_clients[N_HOST_CLIENT_ID].buffer_len = 0;
        n_clients[N_SERVER_ID].connected = TRUE;
        n_clients[N_SERVER_ID].buffer_len = 0;
        n_clients_num = 1;
        n_server_func(N_HOST_CLIENT_ID, N_EV_CONNECTED);
        n_client_func(N_SERVER_ID, N_EV_CONNECTED);

        /* The local client may have disconnected in response to something the
           server tried to do, so we check here if that happened */
        if (n_client_id == N_INVALID_ID)
                return FALSE;

        /* Start the listen server and accept connections */
        C_var_unlatch(&n_port);
        listen_socket = socket(PF_INET, SOCK_STREAM, 0);

        /* Disable "port in use" error */
        yes = 1;
        setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (yes));

        /* Bind the listen socket to our port */
        C_zero(&addr);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(n_port.value.n);
        if (bind(listen_socket, (struct sockaddr *)&addr, sizeof (addr)) ||
            listen(listen_socket, 16)) {
                C_warning("Failed to bind to port %d", n_port.value.n);
                return FALSE;
        }
        N_socket_no_block(listen_socket);
        C_debug("Started listen server");
        return TRUE;
}

/******************************************************************************\
 Accept any incoming connections.
\******************************************************************************/
static void accept_connections(void)
{
        struct sockaddr_in addr;
        socklen_t socklen;
        SOCKET socket;
        int i;

        socklen = sizeof (addr);
        if ((socket = accept(listen_socket, (struct sockaddr *)&addr,
                             &socklen)) == INVALID_SOCKET)
                return;

        /* Find a client id for this client */
        for (i = 0; n_clients[i].connected; i++)
                if (i >= N_CLIENTS_MAX) {
                        C_debug("Server full, rejected new connection");
                        closesocket(socket);
                        return;
                }
        C_debug("Connected '%s' as client %d", inet_ntoa(addr.sin_addr), i);
        N_socket_no_block(socket);

        /* Initialize the client */
        n_clients[i].connected = TRUE;
        n_clients[i].buffer_len = 0;
        n_clients[i].socket = socket;
        n_clients_num++;
        n_server_func(i, N_EV_CONNECTED);
}

/******************************************************************************\
 Disconnect a client from the server.
\******************************************************************************/
void N_drop_client(int client)
{
        /* If we're not the server just disconnect */
        if (n_client_id != N_HOST_CLIENT_ID) {
                C_assert(client == N_SERVER_ID);
                N_disconnect();
                return;
        }

        C_assert(client >= 0 && client < N_CLIENTS_MAX);
        if (!n_clients[client].connected) {
                C_warning("Tried to drop unconnected client %d", client);
                return;
        }
        n_clients[client].connected = FALSE;
        n_clients[client].buffer_len = 0;
        n_clients_num--;

        /* The server kicked itself */
        if (client == n_client_id) {
                N_disconnect();
                C_debug("Server dropped itself");
                return;
        }

        n_server_func(client, N_EV_DISCONNECTED);
        closesocket(n_clients[client].socket);
        C_debug("Dropped client %d", client);
}

/******************************************************************************\
 Poll connections and dispatch any messages that arrive.
\******************************************************************************/
void N_poll_server(void)
{
        int i;

        if (n_client_id != N_HOST_CLIENT_ID)
                return;
        accept_connections();

        /* Send to and receive from clients */
        for (i = 0; i < N_CLIENTS_MAX; i++)
                if (!N_send_buffer(i) || !N_receive(i))
                        N_drop_client(i);
}

