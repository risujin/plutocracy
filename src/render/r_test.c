/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "r_common.h"

static r_model_t *test_model;
static r_billboard_t *test_sprites;
static r_text_t test_text;

/******************************************************************************\
 Cleanup test assets.
\******************************************************************************/
void R_free_test_assets(void)
{
        Py_CLEAR(test_model);
        if (test_sprites) {
                int i;

                for (i = 0; i < r_test_sprite_num.value.n; i++)
                        R_billboard_cleanup(test_sprites + i);
                C_free(test_sprites);
        }
        R_text_cleanup(&test_text);
}

/******************************************************************************\
 Reloads the test model when [r_test_model] changes value.
\******************************************************************************/
static int test_model_update(c_var_t *var, c_var_value_t value)
{
        Py_CLEAR(test_model);
        if (!value.s[0])
                return TRUE;
        test_model = R_model_init(value.s, FALSE);
        return (test_model) ? TRUE : FALSE;
}

/******************************************************************************\
 Reloads the testing sprite when [r_test_sprite] or [r_test_sprites] changes.
\******************************************************************************/
static int test_sprite_update(c_var_t *var, c_var_value_t value)
{
        r_texture_t *texture;
        int i;

        /* Cleanup old sprites */
        if (test_sprites) {
                for (i = 0; i < r_test_sprite_num.value.n; i++)
                        R_billboard_cleanup(test_sprites + i);
                C_free(test_sprites);
                test_sprites = NULL;
        }

        /* Create new sprites */
        var->value = value;
        if (r_test_sprite_num.value.n < 1 || !r_test_sprite.value.s[0])
                return TRUE;
        C_rand_seed((unsigned int)time(NULL));
        test_sprites = C_malloc(r_test_sprite_num.value.n *
                                sizeof (*test_sprites));
        texture = R_texture_load(r_test_sprite.value.s, TRUE);
        for (i = 0; i < r_test_sprite_num.value.n; i++) {
                c_vec3_t origin;

                R_billboard_init(test_sprites + i, texture);
                origin = C_vec3(r_globe_radius * (C_rand_real() - 0.5f),
                                r_globe_radius * (C_rand_real() - 0.5f),
                                r_globe_radius + 3.f);
                test_sprites[i].world_origin = origin;
                test_sprites[i].sprite.angle = C_rand_real();
        }
        R_texture_free(texture);

        return TRUE;
}

/******************************************************************************\
 Loads all the render testing assets.
\******************************************************************************/
void R_load_test_assets(void)
{
        /* Test model */
        C_var_unlatch(&r_test_model);
        if (*r_test_model.value.s)
                test_model = R_model_init(r_test_model.value.s, FALSE);
        r_test_model.edit = C_VE_FUNCTION;
        r_test_model.update = test_model_update;

        /* Spinning sprites */
        C_var_unlatch(&r_test_sprite);
        r_test_sprite.edit = C_VE_FUNCTION;
        r_test_sprite.update = test_sprite_update;
        C_var_update(&r_test_sprite_num, test_sprite_update);

        /* Spinning text */
        C_var_unlatch(&r_test_text);
        R_text_init(&test_text);
        if (r_test_text.value.s[0]) {
                R_text_configure(&test_text, R_FONT_CONSOLE, 100.f, 1.f,
                                 TRUE, r_test_text.value.s);
                test_text.sprite.origin = C_vec2(r_width_2d / 2.f,
                                                 r_height_2d / 2.f);
        }
}

/******************************************************************************\
 Render the test model.
\******************************************************************************/
static void render_test_model(void)
{
        if (!test_model || !test_model->data)
                return;
        R_push_mode(R_MODE_3D);
        r_mode_hold = TRUE;
        glClear(GL_DEPTH_BUFFER_BIT);

        /* Disable some default 3D mode features */
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glColor4f(1.f, 1.f, 1.f, 1.f);
        R_check_errors();

        /* Orient the model */
        test_model->normal = C_vec3(0.f, 1.f, 0.f);
        test_model->forward = C_vec3(cosf(c_time_msec / 5000.f),
                                    0.f, sinf(c_time_msec / 5000.f));

        /* Render the test model */
        test_model->origin.z = -7;
        R_model_render(test_model);

        r_mode_hold = FALSE;
        R_pop_mode();
}

/******************************************************************************\
 Render test sprites.
\******************************************************************************/
static void render_test_sprites(void)
{
        int i;

        if (!r_test_sprite.value.s[0] || r_test_sprite_num.value.n < 1)
                return;
        for (i = 0; i < r_test_sprite_num.value.n; i++) {
                R_billboard_render(test_sprites + i);
                test_sprites[i].sprite.angle += i * c_frame_sec /
                                                r_test_sprite_num.value.n;
        }
}

/******************************************************************************\
 Render test text.
\******************************************************************************/
static void render_test_text(void)
{
        if (!r_test_text.value.s[0])
                return;
        R_text_render(&test_text);
        test_text.sprite.angle += 0.5f * c_frame_sec;
}

/******************************************************************************\
 Render testing models, sprites, etc.
\******************************************************************************/
void R_render_tests(void)
{
        render_test_model();
        render_test_sprites();
        render_test_text();
}

/******************************************************************************\
 Render normals. Must be called within 3D mode.
\******************************************************************************/
void R_render_normals(int count, c_vec3_t *co, c_vec3_t *no, int stride)
{
        c_vec3_t co2;
        int i;

        if (!r_test_normals.value.n)
                return;
        R_gl_disable(GL_FOG);
        R_gl_disable(GL_LIGHTING);
        R_texture_select(NULL);
        glEnableClientState(GL_VERTEX_ARRAY);
        for (i = 0; i < count; i++) {
                co2 = C_vec3_add(*co, *no);
                glBegin(GL_LINE_STRIP);
                glColor4f(1.f, 1.f, 0.f, 1.f);
                glVertex3f(co->x, co->y, co->z);
                glColor4f(1.f, 0.f, 0.f, 1.f);
                glVertex3f(co2.x, co2.y, co2.z);
                glEnd();
                co = (c_vec3_t *)((char *)co + stride);
                no = (c_vec3_t *)((char *)no + stride);
        }
        glColor4f(1.f, 1.f, 1.f, 1.f);
        glDisableClientState(GL_VERTEX_ARRAY);
        R_gl_restore();
        R_check_errors();
}

/******************************************************************************\
 Draws a line in world space coordinates.
\******************************************************************************/
void R_render_test_line(c_vec3_t a, c_vec3_t b, c_color_t color)
{
        R_push_mode(R_MODE_3D);
        R_gl_disable(GL_FOG);
        R_gl_disable(GL_LIGHTING);
        R_texture_select(NULL);
        glEnableClientState(GL_VERTEX_ARRAY);
        glBegin(GL_LINE_STRIP);
        glColor4f(color.r * 0.25f, color.g * 0.25f, color.b * 0.25f, color.a);
        glVertex3f(a.x, a.y, a.z);
        glColor4f(color.r, color.g, color.b, color.a);
        glVertex3f(b.x, b.y, b.z);
        glEnd();
        glColor4f(1.f, 1.f, 1.f, 1.f);
        glDisableClientState(GL_VERTEX_ARRAY);
        R_check_errors();
        R_pop_mode();
}

