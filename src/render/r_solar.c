/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Sunlight, moonlight, stars, and atmospheric effects */

#include "r_common.h"

/* Distance of the solar objects from the center of the globe */
#define SOLAR_DISTANCE 350.f

/* Localized light parameters */
#define LIGHT_TRANSITION 8.f

/* Atmospheric fog and halo parameters */
#define FOG_DISTANCE 12.f
#define FOG_ZOOM_SCALE 0.8f
#define HALO_SEGMENTS 32
#define HALO_LOW_SCALE 0.995f
#define HALO_HIGH_SCALE 1.1f

/* Atmospheric halo vertex type */
#pragma pack(push, 4)
typedef struct halo_vertex {
        c_color_t color;
        c_vec3_t co;
} halo_vertex_t;
#pragma pack(pop)

/* The color of the atmospheric fog effect */
c_color_t r_fog_color;

float r_solar_angle;

static r_model_t *sky;
static r_billboard_t moon, sun;
static c_color_t moon_diffuse, moon_specular, sun_diffuse, sun_specular,
                 light_ambient;
static halo_vertex_t halo_verts[2 + HALO_SEGMENTS * 2];

/******************************************************************************\
 Initializes light-related variables.
\******************************************************************************/
void R_init_solar(void)
{
        C_status("Initializing solar objects");

        /* Update light colors */
        C_var_update_data(&r_sun_diffuse, C_color_update, &sun_diffuse);
        C_var_update_data(&r_sun_specular, C_color_update, &sun_specular);
        C_var_update_data(&r_moon_diffuse, C_color_update, &moon_diffuse);
        C_var_update_data(&r_moon_specular, C_color_update, &moon_specular);
        C_var_update_data(&r_light_ambient, C_color_update, &light_ambient);

        /* Load solar object sprites */
        R_billboard_load(&moon, "models/solar/moon.png");
        R_billboard_load(&sun, "models/solar/sun.png");

        /* Sky is just a model */
        sky = R_model_init("models/solar/sky.plum", FALSE);
        sky->scale = 400.f;
}

/******************************************************************************\
 Cleanup light-related variables.
\******************************************************************************/
void R_cleanup_solar(void)
{
        R_billboard_cleanup(&moon);
        R_billboard_cleanup(&sun);
        Py_CLEAR(sky);
}

/******************************************************************************\
 Sets up the moon and sun lights.
\******************************************************************************/
void R_enable_light(void)
{
        c_color_t black;
        float sun_pos[4], moon_pos[4];

        if (!r_light.value.n)
                return;
        glEnable(GL_LIGHTING);
        glPushMatrix();
        glRotatef(C_rad_to_deg(r_solar_angle), 0.f, 1.f, 0.f);
        black = C_color(0.f, 0.f, 0.f, 0.f);

        /* Ambient light */
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, C_ARRAYF(light_ambient));

        /* Sunlight */
        sun_pos[0] = r_globe_radius + r_moon_height.value.f;
        sun_pos[1] = sun_pos[2] = sun_pos[3] = 0.f;
        glEnable(GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_POSITION, C_ARRAYF(sun_pos));
        glLightfv(GL_LIGHT0, GL_AMBIENT, C_ARRAYF(black));
        glLightfv(GL_LIGHT0, GL_DIFFUSE, C_ARRAYF(sun_diffuse));
        glLightfv(GL_LIGHT0, GL_SPECULAR, C_ARRAYF(sun_specular));

        /* Moonlight */
        moon_pos[0] = -sun_pos[0];
        moon_pos[1] = moon_pos[2] = 0.f;
        moon_pos[3] = 1.f;
        glEnable(GL_LIGHT1);
        glLightfv(GL_LIGHT1, GL_POSITION, C_ARRAYF(moon_pos));
        glLightfv(GL_LIGHT1, GL_AMBIENT, C_ARRAYF(black));
        glLightfv(GL_LIGHT1, GL_DIFFUSE, C_ARRAYF(moon_diffuse));
        glLightfv(GL_LIGHT1, GL_SPECULAR, C_ARRAYF(moon_specular));
        glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, r_moon_atten.value.f);
        glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.f);
        glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 0.f);

        glPopMatrix();
}

/******************************************************************************\
 Adjust the colors for one light.
\******************************************************************************/
static void adjust_light(int light, float dist,
                         c_color_t diffuse, c_color_t specular)
{
        float scale;

        scale = dist / LIGHT_TRANSITION + 0.5f;
        if (scale <= 0.f) {
                glDisable(light);
                return;
        }

        /* If the model is in the transition area, scale down the lights */
        if (scale <= 1.f) {
                diffuse = C_color_scalef(diffuse, scale);
                specular = C_color_scalef(specular, scale);
        }

        /* Setup OpenGL parameters */
        glEnable(light);
        glLightfv(light, GL_DIFFUSE, C_ARRAYF(diffuse));
        glLightfv(light, GL_SPECULAR, C_ARRAYF(specular));
}

