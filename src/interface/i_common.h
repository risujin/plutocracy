/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "../common/c_shared.h"
#include "../render/r_shared.h"
#include "../game/g_shared.h"
#include "../network/n_shared.h"
#include "i_shared.h"

/* Pixels to scroll for mouse wheel scrolling */
#define I_WHEEL_SCROLL 32.f

/* Size of the history queue kept by the text entry widget */
#define I_ENTRY_HISTORY 32

/* Limit for toolbar buttons */
#define I_TOOLBAR_BUTTONS 6

/* Possible events. The arguments (mouse position etc) for the events are
   stored in global variables, so the interface system only processes one
   event at a time. */
typedef enum {
        I_EV_NONE,              /* Do nothing */
        I_EV_ADD_CHILD,         /* A child widget was added */
        I_EV_CLEANUP,           /* Should free resources */
        I_EV_CONFIGURE,         /* Initialized or resized */
        I_EV_GRAB_FOCUS,        /* Entry widgets should take key focus */
        I_EV_HIDE,              /* Widget is being hidden */
        I_EV_KEY_DOWN,          /* A key is pressed down or repeated */
        I_EV_KEY_UP,            /* A key is released */
        I_EV_KEY_FOCUS,         /* This widget is the new keyboard focus */
        I_EV_MOUSE_IN,          /* Mouse just entered widget area */
        I_EV_MOUSE_OUT,         /* Mouse just left widget area */
        I_EV_MOUSE_MOVE,        /* Mouse is moved over widget */
        I_EV_MOUSE_DOWN,        /* A mouse button is pressed on the widget */
        I_EV_MOUSE_UP,          /* A mouse button is released on the widget */
        I_EV_MOUSE_FOCUS,       /* Event used to find which widget has focus */
        I_EV_MOVED,             /* Widget was moved */
        I_EV_RENDER,            /* Should render */
        I_EV_SHOW,              /* Widget is being shown */
        I_EVENTS
} i_event_t;

/* Widgets can only be packed as lists horizontally or vertically */
typedef enum {
        I_PACK_NONE,
        I_PACK_H,
        I_PACK_V,
        I_PACK_TYPES
} i_pack_t;

/* Widget input states */
typedef enum {
        I_WS_READY,
        I_WS_HOVER,
        I_WS_ACTIVE,
        I_WS_DISABLED,
        I_WS_NO_FOCUS,
        I_WIDGET_STATES
} i_widget_state_t;

/* Determines how a widget will use any extra space it has been assigned */
typedef enum {
        I_FIT_NONE,
        I_FIT_TOP,
        I_FIT_BOTTOM,
} i_fit_t;

/* Text justification */
typedef enum {
        I_JUSTIFY_LEFT,
        I_JUSTIFY_CENTER,
        I_JUSTIFY_RIGHT,
} i_justify_t;

/* Function to handle mouse, keyboard, or other events. Return FALSE to
   prevent the automatic propagation of an event to the widget's children. */
typedef int (*i_event_f)(void *widget, i_event_t);

/* Simple callback for handling widget-specific events */
typedef void (*i_callback_f)(void *widget);

/* Entry-widget auto-complete feed function */
typedef const char *(*i_auto_complete_f)(const char *root);

/* The basic properties all interface widgets need to have. The widget struct
   should be the first member of any derived widget struct.

   When a widget receives the I_EV_CONFIGURE event it, its parent has set its
   origin and size to the maximum available space. The widget must then change
   its size to its actual size.

   The reason for keeping [name] as a buffer rather than a pointer to a
   statically-allocated string is to assist in memory leak detection. The
   leak detector thinks the memory is a string and prints the widget name. */
typedef struct i_widget {
        char name[32];
        struct i_widget *parent, *next, *child;
        c_vec2_t origin, size;
        i_event_f event_func;
        i_widget_state_t state;
        float expand, fade, margin_front, margin_rear, padding;
        bool auto_configure, configured, entry, shrink_only, shown, heap,
             pack_skip, steal_keys;
} i_widget_t;

/* Windows are decorated containers */
typedef struct i_window {
        i_widget_t widget;
        i_pack_t pack_children;
        i_fit_t fit;
        i_widget_t *key_focus, *hanger_align;
        r_window_t window;
        r_sprite_t hanger;
        float hanger_x;
        bool auto_hide, decorated, hanger_shown, popup;
} i_window_t;

/* Button type */
typedef enum {
        I_BT_DECORATED,
        I_BT_SQUARE,
        I_BT_ROUND,
} i_button_type_t;

