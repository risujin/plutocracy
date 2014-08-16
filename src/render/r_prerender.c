/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Contains routines for pre-rendering textures to the back buffer and reading
   them back in before the main rendering loop begins. Because of the special
   mode set for these operations, normal rendering calls should not be used. */

#include "r_common.h"

static r_vertex2_t verts[9];
static c_vec2_t tile, sheet;
static unsigned short indices[] = {0, 1, 2,  3, 4, 0,  0, 4, 1,  4, 5, 1,
                                   1, 5, 6,  1, 6, 2,  2, 6, 7,  8, 2, 7,
                                   0, 2, 8,  3, 0, 8};

/******************************************************************************\
 Finishes a buffer of pre-rendered textures. If testing is enabled, flips
 buffer to display the pre-rendered buffer and pauses in a fake event loop
 until Escape is pressed.
\******************************************************************************/
static void finish_buffer(void)
{
        SDL_Event ev;

        if (r_test_prerender.value.n) {
                SDL_GL_SwapBuffers();
                for (;;) {
                        while (SDL_PollEvent(&ev)) {
                                switch(ev.type) {
                                case SDL_KEYDOWN:
                                        if (ev.key.keysym.sym != SDLK_ESCAPE)
                                                goto done;
                                case SDL_QUIT:
                                        C_error("Interrupted during "
                                                "prerendering");
                                default:
                                        break;
                                }
                        }
                        C_throttle_fps();
                }
        }
done:   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        R_check_errors();
}

/******************************************************************************\
 Allocates a new texture and reads in into it a rectangle from the upper
 left-hand corner of the back buffer. You must manually upload the texture
 after calling this function.
\******************************************************************************/
static r_texture_t *save_buffer(int w, int h)
{
        r_texture_t *tex;

        tex = R_texture_alloc(w, h, TRUE);
        R_texture_screenshot(tex, 0, 0);
        return tex;
}

/******************************************************************************\
 Sets up the vertex coordinates and renders the current tile.
\******************************************************************************/
static void render_tile(int tx, int ty)
{
        glPushMatrix();
        glLoadIdentity();
        glTranslatef((tx / 2) * tile.x, ty * tile.y, 0.f);
        if (tx & 1) {
                glTranslatef(0.5f * tile.x, tile.y, 0.f);
                glScalef(1.f, -1.f, 1.f);
        }
        glInterleavedArrays(R_VERTEX2_FORMAT, 0, verts);
        glDrawElements(GL_TRIANGLES, 30, GL_UNSIGNED_SHORT, indices);

        /* The last tile in a row runs over onto the first tile so we need
           to render it there as well */
        if (tx == R_TILE_SHEET_W - 1) {
                glTranslatef(-sheet.x, 0.f, 0.f);
                glInterleavedArrays(R_VERTEX2_FORMAT, 0, verts);
                glDrawElements(GL_TRIANGLES, 30, GL_UNSIGNED_SHORT, indices);
        }

        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        glPopMatrix();
        R_check_errors();
}

/******************************************************************************\
 Sets up the vertex uv coordinates for rendering a tile mask to size.
\******************************************************************************/
static void setup_tile_uv_mask(void)
{
        verts[0].uv = C_vec2(0.50f, 0.01f);
        verts[1].uv = C_vec2(0.01f, 0.99f);
        verts[2].uv = C_vec2(0.99f, 0.99f);
        verts[3].uv = C_vec2(0.50f, 0.00f);
        verts[4].uv = C_vec2(0.00f, 0.50f);
        verts[5].uv = C_vec2(0.00f, 1.00f);
        verts[6].uv = C_vec2(0.50f, 1.00f);
        verts[7].uv = C_vec2(1.00f, 1.00f);
        verts[8].uv = C_vec2(1.00f, 0.50f);
}

