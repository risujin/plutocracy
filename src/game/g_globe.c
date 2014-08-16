/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Implements globe terrain generation. Note that changing any of these
   parameters will invalidate the protocol! */

#include "g_common.h"

/* Maximum island size */
#define ISLAND_SIZE 256

/* Maximum height off the globe surface of a tile */
#define ISLAND_HEIGHT 4.f

/* Minimum number of land tiles for island to succeed */
#define ISLAND_LAND 8

/* Rate of model fades in alpha per second */
#define MODEL_FADE 1.f

/* Distance that models fade out */
#define MODEL_FADE_DIST 4.f

/* Array of islands */
g_island_t g_islands[G_ISLAND_NUM];
int g_islands_len;

static float visible_range;

/******************************************************************************\
 Randomly selects a tile ground terrain based on climate approximations.
 FIXME: Does not choose terrain correctly, biased toward 'hot'.
\******************************************************************************/
static r_terrain_t choose_terrain(int tile)
{
        float prop;

        prop = R_tile_latitude(tile);
        if (prop < 0.f)
                prop = -prop;
        prop = 4.f * prop / C_PI - 1.f;
        if (prop >= 0.f) {
                if (C_rand_real() > prop)
                        return R_T_GROUND;
                return R_T_GROUND_COLD;
        }
        if (C_rand_real() > -prop)
                return R_T_GROUND;
        return R_T_GROUND_HOT;
}

/******************************************************************************\
 Iterates over the entire globe and configures ground tile terrain.
\******************************************************************************/
static void sanitise_terrain(void)
{
        int i, j, region_len, region[12], hot, cold, temp, sand, land, failed;

        /* During the first pass we convert shallow tiles to sand tiles */
        land = 0;
        for (i = 0; i < r_tiles_max; i++) {
                if (r_tiles[i].terrain != R_T_SHALLOW)
                        continue;
                region_len = R_tile_region(i, region);
                for (j = 0; j < region_len; j++)
                        if (r_tiles[region[j]].terrain == R_T_WATER ||
                            g_tiles[region[j]].island != g_tiles[i].island)
                                goto skip_shallow;
                r_tiles[i].terrain = R_T_SAND;
                g_islands[g_tiles[i].island].land++;
                land++;
skip_shallow:   continue;
        }

        /* During the second pass, convert sand tiles to ground tiles and
           set terrain height. Also remove any shallow water tiles that are
           not next to shore. */
        hot = cold = temp = 0;
        for (i = 0; i < r_tiles_max; i++) {
                r_terrain_t terrain;

                /* Convert shallow water to deep water if not next to shore */
                if (r_tiles[i].terrain == R_T_SHALLOW) {
                        region_len = R_tile_region(i, region);
                        for (j = 0; j < region_len; j++) {
                                terrain = r_tiles[region[j]].terrain;
                                if (terrain == R_T_SAND)
                                        goto skip_deep;
                        }
                        r_tiles[i].terrain = R_T_WATER;
skip_deep:              continue;
                }

                /* Convert fully surrounded sand to ground */
                if (r_tiles[i].terrain != R_T_SAND)
                        continue;
                region_len = R_tile_region(i, region);
                for (j = 0; j < region_len; j++) {
                        terrain = r_tiles[region[j]].terrain;
                        if (terrain == R_T_SHALLOW || terrain == R_T_WATER ||
                            g_tiles[region[j]].island != g_tiles[i].island)
                                goto skip_sand;
                }
                r_tiles[i].terrain = choose_terrain(i);

                /* Give height to tile region */
                r_tiles[i].height = C_rand_real() * ISLAND_HEIGHT;

                /* Keep track of terrain statistics */
                if (r_tiles[i].terrain == R_T_GROUND)
                        temp++;
                else if (r_tiles[i].terrain == R_T_GROUND_HOT)
                        hot++;
                else if (r_tiles[i].terrain == R_T_GROUND_COLD)
                        cold++;
skip_sand:      continue;
        }

        /* Count failed islands */
        for (failed = 0, i = 0; i < g_islands_len; i++)
                if (g_islands[i].land < ISLAND_LAND)
                        failed++;
        C_debug("%d of %d islands succeeded",
                g_islands_len - failed, g_islands_len);

        /* During the third pass, smooth terrain height and remove failed
           islands*/
        for (i = 0; i < r_tiles_max; i++) {
                r_terrain_t terrain;
                float height;

                /* Remove this tile if it belongs to a failed island */
                if (g_tiles[i].island != G_ISLAND_INVALID &&
                    g_islands[g_tiles[i].island].land < ISLAND_LAND) {
                        r_tiles[i].terrain = R_T_WATER;
                        r_tiles[i].height = 0.f;
                        g_tiles[i].island = G_ISLAND_INVALID;
                        continue;
                }

                /* Smooth the height for ground tiles */
                terrain = R_terrain_base(r_tiles[i].terrain);
                if (terrain != R_T_GROUND)
                        continue;
                region_len = R_tile_region(i, region);
                for (j = 0, height = 0.f; j < region_len; j++)
                        height += r_tiles[region[j]].height;
                r_tiles[i].height = (r_tiles[i].height +
                                     height / region_len) / 2.f;
        }

        /* Output statistics */
        sand = land - hot - temp - cold;
        C_debug("%d land tiles (%d%%)", land,
                100 * land / (r_tiles_max - land));
        if (land)
                C_debug("%d sand (%d%%), %d temp (%d%%), %d hot (%d%%), "
                        "%d cold (%d%%)", sand, 100 * sand / land,
                        temp, 100 * temp / land, hot, 100 * hot / land,
                        cold, 100 * cold / land);
        for (i = 0; i < g_islands_len; i++)
                C_trace("Island %d, %d of %d land tiles (%d%%)",
                        i, g_islands[i].land, g_islands[i].tiles,
                        100 * g_islands[i].land / g_islands[i].tiles);
}

