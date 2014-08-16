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

/* Video parameters */
c_var_t r_width, r_height, r_color_bits, r_depth_bits, r_windowed, r_vsync,
        r_gamma, r_pixel_scale, r_clear, r_gl_errors, r_multisample;

/* Render testing */
c_var_t r_globe, r_test_normals, r_test_model, r_test_prerender, r_test_sprite,
        r_test_sprite_num, r_test_text, r_textures;

/* Effects parameters */
c_var_t r_atmosphere, r_globe_smooth, r_globe_transitions, r_model_lod;

/* Lighting parameters */
c_var_t r_globe_colors[3], r_globe_shininess, r_light, r_light_ambient,
        r_moon_atten, r_moon_diffuse, r_moon_height, r_moon_specular, r_solar,
        r_sun_diffuse, r_sun_specular;

/* Fonts */
c_var_t r_font_descriptions[R_FONTS];

/* Extension overrides */
c_var_t r_ext_point_sprites;

/* Screenshots */
c_var_t r_screenshots_dir;

/******************************************************************************\
 Registers the render namespace variables.
\******************************************************************************/
void R_register_variables(void)
{
        int i;

        /* Video parameters */
        C_register_integer(&r_width, "r_width", 800, "window width in pixels");
        r_width.unsafe = TRUE;
        C_register_integer(&r_height, "r_height", 600,
                           "window height in pixels");
        r_height.unsafe = TRUE;
        C_register_integer(&r_color_bits, "r_color_bits", 0,
                           "texture color depth: 16 or 32 bits");
        r_color_bits.unsafe = TRUE;
        C_register_integer(&r_depth_bits, "r_depth_bits", 16,
                           "depth buffer bits: 16 or 24 bits");
        r_depth_bits.unsafe = TRUE;
        C_register_integer(&r_windowed, "r_windowed", TRUE,
                           "1 = windowed mode, 0 = fullscreen");
        r_windowed.unsafe = TRUE;
        C_register_integer(&r_vsync, "r_vsync", TRUE, "enable vertical sync");
        C_register_float(&r_gamma, "r_gamma", 1, "brightness gamma correction");
        C_register_float(&r_pixel_scale, "r_pixel_scale", 0.f,
                         "scales the interface pixel unit");
        C_register_string(&r_clear, "r_clear", "", "background clear color");
        C_register_integer(&r_gl_errors, "r_gl_errors", 0,
                           "enable to quit if there is a GL error");
        r_gl_errors.edit = C_VE_ANYTIME;
        C_register_integer(&r_multisample, "r_multisample", 0,
                           "set to number of samples to enable FSAA");
        r_multisample.unsafe = TRUE;

        /* Render testing */
        C_register_string(&r_test_model, "r_test_model", "",
                          "path to PLUM model to test");
        C_register_integer(&r_test_prerender, "r_test_prerender", FALSE,
                           "display pre-rendered textures");
        C_register_string(&r_test_sprite, "r_test_sprite", "",
                          "path to test sprite texture");
        C_register_integer(&r_test_sprite_num, "r_test_sprites", 0,
                           "number of test sprites to show");
        C_register_string(&r_test_text, "r_test_text", "",
                          "text to test rendering");
        C_register_integer(&r_test_normals, "r_test_normals", 0,
                           "renders model and globe normals");
        r_test_normals.edit = C_VE_ANYTIME;
        C_register_integer(&r_globe, "r_globe", TRUE,
                           "disable to turn off globe rendering");
        r_globe.edit = C_VE_ANYTIME;
        C_register_integer(&r_textures, "r_textures", TRUE,
                           "disable to turn off textures");
        r_textures.edit = C_VE_ANYTIME;

        /* Visual effects parameters */
        C_register_float(&r_globe_smooth, "r_globe_smooth", 1.f,
                         "amount to smooth globe normals: 0.0-1.0");
        C_register_string(&r_atmosphere, "r_atmosphere",
                          "#c06080a0", "color of the atmosphere");
        C_register_integer(&r_globe_transitions, "r_globe_transitions", 1,
                           "use transition tiles");
        C_register_float(&r_model_lod, "r_model_lod", 1.f,
                         "model level-of-detail: 0.0-...");

        /* Lighting parameters */
        C_register_integer(&r_light, "r_light", TRUE,
                          "enable light from the sun and moon");
        r_light.edit = C_VE_ANYTIME;
        C_register_integer(&r_solar, "r_solar", TRUE,
                          "0 = no sky, 1 = rotating, 2 = static");
        r_solar.edit = C_VE_ANYTIME;
        C_register_integer(&r_globe_shininess, "r_globe_shininess", 64,
                          "globe specular shininess: 0-128");
        r_globe_shininess.edit = C_VE_ANYTIME;
        C_register_float(&r_moon_height, "r_moon_height", 32.f,
                         "height of the moon light above the globe");
        r_moon_height.edit = C_VE_ANYTIME;
        C_register_float(&r_moon_atten, "r_moon_atten", 0.0001f,
                         "moonlight quadratic attenuation");
        r_moon_atten.edit = C_VE_ANYTIME;
        C_register_string(r_globe_colors, "r_globe_ambient", "#808080",
                          "globe ambient light color");
        C_register_string(r_globe_colors + 1, "r_globe_diffuse", "#c0c0c0",
                          "globe diffuse light color");
        C_register_string(r_globe_colors + 2, "r_globe_specular", "#404040",
                          "globe specular light color");
        C_register_string(&r_light_ambient, "r_light_ambient", "#804020",
                          "universal ambient light color");
        C_register_string(&r_sun_diffuse, "r_sun_diffuse", "white",
                          "sun diffuse light color");
        C_register_string(&r_sun_specular, "r_sun_specular", "#606060",
                          "sun specular light color");
        C_register_string(&r_moon_diffuse, "r_moon_diffuse", "#001040",
                          "moon diffuse light color");
        C_register_string(&r_moon_specular, "r_moon_specular", "#000008",
                          "moon specular light color");

        /* Fonts */
        C_register_string(r_font_descriptions + R_FONT_CONSOLE, "r_font_console",
                          "DejaVu Sans Mono Bold 12",
                          "console font description string");
        C_register_string(r_font_descriptions + R_FONT_GUI, "r_font_gui",
                          "DejaVu Sans Bold 12",
                          "GUI font description string");
        C_register_string(r_font_descriptions + R_FONT_TITLE, "r_font_title",
                          "BlackChancery Ultra-Light 22",
                          "title font description string");
        C_register_string(r_font_descriptions + R_FONT_LCD, "r_font_lcd",
                          "LCD2 22",
                          "LCD font description string");
        for (i = 0; i < R_FONTS; i++) {
                r_font_descriptions[i].archive = FALSE;
        }

        /* Hardware point sprites are disabled by default because ATI drivers
           choke on them */
        C_register_integer(&r_ext_point_sprites, "r_ext_point_sprites", FALSE,
                           "0 = disable hardware point sprites");

        /* Screenshots Directory */
        C_register_string(&r_screenshots_dir, "r_screenshots_dir",
                          C_va("%s/screenshots", C_user_dir()),
                          "directory where screenshots are saved");
        r_screenshots_dir.edit = C_VE_ANYTIME;
}