/******************************************************************************\
 Sets up the vertex uv coordinates for rendering a tile.
\******************************************************************************/
static void setup_tile_uv(int rotate, int flip, int tx, int ty)
{
        c_vec2_t new_uv[9], size, shift;
        int i;

        /* Original rotation */
        for (i = 0; i < 9; i++) {
                new_uv[i] = C_vec2_div(C_vec2(verts[i].co.x,
                                              verts[i].co.y), tile);
                verts[i].uv = new_uv[i];
        }

        /* Rotate counter-clockwise */
        if (rotate > 0) {
                for (i = 0; i < 3; i++)
                        new_uv[i] = verts[(3 + i - rotate) % 3].uv;
                for (i = 3; i < 9; i++)
                        new_uv[i] = verts[3 + ((3 + i - 2 * rotate) % 6)].uv;
                for (i = 0; i < 9; i++)
                        verts[i].uv = new_uv[i];
        }

        /* Flip over a vertex */
        switch (flip) {
        default:
                break;

        case 0: /* Flip over top vertex */
                new_uv[4] = verts[8].uv;
                new_uv[8] = verts[4].uv;
                new_uv[1] = verts[2].uv;
                new_uv[2] = verts[1].uv;
                new_uv[5] = verts[7].uv;
                new_uv[7] = verts[5].uv;
                break;

        case 1: /* Flip over bottom-left vertex */
                new_uv[4] = verts[6].uv;
                new_uv[6] = verts[4].uv;
                new_uv[0] = verts[2].uv;
                new_uv[2] = verts[0].uv;
                new_uv[3] = verts[7].uv;
                new_uv[7] = verts[3].uv;
                break;

        case 2: /* Flip over bottom-right vertex */
                new_uv[5] = verts[3].uv;
                new_uv[3] = verts[5].uv;
                new_uv[1] = verts[0].uv;
                new_uv[0] = verts[1].uv;
                new_uv[6] = verts[8].uv;
                new_uv[8] = verts[6].uv;
                break;
        }
        if (tx < 0 || ty < 0)
                return;

        /* Shift and scale the uv coordinates to cover the tile */
        size = C_vec2_div(sheet, tile);
        shift = C_vec2_div(C_vec2(tile.x * (tx / 2), tile.y * ty), sheet);
        for (i = 0; i < 9; i++) {
                if (tx & 1) {
                        new_uv[i].x += 0.5f;
                        new_uv[i].y = 1.f - new_uv[i].y;
                }
                new_uv[i] = C_vec2_div(new_uv[i], size);
                verts[i].uv = C_vec2_add(new_uv[i], shift);
        }
}

/******************************************************************************\
 Pre-renders the extra rotations of the tile mask and the terrain tile sheets.
 This function will generate tile textures 1-3.
\******************************************************************************/
static void prerender_tiles(void)
{
        r_texture_t *blend_mask, *rotated_mask, *masked_terrain;
        int i, x, y;

        blend_mask = R_texture_load("models/globe/blend_mask.png", FALSE);
        if (!blend_mask || !r_terrain_tex)
                C_error("Failed to load essential prerendering assets");

        /* Render the blend mask to tile size */
        R_texture_select(blend_mask);
        setup_tile_uv_mask();
        render_tile(0, 0);
        R_texture_free(blend_mask);
        blend_mask = save_buffer((int)tile.x, (int)tile.y);
        R_texture_upload(blend_mask);
        finish_buffer();

        rotated_mask = NULL;
        for (i = 0; i < 3; i++) {

                /* Render the blend mask at tile size and orientation */
                R_texture_select(blend_mask);
                setup_tile_uv(i, -1, -1, -1);
                for (y = 0; y < R_TILE_SHEET_H; y++)
                        for (x = 0; x < R_TILE_SHEET_W; x++)
                                render_tile(x, y);
                rotated_mask = save_buffer((int)sheet.x, (int)sheet.y);
                R_texture_upload(rotated_mask);
                finish_buffer();

                /* Render the tiles at this orientation */
                R_texture_select(r_terrain_tex);
                for (y = 0; y < R_TILE_SHEET_H; y++)
                        for (x = 0; x < R_TILE_SHEET_W; x++) {
                                setup_tile_uv(0, i, x, y);
                                render_tile(x, y);
                        }

                /* Read the terrain back in and mask it */
                masked_terrain = save_buffer((int)sheet.x, (int)sheet.y);
                R_surface_mask(masked_terrain->surface, rotated_mask->surface);
                R_texture_free(rotated_mask);
                finish_buffer();

                /* Render the masked terrain on top of the terrain texture
                   and read it back in to replace the old terrain texture */
                R_texture_render(r_terrain_tex, 0, 0);
                R_texture_upload(masked_terrain);
                R_texture_render(masked_terrain, 0, 0);
                if (r_test_prerender.value.n)
                        R_texture_render(masked_terrain, (int)sheet.x, 0);
                R_texture_free(masked_terrain);
                R_texture_free(r_terrain_tex);
                r_terrain_tex = save_buffer((int)sheet.x, (int)sheet.y);
                R_texture_upload(r_terrain_tex);
                finish_buffer();
        }
        R_texture_free(blend_mask);
}

