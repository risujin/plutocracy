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

static r_texture_t *work_area;

/******************************************************************************\
 Initialize entry widget themeable assets.
\******************************************************************************/
void I_theme_entries(void)
{
        I_theme_texture(&work_area, "work_area");
}

/******************************************************************************\
 Set the entry widget cursor position.
\******************************************************************************/
static void entry_set_pos(i_entry_t *entry, int pos)
{
        float diff;
        int buffer_len, buffer_chars, buf_i, pos_i;
        char buf[256];

        /* Make sure position stays in range */
        buffer_len = C_utf8_strlen(entry->buffer, &buffer_chars);
        if (pos < 0)
                pos = 0;
        if (pos > buffer_chars)
                pos = buffer_chars;
        pos_i = C_utf8_index(entry->buffer, pos);

        /* Render the inverted cursor character */
        if (entry->buffer[pos_i]) {
                buf_i = 0;
                C_utf8_append(buf, &buf_i, sizeof (buf), entry->buffer + pos_i);
                buf[buf_i] = NUL;
        } else {
                buf[0] = ' ';
                buf[1] = NUL;
        }
        R_sprite_cleanup(&entry->cursor);
        R_sprite_init_text(&entry->cursor, R_FONT_CONSOLE, 0,
                           i_shadow.value.f, TRUE, buf);
        entry->cursor.modulate = i_colors[I_COLOR];
        entry->cursor.origin = entry->text.origin;
        if (pos > 0) {
                int x;
                x = R_font_index_to_x(R_FONT_CONSOLE, entry->buffer, pos);
                entry->cursor.origin.x += x;
        }

        /* Check scrolling */
        diff = entry->cursor.origin.x + entry->cursor.size.x -
               entry->widget.origin.x - entry->widget.size.x;
        if (diff > 0) {
                entry->scroll += diff;
                entry->cursor.origin.x -= diff;
                entry->text.origin.x -= diff;
        }
        diff = entry->widget.origin.x - entry->cursor.origin.x;
        if (diff > 0) {
                entry->scroll -= diff;
                entry->cursor.origin.x += diff;
                entry->text.origin.x += diff;
        }

        entry->pos = pos;
}

/******************************************************************************\
 Setup an entry widget text and cursor positions after a move.
\******************************************************************************/
static void entry_moved(i_entry_t *entry)
{
        c_vec2_t origin;

        origin = entry->widget.origin;
        entry->window.sprite.origin = origin;
        origin.x -= entry->scroll;
        origin = R_pixel_clamp(origin);
        entry->text.origin = origin;
        entry_set_pos(entry, entry->pos);
}

/******************************************************************************\
 Reinitializes the entry widget sprites after the buffer is modified.
\******************************************************************************/
static void entry_modified(i_entry_t *entry)
{
        R_sprite_cleanup(&entry->text);
        R_sprite_init_text(&entry->text, R_FONT_CONSOLE, 0,
                           i_shadow.value.f, FALSE, entry->buffer);
        entry->text.modulate = i_colors[I_COLOR];
        entry_moved(entry);

        /* Active change handler */
        if (entry->on_change)
                entry->on_change(entry);
}

/******************************************************************************\
 Delete a character from an entry widget buffer.
\******************************************************************************/
static void entry_delete(i_entry_t *entry, int pos)
{
        int buffer_len, size, pos_i;

        if (pos < 0)
                return;
        buffer_len = C_strlen(entry->buffer);
        pos_i = C_utf8_index(entry->buffer, pos);
        if (pos_i >= buffer_len)
                return;
        size = C_utf8_size(entry->buffer[pos_i]);
        memmove(entry->buffer + pos_i, entry->buffer + pos_i + size,
                buffer_len - pos_i - size);
        entry->buffer[buffer_len - size] = NUL;
        entry->pos = pos;
        entry_modified(entry);
}