/******************************************************************************\
 Grows an island iteratively. Returns TRUE if an island was successfully
 created.
\******************************************************************************/
static bool grow_island(int i, int limit)
{
        int j, start, size, edges[ISLAND_SIZE];

        if (limit < ISLAND_LAND * 3)
                limit = ISLAND_LAND * 3;
        if (limit > ISLAND_SIZE)
                limit = ISLAND_SIZE;

        /* Initialize island structure */
        g_islands[i].tiles = 0;
        g_islands[i].land = 0;
        g_islands[i].town_tile = -1;

        /* Find an unused root tile */
        for (j = start = C_rand() % r_tiles_max; j < r_tiles_max; j++)
                if (g_tiles[j].island == G_ISLAND_INVALID)
                        goto grow;
        for (j = 0; j < start; j++)
                if (g_tiles[j].island == G_ISLAND_INVALID)
                        goto grow;
        return FALSE;

grow:   edges[0] = g_islands[i].root = j;
        for (size = 1; size && g_islands[i].tiles < limit;
             g_islands[i].tiles++) {
                int k, index, neighbors[3];

                index = C_rand() % size;
                R_tile_neighbors(edges[index], neighbors);
                for (j = 0; j < 3; j++) {

                        /* Valid edge tile? */
                        if (g_tiles[neighbors[j]].island != G_ISLAND_INVALID)
                                continue;

                        /* Add to the edges array if it isn't there already */
                        for (k = 0; edges[k] != neighbors[j]; k++)
                                if (k >= size - 1) {
                                        edges[size++] = neighbors[j];
                                        break;
                                }
                }
                r_tiles[edges[index]].terrain = R_T_SHALLOW;
                g_tiles[edges[index]].island = i;

                /* Consume edge */
                memmove(edges + index, edges + index + 1,
                        (--size - index) * sizeof (*edges));
        }
        return TRUE;
}

/******************************************************************************\
 Seeds the globe with at most [num] island seeds and interatively grows their
 edges until all space is consumed or all islands reach [island_size].
\******************************************************************************/
static void grow_islands(int num, int island_size, float variance)
{
        int limit;

        if (num < 1 || island_size < 1)
                return;
        if (num > G_ISLAND_NUM)
                num = G_ISLAND_NUM;
        if (island_size > ISLAND_SIZE)
                island_size = ISLAND_SIZE;
        C_debug("Growing %d, %d-tile islands", num, island_size);
        for (g_islands_len = 0; g_islands_len < num; g_islands_len++) {
                limit = (int)((1.f - variance * C_rand_real()) * island_size);
                if (!grow_island(g_islands_len, limit)) {
                        C_debug("Could only fit %d g_islands", g_islands_len);
                        break;
                }
        }
}

