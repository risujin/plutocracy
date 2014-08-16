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

/* Largest number of players the window supports */
#define PLAYERS 32

/* Structure for player entries */
typedef struct player_line {
        i_box_t box;
        i_label_t index, name, gold, ping;
        i_button_t kick;
} player_line_t;

static player_line_t players[PLAYERS];
static i_label_t title;

/******************************************************************************\
 Player clicked the kick button.
\******************************************************************************/
static void kick_clicked(i_button_t *button)
{
        /* This weirdness avoids GCC's 64-bit bitching */
        G_kick_client((int)(size_t)button->data);
}

/******************************************************************************\
 Configure a player but do not issue the configure event.
\******************************************************************************/
static void configure_player(int i, const char *name, i_color_t color,
                             bool host)
{
        /* No player in this slot */
        if (!name || !name[0]) {
                players[i].index.widget.state = I_WS_DISABLED;
                players[i].kick.widget.state = I_WS_DISABLED;
                players[i].name.widget.shown = FALSE;
                players[i].gold.widget.state = I_WS_DISABLED;
                players[i].ping.widget.state = I_WS_DISABLED;
                return;
        }

        /* Set player */
        players[i].index.widget.state = I_WS_READY;
        C_strncpy_buf(players[i].name.buffer, name);
        players[i].name.color = color;
        players[i].name.widget.shown = TRUE;
        players[i].gold.widget.state = I_WS_READY;
        players[i].ping.widget.state = I_WS_READY;
        players[i].kick.widget.state = host ? I_WS_READY : I_WS_DISABLED;
}

/******************************************************************************\
 Configure a specific player.
\******************************************************************************/
void I_configure_player(int index, const char *name, i_color_t color, bool host)
{
        I_configure_chat_player(index, name, color);
        C_assert(index >= 0 && index < PLAYERS);
        configure_player(index, name, color, host);
        I_widget_event(&players[index].box.widget, I_EV_CONFIGURE);

        /* Window may need repacking */
        I_widget_event(I_widget_top_level(&players[index].box.widget),
                       I_EV_CONFIGURE);
}

/******************************************************************************\
 Configure the players window.
\******************************************************************************/
void I_configure_player_num(int num)
{
        int i;

        I_configure_chat_player_num(num);
        for (i = 0; i < num; i++) {
                players[i].box.widget.shown = TRUE;
                players[i].box.widget.pack_skip = FALSE;
                I_configure_player(i, NULL, I_COLOR, FALSE);
        }
        for (; i < PLAYERS; i++) {
                players[i].box.widget.shown = FALSE;
                players[i].box.widget.pack_skip = TRUE;
        }
        I_widget_event(&i_right_toolbar.windows[i_players_button].widget,
                       I_EV_CONFIGURE);
        I_toolbar_position(&i_right_toolbar, i_players_button);
}

/******************************************************************************\
 Update ping time and gold
\******************************************************************************/
void I_update_player(int player, int gold, short ping)
{
        char *buff;
        buff = C_va("% 5d", gold);
        C_strncpy_buf(players[player].gold.buffer, buff);
        buff = C_va("% 3d msec", ping);
        C_strncpy_buf(players[player].ping.buffer, buff);
        I_widget_event(&players[player].box.widget, I_EV_CONFIGURE);

        /* Window may need repacking */
        I_widget_event(I_widget_top_level(&players[player].box.widget),
                       I_EV_CONFIGURE);
}

/******************************************************************************\
 Initialize the players window.
\******************************************************************************/
void I_init_players(i_window_t *window)
{
        int i;

        I_window_init(window);
        window->widget.size = C_vec2(250.f, 0.f);
        window->fit = I_FIT_TOP;

        /* Label */
        I_label_init(&title, "Players");
        title.font = R_FONT_TITLE;
        I_widget_add(&window->widget, &title.widget);

        /* Players */
        for (i = 0; i < PLAYERS; i++) {

                /* Horizontal box */
                I_box_init(&players[i].box, I_PACK_H, I_FIT_NONE);
                players[i].box.widget.shown = FALSE;
                players[i].box.widget.pack_skip = TRUE;
                I_widget_add(&window->widget, &players[i].box.widget);

                /* Index label */
                I_label_init(&players[i].index, C_va("%d.", i + 1));
                players[i].index.widget.state = I_WS_DISABLED;
                I_widget_add(&players[i].box.widget, &players[i].index.widget);

                /* Name label */
                I_label_init(&players[i].name, "");
                players[i].name.widget.expand = TRUE;
                I_widget_add(&players[i].box.widget, &players[i].name.widget);

                /* Gold label */
                I_label_init(&players[i].gold, "");
                players[i].gold.widget.state = I_WS_DISABLED;
                I_widget_add(&players[i].box.widget, &players[i].gold.widget);

                /* Ping time label */
                I_label_init(&players[i].ping, "     ");
                players[i].ping.widget.state = I_WS_DISABLED;
                players[i].ping.widget.size.y = 30.0f;
                I_widget_add(&players[i].box.widget, &players[i].ping.widget);

                /* Kick button */
                I_button_init(&players[i].kick, "gui/icons/kick.png", NULL,
                              I_BT_ROUND);
                players[i].kick.widget.state = I_WS_DISABLED;
                players[i].kick.on_click = (i_callback_f)kick_clicked;
                players[i].kick.data = (void *)(size_t)i;
                I_widget_add(&players[i].box.widget, &players[i].kick.widget);

                /* First line does not have a kick button */
                if (!i)
                        players[i].kick.widget.shown = FALSE;
        }
}

