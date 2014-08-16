/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Implements the generation of the world globe */

#include "r_common.h"

/* Globe radius from the center to sea-level */
float r_globe_radius;

/* Number of tiles on the globe */
int r_tiles_max;

/* Tile vectors, terrain, height, etc */
r_tile_t r_tiles[R_TILES_MAX];

/* Globe tile vertices */
r_globe_vertex_t r_globe_verts[R_TILES_MAX * 3];

/* Vector buffer object containing globe vertices */
r_vbo_t r_globe_vbo;

/* Tiles below (exclusive) this tile index are flipped over the 0 vertex */
static int flip_limit;

/******************************************************************************\
 Space out the vertices at even distance from the sphere.
\******************************************************************************/
static void sphericize(void)
{
        c_vec3_t origin, co;
        float scale;
        int i;

        origin = C_vec3(0.f, 0.f, 0.f);
        for (i = 0; i < r_tiles_max * 3; i++) {
                co = r_globe_verts[i].v.co;
                scale = r_globe_radius / C_vec3_len(co);
                r_globe_verts[i].v.co = C_vec3_scalef(co, scale);
        }
}

/******************************************************************************\
 Subdivide each globe tile into four tiles. Partioned tile vertices are
 numbered in the following manner:

          3
         / \
        4---5

     6  2---1  9
    / \  \ /  / \
   7---8  0  10-11

 A vertex's neighbor is the vertex of the tile that shares the next counter-
 clockwise edge of the tile from that vertex.
\******************************************************************************/
static void subdivide4(void)
{
        c_vec3_t mid_0_1, mid_0_2, mid_1_2;
        r_globe_vertex_t *verts;
        int i, i_flip, j, n[3], n_flip[3];

        for (i = r_tiles_max - 1; i >= 0; i--) {
                verts = r_globe_verts + 12 * i;

                /* Determine which faces are flipped (over 0 vertex) */
                i_flip = i < flip_limit;
                for (j = 0; j < 3; j++) {
                        n[j] = r_globe_verts[3 * i + j].next / 3;
                        n_flip[j] = (n[j] < flip_limit) != i_flip;
                }

                /* Compute mid-point coordinates */
                mid_0_1 = C_vec3_add(r_globe_verts[3 * i].v.co,
                                     r_globe_verts[3 * i + 1].v.co);
                mid_0_1 = C_vec3_divf(mid_0_1, 2.f);
                mid_0_2 = C_vec3_add(r_globe_verts[3 * i].v.co,
                                     r_globe_verts[3 * i + 2].v.co);
                mid_0_2 = C_vec3_divf(mid_0_2, 2.f);
                mid_1_2 = C_vec3_add(r_globe_verts[3 * i + 1].v.co,
                                     r_globe_verts[3 * i + 2].v.co);
                mid_1_2 = C_vec3_divf(mid_1_2, 2.f);

                /* Bottom-right triangle */
                verts[9].v.co = mid_0_2;
                verts[9].next = 12 * i + 1;
                verts[10].v.co = mid_1_2;
                verts[10].next = 12 * n[1] + 8;
                verts[11].v.co = r_globe_verts[3 * i + 2].v.co;
                verts[11].next = 12 * n[2] + (n_flip[2] ? 7 : 3);

                /* Bottom-left triangle */
                verts[6].v.co = mid_0_1;
                verts[6].next = 12 * n[0] + (n_flip[0] ? 9 : 4);
                verts[7].v.co = r_globe_verts[3 * i + 1].v.co;
                verts[7].next = 12 * n[1] + 11;
                verts[8].v.co = mid_1_2;
                verts[8].next = 12 * i;

                /* Top triangle */
                verts[3].v.co = r_globe_verts[3 * i].v.co;
                verts[3].next = 12 * n[0] + (n_flip[0] ? 3 : 7);
                verts[4].v.co = mid_0_1;
                verts[4].next = 12 * i + 2;
                verts[5].v.co = mid_0_2;
                verts[5].next = 12 * n[2] + (n_flip[2] ? 4 : 9);

                /* Center triangle */
                verts[0].v.co = mid_1_2;
                verts[0].next = 12 * i + 10;
                verts[1].v.co = mid_0_2;
                verts[1].next = 12 * i + 5;
                verts[2].v.co = mid_0_1;
                verts[2].next = 12 * i + 6;
        }
        flip_limit *= 4;
        r_tiles_max *= 4;
        r_globe_radius *= 2;
        sphericize();
}