/******************************************************************************\
 Pre-renders tiles for transition between base tile types.
\******************************************************************************/
static void prerender_transitions(void)
{
        r_texture_t *trans_mask, *inverted_mask, *large_mask, *masked_terrain;
        int x, y, tiles_a[] = {0, 1, 1, 2}, tiles_b[] = {1, 0, 2, 1};

        trans_mask = R_texture_load("models/globe/trans_mask.png", FALSE);
        if (!trans_mask || !r_terrain_tex)
                C_error("Failed to load essential prerendering assets");

        /* Render an inverted version of the mask */
        inverted_mask = R_texture_clone(trans_mask);
        R_surface_flip_v(inverted_mask->surface);
        R_surface_invert(inverted_mask->surface, TRUE, FALSE);
        R_texture_upload(inverted_mask);
        R_texture_select(inverted_mask);
        setup_tile_uv_mask();
        render_tile(0, 0);
        R_texture_free(inverted_mask);
        inverted_mask = save_buffer((int)tile.x, (int)tile.y);
        R_texture_upload(inverted_mask);
        finish_buffer();

        /* Render the transition mask to tile size */
        R_texture_select(trans_mask);
        setup_tile_uv_mask();
        render_tile(0, 0);
        R_texture_free(trans_mask);
        trans_mask = save_buffer((int)tile.x, (int)tile.y);
        R_texture_upload(trans_mask);
        finish_buffer();

        /* Render the transition mask tile sheet */
        for (y = 0; y < R_T_BASES - 1; y++) {
                R_texture_select(trans_mask);
                for (x = 0; x < 3; x++) {
                        setup_tile_uv(x, -1, -1, -1);
                        render_tile(x, y + 1);
                }
                R_texture_select(inverted_mask);
                for (x = 3; x < 6; x++) {
                        setup_tile_uv(x - 3, -1, -1, -1);
                        render_tile(x, y + 1);
                }
        }
        large_mask = save_buffer((int)sheet.x, (int)sheet.y);
        R_texture_free(trans_mask);
        R_texture_free(inverted_mask);
        finish_buffer();

        /* Render the masked layer of the terrain and read it back in */
        R_texture_select(r_terrain_tex);
        for (y = 0; y < R_T_BASES - 1; y++)
                for (x = 0; x < 3; x++) {
                        setup_tile_uv(0, -1, tiles_b[2 * y], 0);
                        render_tile(x, y + 1);
                        setup_tile_uv(0, -1, tiles_b[2 * y + 1], 0);
                        render_tile(x + 3, y + 1);
                }
        masked_terrain = save_buffer((int)sheet.x, (int)sheet.y);
        R_surface_mask(masked_terrain->surface, large_mask->surface);
        R_texture_free(large_mask);
        R_texture_upload(masked_terrain);
        finish_buffer();
        if (r_test_prerender.value.n)
                R_texture_render(masked_terrain, (int)sheet.x, 0);

        /* Render the base layer of the terrain */
        R_texture_render(r_terrain_tex, 0, 0);
        R_texture_select(r_terrain_tex);
        for (y = 0; y < R_T_BASES - 1; y++)
                for (x = 0; x < 3; x++) {
                        setup_tile_uv(0, -1, tiles_a[2 * y], 0);
                        render_tile(x, y + 1);
                        setup_tile_uv(0, -1, tiles_a[2 * y + 1], 0);
                        render_tile(x + 3, y + 1);
                }

        /* Render the masked terrain over the tile sheet and read it back in */
        R_texture_render(masked_terrain, 0, 0);
        R_texture_free(masked_terrain);
        R_texture_free(r_terrain_tex);
        r_terrain_tex = save_buffer((int)sheet.x, (int)sheet.y);
        r_terrain_tex->mipmaps = TRUE;
        R_texture_upload(r_terrain_tex);
        finish_buffer();
}

