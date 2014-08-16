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

/* Duration that chat stays on the screen */
#define CHAT_DURATION 15000

/* Maximum number of chat lines */
#define CHAT_LINES 10

/* Maximum players in player list */
#define PLAYERS N_CLIENTS_MAX

/* Chat line widget */
typedef struct chat {
        i_widget_t widget;
        i_label_t name, text;
        int time;
} chat_t;

/* Player list widget */
typedef struct player_line {
        i_box_t box;
        i_checkbox_t checkbox;
        i_label_t name;
} player_line_t;

/* Fraction of input window length */
#define SCROLLBACK_FRACTION 0.7f
#define PLAYERLIST_FRACTION 0.3f

static chat_t chat_lines[CHAT_LINES];
static i_box_t chat_box, input_box;
static i_entry_t input;
static i_window_t input_window;
static i_button_t scrollback_button;
static i_scrollback_t scrollback;
static i_window_t playerlist_window;
static i_button_t playerlist_button;
static i_box_t    select_box;
static i_label_t  select_box_label;
static i_button_t select_all_button;
static i_button_t select_none_button;
static i_button_t select_team_button;
static bool playerlist_shown;
static player_line_t player_lines[PLAYERS];

/******************************************************************************\
 Chat widget event function.
\******************************************************************************/
static int chat_event(chat_t *chat, i_event_t event)
{
        switch (event) {
        case I_EV_CONFIGURE:
                I_widget_pack(&chat->widget, I_PACK_H, I_FIT_NONE);
                chat->widget.size = I_widget_child_bounds(&chat->widget);
                return FALSE;
        case I_EV_RENDER:
                if (chat->time >= 0 && c_time_msec - chat->time > CHAT_DURATION)
                        I_widget_event(&chat->widget, I_EV_HIDE);
                break;
        default:
                break;
        }
        return TRUE;
}

/******************************************************************************\
 Initialize a chat widget.
\******************************************************************************/
static void chat_init(chat_t *chat, const char *name, i_color_t color,
                      const char *text)
{
        I_widget_init(&chat->widget, "Chat Line");
        chat->widget.state = I_WS_READY;
        chat->widget.event_func = (i_event_f)chat_event;
        chat->time = c_time_msec;

        /* No text */
        if (!text || !text[0]) {
                I_label_init(&chat->name, name);
                chat->name.color = color;
                I_widget_add(&chat->widget, &chat->name.widget);
                return;
        }

        /* /me action */
        if ((text[0] == '/' || text[0] == '\\') &&
            !strncasecmp(text + 1, "me ", 3)) {
                I_label_init(&chat->name, C_va("*** %s", name));
                text += 4;
        }

        /* Normal colored name */
        else
                I_label_init(&chat->name, C_va("%s:", name));

        chat->name.color = color;
        I_widget_add(&chat->widget, &chat->name.widget);

        /* Message text */
        I_label_init(&chat->text, text);
        chat->text.widget.expand = TRUE;
        I_widget_add(&chat->widget, &chat->text.widget);
}

/******************************************************************************\
 Allocate a new chat widget. Allocated chats don't vanish.
\******************************************************************************/
static chat_t *chat_alloc(const char *name, i_color_t color, const char *text)
{
        chat_t *chat;

        chat = C_malloc(sizeof (*chat));
        chat_init(chat, name, color, text);
        chat->time = -1;
        chat->widget.heap = TRUE;
        return chat;
}