/******************************************************************************\
 Returns the [n]th vertex in the face, clockwise if positive or counter-
 clockwise if negative.
\******************************************************************************/
static int face_next(int vertex, int n)
{
        return 3 * (vertex / 3) + (3 + vertex + n) % 3;
}

/******************************************************************************\
 Finds vertex neighbors by iteration. Runs in O(n^2) time.
\******************************************************************************/
static void find_neighbors(void)
{
        int i, i_next, j, j_next;

        for (i = 0; i < r_tiles_max * 3; i++) {
                i_next = face_next(i, 1);
                for (j = 0; ; j++) {
                        if (j == i)
                                continue;
                        if (C_vec3_eq(r_globe_verts[i].v.co,
                                      r_globe_verts[j].v.co)) {
                                j_next = face_next(j, -1);
                                if (C_vec3_eq(r_globe_verts[i_next].v.co,
                                              r_globe_verts[j_next].v.co)) {
                                        r_globe_verts[i].next = j;
                                        break;
                                }
                        }
                        if (j >= r_tiles_max * 3)
                                C_error("Failed to find next vertex for "
                                        "vertex %d", i);
                }
        }
}

/******************************************************************************\
 Sets up a plain icosahedron.

 The icosahedron has 12 vertices: (0, ±1, ±φ) (±1, ±φ, 0) (±φ, 0, ±1)
 http://en.wikipedia.org/wiki/Icosahedron#Cartesian_coordinates

 We need to have duplicates however because we keep three vertices for each
 face, regardless of unique position because their UV coordinates will probably
 be different.
\******************************************************************************/
static void generate_icosahedron(void)
{
        int i, regular_faces[] = {

                /* Front faces */
                7, 5, 4,        5, 7, 0,        0, 2, 5,
                3, 5, 2,        2, 10, 3,       10, 2, 1,

                /* Rear faces */
                1, 11, 10,      11, 1, 6,       6, 8, 11,
                9, 11, 8,       8, 4, 9,        4, 8, 7,

                /* Top/bottom faces */
                0, 6, 1,        6, 0, 7,        9, 3, 10,       3, 9, 4,
        };

        flip_limit = 4;
        r_tiles_max = 20;
        r_globe_radius = sqrtf(C_TAU + 2);

        /* Flipped (over 0 vertex) face vertices */
        r_globe_verts[0].v.co = C_vec3(0, C_TAU, 1);
        r_globe_verts[1].v.co = C_vec3(-C_TAU, 1, 0);
        r_globe_verts[2].v.co = C_vec3(-1, 0, C_TAU);
        r_globe_verts[3].v.co = C_vec3(0, -C_TAU, 1);
        r_globe_verts[4].v.co = C_vec3(C_TAU, -1, 0);
        r_globe_verts[5].v.co = C_vec3(1, 0, C_TAU);
        r_globe_verts[6].v.co = C_vec3(0, C_TAU, -1);
        r_globe_verts[7].v.co = C_vec3(C_TAU, 1, 0);
        r_globe_verts[8].v.co = C_vec3(1, 0, -C_TAU);
        r_globe_verts[9].v.co = C_vec3(0, -C_TAU, -1);
        r_globe_verts[10].v.co = C_vec3(-C_TAU, -1, 0);
        r_globe_verts[11].v.co = C_vec3(-1, 0, -C_TAU);

        /* Regular face vertices */
        for (i = 12; i < r_tiles_max * 3; i++) {
                int index;

                index = regular_faces[i - 12];
                r_globe_verts[i].v.co = r_globe_verts[index].v.co;
        }

        find_neighbors();
}

/******************************************************************************\
 Generates the globe by subdividing an icosahedron and spacing the vertices
 out at the sphere's surface.
\******************************************************************************/
void R_generate_globe(int subdiv4)
{
        int i;

        if (subdiv4 < 0)
                subdiv4 = 0;
        else if (subdiv4 > R_SUBDIV4_MAX) {
                subdiv4 = R_SUBDIV4_MAX;
                C_warning("Too many subdivisions requested");
        }
        C_debug("Generating globe with %d subdivisions", subdiv4);
        memset(r_globe_verts, 0, sizeof (r_globe_verts));
        generate_icosahedron();
        for (i = 0; i < subdiv4; i++)
                subdivide4();

        /* Delete any old vertex buffers */
        R_vbo_cleanup(&r_globe_vbo);

        /* Maximum zoom distance is a function of the globe radius */
        r_zoom_max = r_globe_radius * R_ZOOM_MAX_SCALE;

        R_select_tile(-1, R_ST_NONE);
        R_generate_halo();
}

