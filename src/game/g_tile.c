/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "g_common.h"

/* Island tiles with game data */
g_tile_t g_tiles[R_TILES_MAX];

/* The tile the mouse is hovering over and the currently selected tile */
int g_hover_tile, g_selected_tile;

/* Number of gibs on the globe */
int g_gibs;

/* Ships dict */
PyObject *g_building_dict;


/* Current ship id */
int g_building_id = 0;

/******************************************************************************\
 Cleanup a gib structure.
\******************************************************************************/
static void gib_free(g_gib_t *gib)
{
        if (!gib)
                return;
        Py_CLEAR(gib->model);
        C_free(gib);
        g_gibs--;
}

/******************************************************************************\
 Cleanup a tile structures.
\******************************************************************************/
void G_cleanup_tiles(void)
{
        int i;

        for (i = 0; i < r_tiles_max; i++) {
                Py_CLEAR(g_tiles[i].ship);
                if(g_tiles[i].building) {
                        int id;
                        id = g_tiles[i].building->id;
                        Py_CLEAR(g_tiles[i].building);
                }
                gib_free(g_tiles[i].gib);
                C_zero(g_tiles + i);
        }
        PyDict_Clear(g_building_dict);
}

/******************************************************************************\
 Setup tile quick info.
\******************************************************************************/
static void tile_quick_info(int index)
{
        g_tile_t *tile;
        BuildingClass *building_class;
        i_color_t color;
        float prop;

        if (index < 0) {
                I_quick_info_close();
                return;
        }
        tile = g_tiles + index;
        if (tile->building) {
                building_class = tile->building->class;
                I_quick_info_show(building_class->name, &r_tiles[index].origin);
                /* Owner */
                color = G_nation_to_color(tile->building->nation);
                if(building_class->buildable)
                        I_quick_info_add_color("Owner:",
                                       g_clients[tile->building->client].name,
                                       color);
        } else {
                I_quick_info_show(G_get_building_class(0)->name,
                                  &r_tiles[index].origin);
                I_quick_info_add("Terrain:",
                                 R_terrain_to_string(r_tiles[index].terrain));
                return;
        }

        /* Terrain */
        I_quick_info_add("Terrain:",
                         R_terrain_to_string(r_tiles[index].terrain));

        /* Health */
        color = I_COLOR_ALT;
        prop = (float)tile->building->health / building_class->health;
        if (prop >= 0.67)
                color = I_COLOR_GOOD;
        if (prop <= 0.33)
                color = I_COLOR_BAD;
        I_quick_info_add_color("Health:", C_va("%d/%d", tile->building->health,
                                               building_class->health), color);
}

/******************************************************************************\
 Configure the interface to reflect the building's cargo and trade status.
\******************************************************************************/
static void building_configure_trade(g_building_t *building)
{
        g_ship_t *partner;
        int i;

        /* Our client can't actually see this cargo -- we probably don't have
           the right data for it anyway! */
        if (!building /*|| !building->store->visible[n_client_id]*/) {
                I_disable_trade();
                return;
        }

        /* Do we have a trading partner? */
        G_store_space(building->store);
        partner = NULL;
        I_enable_trade(building->client == n_client_id, FALSE, NULL,
                       building->store->space_used, building->store->capacity);

        /* Configure cargo items */
        for (i = 0; i < G_CARGO_TYPES; i++) {
                i_cargo_info_t info;
                g_cargo_t *cargo;

                /* Our side */
                cargo = building->store->cargo + i;
                info.amount = cargo->amount;
                info.auto_buy = cargo->auto_buy;
                info.auto_sell = cargo->auto_sell;
                info.buy_price = cargo->buy_price;
                info.sell_price = cargo->sell_price;
                info.minimum = cargo->minimum;
                info.maximum = cargo->maximum;

                /* No trade partner */
                info.p_buy_price = -1;
                info.p_sell_price = -1;
                if (!partner) {
                        info.p_amount = -1;
                        I_configure_cargo(i, &info);
                        continue;
                }

                /* Trade partner's side */
                cargo = partner->store->cargo + i;
                /* Free trading */
                if(building->client == partner->client){
                        info.auto_buy = TRUE;
                        info.auto_sell = TRUE;
                        info.p_buy_price = 0;
                        info.p_sell_price = 0;
                        info.p_maximum = cargo->maximum;
                        info.p_minimum = cargo->minimum;
                }
                info.p_amount = cargo->amount;
                I_configure_cargo(i, &info);
        }
}