/******************************************************************************\
 After light is enabled, modulates sunlight to prevent backlighting models on
 the night side of the globe.
\******************************************************************************/
void R_adjust_light_for(c_vec3_t origin)
{
        c_color_t ambient;
        float dist;

        if (!r_light.value.n)
                return;
        dist = C_vec3_dot(origin, sky->forward);

        /* Global as opposed to per-light ambient color is used. In order to
           fully light models close to the light source, we add some diffuse
           color to the ambient lighting. */
        ambient = dist >= 0.f ?
                  C_color_lerp(light_ambient, dist / r_globe_radius,
                               sun_diffuse) :
                  C_color_lerp(light_ambient, -dist / r_globe_radius,
                               moon_diffuse);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, C_ARRAYF(ambient));

        /* Setup per-light options */
        adjust_light(GL_LIGHT0, dist, sun_diffuse, sun_specular);
        adjust_light(GL_LIGHT1, -dist, moon_diffuse, moon_specular);
}

/******************************************************************************\
 Turns out the lights.
\******************************************************************************/
void R_disable_light(void)
{
        glDisable(GL_LIGHTING);
        glDisable(GL_LIGHT0);
        glDisable(GL_LIGHT1);
}

/******************************************************************************\
 Renders the background behind the globe.
\******************************************************************************/
void R_render_solar(void)
{
        if (!r_solar.value.n)
                return;
        if (r_solar.value.n != 2)
                r_solar_angle -= c_frame_sec * C_PI / 60.f / R_MINUTES_PER_DAY;
        sky->forward = C_vec3(cosf(-r_solar_angle), 0.f, sinf(-r_solar_angle));
        R_model_render(sky);

        /* Render the sun and moon point sprites */
        sun.world_origin.x = sky->forward.x * SOLAR_DISTANCE;
        sun.world_origin.z = sky->forward.z * SOLAR_DISTANCE;
        R_billboard_render(&sun);
        moon.world_origin.x = -sun.world_origin.x;
        moon.world_origin.z = -sun.world_origin.z;
        R_billboard_render(&moon);
}

/******************************************************************************\
 Update function for atmosphere color.
\******************************************************************************/
static int atmosphere_update(c_var_t *var, c_var_value_t value)
{
        int i;

        r_fog_color = C_color_string(value.s);
        for (i = 0; i <= HALO_SEGMENTS; i++)
                halo_verts[2 * i].color = r_fog_color;
        return TRUE;
}

/******************************************************************************\
 Generates the halo vertices.
\******************************************************************************/
void R_generate_halo(void)
{
        c_vec3_t unit;
        halo_vertex_t *vert;
        float angle;
        int i;

        for (i = 0; i <= HALO_SEGMENTS; i++) {
                angle = 2.f * C_PI * i / HALO_SEGMENTS;
                unit = C_vec3(cosf(angle), sinf(angle), 0.f);

                /* Bottom vertex */
                vert = halo_verts + 2 * i;
                vert->co = C_vec3_scalef(unit, HALO_LOW_SCALE);

                /* Top vertex */
                vert++;
                vert->color = C_color(0.f, 0.f, 0.f, 0.f);
                vert->co = C_vec3_scalef(unit, HALO_HIGH_SCALE);
        }
        C_var_update(&r_atmosphere, atmosphere_update);
}

/******************************************************************************\
 Renders the halo around the globe.
\******************************************************************************/
static void render_halo(void)
{
        float scale, dist, z;

        /* Find out how far away and how big the halo is */
        z = r_globe_radius + r_cam_zoom;
        dist = r_globe_radius * r_globe_radius / z;
        scale = sqrtf(r_globe_radius * r_globe_radius - dist * dist);

        /* Render the halo */
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(0, 0, -r_globe_radius - r_cam_zoom + dist);
        glScalef(scale, scale, scale);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, sizeof (*halo_verts), &halo_verts[0].co);
        glColorPointer(4, GL_FLOAT, sizeof (*halo_verts), &halo_verts[0].color);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 2 + 2 * HALO_SEGMENTS);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        glPopMatrix();
        glEnable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1.f, 1.f, 1.f, 1.f);

        R_check_errors();
        C_count_add(&r_count_faces, 2 * HALO_SEGMENTS);
}

/******************************************************************************\
 Enable the atmospheric fog effects and render the halo.
\******************************************************************************/
void R_start_atmosphere(void)
{
        float fog_start;

        if (r_fog_color.a <= 0.f)
                return;
        fog_start = r_cam_zoom * FOG_ZOOM_SCALE +
                    (1.f - r_fog_color.a) * r_globe_radius / 2;
        render_halo();
        glEnable(GL_FOG);
        glFogfv(GL_FOG_COLOR, C_ARRAYF(r_fog_color));
        glFogf(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, fog_start);
        glFogf(GL_FOG_END, fog_start + FOG_DISTANCE);
}

/******************************************************************************\
 Disable the atmospheric fog effects.
\******************************************************************************/
void R_finish_atmosphere(void)
{
        glDisable(GL_FOG);
}