/******************************************************************************\
 Returns the vertices associated with a specific tile via [verts].
\******************************************************************************/
void R_tile_coords(int tile, c_vec3_t verts[3])
{
        verts[0] = r_globe_verts[3 * tile].v.co;
        verts[1] = r_globe_verts[3 * tile + 1].v.co;
        verts[2] = r_globe_verts[3 * tile + 2].v.co;
}

/******************************************************************************\
 Returns the tiles this tile shares a face with via [neighbors].
\******************************************************************************/
void R_tile_neighbors(int tile, int neighbors[3])
{
        neighbors[0] = r_globe_verts[3 * tile].next / 3;
        neighbors[1] = r_globe_verts[3 * tile + 1].next / 3;
        neighbors[2] = r_globe_verts[3 * tile + 2].next / 3;
}

/******************************************************************************\
 Returns the tiles this tile shares a vertex with via [neighbors]. Returns the
 number of entries used in the array.
\******************************************************************************/
int R_tile_region(int tile, int neighbors[12])
{
        int i, j, n, next_tile;

        for (n = i = 0; i < 3; i++) {
                next_tile = r_globe_verts[face_next(3 * tile + i, -1)].next / 3;
                for (j = r_globe_verts[3 * tile + i].next;
                     j / 3 != next_tile; j = r_globe_verts[j].next)
                        neighbors[n++] = j / 3;
        }
        return n;
}

/******************************************************************************\
 Returns the "geocentric" latitude (in radians) of the tile:
 http://en.wikipedia.org/wiki/Latitude
\******************************************************************************/
float R_tile_latitude(int tile)
{
        float center_y;

        center_y = (r_globe_verts[3 * tile].v.co.y +
                    r_globe_verts[3 * tile + 1].v.co.y +
                    r_globe_verts[3 * tile + 2].v.co.y) / 3.f;
        return asinf(center_y / r_globe_radius);
}

/******************************************************************************\
 Fills [verts] with pointers to the vertices that are co-located with [vert].
 Returns the number of entries that are used. The first vertex is always
 [vert].
\******************************************************************************/
static int vertex_indices(int vert, int verts[6])
{
        int i, pos;

        verts[0] = vert;
        pos = r_globe_verts[vert].next;
        for (i = 1; pos != vert; i++) {
                if (i > 6)
                        C_error("Vertex %d ring overflow", vert);
                verts[i] = pos;
                pos = r_globe_verts[pos].next;
        }
        return i;
}

/******************************************************************************\
 Sets the height of one tile.
\******************************************************************************/
static void set_tile_height(int tile, float height)
{
        c_vec3_t co;
        float dist;
        int i, j, verts[6], verts_len;

        for (i = 0; i < 3; i++) {
                verts_len = vertex_indices(3 * tile + i, verts);
                height = height / verts_len;
                for (j = 0; j < verts_len; j++) {
                        co = r_globe_verts[verts[j]].v.co;
                        dist = C_vec3_len(co);
                        co = C_vec3_scalef(co, (dist + height) / dist);
                        r_globe_verts[verts[j]].v.co = co;
                }
        }
}

/******************************************************************************\
 Smooth globe vertex normals.
\******************************************************************************/
static void smooth_normals(void)
{
        c_vec3_t vert_no, normal;
        int i, j, len;

        C_var_unlatch(&r_globe_smooth);
        if (r_globe_smooth.value.f <= 0.f)
                return;
        if (r_globe_smooth.value.f > 1.f)
                r_globe_smooth.value.f = 1.f;
        for (i = 0; i < r_tiles_max * 3; i++) {

                /* Compute the average normal for this point */
                normal = C_vec3(0.f, 0.f, 0.f);
                len = 0;
                j = r_globe_verts[i].next;
                while (j != i) {
                        normal = C_vec3_add(normal, r_globe_verts[j].v.no);
                        j = r_globe_verts[j].next;
                        len++;
                }
                normal = C_vec3_scalef(normal, r_globe_smooth.value.f / len);

                /* Set the normal for all vertices in the ring */
                j = r_globe_verts[i].next;
                while (j != i) {
                        vert_no = C_vec3_scalef(r_tiles[j / 3].normal,
                                                1.f - r_globe_smooth.value.f);
                        vert_no = C_vec3_add(vert_no, normal);
                        r_globe_verts[j].v.no = vert_no;
                        j = r_globe_verts[j].next;
                }
        }
}

