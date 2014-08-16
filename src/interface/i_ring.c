/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Implements ring menus, callable from within the game namespace */

#include "i_common.h"

/* Ring radius cannot be accurately determined from the graphic alone */
#define RING_OUTER_RADIUS 48.f
#define RING_INNER_RADIUS 17.f
#define RING_ICON_SIZE 32.f

/* Milliseconds to hover over a button before showing the details window */
#define DETAIL_HOVER_TIME 500

/* Details popup */
static i_window_t detail_window;
static i_label_t detail_title, detail_sub_title;
static char details[I_RING_ICONS][32], detail_subs[I_RING_ICONS][32];
static int detail_button, detail_time;

/* Ring widget */
static i_widget_t ring_widget;
static i_ring_f callback;
static i_button_t button_widgets[I_RING_ICONS];
static r_texture_t *ring_texture;
static r_sprite_t ring_sprite;
static c_vec2_t screen_origin;
static int buttons;
static int total_buttons;

/* Ring icon ids */
int i_ri_board;
int i_ri_follow;

/******************************************************************************\
 Position a ring on its world origin.
\******************************************************************************/
static void position_and_pack(void)
{
        c_vec2_t origin, size;
        float angle;
        int i, j;

        /* Position the ring */
        size = C_vec2_scalef(ring_widget.size, 0.5f);
        I_widget_move(&ring_widget, C_vec2_sub(screen_origin, size));

        /* Pack the visible buttons */
        for (j = i = 0; i < I_RING_ICONS; i++) {
                if (!button_widgets[i].widget.shown)
                        continue;
                angle = 2.f * C_PI * ((float)j++ / buttons - 0.25f);
                origin = C_vec2_scalef(C_vec2(cosf(angle), sinf(angle)),
                                       RING_INNER_RADIUS +
                                       RING_ICON_SIZE / 2.f);
                origin = C_vec2_add(screen_origin, origin);
                origin = C_vec2(origin.x - RING_ICON_SIZE / 2.f,
                                origin.y - RING_ICON_SIZE / 2.f);
                I_widget_move(&button_widgets[i].widget, origin);
        }
}

/******************************************************************************\
 Configure detail widgets.
\******************************************************************************/
static void detail_configure(void)
{
        /* Set label text */
        I_label_configure(&detail_title, details[detail_button]);
        I_label_configure(&detail_sub_title, detail_subs[detail_button]);

        /* Position and size the window */
        detail_window.widget.size = C_vec2(128.f, 0.f);
        detail_window.widget.origin = ring_widget.origin;
        detail_window.widget.origin.x += ring_widget.size.x / 2.f -
                                         detail_window.widget.size.x /
                                         2.f;
        detail_window.widget.origin.y -= i_border.value.n;
        I_widget_event(&detail_window.widget, I_EV_CONFIGURE);

        /* Color sub title depending on if the button is pressable */
        detail_sub_title.color = I_COLOR_GOOD;
        if (button_widgets[detail_button].widget.state == I_WS_DISABLED)
                detail_sub_title.color = I_COLOR_BAD;

        I_widget_event(&detail_window.widget, I_EV_SHOW);
}

/******************************************************************************\
 See if the hover timer expired and its time to show the detail window.
\******************************************************************************/
static void detail_update(void)
{
        if (!detail_time || detail_button < 0 ||
            c_time_msec < detail_time + DETAIL_HOVER_TIME)
                return;
        detail_time = 0;
        detail_configure();
}

/******************************************************************************\
 Find which button is being hovered over and configure the detail window.
\******************************************************************************/
static void detail_hover(void)
{
        int i;

        /* Find which button we are hovering over */
        for (i = 0; i < I_RING_ICONS; i++) {
                if (!button_widgets[i].widget.shown ||
                    !C_rect_contains(button_widgets[i].widget.origin,
                                     button_widgets[i].widget.size,
                                     i_mouse))
                        continue;

                /* Same button as before */
                if (detail_button == i)
                        return;

                /* Button changed, start a new hover timer */
                I_widget_event(&detail_window.widget, I_EV_HIDE);
                detail_button = i;
                detail_time = c_time_msec;
                return;
        }

        /* Missed all the buttons, close it */
        I_widget_event(&detail_window.widget, I_EV_HIDE);
        detail_button = -1;
        detail_time = 0;
}

/******************************************************************************\
 Ring widget event function.
\******************************************************************************/
static int ring_event(i_widget_t *widget, i_event_t event)
{
        float radius;


        switch (event) {
        case I_EV_CONFIGURE:
                R_sprite_cleanup(&ring_sprite);
                R_sprite_init(&ring_sprite, ring_texture);
                ring_widget.size = ring_sprite.size;
                I_widget_propagate(widget, I_EV_CONFIGURE);
                position_and_pack();
                return FALSE;
        case I_EV_MOVED:
                ring_sprite.origin = ring_widget.origin;
                break;
        case I_EV_MOUSE_OUT:
                I_close_ring();
                break;
        case I_EV_MOUSE_MOVE:
                /* Calculate mouse radius for mouse events */
                radius = C_vec2_len(C_vec2_sub(i_mouse, screen_origin));
                if (radius > RING_OUTER_RADIUS)
                        I_close_ring();
                else
                        detail_hover();
                break;
        case I_EV_MOUSE_DOWN:
                radius = C_vec2_len(C_vec2_sub(i_mouse, screen_origin));
                if (i_mouse_button != SDL_BUTTON_LEFT)
                        I_close_ring();
                if (radius <= RING_INNER_RADIUS) {
                        I_close_ring();
                        return FALSE;
                }
                break;
        case I_EV_CLEANUP:
                R_sprite_cleanup(&ring_sprite);
                break;
        case I_EV_RENDER:
                ring_sprite.modulate.a = ring_widget.fade;
                R_sprite_render(&ring_sprite);
                detail_update();
                break;
        default:
                break;
        }
        return TRUE;
}