/* Buttons can have an icon and/or text */
typedef struct i_button {
        i_widget_t widget;
        r_window_t normal, hover, active;
        r_sprite_t icon, text, icon_active, icon_hover;
        i_callback_f on_click;
        i_button_type_t type;
        void *data;
        char buffer[64];
        bool hover_activate;
} i_button_t;

/* Labels only have text */
typedef struct i_label {
        i_widget_t widget;
        r_text_t text;
        r_font_t font;
        i_color_t color;
        i_justify_t justify;
        float width;
        const char *width_sample;
        char buffer[256];
} i_label_t;

/* A one-line text field */
typedef struct i_entry {
        i_widget_t widget;
        r_sprite_t text, cursor;
        r_window_t window;
        i_callback_f on_change, on_enter;
        i_auto_complete_f auto_complete;
        float scroll;
        int pos;
        char buffer[128];
        bool just_tabbed;
} i_entry_t;

/* An entry with a history */
typedef struct i_history_entry {
        i_entry_t entry;
        int pos, size;
        char history[I_ENTRY_HISTORY][256];
} i_history_entry_t;

/* A fixed-size, work-area widget that can dynamically add new components
   vertically and scroll up and down. Widgets will begin to be removed after
   a specified limit is reached. */
typedef struct i_scrollback {
        i_widget_t widget;
        r_window_t window;
        float scroll;
        int children, limit;
        bool bottom_padding;
} i_scrollback_t;

/* Data type for a select widget option */
typedef struct i_select_option {
        char string[32];
        float value;
        struct i_select_option *next;
} i_select_option_t;

/* Displays a label and a selection widget which can select text items from
   a list to the right of it */
typedef struct i_select {
        i_widget_t widget;
        i_label_t label, item;
        i_button_t left, right;
        i_callback_f on_change;
        i_select_option_t *options;
        c_var_t *variable;
        void *data;
        float min, max, increment;
        int decimals, digits, index, list_len;
        const char *suffix;
} i_select_t;

/* Toolbar widget used for the left and right toolbars on the screen */
typedef struct i_toolbar {
        i_widget_t widget;
        i_window_t window, windows[I_TOOLBAR_BUTTONS], *open_window;
        i_button_t buttons[I_TOOLBAR_BUTTONS];
        bool right, children, was_open[I_TOOLBAR_BUTTONS];
} i_toolbar_t;

/* Box widget used for aligning other widgets */
typedef struct i_box {
        i_widget_t widget;
        i_fit_t fit;
        i_pack_t pack_children;
        float width;
} i_box_t;

/* A selectable line */
typedef struct i_selectable {
        i_widget_t widget;
        i_callback_f on_select;
        r_window_t on, off, hover;
        struct i_selectable **group;
        void *data;
        float height;
} i_selectable_t;

/* A left-aligned and a right-aligned label packed into a horizontal box */
typedef struct i_info {
        i_widget_t widget;
        i_label_t left, right;
} i_info_t;

/* Images just display a sprite */
typedef struct i_image {
        i_widget_t widget;
        r_sprite_t sprite;
        r_texture_t **theme_texture;
        c_vec2_t original_size;
        bool resize;
} i_image_t;

/* Checkbox */
typedef struct i_checkbox {
        i_widget_t widget;
        r_window_t normal;
        r_sprite_t checkmark, hover, active;
        i_callback_f on_change;
        void *data;
        bool hover_activate, checkbox_checked;
} i_checkbox_t;

/* i_button.c */
int I_button_event(i_button_t *, i_event_t);
void I_button_init(i_button_t *, const char *icon, const char *text,
                   i_button_type_t);
void I_button_configure(i_button_t *, const char *icon, const char *text,
                        i_button_type_t);
void I_theme_buttons(void);

/* i_chat.c */
bool I_chat_event(i_event_t);
void I_focus_chat(void);
void I_hide_chat(void);
void I_init_chat(void);
void I_position_chat(void);
void I_show_chat(void);
void I_configure_chat_player(int index, const char *name, i_color_t color);
void I_configure_chat_player_num(int num);

/* i_container.c */
void I_widget_add(i_widget_t *parent, i_widget_t *child);
void I_widget_add_pack(i_widget_t *parent, i_widget_t *child,
                       i_pack_t, i_fit_t fit);
c_vec2_t I_widget_child_bounds(const i_widget_t *);
void I_widget_pack(i_widget_t *, i_pack_t, i_fit_t);
void I_widget_remove_children(i_widget_t *, int cleanup);

/* i_console.c */
void I_init_console(i_window_t *);
int I_scrollback_event(i_scrollback_t *, i_event_t);
void I_scrollback_init(i_scrollback_t *);
void I_scrollback_scroll(i_scrollback_t *, bool up);
void I_theme_scrollbacks(void);