/******************************************************************************\
 Position the chat windows after a theme change.
\******************************************************************************/
void I_position_chat(void)
{
        float scrollback_width, playerlist_width;

        /* Position window */
        I_widget_move(&input_window.widget,
                      C_vec2((float)i_border.value.n,
                             i_right_toolbar.widget.origin.y -
                             input_window.widget.size.y - i_border.value.n));

        /* Position chat box */
        I_widget_move(&chat_box.widget,
                      C_vec2((float)i_border.value.n,
                             input_window.widget.origin.y -
                             chat_box.widget.size.y));

        /* Position scrollback */
        I_widget_move(&scrollback.widget,
                      C_vec2(input_window.widget.origin.x,
                             input_window.widget.origin.y -
                             scrollback.widget.size.y -
                             i_border.value.n * chat_box.widget.padding));

        /* Position playerlist */
        I_widget_move(&playerlist_window.widget,
                      C_vec2(input_window.widget.origin.x +
                             input_window.widget.size.x * SCROLLBACK_FRACTION,
                             input_window.widget.origin.y -
                             playerlist_window.widget.size.y -
                             i_border.value.n * chat_box.widget.padding));

        /* Make the scrollback and player list look nicely aligned to
         * the input window */
        scrollback_width = (input_window.widget.size.x * SCROLLBACK_FRACTION) -
                           scrollback.widget.padding * i_border.value.n * 2.f;
        playerlist_width = (input_window.widget.size.x * PLAYERLIST_FRACTION) -
                      playerlist_window.widget.padding * i_border.value.n * 2.f;
        if (playerlist_width != playerlist_window.widget.size.x) {
                playerlist_window.widget.size.x = playerlist_width;
                I_widget_event(&input_window.widget, I_EV_CONFIGURE);
        }
        if (scrollback_width != scrollback.widget.size.x) {
                scrollback.widget.size.x = scrollback_width;
                I_widget_event(&scrollback.widget, I_EV_CONFIGURE);
        }
}

/******************************************************************************\
 Hide the chat input window.
\******************************************************************************/
void I_hide_chat(void)
{
        I_widget_event(&input_window.widget, I_EV_HIDE);
        I_widget_event(&playerlist_window.widget, I_EV_HIDE);
}

/******************************************************************************\
 User pressed enter on the input box.
\******************************************************************************/
static void input_enter(void)
{
        int i, clients;
        bool all;

        clients = 0;
        all = TRUE;

        for (i = 0; i < PLAYERS; i++) {
                if(!N_client_valid(i) || i == n_client_id)
                        continue;
                if(player_lines[i].checkbox.checkbox_checked)
                        clients |= 1 << i;
                else
                        all = FALSE;
        }

        if(all)
                G_input_chat(input.buffer);
        else
                G_input_privmsg(input.buffer, clients);

        I_hide_chat();
        I_entry_configure(&input, "");
}

/******************************************************************************\
 Show or hide the chat scrollback.
\******************************************************************************/
static void show_scrollback(bool show)
{
        I_widget_event(&scrollback.widget, show ? I_EV_SHOW : I_EV_HIDE);
        I_widget_event(&chat_box.widget, show ? I_EV_HIDE : I_EV_SHOW);
}

/******************************************************************************\
 Scrollback button was clicked.
\******************************************************************************/
static void scrollback_button_click(void)
{
        show_scrollback(!scrollback.widget.shown);
}

/******************************************************************************\
 Show or hide the chat playerlist.
\******************************************************************************/
static void show_playerlist(bool show)
{
        playerlist_shown = show;
        I_widget_event(&playerlist_window.widget, show ? I_EV_SHOW : I_EV_HIDE);
}

/******************************************************************************\
 playerlist button was clicked.
\******************************************************************************/
static void playerlist_button_click(void)
{
        show_playerlist(!playerlist_window.widget.shown);
}

/******************************************************************************\
 playerlist select all button was clicked.
\******************************************************************************/
static void select_all_button_click(void)
{
        int i;
        for (i = 0; i < PLAYERS; i++) {
                if(!N_client_valid(i))
                        continue;
                player_lines[i].checkbox.checkbox_checked = TRUE;
        }

}

/******************************************************************************\
 playerlist select none button was clicked.
\******************************************************************************/
static void select_none_button_click(void)
{
        int i;
        for (i = 0; i < PLAYERS; i++) {
                if(!N_client_valid(i))
                        continue;
                player_lines[i].checkbox.checkbox_checked = FALSE;
        }
}

/******************************************************************************\
 playerlist select team button was clicked.
\******************************************************************************/
static void select_team_button_click(void)
{
        int i;
        i_color_t mycolor;
        
        mycolor = player_lines[n_client_id].name.color;
        for (i = 0; i < PLAYERS; i++) {
                if(!N_client_valid(i))
                        continue;
                if(player_lines[i].name.color != mycolor)
                        player_lines[i].checkbox.checkbox_checked = FALSE;
                else
                        player_lines[i].checkbox.checkbox_checked = TRUE;
        }
}

