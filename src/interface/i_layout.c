/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Contains the root window and toolbars */

#include "i_common.h"

/* Globe light modulation in limbo mode */
#define LIMBO_GLOBE_LIGHT 0.1f

/* The logo fades in/out during limbo mode changes, but not as fast as the
   other windows and GUI elements */
#define LIMBO_FADE_SCALE 0.2f

/* SDL key-repeat initial timeout and repeat interval */
#define KEY_REPEAT_TIMEOUT 300
#define KEY_REPEAT_INTERVAL 30

/* Opacity per-frame faded in from initial black fill */
#define INIT_FADE_RATE 0.05f

/* Root widget contains all other widgets */
i_widget_t i_root;

/* TRUE if the interface is in limbo mode */
int i_limbo;

/* Indices of buttons on the right toolbar */
int i_nations_button, i_players_button, i_trade_button;

/* Right toolbar */
i_toolbar_t i_right_toolbar;

static i_label_t clock_label, time_limit_label;
static i_toolbar_t left_toolbar;
static r_sprite_t limbo_logo;
static float limbo_fade, init_fade;
static int layout_frame, cleanup_theme;

/******************************************************************************\
 Update national colors.
\******************************************************************************/
void I_update_colors(void)
{
        /* National colors */
        i_colors[I_COLOR_RED] = g_nations[G_NN_RED].color;
        i_colors[I_COLOR_GREEN] = g_nations[G_NN_GREEN].color;
        i_colors[I_COLOR_BLUE] = g_nations[G_NN_BLUE].color;
        i_colors[I_COLOR_PIRATE] = g_nations[G_NN_PIRATE].color;

        /* Nation color alpha values are not for the UI */
        i_colors[I_COLOR_RED].a = i_colors[I_COLOR].a;
        i_colors[I_COLOR_GREEN].a = i_colors[I_COLOR].a;
        i_colors[I_COLOR_BLUE].a = i_colors[I_COLOR].a;
        i_colors[I_COLOR_PIRATE].a = i_colors[I_COLOR].a;
}

/******************************************************************************\
 Root window event function.
\******************************************************************************/
static int root_event(i_widget_t *root, i_event_t event)
{
        switch (event) {
        case I_EV_CONFIGURE:
                i_colors[I_COLOR] = C_color_string(i_color.value.s);
                i_colors[I_COLOR_ALT] = C_color_string(i_color_alt.value.s);
                i_colors[I_COLOR_BAD] = C_color_string(i_color_bad.value.s);
                i_colors[I_COLOR_GOOD] = C_color_string(i_color_good.value.s);
                i_colors[I_COLOR_GUI] = C_color_string(i_color_gui.value.s);
                I_update_colors();

                /* Size and position limbo logo */
                limbo_logo.origin.x = r_width_2d / 2 - limbo_logo.size.x / 2;
                limbo_logo.origin.y = r_height_2d / 2 - limbo_logo.size.y / 2;

                /* Make sure the globe is configured */
                I_globe_event(event);

                /* Do the propagation ourselves */
                I_widget_propagate(root, event);

                /* Move chat into proper position after configuration */
                I_position_chat();

                return FALSE;
        case I_EV_KEY_FOCUS:
                I_focus_chat();
                break;
        case I_EV_RENDER:

                /* Rotate around the globe during limbo */
                if (limbo_fade > 0.f)
                        R_rotate_cam_by(C_vec3(0.f, limbo_fade *
                                                    c_frame_sec / 60.f * C_PI /
                                                    R_MINUTES_PER_DAY, 0.f));
                if (i_limbo) {
                        R_zoom_cam_by((r_zoom_max - r_cam_zoom) * c_frame_sec);
                        limbo_fade += i_fade.value.f * c_frame_sec *
                                      LIMBO_FADE_SCALE;
                        if (limbo_fade > 1.f)
                                limbo_fade = 1.f;
                } else {
                        limbo_fade -= i_fade.value.f * c_frame_sec *
                                      LIMBO_FADE_SCALE;
                        if (limbo_fade < 0.f)
                                limbo_fade = 0.f;
                }

                /* Globe lighting is modulated by limbo mode */
                r_globe_light = LIMBO_GLOBE_LIGHT +
                                (1.f - LIMBO_GLOBE_LIGHT) * (1.f - limbo_fade);

                /* Render and fade limbo logo */
                if (limbo_fade > 0.f) {
                        limbo_logo.modulate.a = limbo_fade;
                        R_sprite_render(&limbo_logo);
                }

                break;
        default:
                break;
        }

        /* Propagate events to special handlers */
        if (!I_chat_event(event))
                return TRUE;
        I_globe_event(event);

        return TRUE;
}