/******************************************************************************\
 Called when a ring button is clicked.
\******************************************************************************/
static void button_clicked(i_button_t *button)
{
        I_close_ring();
        if (!callback)
                return;
        callback((i_ring_icon_t)(button - button_widgets));
}

/******************************************************************************\
 Initialize ring widgets and attach them to the root window.
\******************************************************************************/
void I_init_ring(void)
{
        int i, j, classes;
        ShipClass *sc;
        BuildingClass *bc;

        I_widget_init(&ring_widget, "Ring");
        ring_widget.event_func = (i_event_f)ring_event;
        ring_widget.state = I_WS_READY;
        ring_widget.shown = FALSE;

        i = 0;
        /* Generic buttons */
        I_button_init(button_widgets + total_buttons,
                      "gui/icons/ring/none.png", NULL, I_BT_ROUND);
        total_buttons++;
        I_button_init(button_widgets + total_buttons,
                      "gui/icons/ring/unknown.png", NULL, I_BT_ROUND);
        total_buttons++;
        I_button_init(button_widgets + total_buttons,
                      "gui/icons/ring/ship.png", NULL, I_BT_ROUND);
        /* Building buttons */
        classes = PyList_GET_SIZE(g_building_class_list);
        /* Go through the class list and add the buttons */
        for(j=0; j < classes; j++)
        {
                bc = (BuildingClass*)PyList_GET_ITEM(g_building_class_list, j);
                if(strcmp(bc->ring_icon, "") == 0) continue;
                total_buttons++;
                I_button_init(button_widgets + total_buttons, bc->ring_icon,
                              NULL, I_BT_ROUND);
                bc->ring_id = total_buttons;
        }
        total_buttons++;
        /* Ship interacions */
        I_button_init(button_widgets + total_buttons,
                      "gui/icons/ring/follow.png", NULL, I_BT_ROUND);
        i_ri_follow = total_buttons;
        total_buttons++;
        I_button_init(button_widgets + total_buttons,
                      "gui/icons/ring/board.png", NULL, I_BT_ROUND);
        i_ri_board = total_buttons;
        /* Tech preview buttons */
        classes = PyList_GET_SIZE(g_ship_class_list);
        /* Go through the class list and add the buttons */
        for(j=0; j < classes; j++)
        {
                sc = (ShipClass*)PyList_GET_ITEM(g_ship_class_list, j);
                total_buttons++;
                I_button_init(button_widgets + total_buttons, sc->ring_icon,
                              NULL, I_BT_ROUND);
                sc->ring_id = total_buttons;
        }

        /* Finish initializing the button widgets */
        for (i = 0; i <= total_buttons; i++) {
                I_widget_add(&ring_widget, &button_widgets[i].widget);
                button_widgets[i].on_click = (i_callback_f)button_clicked;
        }

        I_widget_add(&i_root, &ring_widget);

        /* Detail popup window */
        I_window_init(&detail_window);
        I_label_init(&detail_title, NULL);
        I_widget_add(&detail_window.widget, &detail_title.widget);
        I_label_init(&detail_sub_title, NULL);
        I_widget_add(&detail_window.widget, &detail_sub_title.widget);
        detail_window.widget.shown = FALSE;
        detail_window.fit = I_FIT_TOP;
        detail_window.popup = TRUE;
        I_widget_add(&i_root, &detail_window.widget);
}

/******************************************************************************\
 Show the ring UI centered on a world-space [origin].
\******************************************************************************/
void I_show_ring(i_ring_f _callback)
{
        int i;

        screen_origin = i_mouse;
        position_and_pack();
        I_widget_event(&ring_widget, I_EV_SHOW);
        callback = _callback;
        detail_button = -1;

        /* Set button widgets to activate on hover, they will clear this flag
           when they receive a mouse up event (from anywhere) */
        for (i = 0; i <= total_buttons; i++)
                button_widgets[i].hover_activate = TRUE;
}

/******************************************************************************\
 Close the open ring.
\******************************************************************************/
void I_close_ring(void)
{
        I_widget_event(&ring_widget, I_EV_HIDE);
        I_widget_event(&detail_window.widget, I_EV_HIDE);
        detail_button = -1;
        detail_time = 0;
}

/******************************************************************************\
 Reset ring contents.
\******************************************************************************/
void I_reset_ring(void)
{
        int i;

        for (i = 0; i <= total_buttons; i++) {
                I_widget_event(&button_widgets[i].widget, I_EV_HIDE);
                button_widgets[i].widget.fade = 0.f;
        }
        buttons = 0;
}

/******************************************************************************\
 Add a button to the ring.
\******************************************************************************/
void I_add_to_ring(i_ring_icon_t icon, int enabled, const char *title,
                   const char *sub)
{
        C_assert(icon >= 0 && icon < I_RING_ICONS);

        /* Show the button */
        if (!button_widgets[icon].widget.shown) {
                I_widget_event(&button_widgets[icon].widget, I_EV_SHOW);
                buttons++;
        }
        button_widgets[icon].widget.state = enabled ? I_WS_READY :
                                                      I_WS_DISABLED;

        /* Save the detail information */
        C_strncpy_buf(details[icon], title);
        C_strncpy_buf(detail_subs[icon], sub);
}

/******************************************************************************\
 Returns TRUE if the ring widget is showing.
\******************************************************************************/
int I_ring_shown(void)
{
        return ring_widget.shown;
}

/******************************************************************************\
 Initialize themeable ring assets.
\******************************************************************************/
void I_theme_ring(void)
{
        I_theme_texture(&ring_texture, "ring");
}