/******************************************************************************\
 Called when [r_globe_smooth] changes.
\******************************************************************************/
static int globe_smooth_update(c_var_t *var, c_var_value_t value)
{
        int i;

        if (value.f > 0.f) {
                r_globe_smooth.value.f = value.f;
                smooth_normals();
        } else
                for (i = 0; i < r_tiles_max * 3; i++)
                        r_globe_verts[i].v.no = r_tiles[i / 3].normal;
        R_vbo_update(&r_globe_vbo);
        return TRUE;
}

/******************************************************************************\
 Returns the base terrain for a terrain variant.
\******************************************************************************/
r_terrain_t R_terrain_base(r_terrain_t terrain)
{
        switch (terrain) {
        case R_T_GROUND_HOT:
        case R_T_GROUND_COLD:
                return R_T_GROUND;
        case R_T_WATER:
        case R_T_SHALLOW:
                return R_T_SHALLOW;
        default:
                return terrain;
        }
}

/******************************************************************************\
 Returns a string containing the name of the terrain.
\******************************************************************************/
const char *R_terrain_to_string(r_terrain_t terrain)
{
        switch (terrain) {
        case R_T_GROUND_HOT:
                return "Tropical";
        case R_T_GROUND_COLD:
                return "Tundra";
        case R_T_GROUND:
                return "Temperate";
        case R_T_SAND:
                return "Sand";
        case R_T_WATER:
                return "Ocean";
        case R_T_SHALLOW:
                return "Shallow";
        default:
                return "Invalid";
        }
}

/******************************************************************************\
 Selects a terrain index for a tile depending on its region.
\******************************************************************************/
static int tile_terrain(int tile)
{
        int i, j, base, base_num, trans, terrain, vert_terrain[3], offset;

        base = R_terrain_base(r_tiles[tile].terrain);
        if (base >= R_T_BASES - 1)
                return r_tiles[tile].terrain;

        /* If we are not using transition tiles, just return the base */
        if (!r_globe_transitions.value.n)
                return base;

        /* Each tile vertex can only have up to two base terrains on it, we
           need to find out if the dominant one is present */
        base_num = 0;
        for (i = 0; i < 3; i++) {
                j = r_globe_verts[3 * tile + i].next;
                vert_terrain[i] = base;
                while (j != 3 * tile + i) {
                        terrain = R_terrain_base(r_tiles[j / 3].terrain);
                        if (terrain > vert_terrain[i])
                                trans = vert_terrain[i] = terrain;
                        j = r_globe_verts[j].next;
                }
                if (vert_terrain[i] == base)
                        base_num++;
        }
        if (base_num > 2)
                return r_tiles[tile].terrain;

        /* Swap the base and single terrain if the base only appears once */
        offset = 2 * base;
        if (base_num == 1) {
                trans = base;
                offset++;
        }

        /* Find the lone terrain and assign the transition tile */
        for (i = 0; i < 3; i++)
                if (vert_terrain[i] == trans)
                        break;

        /* Flipped tiles need reversing */
        if (tile < flip_limit) {
                if (i == 1)
                        i = 2;
                else if (i == 2)
                        i = 1;
        }

        return R_T_TRANSITION + offset * 3 + i;
}

/******************************************************************************\
 Computes tile vectors for the parameter array.
\******************************************************************************/
static void compute_tile_vectors(int i)
{
        c_vec3_t ab, ac;

        /* Set tile normal vector */
        ab = C_vec3_sub(r_globe_verts[3 * i].v.co,
                        r_globe_verts[3 * i + 1].v.co);
        ac = C_vec3_sub(r_globe_verts[3 * i].v.co,
                        r_globe_verts[3 * i + 2].v.co);
        r_tiles[i].normal = C_vec3_norm(C_vec3_cross(ab, ac));
        r_globe_verts[3 * i].v.no = r_tiles[i].normal;
        r_globe_verts[3 * i + 1].v.no = r_tiles[i].normal;
        r_globe_verts[3 * i + 2].v.no = r_tiles[i].normal;

        /* Centroid */
        r_tiles[i].origin = C_vec3_add(r_globe_verts[3 * i].v.co,
                                       r_globe_verts[3 * i + 1].v.co);
        r_tiles[i].origin = C_vec3_add(r_tiles[i].origin,
                                       r_globe_verts[3 * i + 2].v.co);
        r_tiles[i].origin = C_vec3_divf(r_tiles[i].origin, 3.f);

        /* Forward vector */
        r_tiles[i].forward = C_vec3_sub(r_globe_verts[3 * i].v.co,
                                        r_tiles[i].origin);
        r_tiles[i].forward = C_vec3_norm(r_tiles[i].forward);
}