/******************************************************************************\
 A key event was delivered to the root window that may be relevant to chat.
 Returns FALSE if the event should not be propagated to the other handlers.
\******************************************************************************/
bool I_chat_event(i_event_t event)
{
        if (event != I_EV_KEY_DOWN || i_key_focus != &i_root)
                return TRUE;
        if (i_key == SDLK_ESCAPE) {
                if (!scrollback.widget.shown)
                        return TRUE;
                show_scrollback(FALSE);
                return FALSE;
        } else if (i_key == SDLK_PAGEUP) {
                show_scrollback(TRUE);
                I_scrollback_scroll(&scrollback, TRUE);
        } else if (i_key == SDLK_PAGEDOWN) {
                show_scrollback(TRUE);
                I_scrollback_scroll(&scrollback, FALSE);
        } else if (i_key == SDLK_RETURN)
                I_show_chat();
        return TRUE;
}

/******************************************************************************\
 Catch events to the entry widget.
\******************************************************************************/
static int entry_event(i_entry_t *entry, i_event_t event)
{
        if (event == I_EV_KEY_DOWN) {
                if (i_key == SDLK_PAGEUP) {
                        show_scrollback(TRUE);
                        I_scrollback_scroll(&scrollback, TRUE);
                } else if (i_key == SDLK_PAGEDOWN) {
                        show_scrollback(TRUE);
                        I_scrollback_scroll(&scrollback, FALSE);
                } else if (i_key == SDLK_ESCAPE) {
                        I_widget_event(&playerlist_window.widget, I_EV_HIDE);
                }
        }
        return I_entry_event(entry, event);
}

/******************************************************************************\
 Catch scrollback event to close it when the user wants it gone.
\******************************************************************************/
static int scrollback_event(i_scrollback_t *scrollback, i_event_t event)
{
        if ((event == I_EV_MOUSE_DOWN && i_mouse_button == SDL_BUTTON_RIGHT) ||
            (event == I_EV_KEY_DOWN && i_key == SDLK_ESCAPE))
                show_scrollback(FALSE);
        return I_scrollback_event(scrollback, event);
}

/******************************************************************************\
 Catch name label events to allow clicking of label to change checkbox
\******************************************************************************/
static int name_label_event(i_label_t *name_label, i_event_t event)
{
        i_checkbox_t *checkbox;
        int i;
        for (i = 0; i < PLAYERS; i++) {
                if(&player_lines[i].name != name_label)
                        continue;
                if(&player_lines[i].name == name_label)
                        break;
        }
        checkbox = &player_lines[i].checkbox;
        if(event == I_EV_MOUSE_DOWN) {
                if (i_mouse_button == SDL_BUTTON_LEFT)
                        checkbox->widget.state = I_WS_ACTIVE;
        } else if(event == I_EV_MOUSE_UP) {
                if (checkbox->widget.state == I_WS_ACTIVE) {
                        if (checkbox->on_change)
                                checkbox->on_change(checkbox);
                        checkbox->checkbox_checked =
                                (checkbox->checkbox_checked) ? FALSE : TRUE;
                        if (checkbox->widget.state == I_WS_ACTIVE)
                                checkbox->widget.state = I_WS_READY;
                }
                checkbox->hover_activate = FALSE;
        }
        return I_label_event(name_label, event);
}

/******************************************************************************\
 Configure a player but do not issue the configure event.
\******************************************************************************/
static void configure_player(int i, const char *name, i_color_t color)
{
        /* No player in this slot */
        if (!name || !name[0]) {
                player_lines[i].checkbox.widget.shown = FALSE;
                player_lines[i].name.widget.shown = FALSE;
                return;
        }

        /* Set player */
        player_lines[i].checkbox.widget.shown = TRUE;
        C_strncpy_buf(player_lines[i].name.buffer, name);
        player_lines[i].name.color = color;
        player_lines[i].name.widget.shown = TRUE;
}


/******************************************************************************\
 Configure a specific player.
\******************************************************************************/
void I_configure_chat_player(int index, const char *name, i_color_t color)
{
        C_assert(index >= 0 && index < PLAYERS);
        configure_player(index, name, color);
        I_widget_event(&player_lines[index].box.widget, I_EV_CONFIGURE);

        /* Window may need repacking */
        I_widget_event(I_widget_top_level(&player_lines[index].box.widget),
                       I_EV_CONFIGURE);
}

