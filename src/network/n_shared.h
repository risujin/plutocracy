/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Largest amount of data that can be sent via a message */
#define N_SYNC_MAX 32000

/* Sentinel added to the end of N_send_full() calls */
#define N_SENTINEL -1234567890

/* Windows compatibility */
#ifdef WINDOWS
typedef UINT_PTR SOCKET;
#else
typedef int SOCKET;
#endif

/* Special client IDs */
typedef enum {
        N_INVALID_ID = -1,
        N_HOST_CLIENT_ID = 0,
        N_CLIENTS_MAX = 32,
        N_SERVER_ID = N_CLIENTS_MAX,
        N_UNASSIGNED_ID,
        N_BROADCAST_ID,
        N_SELECTED_ID,
} n_client_id_t;

/* Callback function events */
typedef enum {
        N_EV_MESSAGE,
        N_EV_CONNECTED,
        N_EV_CONNECT_FAILED,
        N_EV_DISCONNECTED,
        N_EV_SEND_COMPLETE,
} n_event_t;

/* Client/server network callback function */
typedef void (*n_callback_f)(n_client_id_t, n_event_t);

/* HTTP network callback function */
typedef void (*n_callback_http_f)(n_event_t, const char *text, int length);

/* Structure for connected clients */
typedef struct n_client {
        SOCKET socket;
        int buffer_len;
        char buffer[N_SYNC_MAX];
        bool connected, selected;
} n_client_t;

/* n_client.c */
void N_cleanup(void);
const char *N_client_to_string(n_client_id_t);
bool N_client_valid(n_client_id_t);
void N_connect(const char *address, n_callback_f client);
void N_disconnect(void);
void N_init(void);
void N_poll_client(void);

extern n_client_id_t n_client_id;

/* n_http.c */
void N_connect_http(const char *address, n_callback_http_f);
bool N_connect_http_wait(const char *address, n_callback_http_f);
void N_disconnect_http(void);
void N_poll_http(void);
bool N_resolve(char *address, int address_max, int *port, const char *hostname);
#define N_resolve_buf(a, p, h) N_resolve((a), sizeof (a), (p), (h))
void N_send_get(const char *url);
#define N_send_post(url, ...) N_send_post_full(url, ## __VA_ARGS__, NULL)
void N_send_post_full(const char *url, ...);

/* n_server.c */
void N_drop_client(n_client_id_t);
void N_poll_server(void);
int N_start_server(n_callback_f server, n_callback_f client);
void N_stop_server(void);

extern n_client_t n_clients[N_CLIENTS_MAX + 1];
extern int n_clients_num;

/* n_sync.c */
#define N_broadcast(f, ...) \
        N_send_full(__FILE__, __LINE__, __func__, N_BROADCAST_ID, f, \
                    ## __VA_ARGS__, N_SENTINEL)
#define N_broadcast_except(c, f, ...) \
        N_send_full(__FILE__, __LINE__, __func__, -(c) - 1, f, \
                    ## __VA_ARGS__, N_SENTINEL)
char N_receive_char(void);
float N_receive_float(void);
int N_receive_int(void);
short N_receive_short(void);
void N_receive_string(char *buffer, int size);
#define N_receive_string_buf(b) N_receive_string(b, sizeof (b))
#define N_send(n, fmt, ...) N_send_full(__FILE__, __LINE__, __func__, n, fmt, \
                                        ## __VA_ARGS__, N_SENTINEL);
#define N_send_selected(fmt, ...) N_send_full(__FILE__, __LINE__, __func__, \
                                              N_SELECTED_ID, fmt, \
                                              ## __VA_ARGS__, N_SENTINEL)

bool N_send_char(char);;
bool N_send_short(short);
bool N_send_int(int);
bool N_send_float(float);
void N_send_full(const char *file, int line, const char *func,
                 n_client_id_t, const char *format, ...);
void N_send_start(void);
bool N_send_string(const char *);

extern int n_bytes_received, n_bytes_sent;

/* n_variables.c */
void N_register_variables(void);

extern c_var_t n_port;