/******************************************************************************\
 Set the selection state of a tile's building. Will not change a selected
 model's selection state if [protect_selection] is TRUE.
\******************************************************************************/
static void tile_building_select(int tile, r_model_select_t select,
                                 bool protect_selection)
{
        g_building_t *building;

        if (tile < 0 || tile >= r_tiles_max)
                return;
        building = g_tiles[tile].building;
        if (!building || (protect_selection &&
                          building->model->selected == R_MS_SELECTED))
                return;
        building->model->selected = select;
}

/******************************************************************************\
 Selects a tile.
\******************************************************************************/
void G_tile_select(int tile)
{
        g_building_t *building;
        r_terrain_t terrain;

        if (g_selected_tile == tile)
                return;
        building = g_tiles[tile].building;

        /* Can't select water */
        if (tile >= 0) {
                terrain = R_terrain_base(r_tiles[tile].terrain);
                if (terrain != R_T_SAND && terrain != R_T_GROUND) {
                        /* Deselect everything */
                        R_select_tile(-1, R_ST_NONE);
                        tile_building_select(g_selected_tile, R_MS_NONE, FALSE);
                        tile_quick_info(-1);
                        building_configure_trade(NULL);
                        g_selected_tile = -1;
                        return;
                }
        }

        /* Deselect previous tile */
        tile_building_select(g_selected_tile, R_MS_NONE, FALSE);

        /* Select the new tile */
        if ((g_selected_tile = tile) >= 0) {
                R_hover_tile(-1, R_ST_NONE);
                tile_building_select(tile, R_MS_SELECTED, FALSE);
        }

        R_select_tile(tile, R_ST_TILE);
        if(tile > 0)
                building_configure_trade(building);
        tile_quick_info(tile);
}

/******************************************************************************\
 Updates which tile the mouse is hovering over. Pass -1 to turn off.
\******************************************************************************/
void G_tile_hover(int tile)
{
        static r_select_type_t select_type;
        r_select_type_t new_select_type;

        C_assert(tile < r_tiles_max);
        new_select_type = R_ST_NONE;

        /* Selecting a tile to move the current ship to */
        if (G_ship_controlled_by(g_selected_ship, n_client_id) &&
            G_tile_open(tile, NULL) && !g_game_over)
                new_select_type = R_ST_GOTO;

        /* Selecting an island tile */
        else if (tile >= 0) {
                r_terrain_t terrain = R_terrain_base(r_tiles[tile].terrain);
                if ((terrain == R_T_SAND || terrain == R_T_GROUND) &&
                    tile != g_selected_tile)
                        new_select_type = R_ST_TILE;
        }

        /* If there is no ship, can't select this tile */
        else if (g_tiles[tile].ship < 0)
                tile = -1;

        /* Still hovering over the same tile */
        if (tile == g_hover_tile && new_select_type == select_type) {
                G_ship_hover(tile >= 0 ? g_tiles[tile].ship : NULL);

                /* Tile can get deselected for whatever reason */
                if (select_type == R_ST_TILE)
                        tile_building_select(tile, R_MS_HOVER, TRUE);
                return;
        }

        /* Deselect the old tile */
        tile_building_select(g_hover_tile, R_MS_NONE, TRUE);

        /* Apply new hover */
        R_hover_tile(tile, (select_type = new_select_type));
        if ((g_hover_tile = tile) < 0 || new_select_type == R_ST_NONE) {
                G_ship_hover(NULL);
                return;
        }
        if (g_tiles[tile].ship) {
                G_ship_hover(g_tiles[tile].ship);
                return;
        }

        /* Select the tile building model if there is no ship there */
        if (select_type != R_ST_NONE)
                tile_building_select(tile, R_MS_HOVER, TRUE);
}

/******************************************************************************\
 Position a model on this tile.
\******************************************************************************/
void G_tile_position_model(int tile, r_model_t *model)
{
        if (!model)
                return;
        model->forward = r_tiles[tile].forward;
        model->origin = r_tiles[tile].origin;
        model->normal = r_tiles[tile].normal;
}

/******************************************************************************\
 Send information about a tile's building.
\******************************************************************************/
void G_tile_send_building(int tile, n_client_id_t client)
{
        if (!g_tiles[tile].building) {
                N_send(client, "121", G_SM_BUILDING, tile, G_BT_NONE);
                return;
        }
        N_send(client, "1211", G_SM_BUILDING, tile,
               g_tiles[tile].building->type, g_tiles[tile].building->client);
}