/******************************************************************************\
 Configure the pm players window.
\******************************************************************************/
void I_configure_chat_player_num(int num)
{
        int i;

        for (i = 0; i < num; i++) {
                player_lines[i].box.widget.shown = TRUE;
                player_lines[i].box.widget.pack_skip = FALSE;
//                I_configure_player(i, NULL, I_COLOR, FALSE);
        }
        for (; i < PLAYERS; i++) {
                player_lines[i].box.widget.shown = FALSE;
                player_lines[i].box.widget.pack_skip = TRUE;
        }
        if(n_client_id != N_INVALID_ID) {
                player_lines[n_client_id].box.widget.shown = FALSE;
                player_lines[n_client_id].box.widget.pack_skip = TRUE;
        } else {
                player_lines[0].box.widget.shown = FALSE;
                player_lines[0].box.widget.pack_skip = TRUE;
        }
        I_widget_event(&playerlist_window.widget, I_EV_CONFIGURE);
}

/******************************************************************************\
 Initialize a player line widget.
\******************************************************************************/
static void player_line_init(player_line_t *player, int number)
{
        /* Horizontal box */
        I_box_init(&player->box, I_PACK_H, I_FIT_NONE);
        player->box.widget.shown = FALSE;
        player->box.widget.pack_skip = TRUE;

        /* Check box */
        I_checkbox_init(&player->checkbox);
        player->checkbox.checkbox_checked = TRUE;
        I_widget_add(&player->box.widget, &player->checkbox.widget);

        /* Player name */
        I_label_init(&player->name, "");
        player->name.widget.expand = TRUE;
        player->name.widget.event_func = (i_event_f)name_label_event;
        I_widget_add(&player->box.widget, &player->name.widget);
}

/******************************************************************************\
 Initialize chat windows.
\******************************************************************************/
void I_init_chat(void)
{
        int i;

        /* Chat box */
        I_box_init(&chat_box, I_PACK_V, 0.f);
        chat_box.widget.state = I_WS_NO_FOCUS;
        chat_box.widget.padding = 1.f;
        chat_box.widget.size = C_vec2(512.f, 0.f);
        chat_box.fit = I_FIT_BOTTOM;
        I_widget_add(&i_root, &chat_box.widget);

        /* Chat scrollback */
        I_scrollback_init(&scrollback);
        scrollback.widget.event_func = (i_event_f)scrollback_event;
        scrollback.widget.steal_keys = TRUE;
        scrollback.widget.size = C_vec2(512.f * SCROLLBACK_FRACTION, 256.f);
        scrollback.widget.shown = FALSE;
        scrollback.bottom_padding = FALSE;
        I_widget_add(&i_root, &scrollback.widget);

        /* Input window */
        I_window_init(&input_window);
        input_window.widget.size = C_vec2(512.f, 0.f);
        input_window.fit = I_FIT_BOTTOM;
        input_window.popup = TRUE;
        input_window.auto_hide = TRUE;
        I_widget_add(&i_root, &input_window.widget);

        /* Input window box */
        I_box_init(&input_box, I_PACK_H, 0.f);
        I_widget_add(&input_window.widget, &input_box.widget);

        /* Input entry widget */
        I_entry_init(&input, "");
        input.widget.event_func = (i_event_f)entry_event;
        input.widget.expand = TRUE;
        input.on_enter = (i_callback_f)input_enter;
        I_widget_add(&input_box.widget, &input.widget);

        /* Scrollback button */
        I_button_init(&scrollback_button, "gui/icons/scrollback.png",
                      NULL, I_BT_ROUND);
        scrollback_button.widget.margin_front = 0.5f;
        scrollback_button.on_click = (i_callback_f)scrollback_button_click;
        I_widget_add(&input_box.widget, &scrollback_button.widget);

        /* Playerlist button */
        I_button_init(&playerlist_button, "gui/icons/chat-targets.png",
                      NULL, I_BT_ROUND);
        playerlist_button.widget.margin_front = 0.5f;
        playerlist_button.on_click = (i_callback_f)playerlist_button_click;
        I_widget_add(&input_box.widget, &playerlist_button.widget);

        /* Playerlist window */
        I_window_init(&playerlist_window);
        playerlist_window.widget.size = C_vec2(512.f * PLAYERLIST_FRACTION, 0.f);
        playerlist_window.fit = I_FIT_TOP;
        playerlist_window.popup = TRUE;
        playerlist_window.auto_hide = TRUE;
        playerlist_window.widget.shown = FALSE;
        I_widget_add(&i_root, &playerlist_window.widget);

        /* Box label */
        I_label_init(&select_box_label, "Select");
        select_box_label.justify = I_JUSTIFY_CENTER;
        I_widget_add(&playerlist_window.widget, &select_box_label.widget);

        /*  Box for player selection buttons */
        I_box_init(&select_box, I_PACK_H, I_FIT_NONE);
        I_widget_add(&playerlist_window.widget, &select_box.widget);

        /* Select all button */
        I_button_init(&select_all_button, NULL, "All", I_BT_SQUARE);
        select_all_button.widget.margin_front = 2.0f;
        select_all_button.widget.margin_rear = 0.5f;
        select_all_button.on_click = (i_callback_f)select_all_button_click;
        I_widget_add(&select_box.widget, &select_all_button.widget);

        /* Select none button */
        I_button_init(&select_none_button, NULL, "None", I_BT_SQUARE);
        select_none_button.widget.margin_front = 0.5f;
        select_none_button.widget.margin_rear = 0.5f;
        select_none_button.on_click = (i_callback_f)select_none_button_click;
        I_widget_add(&select_box.widget, &select_none_button.widget);

        /* Select team button */
        I_button_init(&select_team_button, NULL, "Team", I_BT_SQUARE);
        select_team_button.widget.margin_front = 0.5f;
        select_team_button.widget.margin_rear = 0.5f;
        select_team_button.on_click = (i_callback_f)select_team_button_click;
        I_widget_add(&select_box.widget, &select_team_button.widget);

        /* Players */
        for (i = 0; i < PLAYERS; i++) {
                player_line_init(player_lines + i, i);
                I_widget_add(&playerlist_window.widget,
                             &player_lines[i].box.widget);
        }
}