/******************************************************************************\
 Insert a Unicode character into an entry widget buffer.
\******************************************************************************/
static void entry_insert(i_entry_t *entry, int ch)
{
        char *str;
        int pos_i, buffer_len, str_len;

        str = C_utf8_encode(ch, &str_len);
        buffer_len = C_strlen(entry->buffer);
        if (buffer_len >= (int)sizeof (entry->buffer) - str_len)
                return;
        pos_i = C_utf8_index(entry->buffer, entry->pos);
        if (pos_i < buffer_len)
                memmove(entry->buffer + pos_i + str_len,
                        entry->buffer + pos_i, buffer_len - pos_i);
        memcpy(entry->buffer + pos_i, str, str_len);
        entry->pos++;
        entry->buffer[buffer_len + str_len] = NUL;
        entry_modified(entry);
}

/******************************************************************************\
 Sets the entry field text and resets the cursor position. [string] does
 not need to persist after this function is called.
\******************************************************************************/
void I_entry_configure(i_entry_t *entry, const char *string)
{
        C_strncpy_buf(entry->buffer, string);
        entry->pos = C_strlen(string);
        entry->scroll = 0.f;
        I_widget_event(&entry->widget, I_EV_CONFIGURE);

        /* Active change handler */
        if (entry->on_change)
                entry->on_change(entry);
}

/******************************************************************************\
 Entry widget autocomplete.
\******************************************************************************/
static void entry_auto_complete(i_entry_t *entry)
{
        const char *token;
        char *head, *tail;
        int pos_i, swap, prehead_chars, head_chars, tail_len,
            token_len, token_chars;

        if (!entry->auto_complete || entry->just_tabbed)
                return;
        entry->just_tabbed = TRUE;
        pos_i = C_utf8_index(entry->buffer, entry->pos);

        /* Isolate and lookup the token we are trying to complete */
        head = tail = entry->buffer + pos_i;
        if (pos_i > 0)
                while (head > entry->buffer && !C_is_space(head[-1])) {
                        head--;
                }
        while (*tail && !C_is_space(*tail))
                tail++;
        swap = *head;
        *head = NUL;
        C_utf8_strlen(entry->buffer, &prehead_chars);
        *head = swap;
        swap = *tail;
        *tail = NUL;
        C_utf8_strlen(head, &head_chars);
        token = entry->auto_complete(head);
        *tail = swap;
        if (!token[0])
                return;

        /* Add the returned string to the end of the current token */
        token_len = C_utf8_strlen(token, &token_chars);
        if (token[token_len - 1] == ' ') {
                if (*tail == ' ')
                        token_len--;

                /* This is for pressing tab after a complete token, you then
                   move to the start of a new token and get a different list
                   for tab completion */
                entry->just_tabbed = FALSE;
        }
        tail_len = C_strlen(tail);
        if (tail + token_len + tail_len > entry->buffer +
                                          sizeof (entry->buffer))
                return;
        memmove(tail + token_len, tail, tail_len + 1);
        memcpy(tail, token, token_len);
        entry_modified(entry);
        entry_set_pos(entry, prehead_chars + head_chars + token_chars);
}

