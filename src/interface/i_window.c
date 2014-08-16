/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Implements generic windows and toolbars */

#include "i_common.h"

/* Time in milliseconds that the popup widget will remain shown */
#define POPUP_TIME 2500

/* Maximum number of popup messages in the queue */
#define POPUP_MESSAGES_MAX 8

/* Type for queued popup messages */
typedef struct popup_message {
        c_vec3_t goto_pos;
        int has_goto_pos;
        char message[128];
} popup_message_t;

static popup_message_t popup_messages[POPUP_MESSAGES_MAX];
static i_button_t zoom_button;
static i_label_t popup_label;
static i_widget_t popup_widget;
static r_window_t popup_window;
static r_texture_t *decor_window, *hanger, *decor_popup;
static int popup_time;
static bool popup_wait;

/******************************************************************************\
 Initialize themeable window assets.
\******************************************************************************/
void I_theme_windows(void)
{
        I_theme_texture(&decor_window, "window");
        I_theme_texture(&decor_popup, "popup");
        I_theme_texture(&hanger, "hanger");
}

/******************************************************************************\
 Window widget event function.
\******************************************************************************/
int I_window_event(i_window_t *window, i_event_t event)
{
        switch (event) {
        case I_EV_CONFIGURE:
                R_window_cleanup(&window->window);
                I_widget_pack(&window->widget, window->pack_children,
                              window->fit);
                if (window->decorated) {
                        r_texture_t *decor;

                        decor = decor_window;
                        if (window->popup)
                                decor = decor_popup;
                        R_window_init(&window->window, decor);
                        window->window.sprite.origin = window->widget.origin;
                        window->window.sprite.size = window->widget.size;
                        R_sprite_cleanup(&window->hanger);
                        R_sprite_init(&window->hanger, hanger);
                        window->hanger.origin.y = window->widget.origin.y +
                                                  window->widget.size.y;
                        window->hanger.size.y = (float)i_border.value.n;
                }
                return FALSE;
        case I_EV_KEY_DOWN:
                if (i_key == SDLK_ESCAPE && window->auto_hide)
                        I_widget_event(&window->widget, I_EV_HIDE);
                break;
        case I_EV_MOUSE_IN:
                if (I_widget_child_of(&window->widget, i_key_focus))
                        break;
                i_key_focus = NULL;
                I_widget_propagate(&window->widget, I_EV_GRAB_FOCUS);
                break;
        case I_EV_MOVED:
                window->window.sprite.origin = window->widget.origin;
                window->hanger.origin.y = window->widget.origin.y +
                                          window->widget.size.y;
                break;
        case I_EV_MOUSE_DOWN:
                if (i_mouse_button == SDL_BUTTON_RIGHT && window->auto_hide)
                        I_widget_event(&window->widget, I_EV_HIDE);
                break;
        case I_EV_CLEANUP:
                R_window_cleanup(&window->window);
                R_sprite_cleanup(&window->hanger);
                break;
        case I_EV_RENDER:
                if (!window->decorated)
                        break;
                window->window.sprite.modulate.a = window->widget.fade;
                R_window_render(&window->window);
                if (!window->hanger_align)
                        break;
                window->hanger.origin.x = window->hanger_align->origin.x +
                                          window->hanger_align->size.x / 2 -
                                          window->hanger.size.x / 2;
                window->hanger.modulate.a = window->widget.fade;
                R_sprite_render(&window->hanger);
                break;
        default:
                break;
        }
        return TRUE;
}

/******************************************************************************\
 Initializes an window widget.
\******************************************************************************/
void I_window_init(i_window_t *window)
{
        if (!window)
                return;
        C_zero(window);
        I_widget_init(&window->widget, "Window");
        window->widget.event_func = (i_event_f)I_window_event;
        window->widget.state = I_WS_READY;
        window->widget.padding = 1.f;
        window->widget.steal_keys = TRUE;
        window->pack_children = I_PACK_V;
        window->decorated = TRUE;
}

