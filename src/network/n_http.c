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

/* This file handles the HTTP connection */

static n_callback_http_f http_func;
static SOCKET http_socket;
static int http_connect_time, http_buffer_len, http_port;
static char http_address[32], http_buffer[4096], http_host[256];
static bool http_connected;

/******************************************************************************\
 Starts connecting to the HTTP server.
\******************************************************************************/
void N_connect_http(const char *address, n_callback_http_f callback)
{
        N_disconnect_http();

        /* Resolve the address only if it has changed */
        http_port = 80;
        if (strcmp(address, http_host)) {
                C_strncpy_buf(http_host, address);
                N_resolve_buf(http_address, &http_port, address);
        }

        http_func = callback;
        http_socket = N_connect_socket(http_address, http_port);
        http_connect_time = c_time_msec;
}

/******************************************************************************\
 Waits for an HTTP connection to complete.
\******************************************************************************/
bool N_connect_http_wait(const char *address, n_callback_http_f callback)
{
        N_connect_http(address, callback);
        if (N_socket_select(http_socket, 1))
                return TRUE;
        return FALSE;
}

/******************************************************************************\
 Close the HTTP connection if it is still open.
\******************************************************************************/
void N_disconnect_http(void)
{
        if (!http_connected)
                return;
        if (http_connected)
                http_func(N_EV_DISCONNECTED, NULL, -1);
        else
                http_func(N_EV_CONNECT_FAILED, NULL, -1);
        http_connected = FALSE;
        if (http_socket != INVALID_SOCKET) {
                closesocket(http_socket);
                http_socket = INVALID_SOCKET;
        }
        C_debug("Closed HTTP connection");
}

/******************************************************************************\
 Check for data received from the HTTP connection or other events.
 TODO: Support HTML/1.1 chunking
\******************************************************************************/
void N_poll_http(void)
{
        int len, content_len;
        const char *error;
        char buffer[4096], *pos, *line, *token;
        bool end;

        if (http_socket == INVALID_SOCKET)
                return;

        /* Still connecting */
        if (!http_connected) {

                /* Timed out */
                if (c_time_msec > http_connect_time + CONNECT_TIMEOUT) {
                        N_disconnect_http();
                        return;
                }

                /* Connection ready? */
                if (!N_socket_select(http_socket, 0))
                        return;

                /* Success! */
                http_connected = TRUE;
                http_func(N_EV_CONNECTED, NULL, -1);
        }

        /* Overflow */
        if (http_buffer_len >= sizeof (http_buffer)) {
                C_warning("HTTP buffer overflow");
                http_buffer_len = 0;
                N_disconnect_http();
                return;
        }

        /* Try sending the buffer */
        if (http_buffer_len > 0) {
                int ret;

                ret = N_socket_send(http_socket, http_buffer, http_buffer_len);
                if (ret > 0) {
                        http_buffer_len = 0;
                        http_func(N_EV_SEND_COMPLETE, NULL, -1);
                        if (http_socket == INVALID_SOCKET)
                                return;
                } else if (ret < 0)
                        N_disconnect_http();
                        return;
        }

        /* Receive the message size */
        len = (int)recv(http_socket, buffer, sizeof (buffer), 0);

        /* Orderly shutdown */
        if (!len) {
                N_disconnect_http();
                return;
        }

        /* Error */
        if ((error = N_socket_error(len))) {
                C_debug("HTTP socket error: %s", error);
                return;
        }
        if (len < 0)
                return;

        /* Parse the first line */
        buffer[len] = NUL;
        pos = buffer;
        line = C_line(&pos, &end);

        /* "HTTP/1.0" */
        token = C_token(&line, NULL);
        if (strncmp(token, "HTTP", 4)) {
                C_warning("HTTP server sent invalid header: %s", token);
                return;
        }
        if (strcmp(token, "HTTP/1.1"))
                C_warning("Server did not send a HTTP/1.1 response: %s", token);

        /* HTTP return code */
        token = C_token(&line, NULL);
        if (!strcmp(token, "100"))
                return;
        if (strcmp(token, "200")) {
                C_warning("HTTP server code: %s", token);
                http_func(N_EV_MESSAGE, NULL, -1);
                return;
        }

        /* Parse header lines */
        content_len = 0;
        while (*(line = C_line(&pos, &end))) {
                token = C_token(&line, NULL);

                /* Content-Length */
                if (!strcasecmp(token, "Content-Length:")) {
                        token = C_token(&line, NULL);
                        content_len = atoi(token);
                }

                /* Transfer-Encoding */
                if (!strcasecmp(token, "Transfer-Encoding:"))
                        C_warning("Transfer-Encoding not supported");
        }

        /* Make sure we don't overrun */
        if (pos + content_len > buffer + sizeof (buffer))
                content_len = (int)(buffer + sizeof (buffer) - pos);

        /* The rest is "content" */
        http_func(N_EV_MESSAGE, pos, content_len);
}