/******************************************************************************\
 Updates themeable widget assets.
\******************************************************************************/
static void theme_widgets(void)
{
        if (i_debug.value.n)
                C_trace("Theming widgets");
        I_theme_buttons();
        I_theme_windows();
        I_theme_entries();
        I_theme_scrollbacks();
        I_theme_ring();
        I_theme_statics();
        I_theme_checkboxs();
}

/******************************************************************************\
 Updates after a theme change.
\******************************************************************************/
static void theme_configure(void)
{
        float toolbar_height, toolbar_y;

        C_var_unlatch(&i_border);
        C_var_unlatch(&i_color);
        C_var_unlatch(&i_color_alt);
        C_var_unlatch(&i_shadow);

        /* Root window */
        i_root.size = C_vec2((float)r_width_2d, (float)r_height_2d);

        /* Toolbars */
        toolbar_height = 32.f + i_border.value.n * 2;
        toolbar_y = r_height_2d - toolbar_height - i_border.value.n;
        left_toolbar.widget.size = C_vec2(0.f, toolbar_height);
        left_toolbar.widget.origin = C_vec2((float)i_border.value.n, toolbar_y);
        i_right_toolbar.widget.size = C_vec2(0.f, toolbar_height);
        i_right_toolbar.widget.origin = C_vec2((float)(r_width_2d -
                                                       i_border.value.n),
                                               toolbar_y);

        theme_widgets();
}

/******************************************************************************\
 Parse the theme config. Does not unlatch [i_theme]. Returns FALSE if the theme
 name is invalid.
\******************************************************************************/
static int parse_config(const char *name)
{
        const char *filename;

        /* The theme name should never modify the path */
        if (C_is_path(name)) {
                C_warning("Theme name contains path characters");
                return FALSE;
        }

        filename = C_va("gui/themes/%s/theme.cfg", name);
        if (!C_parse_config_file(filename)) {
                C_warning("Failed to parse theme config '%s'", filename);
                return FALSE;
        }
        return TRUE;
}

/******************************************************************************\
 Parse the theme config.
\******************************************************************************/
void I_parse_config(void)
{
        C_var_unlatch(&i_theme);

        /* Blank theme is equivalent to 'default' */
        if (!i_theme.value.s[0])
                i_theme.value = i_theme.stock;

        /* Parse the configured config or fallback to stock */
        if (!parse_config(i_theme.value.s)) {
                i_theme.value = i_theme.stock;
                parse_config(i_theme.stock.s);
        }
}

/******************************************************************************\
 Update function for i_theme. Will also reload fonts.
\******************************************************************************/
static int theme_update(c_var_t *var, c_var_value_t value)
{
        /* Reset theme variables before parsing the theme config */
        C_var_reset(&i_border);
        C_var_reset(&i_color);
        C_var_reset(&i_color_alt);
        C_var_reset(&i_color_bad);
        C_var_reset(&i_color_good);
        C_var_reset(&i_color_gui);
        C_var_reset(&i_fade);
        C_var_reset(&i_shadow);
        R_stock_fonts();

        /* Treat blank as default */
        if (!value.s[0])
                C_strncpy_buf(value.s, "default");

        /* Parse the theme configuration file, if there is one */
        if (!parse_config(value.s))
                return FALSE;
        i_theme.value = value;

        /* Reload fonts */
        R_free_fonts();
        R_load_fonts();

        /* Update theme assets */
        theme_configure();
        I_widget_event(&i_root, I_EV_CONFIGURE);
        return TRUE;
}

/******************************************************************************\
 Convenience function that cleans up and reloads a texture. The [name] argument
 is the filename of the texture file in the theme directory minus the image
 format suffix.
\******************************************************************************/
void I_theme_texture(r_texture_t **ppt, const char *name)
{
        const char *filename;

        R_texture_free(*ppt);
        if (cleanup_theme)
                return;
        *ppt = NULL;

        /* Load the file if it exists, otherwise avoid the warning */
        filename = C_va("gui/themes/%s/%s.png", i_theme.value.s, name);
        if (C_file_exists(filename))
                *ppt = R_texture_load(filename, FALSE);

        /* Try to load default if the selected theme is missing this texture */
        if (!*ppt) {
                C_debug("Theme '%s' is missing texture '%s'",
                        i_theme.value.s, name);
                filename = C_va("gui/themes/%s/%s.png", i_theme.stock.s, name);
                *ppt = R_texture_load(filename, FALSE);
                if (!*ppt)
                        C_error("Stock texture '%s' is missing", filename);
        }
}

