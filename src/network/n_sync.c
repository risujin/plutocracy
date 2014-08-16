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
#include "SDL_endian.h"

/* Receive function that arriving messages are routed to */
n_callback_f n_client_func, n_server_func;

static int sync_pos, sync_size;
static char sync_buffer[N_SYNC_MAX];

/* Number of bytes sent/received in the last second */
int n_bytes_received, n_bytes_sent;

/******************************************************************************\
 Call these functions to retrieve an argument from the current message from
 within the [n_receive_f] function when it is called.
\******************************************************************************/
char N_receive_char(void)
{
        char value;

        if (sync_pos + 1 > sync_size)
                return NUL;
        value = sync_buffer[sync_pos++];
        return value;
}

int N_receive_int(void)
{
        int value;

        if (sync_pos + 4 > sync_size)
                return 0;
        value = (int)SDL_SwapLE32(*(Uint32 *)(sync_buffer + sync_pos));
        sync_pos += 4;
        return value;
}

float N_receive_float(void)
{
        union {
                int n;
                float f;
        } value;

        value.n = N_receive_int();
        return value.f;
}

short N_receive_short(void)
{
        short value;

        if (sync_pos + 2 > sync_size)
                return 0;
        value = (short)SDL_SwapLE16(*(Uint16 *)(sync_buffer + sync_pos));
        sync_pos += 2;
        return value;
}

void N_receive_string(char *buffer, int size)
{
        int from, len;

        if (!buffer || size < 1) {
                return;
        }
        for (from = sync_pos; sync_buffer[sync_pos]; sync_pos++)
                if (sync_pos > sync_size) {
                        *buffer = NUL;
                        return;
                }
        len = ++sync_pos - from;
        if (len > size)
                len = size;
        memmove(buffer, sync_buffer + from, len);
}

/******************************************************************************\
 Write bytes to the data buffer. The datum is assumed to be an integer or
 float that needs byte-order rearranging.
\******************************************************************************/
static bool write_bytes(int offset, int bytes, void *data)
{
        void *to;

        if (offset + bytes > N_SYNC_MAX)
                return FALSE;
        to = sync_buffer + offset;
        switch (bytes) {
        case 1:
                *(char *)to = *(char *)data;
                break;
        case 2:
                *(Uint16 *)to = SDL_SwapLE16(*(Uint16 *)data);
                break;
        case 4:
        default:
                *(Uint32 *)to = SDL_SwapLE32(*(Uint32 *)data);
                break;
        }
        if (offset + bytes > sync_size)
                sync_size = offset + bytes;
        return TRUE;
}

/******************************************************************************\
 Reset the sync buffer. Use before sending via N_send_* calls.
\******************************************************************************/
void N_send_start(void)
{
        sync_size = 2;
}

/******************************************************************************\
 Add data to the send buffer. Call N_send(client, NULL) to finish sending data.
 Returns FALSE if the buffer overflowed.
\******************************************************************************/
bool N_send_char(char ch)
{
        return write_bytes(sync_size, 1, &ch);
}

bool N_send_short(short n)
{
        return write_bytes(sync_size, 2, &n);
}

bool N_send_int(int n)
{
        return write_bytes(sync_size, 4, &n);
}

bool N_send_float(float f)
{
        return write_bytes(sync_size, 4, &f);
}

bool N_send_string(const char *string)
{
        int string_len;

        string_len = C_strlen(string) + 1;
        if (string_len <= 1) {
                if (sync_size > N_SYNC_MAX - 1)
                        return FALSE;
                sync_buffer[sync_size++] = NUL;
                return TRUE;
        }
        if (sync_size + string_len > N_SYNC_MAX)
               return FALSE;
        memcpy(sync_buffer + sync_size, string, string_len);
        sync_size += string_len;
        return TRUE;
}

/******************************************************************************\
 Pack the current message buffer into the send buffers of a client.
\******************************************************************************/
static void send_buffer(n_client_id_t client)
{
        /* Overflow */
        if (n_clients[client].buffer_len + sync_size >=
            sizeof (n_clients[client].buffer)) {
                C_warning("%s buffer overflow", N_client_to_string(client));
                N_drop_client(client);
                return;
        }

        /* Pack message into the buffer */
        memcpy(n_clients[client].buffer + n_clients[client].buffer_len,
               sync_buffer, sync_size);
        n_clients[client].buffer_len += sync_size;
}