/******************************************************************************\
 One-time globe initialization. Call after rendering has been initialized.
\******************************************************************************/
void G_init_globe(void)
{
        /* Generate a starter globe */
        C_var_unlatch(&g_globe_subdiv4);
        C_var_unlatch(&g_island_num);
        C_var_unlatch(&g_island_size);
        C_var_unlatch(&g_island_variance);
        if (g_globe_subdiv4.value.n < 3)
                g_globe_subdiv4.value.n = 3;
        if (g_globe_subdiv4.value.n > 5)
                g_globe_subdiv4.value.n = 5;
        G_generate_globe(g_globe_subdiv4.value.n, g_island_num.value.n,
                         g_island_size.value.n, g_island_variance.value.f);
}

/******************************************************************************\
 Generate a new globe.
\******************************************************************************/
void G_generate_globe(int subdiv4, int override_islands, int override_size,
                      float override_variance)
{
        float variance;
        int i, islands, island_size;

        C_status("Generating globe");
        C_var_unlatch(&g_globe_seed);
        G_cleanup_tiles();
        R_generate_globe(subdiv4);
        C_rand_seed(g_globe_seed.value.n);

        /* Initialize tile structures */
        for (i = 0; i < r_tiles_max; i++) {
                r_tiles[i].terrain = R_T_WATER;
                r_tiles[i].height = 0;
                g_tiles[i].island = G_ISLAND_INVALID;
                g_tiles[i].ship = NULL;
        }

        /* Grow the islands and set terrain. Globe size affects the island
           growth parameters. */
        switch (subdiv4) {
        case 5: islands = 80;
                island_size = 256;
                variance = 0.3f;
                break;
        case 4: islands = 60;
                island_size = 220;
                variance = 0.2f;
                break;
        case 3: islands = 40;
                island_size = 140;
                variance = 0.f;
                break;
        default:
                C_warning("Invalid subdivision %d", subdiv4);
                g_islands_len = 0;
                return;
        }
        if (override_islands > 0)
                islands = override_islands;
        if (override_size > 0)
                island_size = override_size;
        if (override_variance >= 0.f)
                variance = override_variance;
        grow_islands(islands, island_size, variance);
        sanitise_terrain();

        /* This call actually raises the tiles to match terrain height */
        R_configure_globe();

        /* Deselect everything */
        g_hover_tile = g_selected_tile = -1;
        Py_CLEAR(g_hover_ship);
        Py_CLEAR(g_selected_ship);
}

/******************************************************************************\
 Returns TRUE if the point is within the visible globe hemisphere.
\******************************************************************************/
static bool is_visible(c_vec3_t origin)
{
        return C_vec3_dot(r_cam_forward, origin) < visible_range;
}

/******************************************************************************\
 Returns a modulating factor for fading models out of visible range.
\******************************************************************************/
static float model_fade_mod(c_vec3_t origin)
{
        float dist;

        dist = C_vec3_dot(r_cam_forward, origin);
        if (dist < visible_range - MODEL_FADE_DIST)
                return 1.f;
        if (dist > visible_range)
                return 0.f;
        return (visible_range - dist) / MODEL_FADE_DIST;
}

/******************************************************************************\
 Render a model on the globe.
\******************************************************************************/
static void render_globe_model(r_model_t *model)
{
        float mod;

        if (!model || !model->data ||
            (mod = model_fade_mod(model->origin)) <= 0.f)
                return;
        model->modulate.a = mod;
        R_adjust_light_for(model->origin);
        R_model_render(model);
}

/******************************************************************************\
 Renders the game-over overlay on the globe.
\******************************************************************************/
void G_render_game_over(void)
{
        static float fade;
        c_color_t color;

        /* Smoothly transition fade */
        if (g_game_over && !i_limbo) {
                if ((fade += c_frame_sec) > 1.f)
                        fade = 1.f;
        } else if ((fade -= c_frame_sec) < 0.f) {
                fade = 0.f;
                return;
        }

        color = r_fog_color;
        color.a *= 0.5f * fade;
        R_fill_screen(color);
}