/******************************************************************************\
 Leave limbo mode.
\******************************************************************************/
void I_leave_limbo(void)
{
        i_limbo = FALSE;
        I_widget_event(&i_right_toolbar.widget, I_EV_SHOW);
}

/******************************************************************************\
 Enter limbo mode.
\******************************************************************************/
void I_enter_limbo(void)
{
        i_limbo = TRUE;
        I_widget_event(&i_right_toolbar.widget, I_EV_HIDE);
        I_hide_chat();
        I_quick_info_close();
}

/******************************************************************************\
 Handle global key presses.
\******************************************************************************/
void I_global_key(void)
{
        /* Take screenshot */
        if (i_key == SDLK_F12) {
                const char *filename;

                filename = R_save_screenshot();
                if (!filename || !filename[0])
                        return;
                I_popup(NULL, C_va("Saved screenshot: %s", filename));
        }

        /* Alt + F4 doesn't work fullscreen */
        else if (i_key == SDLK_F4 && i_key_alt) {
                C_debug("Caught Alt + F4");
                exit(0);
        }

        /* Toggle fullscreen */
        else if (i_key == SDLK_F11 || (i_key == SDLK_RETURN && i_key_alt)) {
                C_debug("Fullscreen toggled");
                C_var_set(&r_windowed, r_windowed.value.n ? "0" : "1");
                r_restart = TRUE;
        }

        /* Toggle a toolbar window */
        else if (i_key >= SDLK_F1 && i_key <= SDLK_F3)
                I_toolbar_toggle(&left_toolbar, i_key - SDLK_F1);
        else if (i_key >= SDLK_F6 && i_key <= SDLK_F8)
                I_toolbar_toggle(&i_right_toolbar, i_key - SDLK_F6);
}

/******************************************************************************\
 Loads interface assets and initializes windows.
\******************************************************************************/
void I_init(void)
{
        C_status("Initializing interface");

        /* Enable key repeats so held keys generate key-down events */
        SDL_EnableKeyRepeat(KEY_REPEAT_TIMEOUT, KEY_REPEAT_INTERVAL);

        /* Enable Unicode so we get Unicode key strokes AND capitals */
        SDL_EnableUNICODE(1);

        /* Root window */
        C_zero(&i_root);
        I_widget_init(&i_root, "Root Window");
        i_root.state = I_WS_READY;
        i_root.event_func = (i_event_f)root_event;
        i_root.configured = 1;
        i_root.entry = TRUE;
        i_root.shown = TRUE;
        i_root.steal_keys = TRUE;
        i_key_focus = &i_root;

        /* Initialize special child widgets */
        I_init_ring();
        I_init_popup();
        I_init_chat();

        /* Initialize clock label */
        I_label_init(&clock_label, NULL);
        clock_label.font = R_FONT_LCD;
        clock_label.color = I_COLOR_GUI;
        I_widget_add(&i_root, &clock_label.widget);

        /* Initialize time limit label */
        I_label_init(&time_limit_label, NULL);
        time_limit_label.font = R_FONT_LCD;
        I_widget_add(&i_root, &time_limit_label.widget);

        /* Left toolbar */
        I_toolbar_init(&left_toolbar, FALSE);
        I_toolbar_add_button(&left_toolbar, "gui/icons/game.png",
                             (i_callback_f)I_init_game);
        I_toolbar_add_button(&left_toolbar, "gui/icons/console.png",
                             (i_callback_f)I_init_console);
        I_toolbar_add_button(&left_toolbar, "gui/icons/video.png",
                             (i_callback_f)I_init_video);
        I_widget_add(&i_root, &left_toolbar.widget);

        /* Right toolbar */
        I_toolbar_init(&i_right_toolbar, TRUE);
        i_trade_button = I_toolbar_add_button(&i_right_toolbar,
                                              "gui/icons/trade.png",
                                              (i_callback_f)I_init_trade);
        i_nations_button = I_toolbar_add_button(&i_right_toolbar,
                                                "gui/icons/nations.png",
                                                (i_callback_f)I_init_nations);
        i_players_button = I_toolbar_add_button(&i_right_toolbar,
                                                "gui/icons/players.png",
                                                (i_callback_f)I_init_players);
        I_widget_add(&i_root, &i_right_toolbar.widget);
        i_right_toolbar.buttons[i_trade_button].widget.state = I_WS_DISABLED;

        /* Theme can now be modified dynamically */
        theme_configure();
        i_theme.update = (c_var_update_f)theme_update;
        i_theme.edit = C_VE_FUNCTION;

        /* Limbo resources */
        R_sprite_load(&limbo_logo, "gui/logo.png");

        /* Configure all widgets */
        I_widget_event(&i_root, I_EV_CONFIGURE);

        /* Start in limbo */
        I_enter_limbo();
        limbo_fade = 1.f;
}