/******************************************************************************\
 Sends a message to [client]. If [client] is 0, sends to the server. The
 [format] string describes the variable argument list given to the function.

   c, 1    char       1 byte
   d, 2    short      2 bytes
   l, 4    integer    4 bytes
   f       float      4 bytes
   s       string     NULL-terminated

 If [client] is (-id - 1), all clients except id will receive the message. Note
 that you can either be receiving or sending. The moment you send a new
 message, the one you were receiving is lost!
\******************************************************************************/
void N_send_full(const char *file, int line, const char *func,
                 int client, const char *format, ...)
{
        va_list va;
        int sentinel;

        /* We're not connected */
        if (n_client_id < 0)
                return;

        /* Clients don't send messages to anyone but the server */
        if (n_client_id != N_HOST_CLIENT_ID && client != N_SERVER_ID)
                return;

        /* Pack the message into the sync buffer */
        if (!format || !format[0])
                goto skip;
        va_start(va, format);
        for (sync_size = 2; *format; format++)
                switch (*format) {
                case '1':
                case 'c':
                        if (!N_send_char((char)va_arg(va, int)))
                                goto overflow;
                        break;
                case '2':
                case 'd':
                        if (!N_send_short((short)va_arg(va, int)))
                                goto overflow;
                        break;
                case '4':
                case 'l':
                        if (!N_send_int(va_arg(va, int)))
                                goto overflow;
                        break;
                case 'f':
                        if (!N_send_float((float)va_arg(va, double)))
                                goto overflow;
                        break;
                case 's':
                        if (!N_send_string(va_arg(va, const char *)))
                                goto overflow;
                        break;
                default:
                        C_error_full(file, line, func,
                                     "Invalid format character '%c'", *format);
                }

        /* Check sentinel; if its missing then you probably forgot to add
           an argument to the argument list or have one too many */
        sentinel = va_arg(va, int);
        if (sentinel != N_SENTINEL)
                C_error_full(file, line, func, "Missing sentinel");
        va_end(va);

        /* Write the size of the message as the first 2-bytes */
skip:   write_bytes(0, 2, &sync_size);

        /* Broadcast to every client */
        if (client == N_BROADCAST_ID || client == N_SELECTED_ID || client < 0) {
                int i, except;

                C_assert(n_client_id == N_HOST_CLIENT_ID);
                except = -client - 1;
                for (i = 0; i < N_CLIENTS_MAX; i++) {
                        if (!n_clients[i].connected || i == except ||
                            (!n_clients[i].selected && client == N_SELECTED_ID))
                                continue;
                        send_buffer(i);
                }
                return;
        }

        /* Single-client message */
        if (!n_clients[client].connected) {
                C_warning("Tried to message unconnected %s",
                          N_client_to_string(client));
                return;
        }
        send_buffer(client);
        return;

overflow:
        va_end(va);
        C_warning_full(file, line, func, "Outgoing message buffer overflow");
}

/******************************************************************************\
 Try to send buffer.
\******************************************************************************/
bool N_send_buffer(n_client_id_t client)
{
        SOCKET socket;
        int ret;

        if (!n_clients[client].connected)
                return TRUE;

        /* Local messages don't need to be sent */
        if (n_client_id == N_HOST_CLIENT_ID &&
            (client == N_HOST_CLIENT_ID || client == N_SERVER_ID))
                return TRUE;

        /* Send TCP/IP message */
        socket = N_client_to_socket(client);
        ret = N_socket_send(socket, n_clients[client].buffer,
                            n_clients[client].buffer_len);
        n_bytes_sent += n_clients[client].buffer_len;
        if (ret > 0) {
                n_clients[client].buffer_len -= ret;
                memmove(n_clients[client].buffer, n_clients[client].buffer +
                        ret, n_clients[client].buffer_len);
        } else if (ret < 0)
                return FALSE;
        return TRUE;
}

/******************************************************************************\
 Receive local messages directly from the send buffers. Returns TRUE if
 [client] was local.
\******************************************************************************/
static bool receive_local(n_client_id_t client)
{
        n_client_t *pclient;
        n_callback_f callback;
        int pos;

        if (n_client_id != N_HOST_CLIENT_ID)
                return FALSE;

        /* Determine which event handler to use */
        if (client == N_SERVER_ID) {
                pclient = n_clients + N_HOST_CLIENT_ID;
                callback = n_client_func;
        } else if (client == N_HOST_CLIENT_ID) {
                pclient = n_clients + N_SERVER_ID;
                callback = n_server_func;
        } else
                return FALSE;

        /* Dispatch messages in order */
        for (pos = 0; pos < pclient->buffer_len; ) {
                sync_pos = 0;

                /* Unpack the message size */
                sync_buffer[0] = pclient->buffer[pos];
                sync_buffer[1] = pclient->buffer[pos + 1];
                sync_size = N_receive_short();
                C_assert(sync_size <= pclient->buffer_len - pos);

                /* Copy the entire message into the sync buffer */
                memcpy(sync_buffer, pclient->buffer + pos, sync_size);
                pos += sync_size;
                callback(client, N_EV_MESSAGE);
        }
        pclient->buffer_len = 0;
        return TRUE;
}

/******************************************************************************\
 Receive data from a socket. Returns FALSE if an error occured and the
 connection should be dropped.
\******************************************************************************/
bool N_receive(n_client_id_t client)
{
        SOCKET socket;
        int len, message_size;

        /* Receive from the local queue */
        if (!n_clients[client].connected || receive_local(client))
                return TRUE;

        /* Receive from a socket */
        for (socket = N_client_to_socket(client); ; ) {
                const char *error;

                /* Receive the message size */
                len = (int)recv(socket, sync_buffer, N_SYNC_MAX, MSG_PEEK);

                /* Orderly shutdown */
                if (!len)
                        return FALSE;

                /* Error */
                if ((error = N_socket_error(len))) {
                        C_debug("Error receiving from %s: %s",
                                N_client_to_string(client), error);
                        return FALSE;
                }

                /* Did not receive the message size */
                if (len < 2)
                        return TRUE;

                /* Read the message length */
                sync_pos = 0;
                sync_size = 2;
                message_size = N_receive_short();
                if (message_size < 1 || message_size > N_SYNC_MAX) {
                        C_warning("Invalid message size %d "
                                  "(recv returned %d, %s)", message_size, len,
                                  N_client_to_string(client));
                        return FALSE;
                }

                /* Check if we have enough room now */
                if (len < message_size)
                        return TRUE;
                n_bytes_received += message_size;

                /* Read the entire message */
                recv(socket, sync_buffer, message_size, 0);

                /* Dispatch the message */
                sync_pos = 2;
                sync_size = message_size;
                if (n_client_id == N_HOST_CLIENT_ID)
                        n_server_func(client, N_EV_MESSAGE);
                else
                        n_client_func(N_SERVER_ID, N_EV_MESSAGE);
        }
}