/******************************************************************************\
 Adjusts globe vertices to show the tile's height. Updates the globe with data
 from the [r_tiles] array.
\******************************************************************************/
void R_configure_globe(void)
{
        c_vec2_t tile;
        float left, right, top, bottom, tmp;
        int i, tx, ty, terrain;

        C_debug("Configuring globe");
        C_var_unlatch(&r_globe_transitions);

        /* UV dimensions of tile boundary box */
        tile.x = 2.f * (r_terrain_tex->surface->w / R_TILE_SHEET_W) /
                 r_terrain_tex->surface->w;
        tile.y = 2.f * (int)(C_SIN_60 * r_terrain_tex->surface->h /
                             R_TILE_SHEET_H / 2) / r_terrain_tex->surface->h;

        for (i = 0; i < r_tiles_max; i++) {
                set_tile_height(i, r_tiles[i].height);

                /* Tile terrain texture */
                terrain = tile_terrain(i);
                ty = terrain / R_TILE_SHEET_W;
                tx = terrain - ty * R_TILE_SHEET_W;
                left = tx / 2 * tile.x + C_SIN_60 * R_TILE_BORDER;
                right = (tx / 2 + 1) * tile.x - C_SIN_60 * R_TILE_BORDER;
                if (tx & 1) {
                        bottom = ty * tile.y + C_SIN_30 * R_TILE_BORDER;
                        top = (ty + 1.f) * tile.y - C_SIN_60 * R_TILE_BORDER;
                        left += tile.x / 2.f;
                        right += tile.x / 2.f;
                } else {
                        top = ty * tile.y + R_TILE_BORDER;
                        bottom = (ty + 1.f) * tile.y - C_SIN_30 * R_TILE_BORDER;
                }

                /* Flip tiles are mirrored over the middle */
                if (i < flip_limit) {
                        tmp = left;
                        left = right;
                        right = tmp;
                }

                r_globe_verts[3 * i].v.uv = C_vec2((left + right) / 2.f, top);
                r_globe_verts[3 * i + 1].v.uv = C_vec2(left, bottom);
                r_globe_verts[3 * i + 2].v.uv = C_vec2(right, bottom);
        }
        for (i = 0; i < r_tiles_max; i++)
                compute_tile_vectors(i);
        smooth_normals();

        /* We can update normals dynamically from now on */
        r_globe_smooth.edit = C_VE_FUNCTION;
        r_globe_smooth.update = globe_smooth_update;

        /* If Vertex Buffer Objects are supported, upload the vertices now */
        R_vbo_cleanup(&r_globe_vbo);
        R_vbo_init(&r_globe_vbo, &r_globe_verts[0].v,
                   3 * r_tiles_max, sizeof (*r_globe_verts),
                   R_VERTEX3_FORMAT, NULL, 0);
}

/******************************************************************************\
 Returns TRUE if [terrain] is a ship-passable type.
\******************************************************************************/
int R_water_terrain(int terrain)
{
        return terrain == R_T_WATER || terrain == R_T_SHALLOW;
}

/******************************************************************************\
 Returns TRUE if there is a land bridge between [tile_a] and [tile_b].
\******************************************************************************/
int R_land_bridge(int tile_a, int tile_b)
{
        int i, vert, dir;

        /* Find which side the second tile is on */
        for (dir = 0; ; dir++) {
                if (dir >= 3)
                        C_error("Tiles %d and %d are not neighbors",
                                tile_a, tile_b);
                if (r_globe_verts[3 * tile_a + dir].next / 3 == tile_b)
                        break;
        }

        /* Check right vertex for land */
        vert = 3 * tile_a + dir;
        for (i = r_globe_verts[vert].next; i != vert;
             i = r_globe_verts[i].next)
                if (!R_water_terrain(r_tiles[i / 3].terrain))
                        goto next;
        return FALSE;

next:   /* Check left vertex for land */
        vert = face_next(3 * tile_a + dir, 1);
        for (i = r_globe_verts[vert].next; i != vert;
             i = r_globe_verts[i].next)
                if (!R_water_terrain(r_tiles[i / 3].terrain))
                        return TRUE;
        return FALSE;
}
/******************************************************************************\
 Tile wrapper object
\******************************************************************************/
typedef struct r_tile_wrapper {
        PyObject_HEAD;
        r_tile_t *tile;
} r_tile_wrapper_t;