/******************************************************************************\
 Positions a child window.
\******************************************************************************/
void I_toolbar_position(i_toolbar_t *toolbar, int i)
{
        i_widget_t *widget;
        c_vec2_t origin;

        widget = &toolbar->windows[i].widget;
        origin = toolbar->widget.origin;
        origin.y -= i_border.value.n + widget->size.y;
        if (toolbar->right)
                origin.x += toolbar->widget.size.x - widget->size.x;
        I_widget_move(widget, origin);
}

/******************************************************************************\
 Toolbar event function.
\******************************************************************************/
int I_toolbar_event(i_toolbar_t *toolbar, i_event_t event)
{
        int i;

        switch (event) {
        case I_EV_CONFIGURE:

                /* The toolbar just has the child window widget so we basically
                   slave all the positioning and sizing to it */
                toolbar->window.widget.origin = toolbar->widget.origin;
                toolbar->window.widget.size = toolbar->widget.size;
                I_widget_event(&toolbar->window.widget, event);
                toolbar->widget.origin = toolbar->window.widget.origin;
                toolbar->widget.size = toolbar->window.widget.size;

                for (i = 0; i < toolbar->children; i++)
                        I_toolbar_position(toolbar, i);
                return FALSE;

        case I_EV_MOUSE_DOWN:
                if (i_mouse_button == SDL_BUTTON_RIGHT && toolbar->open_window)
                        I_widget_event(&toolbar->open_window->widget,
                                       I_EV_HIDE);
                break;
        case I_EV_KEY_DOWN:
                if (i_key == SDLK_ESCAPE && toolbar->open_window)
                        I_widget_event(&toolbar->open_window->widget,
                                       I_EV_HIDE);
                break;
        case I_EV_HIDE:
                for (i = 0; i < toolbar->children; i++)
                        I_widget_event(&toolbar->windows[i].widget, I_EV_HIDE);
                break;
        default:
                break;
        }
        return TRUE;
}

/******************************************************************************\
 Initialize a toolbar widget.
\******************************************************************************/
void I_toolbar_init(i_toolbar_t *toolbar, int right)
{
        C_zero(toolbar);
        I_widget_init(&toolbar->widget, "Toolbar");
        toolbar->widget.event_func = (i_event_f)I_toolbar_event;
        toolbar->widget.state = I_WS_READY;
        toolbar->widget.steal_keys = TRUE;
        toolbar->right = right;
        I_window_init(&toolbar->window);
        toolbar->window.pack_children = I_PACK_H;
        toolbar->window.fit = right ? I_FIT_TOP : I_FIT_BOTTOM;
        I_widget_add(&toolbar->widget, &toolbar->window.widget);
}

/******************************************************************************\
 Toolbar button handlers.
\******************************************************************************/
static void toolbar_button_click(i_button_t *button)
{
        i_toolbar_t *toolbar;
        i_window_t *window;

        toolbar = (i_toolbar_t *)button->data;
        window = toolbar->windows + (int)(button - toolbar->buttons);
        if (toolbar->open_window && toolbar->open_window->widget.shown) {
                I_widget_event(&toolbar->open_window->widget, I_EV_HIDE);
                if (toolbar->open_window == window) {
                        toolbar->open_window = NULL;
                        return;
                }
        }
        I_widget_event(&window->widget, I_EV_SHOW);
        toolbar->open_window = window;
}