/* i_entry.c */
int I_entry_event(i_entry_t *, i_event_t);
void I_entry_init(i_entry_t *, const char *);
void I_entry_configure(i_entry_t *, const char *);
int I_history_entry_event(i_history_entry_t *, i_event_t);
void I_history_entry_init(i_history_entry_t *, const char *);
void I_theme_entries(void);

/* i_game.c */
void I_init_game(i_window_t *);

/* i_globe.c */
void I_globe_event(i_event_t);

/* i_layout.c */
void I_global_key(void);
void I_theme_texture(r_texture_t **, const char *name);

extern i_toolbar_t i_right_toolbar;
extern i_widget_t i_root;
extern int i_nations_button, i_players_button, i_trade_button;

/* i_nations.c */
void I_init_nations(i_window_t *);

/* i_players.c */
void I_init_players(i_window_t *);

/* i_ring.c */
void I_close_ring(void);
void I_init_ring(void);
int I_ring_shown(void);
void I_theme_ring(void);

/* i_select.c */
int I_key_amount(void);
void I_select_add_float(i_select_t *, float, const char *override);
void I_select_add_int(i_select_t *, int, const char *override);
void I_select_add_string(i_select_t *, const char *);
void I_select_change(i_select_t *, int index);
void I_select_init(i_select_t *, const char *label, const char *suffix);
void I_select_nearest(i_select_t *, float value);
void I_select_range(i_select_t *, float min, float inc, float max);
void I_select_update(i_select_t *);
float I_select_value(const i_select_t *);

/* i_static.c */
void I_box_init(i_box_t *, i_pack_t, float width);
i_info_t *I_info_alloc(const char *label, const char *info);
void I_info_init(i_info_t *, const char *label, const char *info);
void I_info_configure(i_info_t *, const char *);
int I_label_event(i_label_t *label, i_event_t event);
void I_label_init(i_label_t *, const char *);
void I_label_configure(i_label_t *, const char *);
i_label_t *I_label_alloc(const char *);
void I_image_init(i_image_t *, const char *icon);
void I_image_init_sep(i_image_t *, bool vertical);
void I_image_init_themed(i_image_t *, r_texture_t **);
int I_selectable_event(i_selectable_t *, i_event_t);
void I_selectable_init(i_selectable_t *, i_selectable_t **group, float height);
void I_selectable_off(i_selectable_t *);
void I_selectable_on(i_selectable_t *);
void I_theme_statics(void);

/* i_trade.c */
void I_init_trade(i_window_t *);

/* i_variables.c */
extern c_var_t i_border, i_color, i_color_alt, i_color_bad, i_color_good,
               i_color_gui, i_debug, i_edge_scroll, i_fade, i_ip,
               i_scroll_speed, i_shadow, i_test_globe, i_theme, i_zoom_speed;

/* i_video.c */
void I_init_video(i_window_t *);
void I_update_video(void);

/* i_widgets.c */
const char *I_event_to_string(i_event_t);
c_vec2_t I_widget_bounds(const i_widget_t *, i_pack_t);
bool I_widget_child_of(const i_widget_t *parent, const i_widget_t *child);
void I_widget_enable(i_widget_t *, bool enable);
void I_widget_event(i_widget_t *, i_event_t);
void I_widget_fade(i_widget_t *, float opacity);
void I_widget_focus(i_widget_t *, bool key, bool mouse);
void I_widget_init(i_widget_t *, const char *class_name);
void I_widget_move(i_widget_t *, c_vec2_t new_origin);
void I_widget_propagate(i_widget_t *, i_event_t);
void I_widget_remove(i_widget_t *, int cleanup);
i_widget_t *I_widget_top_level(i_widget_t *);

extern c_color_t i_colors[I_COLORS];
extern c_vec2_t i_mouse;
extern i_widget_t *i_child, *i_key_focus, *i_mouse_focus;
extern int i_key, i_key_alt, i_key_ctrl, i_key_shift, i_key_unicode,
           i_mouse_button;

/* i_window.c */
void I_init_popup(void);
void I_update_popup(void);
void I_theme_windows(void);
int I_toolbar_add_button(i_toolbar_t *, const char *icon,
                         i_callback_f init_func);
void I_toolbar_enable(i_toolbar_t *, int button, bool enable);
void I_toolbar_init(i_toolbar_t *, int right);
void I_toolbar_position(i_toolbar_t *, int index);
void I_toolbar_toggle(i_toolbar_t *, int index);
int I_window_event(i_window_t *, i_event_t);
void I_window_init(i_window_t *);

/* i_checkbox.c */
int I_checkbox_event(i_checkbox_t *checkbox, i_event_t event);
void I_checkbox_init(i_checkbox_t *checkbox);
void I_theme_checkboxs(void);