/******************************************************************************\
 URL-encode [src] into [dest]. Returns FALSE if the entire string could not
 fit.
\******************************************************************************/
static bool url_encode(char **dest, int dest_size, const char *src)
{
        if (!src)
                return FALSE;
        while (*src) {
                int len;

                if (dest_size < 1)
                        return FALSE;

                /* Safe ranges */
                if ((*src >= '0' && *src <= '9') ||
                    (*src >= 'a' && *src <= 'z') ||
                    (*src >= 'A' && *src <= 'Z') || *src == '_') {
                        *((*dest)++) = *(src++);
                        dest_size--;
                        continue;
                }

                /* Hex-encode */
                if (dest_size < 3)
                        return FALSE;
                len = snprintf(*dest, dest_size, "%%%02x", *(src++));
                dest_size -= len;
                *dest += len;
        }
        return TRUE;
}

/******************************************************************************\
 Send GET method data through the HTTP connection.
\******************************************************************************/
void N_send_get(const char *url)
{
        http_buffer_len += snprintf(http_buffer + http_buffer_len,
                                    sizeof (http_buffer) - http_buffer_len,
                                    "GET %s HTTP/1.1\n"
                                    "Host: %s:%d\n"
                                    "Connection: close\n\n",
                                    url, http_host, http_port);
}

/******************************************************************************\
 Send POST method data through the HTTP connection. Pass string variable
 arguments in key-value pairs, ending with a NULL terminator.
\******************************************************************************/
void N_send_post_full(const char *url, ...)
{
        va_list va;
        int text_len;
        char text[4096], *pos;
        bool first;

        /* Pack text buffer */
        first = TRUE;
        va_start(va, url);
        for (pos = text; ; ) {
                const char *key, *value;
                int len;

                if (!(key = va_arg(va, const char *)) ||
                    !(value = va_arg(va, const char *)))
                        break;

                /* See if we have enough room */
                len = C_strlen(key) + C_strlen(value) + 2;
                if (sizeof (text) + text - pos <= len)
                        break;

                /* Separate keys */
                if (first)
                        first = FALSE;
                else
                        *(pos++) = '&';

                /* Encode key */
                if (!url_encode(&pos, (int)(text + sizeof (text) - pos) - 1,
                                key))
                        break;

                /* Separator */
                *(pos++) = '=';

                /* Encode value */
                if (!url_encode(&pos, (int)(text + sizeof (text) - pos),
                                value))
                        break;
        }
        *pos = NUL;
        text_len = (int)(pos - text);
        va_end(va);

        /* Send the message */
        http_buffer_len += snprintf(http_buffer + http_buffer_len,
                                    sizeof (http_buffer) - http_buffer_len,
                                    "POST %s HTTP/1.1\n"
                                    "Host: %s:%d\n"
                                    "Connection: close\n"
                                    "Content-Type:"
                                    " application/x-www-form-urlencoded\n"
                                    "Content-Length: %d\n\n%s",
                                    url, http_host, http_port, text_len, text);
}

