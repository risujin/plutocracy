/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "i_common.h"

/* Debug output level (0-2) */
c_var_t i_debug;

/* Theme variables */
c_var_t i_border, i_color, i_color_alt, i_color_bad, i_color_good,
        i_color_gui, i_fade, i_shadow, i_theme;

/* Interface usability variables */
c_var_t i_edge_scroll, i_scroll_speed, i_zoom_speed;

/* Input parameters */
c_var_t i_ip;

/* Interface test variables */
c_var_t i_test_globe;

/******************************************************************************\
 Registers interface namespace variables.
\******************************************************************************/
void I_register_variables(void)
{
        C_register_integer(&i_debug, "i_debug", FALSE,
                           "interface event traces: 1 = SDL, 2 = widgets");
        i_debug.edit = C_VE_ANYTIME;

        /* Theme variables */
        C_register_integer(&i_border, "i_border", 8,
                           "pixel width of borders in interface");
        i_border.archive = FALSE;
        C_register_string(&i_color, "i_color", "aluminium1",
                          "interface text color");
        i_color.archive = FALSE;
        C_register_string(&i_color_alt, "i_color_alt", "butter1",
                          "interface alternate text color");
        i_color_alt.archive = FALSE;
        C_register_string(&i_color_bad, "i_color_bad", "#fca64f",
                          "interface 'bad' text color");
        i_color_bad.archive = FALSE;
        C_register_string(&i_color_good, "i_color_good", "#d9fc4f",
                          "interface 'good' text color");
        i_color_good.archive = FALSE;
        C_register_string(&i_color_gui, "i_color_gui", "#c0729fcf",
                          "interface color to match GUI");
        i_color_gui.archive = FALSE;
        C_register_float(&i_fade, "i_fade", 4.f,
                         "rate of fade in and out for widgets");
        i_fade.archive = FALSE;
        i_fade.edit = C_VE_ANYTIME;
        C_register_string(&i_theme, "i_theme", "default",
                          "name of interface theme");
        C_register_float(&i_shadow, "i_shadow", 1.f,
                         "darkness of interface shadows: 0-1.");
        i_shadow.archive = FALSE;

        /* Interface usability variables */
        C_register_float(&i_scroll_speed, "i_scroll_speed", 100.f,
                         "globe key scrolling speed in game units");
        i_scroll_speed.edit = C_VE_ANYTIME;
        C_register_float(&i_zoom_speed, "i_zoom_speed", 1.f,
                         "globe zoom speed in units per click");
        i_zoom_speed.edit = C_VE_ANYTIME;
        C_register_float(&i_edge_scroll, "i_edge_scroll", 8.f,
                         "mouse scroll distance from edge of screen");
        i_edge_scroll.edit = C_VE_ANYTIME;

        /* Input parameters */
        C_register_string(&i_ip, "i_ip", "127.0.0.1",
                          "the last entered ip address");
        i_ip.edit = C_VE_ANYTIME;

        /* Interface test variables */
        C_register_integer(&i_test_globe, "i_test_globe", FALSE,
                           "test mouse position detection on the globe");
        i_test_globe.edit = C_VE_ANYTIME;
}