/******************************************************************************\
 Free interface assets.
\******************************************************************************/
void I_cleanup(void)
{
        I_widget_event(&i_root, I_EV_CLEANUP);
        R_sprite_cleanup(&limbo_logo);

        /* We reuse the theme function calls to cleanup their textures by
           setting a special mode */
        cleanup_theme = TRUE;
        theme_widgets();
        cleanup_theme = FALSE;
}

/******************************************************************************\
 Update the clock label.
\******************************************************************************/
static void update_clock(bool force)
{
        static int clock_time;
        struct tm *tm;
        time_t time_msec;
        int hour, mins, sec, msec;
        const char *suffix;

        if (c_time_msec + 1000 < clock_time || force || g_game_over)
                return;
        clock_time = c_time_msec + 1000;
        time(&time_msec);
        tm = localtime(&time_msec);

        /* Convert to 12-hour clock */
        hour = tm->tm_hour;
        if (hour > 12)
                hour -= 12;
        if (hour < 1)
                hour = 12;
        suffix = tm->tm_hour >= 12 ? "pm" : "am";

        /* Configure and position clock */
        clock_label.widget.size = C_vec2(0.f, 0.f);
        I_label_configure(&clock_label,
                          C_va("%d:%02d %s", hour, tm->tm_min, suffix));
        I_widget_move(&clock_label.widget,
                      C_vec2(r_width_2d - clock_label.widget.size.x -
                             i_border.value.n, (float)i_border.value.n));

        /* Compute time left */
        msec = g_time_limit_msec - c_time_msec;
        if (msec < 0 || i_limbo) {
                I_widget_event(&time_limit_label.widget, I_EV_HIDE);
                return;
        }
        I_widget_event(&time_limit_label.widget, I_EV_SHOW);
        mins = msec / 60000;
        msec -= mins * 60000;
        sec = msec / 1000;

        /* Color/format varies with time */
        time_limit_label.widget.size = C_vec2(0.f, 0.f);
        if (mins >= 5) {
                time_limit_label.color = I_COLOR_GOOD;
                I_label_configure(&time_limit_label, C_va("%d min ", mins));
        } else if (mins >= 1) {
                time_limit_label.color = I_COLOR_ALT;
                I_label_configure(&time_limit_label,
                                  C_va("%d:%02d ", mins, sec));
        } else if (sec >= 1) {
                time_limit_label.color = I_COLOR_BAD;
                I_label_configure(&time_limit_label, C_va("%d sec ", sec));
        } else {
                time_limit_label.color = I_COLOR_BAD;
                I_label_configure(&time_limit_label, "TIME ");
        }

        /* Configure and position time limit */
        I_widget_move(&time_limit_label.widget,
                      C_vec2(clock_label.widget.origin.x -
                             time_limit_label.widget.size.x,
                             (float)i_border.value.n));
}

/******************************************************************************\
 Render all visible windows.
\******************************************************************************/
void I_render(void)
{
        /* If video parameters changed, we need to reconfigure */
        if (r_scale_2d_frame > layout_frame ||
            r_width.changed > layout_frame ||
            r_height.changed > layout_frame) {
                theme_configure();
                update_clock(TRUE);
                I_update_video();
                I_widget_event(&i_root, I_EV_CONFIGURE);
                layout_frame = c_frame;
        }
        if (r_windowed.changed > layout_frame) {
                I_update_video();
                layout_frame = c_frame;
        }

        /* Popup message might have changed */
        I_update_popup();

        update_clock(FALSE);
        I_widget_event(&i_root, I_EV_RENDER);

        /* Render the initialization fade-in from black */
        if (init_fade < 1.f) {
                R_fill_screen(C_color(0.f, 0.f, 0.f, 1.f - init_fade));
                init_fade += INIT_FADE_RATE;
        }
}