/******************************************************************************\
 Adds a button to a toolbar widget. The window opened by the icon button is
 initialized with [init_func]. This function should set the window's size as
 the root window does not pack child windows. The window's origin is controlled
 by the toolbar. Returns the new button's index.
\******************************************************************************/
int I_toolbar_add_button(i_toolbar_t *toolbar, const char *icon,
                         i_callback_f init_func)
{
        i_button_t *button;
        i_window_t *window;

        C_assert(toolbar->children < I_TOOLBAR_BUTTONS);

        /* Pad the previous button */
        if (toolbar->children) {
                button = toolbar->buttons + toolbar->children - 1;
                button->widget.margin_rear = 0.5f;
        }

        /* Add a new button */
        button = toolbar->buttons + toolbar->children;
        I_button_init(button, icon, NULL, I_BT_SQUARE);
        button->on_click = (i_callback_f)toolbar_button_click;
        button->data = toolbar;
        I_widget_add(&toolbar->window.widget, &button->widget);

        /* Initialize the window */
        window = toolbar->windows + toolbar->children;
        init_func(window);
        window->widget.shown = FALSE;
        window->auto_hide = TRUE;
        window->hanger_align = &button->widget;
        I_widget_add(&i_root, &window->widget);

        return toolbar->children++;
}

/******************************************************************************\
 Toggle one of the toolbar's windows.
\******************************************************************************/
void I_toolbar_toggle(i_toolbar_t *toolbar, int i)
{
        i_window_t *window;

        if (!toolbar || i < 0 || i >= toolbar->children ||
            toolbar->buttons[i].widget.state == I_WS_DISABLED)
                return;
        if (toolbar->open_window && !toolbar->open_window->widget.shown)
                toolbar->open_window = NULL;
        window = toolbar->windows + i;
        if (toolbar->open_window == window) {
                I_widget_event(&window->widget, I_EV_HIDE);
                toolbar->open_window = NULL;
                return;
        }
        if (toolbar->open_window)
                I_widget_event(&toolbar->open_window->widget, I_EV_HIDE);
        I_widget_event(&window->widget, I_EV_SHOW);
        toolbar->open_window = window;
}

/******************************************************************************\
 Enable or disable a toolbar's button.
\******************************************************************************/
void I_toolbar_enable(i_toolbar_t *toolbar, int i, bool enable)
{
        if (!toolbar || i < 0 || i >= toolbar->children)
                return;

        /* Disable the button and hide its window */
        if (!enable) {
                toolbar->buttons[i].widget.state = I_WS_DISABLED;
                toolbar->was_open[i] = toolbar->windows[i].widget.shown;
                I_widget_event(&toolbar->windows[i].widget, I_EV_HIDE);
                if (toolbar->open_window == toolbar->windows + i)
                        toolbar->open_window = NULL;
                return;
        }

        /* Enable the button */
        if (toolbar->buttons[i].widget.state != I_WS_DISABLED)
                return;
        toolbar->buttons[i].widget.state = I_WS_READY;
        if (toolbar->was_open[i] && !toolbar->open_window) {
                I_widget_event(&toolbar->windows[i].widget, I_EV_SHOW);
                toolbar->open_window = toolbar->windows + i;
        }
}

/******************************************************************************\
 Configure the popup widget to display a message from the queue.
\******************************************************************************/
static void popup_configure(void)
{
        if (!popup_messages[0].message[0]) {
                I_widget_event(&popup_widget, I_EV_HIDE);
                return;
        }
        I_label_configure(&popup_label, popup_messages[0].message);
        zoom_button.widget.state = popup_messages[0].has_goto_pos ?
                                   I_WS_READY : I_WS_DISABLED;
        I_widget_event(&popup_widget, I_EV_CONFIGURE);
        I_widget_event(&popup_widget, I_EV_SHOW);
        popup_time = c_time_msec;
}