/******************************************************************************\
 Start constructing a building on this tile.
\******************************************************************************/
void G_tile_build(int tile, int bc_index, n_client_id_t client)
{
        g_building_t *building;
        BuildingClass *bc;
        g_nation_name_t nation;

        /* Range checks */
        if (tile < 0 || tile >= r_tiles_max ||
            bc_index < 0 || bc_index >= PyList_GET_SIZE(g_building_class_list))
                return;

        if(g_tiles[tile].building) {
                int id;
                id = g_tiles[tile].building->id;
                PyDict_DelItemString(g_building_dict, C_va("%d", id));
                Py_CLEAR(g_tiles[tile].building);
        }

        nation = (client < 0) ? G_NN_NONE : g_clients[client].nation;

        /* "None" building type is special */
        if (bc_index == G_BT_NONE)
                g_tiles[tile].building = NULL;

        /* Allocate and initialize a building structure */
        else {
                bc = (BuildingClass*)PyList_GET_ITEM( g_building_class_list,
                                                      bc_index );
                if(nation == G_NN_NONE && bc->buildable)
                        return;
                building = (g_building_t*)G_building_new(&G_building_type, NULL, NULL);
                building->id = g_building_id++;
                building->type = bc_index;
                building->client = client;
                building->nation = nation;
                building->health = bc->health;
                building->tile = tile;
                building->model = R_model_init(bc->model_path, TRUE);
                G_tile_position_model(tile, building->model);
                Py_INCREF(bc);
                building->class = bc;
                g_tiles[tile].building = building;
                G_set_building(building->id, building);

                /* Initialize store */
                building->store = G_store_init(bc->cargo);
                /* Building's have no minimum crew */
                building->store->cargo[G_CT_CREW].minimum = 0;

                /* Start out selected */
                if (g_selected_tile == tile) {
                        building->model->selected = R_MS_SELECTED;
                        building_configure_trade(building);
                }
        }

        /* If we just built a new town hall, update the island */
        if (bc_index == G_BT_TOWN_HALL)
                g_islands[g_tiles[tile].island].town_tile = tile;

        /* Let all connected clients know about this */
        if (g_host_inited)
                G_tile_send_building(tile, N_BROADCAST_ID);
}

/******************************************************************************\
 Find the building's class from its ring id
 Returns: Borrowed Reference
\******************************************************************************/
BuildingClass *G_building_class_from_ring_id(i_ring_icon_t id)
{
        BuildingClass *bc;
        int i, classes;

        classes = PyList_GET_SIZE(g_building_class_list);

        for(i=0; i < classes; i++)
        {
                bc = (BuildingClass*)PyList_GET_ITEM(g_building_class_list, i);
                if(bc->ring_id == id)
                {
                        return bc;
                }
        }
        return NULL;
}
/******************************************************************************\
 Find the building's class index from its ring id
\******************************************************************************/
int G_building_class_index_from_ring_id(i_ring_icon_t id)
{
        BuildingClass *bc;
        int i, classes;

        classes = PyList_GET_SIZE(g_building_class_list);

        for(i=0; i < classes; i++)
        {
                bc = (BuildingClass*)PyList_GET_ITEM(g_building_class_list, i);
                if(bc->ring_id == id)
                {
                        return i;
                }
        }
        return -1;
}

/******************************************************************************\
 Returns TRUE if a ship can sail into the given tile. If [exclude_ship] is
 non-negative then the tile is still considered open if [exclude_ship] is
 in it.
\******************************************************************************/
bool G_tile_open(int tile, g_ship_t *exclude_ship)
{
        return (!g_tiles[tile].ship ||
                (exclude_ship && g_tiles[tile].ship == exclude_ship)) &&
               R_water_terrain(r_tiles[tile].terrain);
}

/******************************************************************************\
 Pick a random open tile. Returns -1 if the globe is completely full.
\******************************************************************************/
int G_random_open_tile(void)
{
        int start, tile;

        start = C_rand() % r_tiles_max;
        for (tile = start + 1; tile < r_tiles_max; tile++)
                if (G_tile_open(tile, NULL))
                        return tile;
        for (tile = 0; tile <= start; tile++)
                if (G_tile_open(tile, NULL))
                        return tile;

        /* If this happens, the globe is completely full! */
        C_warning("Globe is full");
        return -1;
}

/******************************************************************************\
 Send information about a tile's gibs.
\******************************************************************************/
void G_tile_send_gib(int tile, n_client_id_t client)
{
        g_gib_type_t type;

        type = !g_tiles[tile].gib ? G_GT_NONE : g_tiles[tile].gib->type;
        N_send(client, "121", G_SM_GIB, tile, type);
}