/******************************************************************************\
 Entry widget event function.
\******************************************************************************/
int I_entry_event(i_entry_t *entry, i_event_t event)
{
        switch (event) {
        case I_EV_CONFIGURE:

                /* Setup text */
                R_sprite_cleanup(&entry->text);
                R_sprite_init_text(&entry->text, R_FONT_CONSOLE, 0,
                                   i_shadow.value.f, FALSE, entry->buffer);
                entry->widget.size.y = R_font_line_skip(R_FONT_CONSOLE) /
                                       r_scale_2d + 1;
                entry->text.modulate = i_colors[I_COLOR];

                /* Setup work area window */
                R_window_cleanup(&entry->window);
                R_window_init(&entry->window, work_area);
                entry->window.sprite.size = entry->widget.size;

        case I_EV_MOVED:
                entry_moved(entry);
                break;
        case I_EV_CLEANUP:
                R_sprite_cleanup(&entry->text);
                R_sprite_cleanup(&entry->cursor);
                R_window_cleanup(&entry->window);
                break;
        case I_EV_KEY_DOWN:
                if (i_key == SDLK_LEFT)
                        entry_set_pos(entry, entry->pos - 1);
                else if (i_key == SDLK_RIGHT)
                        entry_set_pos(entry, entry->pos + 1);
                else if (i_key == SDLK_HOME)
                        entry_set_pos(entry, 0);
                else if (i_key == SDLK_END)
                        entry_set_pos(entry, sizeof (entry->buffer));
                else if (i_key == SDLK_BACKSPACE)
                        entry_delete(entry, entry->pos - 1);
                else if (i_key == SDLK_DELETE)
                        entry_delete(entry, entry->pos);
                else if (i_key == SDLK_RETURN && entry->on_enter)
                        entry->on_enter(entry);
                else if (i_key == SDLK_TAB) {
                        entry_auto_complete(entry);
                        break;
                } else if (!i_key_ctrl && i_key >= ' ' && i_key_unicode &&
                           i_key < 256 && i_key_unicode != '<' &&
                           i_key_unicode != '>' && i_key_unicode != '&')
                        entry_insert(entry, i_key_unicode);
                entry->just_tabbed = FALSE;
                break;
        case I_EV_RENDER:
                entry->window.sprite.modulate.a = entry->widget.fade;
                entry->text.modulate.a = entry->widget.fade;
                entry->cursor.modulate.a = entry->widget.fade;
                R_window_render(&entry->window);
                R_push_clip();
                if (i_key_focus == (i_widget_t *)entry) {
                        R_clip_left(entry->cursor.origin.x +
                                    entry->cursor.size.x - 2.f);
                        R_clip_right(entry->widget.origin.x +
                                     entry->widget.size.x);
                        R_sprite_render(&entry->text);
                        R_clip_disable();
                        if (entry->pos > 0) {
                                R_clip_left(entry->widget.origin.x);
                                R_clip_right(entry->cursor.origin.x);
                                R_sprite_render(&entry->text);
                                R_clip_disable();
                        }
                        R_sprite_render(&entry->cursor);
                } else {
                        R_clip_left(entry->widget.origin.x);
                        R_clip_right(entry->widget.origin.x +
                                     entry->widget.size.x);
                        R_sprite_render(&entry->text);
                        R_clip_disable();
                }
                R_pop_clip();
                break;
        default:
                break;
        }
        return TRUE;
}

/******************************************************************************\
 Initializes an entry widget.
\******************************************************************************/
void I_entry_init(i_entry_t *entry, const char *text)
{
        if (!entry)
                return;
        C_zero(entry);
        I_widget_init(&entry->widget, "Entry");
        entry->widget.event_func = (i_event_f)I_entry_event;
        entry->widget.state = I_WS_READY;
        entry->widget.entry = TRUE;
        C_strncpy_buf(entry->buffer, text);
}

/******************************************************************************\
 Cycle through the entry widget's input history.
\******************************************************************************/
static void history_entry_go(i_history_entry_t *he, int change)
{
        int new_pos;

        new_pos = he->pos + change;
        if (new_pos < 0)
                new_pos = 0;
        if (new_pos >= he->size)
                new_pos = he->size;
        if (he->pos == new_pos)
                return;
        if (he->pos == 0)
                C_strncpy_buf(he->history[0], he->entry.buffer);
        he->pos = new_pos;
        I_entry_configure(&he->entry, he->history[new_pos]);
}

/******************************************************************************\
 Save current input in a new history entry.
\******************************************************************************/
static void history_entry_save(i_history_entry_t *he)
{
        if (he->size) {
                if (he->size > I_ENTRY_HISTORY - 2)
                        he->size = I_ENTRY_HISTORY - 2;
                memmove(he->history + 2, he->history + 1,
                        sizeof (he->history[0]) * he->size);
        }
        C_strncpy_buf(he->history[1], he->entry.buffer);
        he->size++;
        he->pos = 0;
}

/******************************************************************************\
 History entry widget event function.
\******************************************************************************/
int I_history_entry_event(i_history_entry_t *he, i_event_t event)
{
        if (event == I_EV_KEY_DOWN) {
                if (i_key == SDLK_UP)
                        history_entry_go(he, 1);
                else if (i_key == SDLK_DOWN)
                        history_entry_go(he, -1);
                else if (i_key == SDLK_RETURN && he->entry.on_enter)
                        history_entry_save(he);
        }
        return I_entry_event(&he->entry, event);
}

/******************************************************************************\
 Initializes a history entry widget.
\******************************************************************************/
void I_history_entry_init(i_history_entry_t *he, const char *text)
{
        I_entry_init(&he->entry, text);
        he->size = he->pos = 0;
}

