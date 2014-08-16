/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Implements 2D sprites and text rendering with the SDL TTF library. This
   should be the only file that calls functions from the library. */

#include "r_common.h"

/******************************************************************************\
 Initialize a sprite structure with a preloaded texture. 2D mode textures are
 expected to be at the maximum pixel scale (2x).
\******************************************************************************/
void R_sprite_init(r_sprite_t *sprite, r_texture_t *texture)
{
        if (!sprite)
                return;
        C_zero(sprite);
        sprite->modulate = C_color(1., 1., 1., 1.);
        if (!texture || !texture->surface)
                return;
        R_texture_ref(texture);
        sprite->texture = texture;
        sprite->size.x = texture->surface->w / R_PIXEL_SCALE_MAX;
        sprite->size.y = texture->surface->h / R_PIXEL_SCALE_MAX;
}

/******************************************************************************\
 Initialize a sprite with an image loaded from disk.
\******************************************************************************/
void R_sprite_load(r_sprite_t *sprite, const char *filename)
{
        r_texture_t *texture;

        if (!sprite)
                return;
        C_zero(sprite);
        if (!filename || !filename[0])
                return;
        texture = R_texture_load(filename, FALSE);
        R_sprite_init(sprite, texture);
        R_texture_free(texture);
}

/******************************************************************************\
 Initialize a sprite structure.
\******************************************************************************/
void R_sprite_cleanup(r_sprite_t *sprite)
{
        if (!sprite)
                return;
        R_texture_free(sprite->texture);
        C_zero(sprite);
}

/******************************************************************************\
 Sets up for a render of a 2D sprite. Returns FALSE if the sprite cannot
 be rendered.
\******************************************************************************/
static int sprite_render_start(const r_sprite_t *sprite)
{
        if (!sprite || !sprite->texture || sprite->z > 0.f ||
            sprite->modulate.a <= 0.f)
                return FALSE;
        R_push_mode(R_MODE_2D);
        R_texture_select(sprite->texture);

        /* Modulate color */
        glColor4f(sprite->modulate.r, sprite->modulate.g,
                  sprite->modulate.b, sprite->modulate.a);
        if (sprite->modulate.a < 1.f)
                glEnable(GL_BLEND);

        /* If z-offset is enabled (non-zero), depth test the sprite */
        if (sprite->z < 0.f)
                glEnable(GL_DEPTH_TEST);

        /* Setup transformation matrix */
        glPushMatrix();
        glTranslatef(sprite->origin.x + sprite->size.x / 2,
                     sprite->origin.y + sprite->size.y / 2, sprite->z);
        glRotatef(C_rad_to_deg(sprite->angle), 0.0, 0.0, 1.0);

        return TRUE;
}

/******************************************************************************\
 Cleans up after rendering a 2D sprite.
\******************************************************************************/
static void sprite_render_finish(void)
{
        glDisable(GL_DEPTH_TEST);
        glColor4f(1.f, 1.f, 1.f, 1.f);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        glPopMatrix();
        R_check_errors();
        R_pop_mode();
}

/******************************************************************************\
 Renders a 2D textured quad on screen. If we are rendering a rotated sprite
 that doesn't use alpha, this function will draw an anti-aliased border for it.
 The coordinates work here like you would expect for 2D, the origin is in the
 upper left of the sprite and y decreases down the screen.

 The vertices are arranged in the following order:

   0---3
   |   |
   1---2

\******************************************************************************/
void R_sprite_render(const r_sprite_t *sprite)
{
        r_vertex2_t verts[4];
        c_vec2_t half;
        unsigned short indices[] = { 0, 1, 2, 3, 0 };

        if (!sprite_render_start(sprite))
                return;

        /* Render textured quad */
        half = C_vec2_divf(sprite->size, 2.f);
        if (sprite->unscaled)
                half = C_vec2_divf(half, r_scale_2d / 2.f);
        verts[0].co = C_vec3(-half.x, half.y, 0.f);
        verts[0].uv = C_vec2(0.f, 1.f);
        verts[1].co = C_vec3(-half.x, -half.y, 0.f);
        verts[1].uv = C_vec2(0.f, 0.f);
        verts[2].co = C_vec3(half.x, -half.y, 0.f);
        verts[2].uv = C_vec2(1.f, 0.f);
        verts[3].co = C_vec3(half.x, half.y, 0.f);
        verts[3].uv = C_vec2(1.f, 1.f);
        C_count_add(&r_count_faces, 2);
        glInterleavedArrays(R_VERTEX2_FORMAT, 0, verts);
        glDrawElements(GL_QUADS, 4, GL_UNSIGNED_SHORT, indices);

        /* Draw the edge lines to anti-alias non-alpha quads */
        if (!sprite->texture->alpha && sprite->angle != 0.f &&
            sprite->modulate.a == 1.f) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_BLEND);
                glDrawElements(GL_LINE_STRIP, 5, GL_UNSIGNED_SHORT, indices);
        }

        sprite_render_finish();
}

