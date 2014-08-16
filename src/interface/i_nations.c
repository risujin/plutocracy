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

static i_label_t title;
static i_button_t nation_buttons[G_NATION_NAMES];

/******************************************************************************\
 A nation button was clicked.
\******************************************************************************/
static void nation_clicked(i_button_t *button)
{
        I_widget_event(I_widget_top_level(&button->widget), I_EV_HIDE);
        G_change_nation((int)(button - nation_buttons));
}

/******************************************************************************\
 Set a nation button's enabled state.
\******************************************************************************/
void I_enable_nation(int nation, bool enable)
{
        if (enable) {
                if (nation_buttons[nation].widget.state == I_WS_DISABLED)
                        nation_buttons[nation].widget.state = I_WS_READY;
                return;
        }
        nation_buttons[nation].widget.state = I_WS_DISABLED;
}

/******************************************************************************\
 Disables one of the nation buttons. Pass G_NN_NONE to enable all buttons or a
 negative value to disable the nations button.
\******************************************************************************/
void I_select_nation(int nation)
{
        int i;

        if (nation >= 0) {
                for (i = 0; i < G_NATION_NAMES; i++)
                        I_enable_nation(i, TRUE);
                I_toolbar_enable(&i_right_toolbar, i_nations_button, TRUE);
        } else {
                I_toolbar_enable(&i_right_toolbar, i_nations_button, FALSE);
                return;
        }
        if (nation == G_NN_NONE || nation >= G_NATION_NAMES)
                return;
        I_enable_nation(nation, FALSE);
}

/******************************************************************************\
 Initializes nations window widgets on the given window.
\******************************************************************************/
void I_init_nations(i_window_t *window)
{
        const char *long_name;
        int i;

        I_window_init(window);
        window->widget.size = C_vec2(200.f, 0.f);
        window->fit = I_FIT_TOP;

        /* Title label */
        I_label_init(&title, C_str("i-nations", "Affiliation"));
        title.font = R_FONT_TITLE;
        I_widget_add(&window->widget, &title.widget);

        /* Setup nation buttons */
        for (i = G_NN_NONE + 1; i < G_NATION_NAMES; i++) {
                long_name = C_str(C_va("c-team-%s", g_nations[i].short_name),
                                  g_nations[i].long_name);
                I_button_init(nation_buttons + i,
                              C_va("gui/flags/%s.png", g_nations[i].short_name),
                              long_name, I_BT_DECORATED);
                nation_buttons[i].on_click = (i_callback_f)nation_clicked;
                if (i == G_NATION_NAMES - 1)
                        nation_buttons[i].widget.margin_front = 1.f;
                I_widget_add(&window->widget, &nation_buttons[i].widget);
        }
}

