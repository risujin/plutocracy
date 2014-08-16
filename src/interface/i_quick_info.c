/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Window to provide quick information about the current selection */

#include "i_common.h"

static i_label_t title_label;
static i_box_t title_box;
static i_button_t zoom_button;
static i_window_t quick_info_window;
static const c_vec3_t *goto_ptr;

/******************************************************************************\
 Event function for hover window.
\******************************************************************************/
static bool quick_info_event(i_window_t *window, i_event_t event)
{
        /* Position and size the window */
        if (event == I_EV_CONFIGURE) {
                window->widget.origin = C_vec2((float)i_border.value.n,
                                               (float)i_border.value.n);
                window->widget.size = C_vec2(180.f, 0.f);
        }

        return I_window_event(window, event);
}

/******************************************************************************\
 Zoom button pressed.
\******************************************************************************/
static void zoom_pressed(void)
{
        if (goto_ptr)
                R_rotate_cam_to(*goto_ptr);
}

/******************************************************************************\
 Show the quick info window and set its title. [goto_ptr] is expected to
 be either NULL or valid after the call and while the window is shown.
\******************************************************************************/
void I_quick_info_show(const char *title, const c_vec3_t *_goto_ptr)
{
        float fade;

        if (i_limbo)
                return;

        /* Get rid of the old window */
        fade = quick_info_window.widget.fade;
        I_widget_event(&quick_info_window.widget, I_EV_CLEANUP);

        /* Add the window to the root widget */
        I_window_init(&quick_info_window);
        quick_info_window.widget.auto_configure = TRUE;
        quick_info_window.widget.event_func = (i_event_f)quick_info_event;
        quick_info_window.fit = I_FIT_BOTTOM;
        quick_info_window.popup = TRUE;
        quick_info_window.auto_hide = TRUE;
        I_widget_add(&i_root, &quick_info_window.widget);

        /* Title box */
        I_box_init(&title_box, I_PACK_H,
                   R_font_height(R_FONT_TITLE) / r_scale_2d);
        I_widget_add(&quick_info_window.widget, &title_box.widget);

        /* Add the title */
        I_label_init(&title_label, title);
        title_label.widget.expand = TRUE;
        title_label.font = R_FONT_TITLE;
        I_widget_add(&title_box.widget, &title_label.widget);

        /* Add the zoom button */
        I_button_init(&zoom_button, "gui/icons/zoom.png", NULL, I_BT_ROUND);
        zoom_button.on_click = (i_callback_f)zoom_pressed;
        I_widget_add(&title_box.widget, &zoom_button.widget);
        if (!(goto_ptr = _goto_ptr))
                zoom_button.widget.state = I_WS_DISABLED;

        /* Preserve the window's fade */
        I_widget_fade(&quick_info_window.widget, fade);
}

/******************************************************************************\
 Close the quick info window. The window will need to be initialized again the
 next time it is shown.
\******************************************************************************/
void I_quick_info_close(void)
{
        if (!quick_info_window.widget.configured)
                return;
        I_widget_event(&quick_info_window.widget, I_EV_HIDE);
}

/******************************************************************************\
 Add an information item to the quick info window.
\******************************************************************************/
void I_quick_info_add(const char *label, const char *value)
{
        i_info_t *info;

        info = I_info_alloc(label, value);
        I_widget_add(&quick_info_window.widget, &info->widget);
        I_widget_fade(&info->widget, quick_info_window.widget.fade);
}

/******************************************************************************\
 Add a colored information item to the quick info window.
\******************************************************************************/
void I_quick_info_add_color(const char *label, const char *value,
                            i_color_t color)
{
        i_info_t *info;

        info = I_info_alloc(label, value);
        I_widget_add(&quick_info_window.widget, &info->widget);
        I_widget_fade(&info->widget, quick_info_window.widget.fade);
        info->right.color = color;
}