/******************************************************************************\
 Render the globe and updates tile visibility.
\******************************************************************************/
void G_render_globe(void)
{
        g_building_t *building;
        int i;

        /* Set the invisible tile boundary */
        visible_range = -r_globe_radius + g_draw_distance.value.f;

        /* Render tile models */
//        R_start_globe();
        for (i = 0; i < r_tiles_max; i++) {
                g_tiles[i].visible = is_visible(r_tiles[i].origin);

                /* Render the tile's building */
                if ((building = g_tiles[i].building)) {
                        g_nation_name_t nation;

                        nation = g_tiles[i].building->nation;
                        if (nation != G_NN_NONE)
                                R_render_border(i, g_nations[nation].color,
                                               building->client != n_client_id);
                        render_globe_model(g_tiles[i].building->model);
                }

                /* Render the tile's gib */
                if (g_tiles[i].gib)
                        render_globe_model(g_tiles[i].gib->model);

                /* Render the tile's ship */
                if (g_tiles[i].ship)
                        render_globe_model(g_tiles[i].ship->model);
        }
//        R_finish_globe();

        /* Render a test line from the hover tile */
        if (g_test_globe.value.n && g_hover_tile >= 0) {
                c_vec3_t b;

                b = C_vec3_add(r_tiles[g_hover_tile].origin,
                               r_tiles[g_hover_tile].normal);
                R_render_test_line(r_tiles[g_hover_tile].origin, b,
                                   C_color(0.f, 1.f, 0.f, 1.f));
        }

//        G_render_ships();
//        render_game_over();
}

/******************************************************************************\
 Returns TRUE if the given ray intersects the tile.

 The algorithm used here projects the ray onto the triangle's plane and
 tests the barycentric coordinates of the intersection point to determine
 whether the triangle was hit. Source:
 http://www.devmaster.net/wiki/Ray-triangle_intersection
\******************************************************************************/
static int ray_intersects_tile(c_vec3_t o, c_vec3_t d, int tile)
{
        c_vec3_t p, triangle[3], normal;
        c_vec2_t q, b, c;
        float t, u, v, b_cross_c;
        int axis;

        R_tile_coords(tile, triangle);
        normal = r_tiles[tile].normal;

        /* Find [P], the ray's location in the triangle plane */
        o = C_vec3_sub(o, triangle[0]);
        t = C_vec3_dot(normal, o) / -C_vec3_dot(normal, d);
        if (t <= 0.f)
                return FALSE;
        p = C_vec3_add(o, C_vec3_scalef(d, t));

        /* Project the points onto one axis to simplify calculations. Choose the
           dominant axis of the normal for numeric stability. */
        axis = C_vec3_dominant(normal);
        q = C_vec2_from_3(p, axis);
        b = C_vec2_from_3(C_vec3_sub(triangle[1], triangle[0]), axis);
        c = C_vec2_from_3(C_vec3_sub(triangle[2], triangle[0]), axis);

        /* Find the barycentric coordinates */
        b_cross_c = C_vec2_cross(b, c);
        u = C_vec2_cross(q, c) / b_cross_c;
        v = C_vec2_cross(q, b) / -b_cross_c;

        /* Check if the point is within triangle bounds */
        if (u >= 0.f && v >= 0.f && u + v <= 1.f) {
                if (g_test_globe.value.n) {
                        p = C_vec3_add(p, triangle[0]);
                        R_render_test_line(p, C_vec3_add(p, normal),
                                           C_color(1.f, 0.f, 0.f, 1.f));
                }
                return TRUE;
        }

        return FALSE;
}

/******************************************************************************\
 Call when we know the mouse ray missed without or after tracing it.
\******************************************************************************/
void G_mouse_ray_miss(void)
{
        G_tile_hover(-1);
}

/******************************************************************************\
 The mouse screen position is transformed into a ray with [origin] and
 [forward] vector, this function will find which tile (if any) the mouse is
 hovering over.
\******************************************************************************/
void G_mouse_ray(c_vec3_t origin, c_vec3_t forward)
{
        float tile_z, z;
        int i, tile;

        /* We can quit early if the hover tile is still being hovered over */
        if (g_hover_tile >= 0 && g_tiles[g_hover_tile].visible &&
            ray_intersects_tile(origin, forward, g_hover_tile)) {
                G_tile_hover(g_hover_tile);
                return;
        }

        /* Iterate over all visible tiles to find the selected tile */
        for (i = 0, tile = -1, tile_z = 0.f; i < r_tiles_max; i++) {
                if (!g_tiles[i].visible ||
                    !ray_intersects_tile(origin, forward, i))
                        continue;

                /* Make sure we only select the closest match */
                z = C_vec3_dot(r_cam_forward, r_tiles[i].origin);
                if (z < tile_z) {
                        tile = i;
                        tile_z = z;
                }
        }

        G_tile_hover(tile);
}

