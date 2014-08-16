/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "../common/c_shared.h"
#include "../network/n_shared.h"
#include "../render/r_shared.h"
#include "../interface/i_shared.h"

#include "g_shared.h"

/* Network protocol used by the client and server. Increment when no longer
   compatible before releasing a new version of the game.*/
#define G_PROTOCOL 7

/* Invalid island index */
#define G_ISLAND_INVALID 255

/* Maximum number of islands. Do not set this value above G_ISLAND_INVALID. */
#define G_ISLAND_NUM 128

/* Proportion of cargo defined as the "optimal" crew amount */
#define G_SHIP_OPTIMAL_CREW 0.2f

/* Message tokens sent by clients */
typedef enum {
        G_CM_NONE,

        /* Messages for changing client status */
        G_CM_AFFILIATE,
        G_CM_NAME,

        /* Echo back */
        G_CM_ECHO_BACK,

        /* Communication */
        G_CM_CHAT,
        G_CM_PRIVMSG,

        /* Commands */
        G_CM_SHIP_BUY,
        G_CM_BUILDING_BUY,
        G_CM_SHIP_DROP,
        G_CM_SHIP_MOVE,
        G_CM_SHIP_NAME,
        G_CM_SHIP_PRICES,
        G_CM_SHIP_RING,
        G_CM_TILE_RING,

        G_CLIENT_MESSAGES
} g_client_msg_t;

/* Message tokens sent by the server */
typedef enum {
        G_SM_NONE,

        /* Synchronization messages */
        G_SM_CLIENT,
        G_SM_INIT,

        /* Echo request */
        G_SM_ECHO_REQUEST,

        /* Messages for when clients change status */
        G_SM_AFFILIATE,
        G_SM_CONNECTED,
        G_SM_DISCONNECTED,
        G_SM_NAME,
        G_SM_GAME_OVER,
        G_SM_CLIENT_UPDATE,

        /* Communication */
        G_SM_CHAT,
        G_SM_PRIVMSG,
        G_SM_POPUP,

        /* Entity updates */
        G_SM_SHIP_CARGO,
        G_SM_SHIP_NAME,
        G_SM_SHIP_OWNER,
        G_SM_SHIP_PATH,
        G_SM_SHIP_PRICES,
        G_SM_SHIP_SPAWN,
        G_SM_SHIP_STATE,
        G_SM_SHIP_TRANSACT,
        G_SM_BUILDING,
        G_SM_BUILDING_CARGO,
        G_SM_GIB,

        G_SERVER_MESSAGES
} g_server_msg_t;

/* Ship types */
typedef enum {
        G_ST_NONE,
        G_ST_SLOOP,
        G_ST_SPIDER,
        G_ST_GALLEON,
        G_SHIP_TYPES,
} g_ship_type_t;

/* Building types */
typedef enum {
        G_BT_NONE,
        G_BT_TREE,
        G_BT_TOWN_HALL,

        /* Tech preview buildings */
        G_BT_SHIPYARD,

        G_BUILDING_TYPES
} g_building_type_t;

/* Gib types */
typedef enum {
        G_GT_NONE,
        G_GT_CRATES,
        G_GIB_TYPES
} g_gib_type_t;

/* Name types */
typedef enum {
        G_NT_SHIP,
        G_NT_ISLAND,
        G_NAME_TYPES
} g_name_type_t;

/* Structure for information for a single cargo item */
typedef struct g_cargo {
        int amount, buy_price, maximum, minimum, sell_price;
        bool auto_buy, auto_sell;
} g_cargo_t;

/* Trading store structure */
typedef struct g_store {
        PyObject_HEAD;
        g_cargo_t cargo[G_CARGO_TYPES];
        int modified;
        short space_used, capacity;
        bool visible[N_CLIENTS_MAX];
} g_store_t;

/* Type used for ship ids */
typedef short g_ship_id;
/* Structure containing ship information */
typedef struct g_ship g_ship_t;
struct g_ship {
        PyObject_HEAD;
        g_ship_id id;
        r_model_t *model;
        c_vec3_t forward;
        float progress;
        int boarding, client, combat_time, focus_stamp, health,
            lunch_time, rear_tile, target, tile, trade_tile;
        char path[R_PATH_MAX], name[G_NAME_MAX];
        bool in_use, modified, target_board;
        g_ship_t *boarding_ship, *target_ship;
        g_store_t *store;
        ShipClass *class;
};

/* Structure to represent resource cost */
typedef struct g_cost {
        short cargo[G_CARGO_TYPES];
} g_cost_t;

/* Structure for building type parameters */
typedef struct g_building_class {
        g_cost_t cost;
        const char *name, *model_path;
        int health;
} g_building_class_t;