/******************************************************************************\
 Popup widget event function.
\******************************************************************************/
static int popup_event(i_widget_t *widget, i_event_t event)
{
        c_vec2_t origin;
        float height;

        switch (event) {
        case I_EV_CONFIGURE:
                height = R_font_height(R_FONT_GUI) / r_scale_2d;
                if (height < 16.f)
                        height = 16.f;
                widget->size = C_vec2(0.f, height + i_border.value.n * 2.f);
                I_widget_pack(widget, I_PACK_H, I_FIT_BOTTOM);
                origin = C_vec2(r_width_2d / 2.f - widget->size.x / 2.f,
                                r_height_2d - widget->size.y -
                                i_border.value.n);
                R_window_cleanup(&popup_window);
                R_window_init(&popup_window, decor_popup);
                I_widget_move(widget, origin);
        case I_EV_MOVED:
                popup_window.sprite.size = widget->size;
                popup_window.sprite.origin = widget->origin;
                return FALSE;
        case I_EV_MOUSE_IN:
                popup_wait = TRUE;
                i_key_focus = NULL;
                break;
        case I_EV_MOUSE_OUT:
                popup_wait = FALSE;
                break;
        case I_EV_MOUSE_DOWN:
                if (i_mouse_button == SDL_BUTTON_RIGHT)
                        I_widget_event(&popup_widget, I_EV_HIDE);
                break;
        case I_EV_KEY_DOWN:
                if (i_key == SDLK_ESCAPE)
                        I_widget_event(&popup_widget, I_EV_HIDE);
                break;
        case I_EV_CLEANUP:
                R_window_cleanup(&popup_window);
                break;
        case I_EV_RENDER:
                if (popup_wait)
                        popup_time = c_time_msec;
                else if (c_time_msec - popup_time > POPUP_TIME)
                        I_widget_event(&popup_widget, I_EV_HIDE);
                popup_window.sprite.modulate.a = widget->fade;
                R_window_render(&popup_window);
                break;
        default:
                break;
        }
        return TRUE;
}

/******************************************************************************\
 Popup zoom button was clicked.
\******************************************************************************/
static void popup_zoom(void)
{
        C_assert(popup_messages[0].has_goto_pos);
        R_rotate_cam_to(popup_messages[0].goto_pos);
}

/******************************************************************************\
 Initialize popup widget and add it to the root widget.
\******************************************************************************/
void I_init_popup(void)
{
        I_widget_init(&popup_widget, "Popup");
        popup_widget.event_func = (i_event_f)popup_event;
        popup_widget.state = I_WS_READY;
        popup_widget.shown = FALSE;
        popup_widget.padding = 1.f;
        popup_widget.steal_keys = TRUE;

        /* Label */
        I_label_init(&popup_label, NULL);
        I_widget_add(&popup_widget, &popup_label.widget);

        /* Zoom button */
        I_button_init(&zoom_button, "gui/icons/zoom.png", NULL, I_BT_ROUND);
        zoom_button.widget.margin_front = 0.5f;
        zoom_button.on_click = (i_callback_f)popup_zoom;
        I_widget_add(&popup_widget, &zoom_button.widget);

        I_widget_add(&i_root, &popup_widget);
}

/******************************************************************************\
 Updates the popup window.
\******************************************************************************/
void I_update_popup(void)
{
        if (popup_widget.shown || popup_widget.fade > 0.f ||
            !popup_messages[0].message[0])
                return;
        memmove(popup_messages, popup_messages + 1,
                sizeof (popup_messages) - sizeof (*popup_messages));
        popup_messages[POPUP_MESSAGES_MAX - 1].message[0] = NUL;
        popup_configure();
}

/******************************************************************************\
 Queues a new message to popup.
\******************************************************************************/
void I_popup(c_vec3_t *goto_pos, const char *message)
{
        int i;

        /* Find an open slot, if there isn't one, pump the queue */
        for (i = 0; popup_messages[i].message[0]; i++)
                if (i >= POPUP_MESSAGES_MAX) {
                        memcpy(popup_messages, popup_messages + 1,
                               sizeof (popup_messages) -
                               sizeof (*popup_messages));
                        i--;
                        break;
                }

        /* Setup the queue entry */
        C_strncpy_buf(popup_messages[i].message, message);
        if (goto_pos) {
                popup_messages[i].has_goto_pos = TRUE;
                popup_messages[i].goto_pos = *goto_pos;
        } else
                popup_messages[i].has_goto_pos = FALSE;

        /* If the popup window is hidden, configure and open it */
        if ((!popup_widget.shown && popup_widget.fade <= 0.f) ||
            i >= POPUP_MESSAGES_MAX - 1)
                popup_configure();

        C_debug("%s", message);
}

