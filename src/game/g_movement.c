/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Handles ship movement */

#include "g_common.h"

/* Maximum breadth of a path search */
#define SEARCH_BREADTH (R_PATH_MAX * 3)

/* Proportion of the remaining rotation a ship does per second */
#define ROTATION_RATE 3.f

/* Factor in the speed modifier equation that determines how quickly adding
   crew increases ship speed. The larger the factor the faster speed will
   rise with crew. */
#define CREW_SPEED_FACTOR 2.f

/* This is the minimum speed a ship can have */
#define MINIMUM_SPEED 0.25f

/* Structure for searched tile nodes */
typedef struct search_node {
        float dist;
        int tile, moves;
} search_node_t;

/******************************************************************************\
 Returns the linear distance from one tile to another. This is the search
 function heuristic.
\******************************************************************************/
static float tile_dist(int a, int b)
{
        return C_vec3_len(C_vec3_sub(r_tiles[b].origin,
                                     r_tiles[a].origin));
}

/******************************************************************************\
 Scan along a ship's path and find that farthest out open tile. Returns the
 new target tile. Note that any special rules that constrict path-finding
 must be applied here as well or the path-finding function could fall into
 an infinite loop.
\******************************************************************************/
static int farthest_path_tile(g_ship_t *ship)
{
        int tile, next, path_len, neighbors[3];

        tile = ship->tile;
        for (path_len = 0; ; tile = next) {
                next = ship->path[path_len++];
                if (next <= 0)
                        return tile;
                R_tile_neighbors(tile, neighbors);
                next = neighbors[next - 1];
                if (!G_tile_open(next, ship) || R_land_bridge(tile, next))
                        return tile;
        }
}

/******************************************************************************\
 Returns TRUE if the ship in a given tile is leaving it.
\******************************************************************************/
static bool ship_leaving_tile(int tile)
{
        g_ship_t *ship;

        C_assert(tile >= 0 && tile < r_tiles_max);
        ship = g_tiles[tile].ship;
        if (ship && ship->tile != ship->rear_tile && tile == ship->rear_tile)
                return TRUE;
        return FALSE;
}

/******************************************************************************\
 Send the ship's path.
\******************************************************************************/
void G_ship_send_path(g_ship_t *ship, n_client_id_t client)
{
        if (!ship->in_use)
                return;
        N_send(client, "122fs", G_SM_SHIP_PATH,
               ship->id, ship->tile, ship->progress,
               ship->path);
}