/******************************************************************************\
 Blits the entire [src] surface to dest at [x], [y] and applies a one-pixel
 shadow of [alpha] opacity. This function could be made more efficient by
 unrolling the loops a bit, but it is not worth the loss in readablitiy.
\******************************************************************************/
static void blit_shadowed(SDL_Surface *dest, SDL_Surface *src,
                          int to_x, int to_y, float alpha)
{
        float w1, w2;
        int y, x;

        if (SDL_LockSurface(dest) < 0) {
                C_warning("Failed to lock destination surface");
                return;
        }
        if (SDL_LockSurface(src) < 0) {
                C_warning("Failed to lock source surface");
                return;
        }
        if (r_scale_2d < 1.f) {
                alpha *= r_scale_2d;
                w1 = alpha;
        } else
                w1 = (2.f - r_scale_2d) * alpha;
        w2 = alpha - w1;
        for (y = 0; y < src->h + 2; y++)
                for (x = 0; x < src->w + 2; x++) {
                        c_color_t dc, sc;

                        dc = R_surface_get(dest, to_x + x, to_y + y);

                        /* Blit the source image pixel */
                        if (x < src->w && y < src->h) {
                                sc = R_surface_get(src, x, y);
                                dc = C_color_blend(dc, sc);
                                if (dc.a >= 1.f) {
                                        R_surface_put(dest, to_x + x,
                                                      to_y + y, dc);
                                        continue;
                                }
                        }

                        /* Blit the shadow from (x - 1, y - 1) */
                        if (x && y && w1 && x <= src->w && y <= src->h) {
                                sc = R_surface_get(src, x - 1, y - 1);
                                sc = C_color(0, 0, 0, sc.a * w1);
                                dc = C_color_blend(sc, dc);
                        }

                        /* Blit the shadow from (x - 2, y - 2) */
                        if (x > 1 && y > 1 && w2) {
                                sc = R_surface_get(src, x - 2, y - 2);
                                sc = C_color(0, 0, 0, sc.a * w2);
                                dc = C_color_blend(sc, dc);
                        }

                        R_surface_put(dest, to_x + x, to_y + y, dc);
                }
        SDL_UnlockSurface(dest);
        SDL_UnlockSurface(src);
}

/******************************************************************************\
 Setup the sprite for rendering the string. Note that sprite size will be
 reset if it is regenerated. Text can be wrapped (or not, set wrap to 0) and a
 shadow (of variable opacity) can be applied. [string] does need to persist
 after the function call.
\******************************************************************************/
void R_sprite_init_text(r_sprite_t *sprite, r_font_t font, float wrap,
                        float shadow, int invert, const char *string)
{
        r_texture_t *tex;
        int width, height;

        if (font < 0 || font >= R_FONTS)
                C_error("Invalid font index %d", font);
        if (!sprite)
                return;
        C_zero(sprite);
        if (!string || !string[0])
                return;

        tex = R_font_render(font, wrap, invert, string, &width, &height);

        if(!tex)
                return;

        R_texture_upload(tex);

        /* Text is actually just a sprite and after this function has finished
           the sprite itself can be manipulated as expected */
        R_sprite_init(sprite, NULL);
        sprite->texture = tex;
        sprite->size.x = width / r_scale_2d;
        sprite->size.y = height / r_scale_2d;
}

/******************************************************************************\
 Wrapper around the sprite text initializer. Avoids re-rendering the texture
 if the parameters have not changed.
\******************************************************************************/
void R_text_configure(r_text_t *text, r_font_t font, float wrap, float shadow,
                      int invert, const char *string)
{
        if (text->font == font && text->wrap == wrap &&
            text->shadow == shadow && text->invert == invert &&
            r_scale_2d_frame < text->frame && !strcmp(string, text->buffer))
                return;
        R_sprite_cleanup(&text->sprite);
        R_sprite_init_text(&text->sprite, font, wrap, shadow, invert, string);
        text->frame = c_frame;
        text->font = font;
        text->wrap = wrap;
        text->shadow = shadow;
        text->invert = invert;
        C_strncpy_buf(text->buffer, string);
}