/******************************************************************************\
 Traverse tile wrapper object
\******************************************************************************/
static int tile_wrapper_traverse(r_tile_wrapper_t *self, visitproc visit, void *arg) {
        return 0;
}

/******************************************************************************\
 Clear tile wrapper object
\******************************************************************************/
static int tile_wrapper_clear(r_tile_wrapper_t *self) {

        return 0;
}

/******************************************************************************\
 Dealloc tile wrapper object
\******************************************************************************/
static void tile_wrapper_dealloc(r_tile_wrapper_t* self) {
        tile_wrapper_clear(self);
        self->ob_type->tp_free((PyObject*)self);
}

/******************************************************************************\
 Alloc new tile wrapper object
\******************************************************************************/
PyObject *tile_wrapper_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
        r_tile_t *self;
        self = (r_tile_t *)type->tp_alloc(type, 0);
        return (PyObject *)self;
}

/******************************************************************************\
 Initialise tile wrapper object
\******************************************************************************/
int tile_wrapper_init(r_tile_wrapper_t *self , PyObject *args)
{
        const int tile;
        if(!PyArg_ParseTuple(args, "d", &tile))
        {
                return -1;
        }
        if(0 >= tile || r_tiles_max <= tile) {
                PyErr_Format(PyExc_IndexError,"Tile index out of range");
                return -1;
        }
        self->tile = &r_tiles[tile];
        return 0;
}

/******************************************************************************\
 tile object wrapper members
\******************************************************************************/
static PyMemberDef tile_wrapper_members[] =
{
 {NULL}  /* Sentinel */
};

/******************************************************************************\
 tile object wrapper methods
\******************************************************************************/
static PyMethodDef tile_wrapper_methods[] =
{
 {NULL}  /* Sentinel */
};

static PyGetSetDef tile_wrapper_getseters[] =
{
 {NULL}  /* Sentinel */
};

PyTypeObject R_tile_wrapper_type =
{
 PyObject_HEAD_INIT(NULL)
 0,                            /*ob_size*/
 "plutocracy.render.TileWrapper",    /*tp_name*/
 sizeof(r_tile_wrapper_t),     /*tp_basicsize*/
 0,                            /*tp_itemsize*/
 (destructor)tile_wrapper_dealloc,    /*tp_dealloc*/
 0,                            /*tp_print*/
 0,                            /*tp_getattr*/
 0,                            /*tp_setattr*/
 0,                            /*tp_compare*/
 0,                            /*tp_repr*/
 0,                            /*tp_as_number*/
 0,                            /*tp_as_sequence*/
 0,                            /*tp_as_mapping*/
 0,                            /*tp_hash */
 0,                            /*tp_call*/
 0,                            /*tp_str*/
 0,                            /*tp_getattro*/
 0,                            /*tp_setattro*/
 0,                            /*tp_as_buffer*/
 Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
 "TileWrapper",                /* tp_doc */
 (traverseproc)tile_wrapper_traverse, /* tp_traverse */
 (inquiry)tile_wrapper_clear,  /* tp_clear */
 0,                            /* tp_richcompare */
 0,                            /* tp_weaklistoffset */
 0,                            /* tp_iter */
 0,                            /* tp_iternext */
 tile_wrapper_methods,         /* tp_methods */
 tile_wrapper_members,         /* tp_members */
 tile_wrapper_getseters,       /* tp_getset */
 0,                            /* tp_base */
 0,                            /* tp_dict */
 0,                            /* tp_descr_get */
 0,                            /* tp_descr_set */
 0,                            /* tp_dictoffset */
 (initproc)tile_wrapper_init,  /* tp_init */
 0,                            /* tp_alloc */
 tile_wrapper_new,             /* tp_new */
};