/******************************************************************************\
 Find a path from where the [ship] is to the target [tile] and sets that as
 the ship's new path.
\******************************************************************************/
void G_ship_path(g_ship_t *ship, int target)
{
        static int search_stamp;
        search_node_t nodes[SEARCH_BREADTH];
        int i, nodes_len, closest, path_len, neighbors[3];
        bool changed, target_next;

        if (n_client_id != N_HOST_CLIENT_ID)
                return;
        changed = FALSE;

        /* Silent fail */
        if (target < 0 || target >= r_tiles_max ||
            ship->tile == target) {
                changed = ship->path[0] != NUL;
                ship->target = ship->tile;
                if (changed) {
                        ship->path[0] = NUL;
                        G_ship_send_path(ship, N_BROADCAST_ID);
                        if (ship->client == n_client_id &&
                            g_selected_ship == ship)
                                R_select_path(-1, NULL);
                }
                return;
        }

        /* Clear the target for now */
        ship->target = ship->tile;

        /* If the target tile is not available, try to get next to it instead
           of onto it */
        target_next = !G_tile_open(target, ship);

        /* Start with just the initial tile open */
        search_stamp++;
        nodes[0].tile = ship->tile;
        nodes[0].dist = tile_dist(nodes[0].tile, target);
        nodes[0].moves = 0;
        nodes_len = 1;
        g_tiles[nodes[0].tile].search_parent = -1;
        g_tiles[nodes[0].tile].search_stamp = search_stamp;
        closest = 0;

        for (;;) {
                search_node_t node;

                node = nodes[closest];

                /* Ran out of search nodes -- no path to target */
                if (nodes_len < 1)
                        goto failed;

                /* Remove the node from the list */
                nodes_len--;
                memmove(nodes + closest, nodes + closest + 1,
                        (nodes_len - closest) * sizeof (*nodes));

                /* Add its children */
                R_tile_neighbors(node.tile, neighbors);
                for (i = 0; i < 3; i++) {
                        int stamp;
                        bool open;

                        /* Out of space? */
                        if (nodes_len >= SEARCH_BREADTH) {
                                C_warning("Out of search space");
                                return;
                        }

                        /* Made it to an adjacent tile */
                        if (target_next && neighbors[i] == target) {
                                nodes[nodes_len] = node;
                                goto rewind;
                        }

                        /* Tile blocked? */
                        open = G_tile_open(neighbors[i], ship) ||
                               ship_leaving_tile(neighbors[i]);

                        /* Can we open this node? */
                        stamp = g_tiles[neighbors[i]].search_stamp;
                        C_assert(stamp <= search_stamp);
                        if (stamp == search_stamp || !open ||
                            R_land_bridge(node.tile, neighbors[i]))
                                continue;
                        g_tiles[neighbors[i]].search_stamp = search_stamp;

                        /* Add to array */
                        nodes[nodes_len].tile = neighbors[i];
                        g_tiles[neighbors[i]].search_parent = node.tile;

                        /* Did we make it onto the target? */
                        if (neighbors[i] == target)
                                goto rewind;

                        /* Distance to destination */
                        nodes[nodes_len].dist = tile_dist(neighbors[i], target);
                        nodes[nodes_len].moves = node.moves + 1;

                        nodes_len++;
                }

                /* Find the new closest node */
                for (closest = 0, i = 1; i < nodes_len; i++)
                        if (2 * nodes[i].moves + nodes[i].dist <
                            2 * nodes[closest].moves + nodes[closest].dist)
                                closest = i;
        }

rewind: /* Count length of the path */
        path_len = -1;
        for (i = nodes[nodes_len].tile; i >= 0; i = g_tiles[i].search_parent)
                path_len++;

        /* The path is too long */
        if (path_len > R_PATH_MAX) {
                C_warning("Path is too long (%d tiles)", path_len);
                return;
        }

        /* Write the path backwards */
        if (ship->path[path_len])
                changed = TRUE;
        ship->path[path_len] = 0;
        for (i = nodes[nodes_len].tile; i >= 0; ) {
                int j, parent;

                parent = g_tiles[i].search_parent;
                if (parent < 0)
                        break;
                R_tile_neighbors(parent, neighbors);
                for (j = 0; neighbors[j] != i; j++);
                path_len--;
                if (ship->path[path_len] != j + 1)
                        changed = TRUE;
                ship->path[path_len] = j + 1;
                i = parent;
        }
        ship->target = target;

        /* Update ship selection */
        if (changed) {
                if (g_selected_ship == ship &&
                    ship->client == n_client_id)
                        R_select_path(ship->tile, ship->path);
                G_ship_send_path(ship, N_BROADCAST_ID);
        }

        return;

failed: /* If we can't reach the target, and we have a valid path, try
           to at least get through some of it */
        i = farthest_path_tile(ship);
        if (i != target)
                G_ship_path(ship, i);
        else
                ship->path[0] = 0;

        /* Can't reach target ship */
        Py_CLEAR(ship->target_ship);

        if (ship->client == n_client_id) {
                const char *fmt;

                /* Update ship path selection */
                if (g_selected_ship == ship)
                        R_select_path(ship->tile, ship->path);

                fmt = C_str("i-ship-destination",
                            "%s can't reach destination.");
                I_popup(&r_tiles[ship->tile].origin,
                        C_va(fmt, ship->name));
        }
}

/******************************************************************************\
 Return the speed of a ship with all modifiers applied.
\******************************************************************************/
static float ship_speed(g_ship_t *ship)
{
        g_store_t *store;
        float crew, speed;

        speed = ship->class->speed;
        store = ship->store;

        /* Crew modifier */
        C_assert(store->cargo[G_CT_CREW].amount >= 0);
        crew = (float)store->cargo[G_CT_CREW].amount /
               (G_SHIP_OPTIMAL_CREW * store->capacity);
        if (crew > 1.f)
                crew = 1.f;
        speed *= (-1.f / (CREW_SPEED_FACTOR * crew + 1.f) + 1.f) *
                 (1.f + 1.f / CREW_SPEED_FACTOR);

        /* Let them have a minimal speed */
        if (speed < MINIMUM_SPEED)
                speed = MINIMUM_SPEED;

        return speed;
}

/******************************************************************************\
 Position and orient the ship's model.
\******************************************************************************/
static void ship_position_model(g_ship_t *ship)
{
        r_model_t *model;
        int new_tile, old_tile;

        if (!ship->in_use)
                return;
        new_tile = ship->tile;
        old_tile = ship->rear_tile;
        model = ship->model;

        /* If the ship is not moving, just place it on the tile */
        if (ship->rear_tile < 0) {
                model->normal = r_tiles[new_tile].normal;
                model->origin = r_tiles[new_tile].origin;
        }

        /* Otherwise interpolate normal and origin */
        else {
                model->normal = C_vec3_lerp(r_tiles[old_tile].normal,
                                            ship->progress,
                                            r_tiles[new_tile].normal);
                model->normal = C_vec3_norm(model->normal);
                model->origin = C_vec3_lerp(r_tiles[old_tile].origin,
                                            ship->progress,
                                            r_tiles[new_tile].origin);
        }

        /* Rotate toward the forward vector */
        if (!C_vec3_eq(model->forward, ship->forward)) {
                float lerp;

                lerp = ROTATION_RATE * c_frame_sec * ship_speed(ship);
                if (lerp > 1.f)
                        lerp = 1.f;
                model->forward = C_vec3_norm(model->forward);
                model->forward = C_vec3_rotate_to(model->forward, model->normal,
                                                  lerp, ship->forward);
        }
}