/* Structure containing building information */
typedef struct g_building {
        PyObject_HEAD;
        int id;
        g_building_type_t type;
        g_nation_name_t nation;
        n_client_id_t client;
        r_model_t *model;
        int gold, health;
        BuildingClass *class;
        g_store_t *store;
        bool modified;
        int tile;
} g_building_t;

/* Structure for the remains of ships */
typedef struct g_gib {
        g_gib_type_t type;
        g_cost_t loot;
        r_model_t *model;
} g_gib_t;

/* A tile on the globe */
typedef struct g_tile {
        g_building_t *building;
        g_gib_t *gib;
        int island, search_parent, search_stamp;
        g_ship_t *ship;
        bool visible;
} g_tile_t;

/* Structure for each player */
typedef struct g_client {
        int gold, nation, ships, buildings;
        char name[G_NAME_MAX];
        bool kicked;
        int echo_time, echo_data;
        short ping_time;
} g_client_t;

/* Structure containing ship type parameters */
typedef struct g_ship_base {
        g_cost_t cost;
        const char *model_path, *name;
        float speed;
        int health, cargo;
} g_ship_class_t;

///* Structure containing ship information */
/* Island structure */
typedef struct g_island {
        int tiles, land, root, town_tile;
} g_island_t;

/* g_client.c */
void G_client_callback(int client, n_event_t);
i_color_t G_nation_to_color(g_nation_name_t);

extern g_client_t g_clients[N_CLIENTS_MAX + 1];

extern bool g_initilized;

/* g_elements.c */
int G_cargo_nutritional_value(g_cargo_type_t);
void G_init_elements(void);
void G_reset_elements(void);

//extern g_ship_class_t g_ship_classes[G_SHIP_TYPES];
//extern g_building_class_t g_building_classes[G_BUILDING_TYPES];

/* g_globe.c */
void G_init_globe(void);
void G_generate_globe(int subdiv4, int islands, int island_size,
                      float variance);

extern g_island_t g_islands[G_ISLAND_NUM];
extern int g_islands_len;

/* g_host.c */
extern bool g_host_inited;

/* g_movement.c */
bool G_ship_move_to(g_ship_t *ship, int new_tile);
void G_ship_path(g_ship_t *ship, int target);
void G_ship_send_path(g_ship_t *ship, n_client_id_t client);
void G_ship_update_move(g_ship_t *ship);

/* g_names.c */
void G_count_name(g_name_type_t, const char *name);
void G_get_name(g_name_type_t, char *buffer, int buffer_size);
#define G_get_name_buf(t, b) G_get_name(t, b, sizeof (b))
void G_load_names(void);
void G_reset_name_counts(void);

/* g_ship.c */
void G_cleanup_ships(void);
void G_focus_next_ship(void);
bool G_ship_can_trade_with(g_ship_t *ship, int tile);
void G_ship_change_client(g_ship_t *ship, n_client_id_t client);
void G_ship_collect_gib(g_ship_t *ship);
bool G_ship_controlled_by(g_ship_t *ship, n_client_id_t);
void G_ship_drop_cargo(g_ship_t *ship, g_cargo_type_t type, int amount);
bool G_ship_hostile(g_ship_t *ship, n_client_id_t to);
void G_ship_hover(g_ship_t *ship);
void G_ship_reselect(g_ship_t *ship, n_client_id_t);
void G_ship_select(g_ship_t *ship);
void G_ship_send_cargo(g_ship_t *ship, n_client_id_t client);
void G_ship_send_name(g_ship_t *ship, n_client_id_t client);
void G_ship_send_state(g_ship_t *ship, n_client_id_t client);
void G_ship_send_spawn(g_ship_t *ship, n_client_id_t);
g_ship_t *G_ship_spawn(g_ship_id id, n_client_id_t client, int tile,
                       g_ship_type_t type);
void G_ship_update_combat(g_ship_t *ship);
void G_update_ships(void);
ShipClass *G_ship_class_from_ring_id(i_ring_icon_t id);
int G_ship_class_index_from_ring_id(i_ring_icon_t id);
void G_ship_update_trade_ui(g_ship_t *ship);

#define G_get_ship(i) \
        ((g_ship_t*)PyDict_GetItemString(g_ship_dict, C_va("%hd", i)))
#define G_set_ship(i, s) \
        PyDict_SetItemString(g_ship_dict, C_va("%hd", i), (PyObject*)s)
extern PyObject *g_ship_dict;
extern g_ship_t *g_hover_ship, *g_selected_ship;