/******************************************************************************\
 Renders a text sprite. Will re-configure the text sprite when necessary.
\******************************************************************************/
void R_text_render(r_text_t *text)
{
        /* Pixel scale changes require re-initialization */
        if (r_scale_2d_frame > text->frame) {
                c_vec2_t origin;
                c_color_t modulate;

                origin = text->sprite.origin;
                modulate = text->sprite.modulate;
                R_sprite_cleanup(&text->sprite);
                R_sprite_init_text(&text->sprite, text->font, text->wrap,
                                   text->shadow, text->invert, text->buffer);
                text->sprite.origin = origin;
                text->sprite.modulate = modulate;
                text->frame = c_frame;
        }

        R_sprite_render(&text->sprite);
}

/******************************************************************************\
 Initializes a window sprite.
\******************************************************************************/
void R_window_init(r_window_t *window, r_texture_t *texture)
{
        if (!window)
                return;
        R_sprite_init(&window->sprite, texture);
        window->corner = C_vec2_divf(window->sprite.size, 4.f);
}

/******************************************************************************\
 Initializes a window sprite from disk.
\******************************************************************************/
void R_window_load(r_window_t *window, const char *filename)
{
        r_texture_t *texture;

        if (!window)
                return;
        texture = R_texture_load(filename, FALSE);
        if (!texture) {
                C_zero(window);
                return;
        }
        R_window_init(window, texture);
        R_texture_free(texture);
}

/******************************************************************************\
 Renders a window sprite. A window sprite is composed of a grid of nine quads
 where the corner quads have a fixed size and the connecting quads stretch to
 fill the rest of the sprite size.

 The array is indexed this way:

   0---2------4---6
   |   |      |   |
   1---3------5---7
   |   |      |   |
   |   |      |   |
   11--10-----9---8
   |   |      |   |
   12--13-----14--15

 Note that unlike normal sprites, no edge anti-aliasing is done as it is
 assumed that all windows will make use of alpha transparency.
\******************************************************************************/
void R_window_render(r_window_t *window)
{
        r_vertex2_t verts[16];
        c_vec2_t mid_half, mid_uv, corner;
        unsigned short indices[] = {0, 1, 3, 2,      2, 3, 5, 4,
                                    4, 5, 7, 6,      1, 11, 10, 3,
                                    3, 10, 9, 5,     5, 9, 8, 7,
                                    11, 12, 13, 10,  10, 13, 14, 9,
                                    9, 14, 15, 8};

        if (!window || !sprite_render_start(&window->sprite))
                return;

        /* If the window dimensions are too small to fit the corners in,
           we need to trim the corner size a little */
        corner = window->corner;
        mid_uv = C_vec2(0.25f, 0.25f);
        if (window->sprite.size.x <= corner.x * 2) {
                corner.x = window->sprite.size.x / 2;
                mid_uv.x *= corner.x / window->corner.x;
        }
        if (window->sprite.size.y <= corner.y * 2) {
                corner.y = window->sprite.size.y / 2;
                mid_uv.y *= corner.y / window->corner.y;
        }

        mid_half = C_vec2_divf(window->sprite.size, 2.f);
        mid_half = C_vec2_sub(mid_half, corner);
        verts[0].co = C_vec3(-corner.x - mid_half.x,
                             -corner.y - mid_half.y, 0.f);
        verts[0].uv = C_vec2(0.00f, 0.00f);
        verts[1].co = C_vec3(-corner.x - mid_half.x, -mid_half.y, 0.f);
        verts[1].uv = C_vec2(0.00f, mid_uv.y);
        verts[2].co = C_vec3(-mid_half.x, -corner.y - mid_half.y, 0.f);
        verts[2].uv = C_vec2(mid_uv.x, 0.00f);
        verts[3].co = C_vec3(-mid_half.x, -mid_half.y, 0.f);
        verts[3].uv = C_vec2(mid_uv.x, mid_uv.y);
        verts[4].co = C_vec3(mid_half.x, -corner.y - mid_half.y, 0.f);
        verts[4].uv = C_vec2(1.f - mid_uv.x, 0.00f);
        verts[5].co = C_vec3(mid_half.x, -mid_half.y, 0.f);
        verts[5].uv = C_vec2(1.f - mid_uv.x, mid_uv.y);
        verts[6].co = C_vec3(corner.x + mid_half.x,
                             -corner.y - mid_half.y, 0.f);
        verts[6].uv = C_vec2(1.00f, 0.00f);
        verts[7].co = C_vec3(corner.x + mid_half.x, -mid_half.y, 0.f);
        verts[7].uv = C_vec2(1.00f, mid_uv.y);
        verts[8].co = C_vec3(corner.x + mid_half.x, mid_half.y, 0.f);
        verts[8].uv = C_vec2(1.00f, 1.f - mid_uv.y);
        verts[9].co = C_vec3(mid_half.x, mid_half.y, 0.f);
        verts[9].uv = C_vec2(1.f - mid_uv.x, 1.f - mid_uv.y);
        verts[10].co = C_vec3(-mid_half.x, mid_half.y, 0.f);
        verts[10].uv = C_vec2(mid_uv.x, 1.f - mid_uv.y);
        verts[11].co = C_vec3(-corner.x - mid_half.x, mid_half.y, 0.f);
        verts[11].uv = C_vec2(0.00f, 1.f - mid_uv.y);
        verts[12].co = C_vec3(-corner.x - mid_half.x,
                              corner.y + mid_half.y, 0.f);
        verts[12].uv = C_vec2(0.00f, 1.00f);
        verts[13].co = C_vec3(-mid_half.x, corner.y + mid_half.y, 0.f);
        verts[13].uv = C_vec2(mid_uv.x, 1.00f);
        verts[14].co = C_vec3(mid_half.x, corner.y + mid_half.y, 0.f);
        verts[14].uv = C_vec2(1.f - mid_uv.x, 1.00f);
        verts[15].co = C_vec3(corner.x + mid_half.x,
                              corner.y + mid_half.y, 0.f);
        verts[15].uv = C_vec2(1.00f, 1.00f);

        C_count_add(&r_count_faces, 18);
        glInterleavedArrays(R_VERTEX2_FORMAT, 0, verts);
        glDrawElements(GL_QUADS, 36, GL_UNSIGNED_SHORT, indices);

        sprite_render_finish();
}