/******************************************************************************\
 Move a ship to a new tile. Returns TRUE if the ship was moved. This is only
 for cases where the client and server are out of sync.
\******************************************************************************/
bool G_ship_move_to(g_ship_t *ship, int new_tile)
{
        int old_tile;

        old_tile = ship->tile;

        /* Don't bother if we are already there */
        if (new_tile == old_tile || !G_tile_open(new_tile, ship))
                return FALSE;

        /* Remove this ship from the old tile */
        C_assert(ship->rear_tile != ship->tile);
        if (ship->rear_tile >= 0 &&
            g_tiles[ship->rear_tile].ship == ship)
                Py_CLEAR(g_tiles[ship->rear_tile].ship);

        /* Move to the new tile */
        ship->rear_tile = old_tile;
        ship->tile = new_tile;
        Py_INCREF(ship);
        g_tiles[new_tile].ship = ship;

        /* Make a new path to our target */
        G_ship_path(ship, ship->target);

        return TRUE;
}

/******************************************************************************\
 Move a ship forward.
\******************************************************************************/
static void ship_move(g_ship_t *ship)
{
        c_vec3_t forward;
        int old_tile, new_tile, neighbors[3];
        bool arrived, open;

        old_tile = new_tile = -1;

        /* Stopped and target ship is moving */
        if (ship->rear_tile < 0 && ship->target_ship &&
            ship->target_ship->rear_tile >= 0)
                G_ship_path(ship, ship->target_ship->tile);

        /* Is this ship moving? */
        if (ship->path[0] <= 0 && ship->rear_tile < 0)
                return;
        ship->progress += c_frame_sec * ship_speed(ship);

        /* Still in progress */
        if (ship->progress < 1.f)
                return;
        ship->progress = 1.f;

        /* Can't move while boarding */
        if (ship->boarding > 0)
                return;

        /* Keep track of the target ship */
        if (ship->target_ship &&
                        ship->target_ship->tile != ship->target)
                G_ship_path(ship, ship->target_ship->tile);

        /* Update the path */
        else
                G_ship_path(ship, ship->target);

        /* Ship cannot move without any crew */
        if(ship->store->cargo[G_CT_CREW].amount == 0) {
                ship->path[0] = 0;
                ship->rear_tile = -1;
                return;
        }

        /* Get new destination tile */
        if (!(arrived = ship->path[0] <= 0)) {
                old_tile = ship->tile;
                R_tile_neighbors(old_tile, neighbors);
                new_tile = (int)(ship->path[0] - 1);
                new_tile = neighbors[new_tile];
        }

        /* Remove this ship from the old tile */
        C_assert(ship->rear_tile != ship->tile);
        if (ship->rear_tile >= 0 &&
            g_tiles[ship->rear_tile].ship == ship)
                Py_CLEAR(g_tiles[ship->rear_tile].ship);

        /* See if we hit an obstacle */
        if (!arrived) {
                open = G_tile_open(new_tile, ship);

                /* If there is a ship leaving the next tile,
                   wait for it to move out instead of pathing */
                if (!open && ship_leaving_tile(new_tile))
                        return;
        }

        /* If we arrived or hit an obstacle, stop */
        if (arrived || !open) {
                ship->path[0] = 0;
                ship->rear_tile = -1;
                return;
        }

        /* Consume a path move */
        memmove(ship->path, ship->path + 1,
                R_PATH_MAX - 1);
        if (g_selected_ship == ship &&
                ship->client == n_client_id)
                R_select_path(new_tile, ship->path);

        /* Rotate to face the new tile */
        forward = C_vec3_norm(C_vec3_sub(r_tiles[new_tile].origin,
                                         r_tiles[old_tile].origin));

        ship->progress = ship->progress - 1.f;
        ship->rear_tile = old_tile;
        ship->tile = new_tile;
        ship->forward = forward;
        Py_INCREF(ship);
        g_tiles[new_tile].ship = ship;

        /* Pick up crate gibs */
        G_ship_collect_gib(ship);
}

/******************************************************************************\
 Update a ship's movement.
\******************************************************************************/
void G_ship_update_move(g_ship_t *ship)
{
        ship_move(ship);
        ship_position_model(ship);
}