/* g_sync.c */
#define G_corrupt_disconnect() G_corrupt_drop(N_SERVER_ID)
#define G_corrupt_drop(c) G_corrupt_drop_full(__FILE__, __LINE__, __func__, c)
void G_corrupt_drop_full(const char *file, int line, const char *func,
                         n_client_id_t);
#define G_receive_cargo(c) G_receive_range((c), 0, G_CARGO_TYPES)
#define G_receive_client(c) G_receive_client_full(__FILE__, __LINE__, \
                                                  __func__, (c))
int G_receive_client_full(const char *file, int line, const char *func,
                          n_client_id_t);
#define G_receive_nation(c) G_receive_range((c), 0, G_NATION_NAMES)
#define G_receive_range(c, min, max) \
        G_receive_range_full(__FILE__, __LINE__, __func__, (c), (min), (max))
int G_receive_range_full(const char *file, int line, const char *func,
                        n_client_id_t, int min, int max);
#define G_receive_ship(c) G_receive_ship_full(__FILE__, __LINE__, __func__, (c))
g_ship_t *G_receive_ship_full(const char *file, int line, const char *func,
                        n_client_id_t);
#define G_receive_tile(c) G_receive_tile_full(__FILE__, __LINE__, __func__, (c))
int G_receive_tile_full(const char *file, int line, const char *func,
                        n_client_id_t);
#define G_receive_building(c) \
        G_receive_building_full(__FILE__, __LINE__, __func__, (c))
g_building_t *G_receive_building_full(const char *file, int line,
                                      const char *func, int nation);

/* g_tile.c */
void G_cleanup_tiles(void);
void G_tile_build(int tile, int, n_client_id_t);
BuildingClass *G_building_class_from_ring_id(i_ring_icon_t id);
int G_building_class_index_from_ring_id(i_ring_icon_t id);
int G_tile_gib(int tile, g_gib_type_t);
bool G_building_controlled_by(g_building_t *building, n_client_id_t client);
void G_tile_hover(int tile);
void G_tile_select(int tile);
void G_tile_send_building(int tile, n_client_id_t);
void G_tile_send_gib(int tile, n_client_id_t);
bool G_tile_open(int tile, g_ship_t *exclude_ship);
void G_tile_position_model(int tile, r_model_t *);
void G_update_buildings(void);
int G_random_open_tile(void);

#define G_get_building(i) ((g_building_t*)PyDict_GetItemString(g_building_dict,\
                                                       C_va("%d", i)))
#define G_set_building(i, s) PyDict_SetItemString(g_building_dict, \
                                                  C_va("%d", i), (PyObject*)s)

#define G_get_building_class(i) \
        ((BuildingClass*)PyList_GET_ITEM(g_building_class_list, i))

extern g_tile_t g_tiles[R_TILES_MAX];
extern int g_gibs, g_hover_tile, g_selected_tile;
extern PyObject *g_building_dict;

/* g_trade.c */
int G_build_time(const g_cost_t *);
bool G_cargo_equal(const g_cargo_t *, const g_cargo_t *);
const char *G_cost_to_string(const g_cost_t *);
int G_limit_purchase(g_store_t *buyer, g_store_t *seller,
                     g_cargo_type_t, int amount, bool free);
void G_list_to_cost( PyObject *list, g_cost_t *cost );
bool G_pay(n_client_id_t, int tile, const g_cost_t *, bool pay);
int G_store_add(g_store_t *, g_cargo_type_t, int amount);
void G_store_add_cost(g_store_t *, const g_cost_t *);
int G_store_fits(const g_store_t *, g_cargo_type_t);
g_store_t *G_store_init(int capacity);
void G_store_receive(g_store_t *, bool ignore_prices);
void G_store_select_clients(const g_store_t *);
void G_store_send(g_store_t *, bool force);
int G_store_space(g_store_t *);

/* g_classes.c */
int Store_init(g_store_t *, PyObject *);
PyObject *Store_new(PyTypeObject *, PyObject *, PyObject *);
PyTypeObject StoreType;
int Ship_init(g_ship_t *, PyObject *);
PyObject *Ship_new(PyTypeObject *, PyObject *, PyObject *);
PyTypeObject ShipType;

int G_building_init(g_building_t *, PyObject *);
PyObject *G_building_new(PyTypeObject *, PyObject *, PyObject *);
PyTypeObject G_building_type;

/* g_variables.c */
extern c_var_t g_forest, g_debug_net, g_globe_seed, g_globe_subdiv4,
               g_island_num, g_island_size, g_island_variance,
               g_master, g_master_url, g_name, g_nation_colors[G_NATION_NAMES],
               g_players, g_test_globe, g_time_limit, g_victory_gold,
               g_player_ship_limit, g_player_building_limit, g_echo_rate;

/* game api */
extern PyObject *g_callbacks;
