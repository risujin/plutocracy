/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Maximum number of icon buttons a ring can hold */
#define I_RING_BUTTONS 6

/* The interface uses a limited number of colors */
typedef enum {
        I_COLOR,
        I_COLOR_ALT,
        I_COLOR_GUI,
        I_COLOR_RED,
        I_COLOR_GREEN,
        I_COLOR_BLUE,
        I_COLOR_PIRATE,
        I_COLOR_GOOD,
        I_COLOR_BAD,
        I_COLORS
} i_color_t;

/* Ring icon names */
typedef enum {
        I_RI_NONE,

        /* Testing */
        I_RI_UNKNOWN,
        I_RI_MILL,
        I_RI_TREE,
        I_RI_SHIP,

        /* Buildings */
        I_RI_SHIPYARD,

        /* Ship commands */
        I_RI_BOARD,
        I_RI_FOLLOW,

        /* Ships */
        I_RI_GALLEON,
        I_RI_SLOOP,
        I_RI_SPIDER,

        I_RING_ICONS=30,
} i_ring_icon_t;

/* Ring callback function */
typedef void (*i_ring_f)(i_ring_icon_t);

/* Structure for passing information to trade window */
typedef struct i_cargo_info {
        int amount, buy_price, sell_price, maximum, minimum,
            p_amount, p_buy_price, p_sell_price, p_minimum, p_maximum;
        bool auto_buy, auto_sell;
} i_cargo_info_t;

/* i_chat.c */
void I_print_chat(const char *name, i_color_t, const char *message);

/* i_game.c */
void I_add_server(const char *main, const char *alt, const char *address,
                  bool compatible);
void I_reset_servers(void);

/* i_layout.c */
void I_cleanup(void);
void I_dispatch(const SDL_Event *);
void I_enter_limbo(void);
void I_init(void);
void I_leave_limbo(void);
void I_parse_config(void);
void I_render(void);
void I_update_colors(void);

extern int i_limbo;

/* i_nations.c */
void I_enable_nation(int nation, bool enable);
void I_select_nation(int nation);

/* i_players.c */
void I_configure_player(int index, const char *name, i_color_t, bool host);
void I_update_player(int player, int gold, short ping);
void I_configure_player_num(int num);

/* i_quick_info.c */
void I_quick_info_add(const char *label, const char *value);
void I_quick_info_add_color(const char *label, const char *value, i_color_t);
void I_quick_info_close(void);
void I_quick_info_show(const char *title, const c_vec3_t *goto_pos);

/* i_ring.c */
void I_reset_ring(void);
void I_add_to_ring(i_ring_icon_t, int enabled, const char *label,
                   const char *sub_label);
void I_show_ring(i_ring_f callback);
#define I_RING_CALLBACK 1431423
extern int i_ri_board;
extern int i_ri_follow;

/* i_trade.c */
void I_configure_cargo(int index, const i_cargo_info_t *);
void I_disable_trade(void);
void I_enable_trade(bool left_own, bool right_own, const char *partner,
                    int used, int capacity);

/* i_variables.c */
void I_register_variables(void);

/* i_window.c */
void I_popup(c_vec3_t *goto_pos, const char *message);