/******************************************************************************\
 Renders the pre-render textures to the back buffer and reads them back in for
 later use. The tiles are rendered on this vertex arrangement:

          3
         /|\
        / | \
       4--0--8
      / \/ \/ \
     / .1---2. \
    /.^  \ /  ^.\
   5------6------7

\******************************************************************************/
void R_prerender(void)
{
        r_texture_t *generated_tex;

        C_status("Pre-rendering textures");

        /* Check if we have a cached version of the finished texture */
        generated_tex = R_texture_load("models/globe/terrain_full.png", TRUE);
        if (generated_tex) {
                R_texture_free(r_terrain_tex);
                r_terrain_tex = generated_tex;
                C_debug("Using existing terrain texture");
                return;
        }
        C_debug("Generating terrain texture");

        C_var_unlatch(&r_test_prerender);

        /* Initialize with a custom 2D mode */
        r_mode_hold = TRUE;
        glDisable(GL_CULL_FACE);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.f, r_width.value.n, r_height.value.n, 0.f, -1.f, 1.f);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        /* Clear to black */
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        R_check_errors();

        /* Vertices are aligned in the tile so that the top vertex of the
           triangle touches the top center of the tile */
        sheet.x = (GLfloat)r_terrain_tex->surface->w;
        sheet.y = (GLfloat)r_terrain_tex->surface->h;
        tile.x = 2.f * (r_terrain_tex->surface->w / R_TILE_SHEET_W);
        tile.y = 2.f * (int)(C_SIN_60 * r_terrain_tex->surface->h /
                             R_TILE_SHEET_H / 2);

        /* Setup even vertices */
        verts[0].co = C_vec3(tile.x / 2.f, sheet.y * R_TILE_BORDER, 0.f);
        verts[1].co = C_vec3(sheet.x * C_SIN_60 * R_TILE_BORDER,
                             tile.y - sheet.y * C_SIN_30 * R_TILE_BORDER, 0.f);
        verts[2].co = C_vec3(tile.x - sheet.x * C_SIN_60 * R_TILE_BORDER,
                             verts[1].co.y, 0.f);
        verts[3].co = C_vec3(tile.x / 2.f, 0.f, 0.f);
        verts[4].co = C_vec3(tile.x / 4.f, tile.y / 2.f, 0.f);
        verts[5].co = C_vec3(0.f, tile.y, 0.f);
        verts[6].co = C_vec3(verts[0].co.x, verts[5].co.y, 0.f);
        verts[7].co = C_vec3(tile.x, verts[5].co.y, 0.f);
        verts[8].co = C_vec3(tile.x * 3.f / 4.f, verts[4].co.y, 0.f);

        prerender_tiles();
        prerender_transitions();

        /* Save the resulting terrain texture */
        if (R_surface_save(r_terrain_tex->surface,
                           "models/globe/terrain_full.png"))
                C_debug("Saved generated texture");

        r_mode_hold = FALSE;
}