/******************************************************************************\
 Focuses the chat input entry widget if it is open.
\******************************************************************************/
void I_focus_chat(void)
{
        if (input_window.widget.shown)
                I_widget_focus(&input.widget, TRUE, FALSE);
}

/******************************************************************************\
 Show the chat input window.
\******************************************************************************/
void I_show_chat(void)
{
        if (i_limbo)
                return;
        I_entry_configure(&input, "");
        I_widget_event(&input_window.widget, I_EV_SHOW);
        I_widget_event(&playerlist_window.widget,
                       playerlist_shown ? I_EV_SHOW : I_EV_HIDE);
        I_focus_chat();
}

/******************************************************************************\
 Print something in chat.
\******************************************************************************/
void I_print_chat(const char *name, i_color_t color, const char *message)
{
        chat_t *chat;
        int i, oldest;

        /* Remove hidden chat lines */
        for (i = 0; i < CHAT_LINES; i++)
                if (chat_lines[i].widget.parent &&
                    !chat_lines[i].widget.shown &&
                    chat_lines[i].widget.fade <= 0.f)
                        I_widget_remove(&chat_lines[i].widget, TRUE);

        /* Find a free chat line */
        for (oldest = i = 0; chat_lines[i].widget.parent; i++) {
                if (chat_lines[i].time < chat_lines[oldest].time)
                        oldest = i;
                if (i >= CHAT_LINES - 1) {
                        i = oldest;
                        I_widget_remove(&chat_lines[i].widget, TRUE);
                        break;
                }
        }

        /* Initialize and add it back */
        chat_init(chat_lines + i, name, color, message);
        I_widget_add(&chat_box.widget, &chat_lines[i].widget);
        I_widget_event(&chat_box.widget, I_EV_CONFIGURE);
        I_position_chat();

        /* Add a copy to the scrollback */
        chat = chat_alloc(name, color, message);
        I_widget_add(&scrollback.widget, &chat->widget);

        /* Debug log the chat */
        if (!message || !message[0])
                C_debug("%s", name);
        else
                C_debug("%s: %s", name, message);
}