/******************************************************************************\
 Spawn gibs on this tile. Returns the selected tile.
\******************************************************************************/
int G_tile_gib(int tile, g_gib_type_t type)
{
        /* Pick a random tile */
        if (tile < 0)
                tile = G_random_open_tile();
        if (tile < 0)
                return -1;

        gib_free(g_tiles[tile].gib);
        if (type != G_GT_NONE) {
                g_gibs++;
                g_tiles[tile].gib = (g_gib_t *)C_calloc(sizeof (g_gib_t));
                g_tiles[tile].gib->type = type;

                /* Initialize the model */
                g_tiles[tile].gib->model=
                        R_model_init("models/gib/crates.plum", TRUE);
                G_tile_position_model(tile, g_tiles[tile].gib->model);
        } else
                g_tiles[tile].gib = NULL;

        /* Let all connected clients know about this gib */
        if (g_host_inited)
                G_tile_send_gib(tile, N_BROADCAST_ID);

        return tile;
}

/******************************************************************************\
 Returns TRUE if a building is valid and can be controlled by the given
 client. Also doubles as a way to check if the building is valid to begin with.
\******************************************************************************/
bool G_building_controlled_by(g_building_t *building, n_client_id_t client)
{
        return building && building->health > 0 && building->client == client;
}

/******************************************************************************\
 Send updated cargo information to clients. If [client] is negative, all
 clients that can see the building's cargo are updated and only modified cargo
 entries are communicated.
\******************************************************************************/
void G_building_send_cargo(g_building_t *building, n_client_id_t client)
{
        int i, id;
        bool broadcast;

        /* Host already knows everything */
        if (client == N_HOST_CLIENT_ID || n_client_id != N_HOST_CLIENT_ID)
                return;

        /* Nothing is sent if broadcasting an unmodified store */
        if ((broadcast = client < 0 || client == N_BROADCAST_ID) &&
            !building->store->modified)
                return;

        id = building->id;

        /* Pack all the cargo information */
        N_send_start();
        N_send_char(G_SM_BUILDING_CARGO);
        N_send_int(id);
        G_store_send(building->store,
                     !broadcast || client == N_SELECTED_ID);

        /* Already selected the target clients */
        if (client == N_SELECTED_ID) {
                N_send_selected(NULL);
                return;
        }

        /* Send to clients that can see complete cargo information */
        if (broadcast) {
                for (i = 0; i < N_CLIENTS_MAX; i++)
                        n_clients[i].selected = building->store->visible[i];
                N_send_selected(NULL);
                return;
        }

        /* Send to a single client */
        if (building->store->visible[client])
                N_send(client, NULL);
}

/******************************************************************************\
 Update which clients can see [building]'s cargo.
\******************************************************************************/
static void building_update_visible(g_building_t *building)
{
        int i, nation;
        bool old_visible[N_CLIENTS_MAX], selected;

        if(!building->class || !building->store || building->class->cargo <= 0)
                return;

        memcpy(old_visible, building->store->visible, sizeof (old_visible));
        /* Cargo always visible */
        C_one_buf(building->store->visible);

        /* We can always see the stores of our own ships */
        nation = building->nation;
        /* If the game is over, all ships are visible */
        if (g_game_over)
                C_one_buf(building->store->visible);

        /* No need to send updates here if we aren't hosting */
        if (n_client_id != N_HOST_CLIENT_ID)
                return;

        /* Only send the update to clients that can see the store now but
           couldn't see it before */
        for (selected = FALSE, i = 0; i < N_CLIENTS_MAX; i++) {
                n_clients[i].selected = FALSE;
                if (i == N_HOST_CLIENT_ID || old_visible[i])
                        continue;
                n_clients[i].selected = TRUE;
                selected = TRUE;
        }
        if (selected)
                G_building_send_cargo(building, N_SELECTED_ID);
}

/******************************************************************************\
 Updates building status and actions.
\******************************************************************************/
void G_update_buildings(void)
{
        g_building_t *building;
        PyObject *key;
        Py_ssize_t pos = 0;

        while (PyDict_Next(g_building_dict, &pos, &key, (PyObject**)&building)) {
                building_update_visible(building);
                /* If the cargo manfiest changed, send updates once per frame */
                if (building->store && building->store->modified)
                        G_building_send_cargo(building, -1);
        }
}