/******************************************************************************\
 Initializes a billboard point sprite.
\******************************************************************************/
void R_billboard_init(r_billboard_t *bb, r_texture_t *texture)
{
        R_sprite_init(&bb->sprite, texture);
        bb->size = (bb->sprite.size.x + bb->sprite.size.y) / 2;
        bb->world_origin = C_vec3(0.f, 0.f, 0.f);
}

/******************************************************************************\
 Initialize a billboard point sprite with an image loaded from disk.
\******************************************************************************/
void R_billboard_load(r_billboard_t *bb, const char *filename)
{
        r_texture_t *texture;

        if (!bb)
                return;
        C_zero(bb);
        if (!filename || !filename[0])
                return;
        texture = R_texture_load(filename, FALSE);
        R_billboard_init(bb, texture);
        R_texture_free(texture);
}

/******************************************************************************\
 Render a billboard point sprite.
\******************************************************************************/
void R_billboard_render(r_billboard_t *bb)
{
        c_vec3_t co;
        float size;

        if (!bb)
                return;

        /* Scale the size by how far away it is from the camera */
        size = bb->size;
        if (bb->z_scale)
                size /= C_vec3_len(C_vec3_sub(r_cam_origin, bb->world_origin));

        /* If the point sprite extension is available we can use it instead
           of doing all of the math on the CPU */
        if (r_ext.point_sprites) {
                R_push_mode(R_MODE_3D);
                R_texture_select(bb->sprite.texture);

                /* Modulate color */
                glColor4f(bb->sprite.modulate.r, bb->sprite.modulate.g,
                          bb->sprite.modulate.b, bb->sprite.modulate.a);
                if (bb->sprite.modulate.a < 1.f)
                        glEnable(GL_BLEND);

                /* Render point sprite */
                glPointSize(size);
                glBegin(GL_POINTS);
                glVertex3f(bb->world_origin.x, bb->world_origin.y,
                           bb->world_origin.z);
                glEnd();

                R_pop_mode();
                return;
        }

        /* Push the origin point through the transformation matrices */
        co = R_project(bb->world_origin);
        if (co.z >= 0.f)
                return;

        /* Render the billboard as a regular sprite but pushed back in the z
           direction and with depth testing */
        bb->sprite.origin.x = co.x - bb->sprite.size.x * 0.5f;
        bb->sprite.origin.y = co.y - bb->sprite.size.y * 0.5f;
        bb->sprite.size.y = bb->sprite.size.x = size / r_scale_2d;
        bb->sprite.z = co.z;
        R_sprite_render(&bb->sprite);
}

/******************************************************************************\
 Fill the screen with a color.
\******************************************************************************/
void R_fill_screen(c_color_t color)
{
        r_sprite_t sprite;

        if (color.a < 0.f)
                return;
        R_sprite_init(&sprite, r_white_tex);
        sprite.modulate = color;
        sprite.size = C_vec2(r_width_2d, r_height_2d);
        R_sprite_render(&sprite);
        R_sprite_cleanup(&sprite);
}

