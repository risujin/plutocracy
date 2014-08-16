/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Convenience and compatibility wrappers for sockets */

#include "n_common.h"

/******************************************************************************\
 Resolve a hostname.
\******************************************************************************/
bool N_resolve(char *address, int size, int *port, const char *hostname)
{
        struct hostent *host;
        int i, last_colon;
        char buffer[64], *host_ip;

        /* Parse the port out of the hostname string */
        for (last_colon = -1, i = 0; hostname[i]; i++)
                if (hostname[i] == ':')
                        last_colon = i;
        if (last_colon >= 0) {
                int value;

                if ((value = atoi(hostname + last_colon + 1)))
                        *port = value;
                memcpy(buffer, hostname, last_colon);
                buffer[last_colon] = NUL;
                hostname = buffer;
        }

        /* Resolve hostname */
        host = gethostbyname(hostname);
        if (!host) {
                C_warning("Failed to resolve hostname '%s'", hostname);
                *buffer = NUL;
                return FALSE;
        }
        host_ip = inet_ntoa(*((struct in_addr *)host->h_addr));
        C_strncpy(address, host_ip, size);
        C_debug("Resolved '%s' to %s", hostname, host_ip);
        return TRUE;
}

/******************************************************************************\
 Make a generic TCP/IP socket connection.
\******************************************************************************/
SOCKET N_connect_socket(const char *address, int port)
{
        struct sockaddr_in addr;
        SOCKET sock;
        int ret;
        const char *error;

        /* Bad address */
        if (!address || !*address)
                return INVALID_SOCKET;

        /* Connect to the server */
        sock = socket(PF_INET, SOCK_STREAM, 0);
        N_socket_no_block(sock);
        C_zero(&addr);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(address);
        ret = connect(sock, (struct sockaddr *)&addr, sizeof (addr));

        /* Error connecting */
        if ((error = N_socket_error(ret))) {
                C_warning("Connect error: %s", error);
                return INVALID_SOCKET;
        }

        /* Started to connect */
        C_debug("Connecting to %s:%d", address, port);
        return sock;
}

/******************************************************************************\
 Send generic data over a socket. Returns the amount of data sent, or -1 if 
 there was an error.
\******************************************************************************/
int N_socket_send(SOCKET socket, const char *data, int size)
{
        int ret;
        const char *error;

        /* Ready to write? */
        if (!N_socket_select(socket, 0))
                return 0;

        /* Try sending */
        ret = send(socket, data, size, 0);
        if ((error = N_socket_error(ret))) {
                C_warning("Send error: %s", error);
                return -1;
        }

        return ret;
}

/******************************************************************************\
 Get the socket for the given client ID.
\******************************************************************************/
SOCKET N_client_to_socket(n_client_id_t client)
{
        if (client >= 0 && client <= N_CLIENTS_MAX)
                return n_clients[client].socket;
        C_error("Invalid client ID %d", client);
        return INVALID_SOCKET;
}

/******************************************************************************\
 Pass the value returned by the socket operation as [ret]. Returns NULL if no
 real error was generated otherwise returns the string representation of it.
 TODO: Function to convert WinSock error codes to strings
\******************************************************************************/
const char *N_socket_error(int ret)
{
        if (ret >= 0)
                return NULL;
#ifdef WINDOWS
        /* No data (WinSock) */
        if (WSAGetLastError() == WSAEWOULDBLOCK)
                return NULL;
        return C_va("%d", WSAGetLastError());
#else
        /* No data (Berkeley) */
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS)
                return NULL;
        return strerror(errno);
#endif
}

/******************************************************************************\
 Put a socket in non-blocking mode.
\******************************************************************************/
void N_socket_no_block(SOCKET socket)
{
#ifdef WINDOWS
        u_long mode = 1;
        ioctlsocket(socket, FIONBIO, &mode);
#else
        fcntl(socket, F_SETFL, O_NONBLOCK);
#endif
}

/******************************************************************************\
 Waits for a socket to become readable. Returns TRUE on success.
\******************************************************************************/
bool N_socket_select(SOCKET sock, int timeout)
{
        struct timeval tv;
        fd_set fds;
        int nfds;

        /* Timeout */
        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        FD_ZERO(&fds);
        FD_SET(sock, &fds);
#ifdef WINDOWS
        nfds = 0;
#else
        nfds = sock + 1;
#endif
        select(nfds, NULL, &fds, NULL, &tv);
        return FD_ISSET(sock, &fds);
}

