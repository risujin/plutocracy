/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* The button widget implements clickable decorated buttons and toolbar icons */

#include "i_common.h"

static r_texture_t *decor_normal, *decor_hover, *decor_active,
                   *round_hover, *round_active, *square_hover, *square_active;

/******************************************************************************\
 Called when the button theme assets need to be initialized.
\******************************************************************************/
void I_theme_buttons(void)
{
        /* Decorated buttons */
        I_theme_texture(&decor_normal, "button");
        I_theme_texture(&decor_hover, "button_hover");
        I_theme_texture(&decor_active, "button_active");

        /* Icon buttons */
        I_theme_texture(&round_hover, "round_hover");
        I_theme_texture(&round_active, "round_active");
        I_theme_texture(&square_hover, "square_hover");
        I_theme_texture(&square_active, "square_active");
}

/******************************************************************************\
 Button widget event function.
\******************************************************************************/
int I_button_event(i_button_t *button, i_event_t event)
{
        c_vec2_t origin, size;
        float width;

        switch (event) {
        case I_EV_CONFIGURE:

                /* Setup decorations */
                R_window_cleanup(&button->normal);
                R_window_cleanup(&button->active);
                R_window_cleanup(&button->hover);
                R_sprite_cleanup(&button->icon_active);
                R_sprite_cleanup(&button->icon_hover);
                if (button->type == I_BT_DECORATED) {
                        R_window_init(&button->normal, decor_normal);
                        R_window_init(&button->hover, decor_hover);
                        R_window_init(&button->active, decor_active);
                } else if (button->type == I_BT_SQUARE) {
                        R_sprite_init(&button->icon_active, square_active);
                        R_sprite_init(&button->icon_hover, square_hover);
                } else if (button->type == I_BT_ROUND) {
                        R_sprite_init(&button->icon_active, round_active);
                        R_sprite_init(&button->icon_hover, round_hover);
                } else
                        C_error("Invalid button type %d", button->type);

                /* Setup text */
                R_sprite_cleanup(&button->text);
                R_sprite_init_text(&button->text, R_FONT_GUI, 0.f,
                                   i_shadow.value.f, FALSE, button->buffer);
                button->text.modulate = i_colors[I_COLOR];

                /* Size requisition */
                if (button->widget.size.y < 1) {
                        button->widget.size.y = button->text.size.y;
                        if (button->icon.texture &&
                            button->icon.size.y > button->widget.size.y)
                                button->widget.size.y = button->icon.size.y;
                        if (button->type == I_BT_DECORATED)
                                button->widget.size.y += i_border.value.n * 2;
                }
                if (button->widget.size.x < 1) {
                        button->widget.size.x = button->text.size.x +
                                                button->icon.size.x;
                        if (button->type == I_BT_DECORATED)
                                button->widget.size.x += i_border.value.n * 2;
                        if (button->icon.texture && button->buffer[0])
                                button->widget.size.x += i_border.value.n;
                }

        case I_EV_MOVED:

                /* Setup decorations (for each state) to cover full area */
                origin = button->widget.origin;
                size = button->widget.size;
                if (button->type == I_BT_DECORATED) {
                        button->normal.sprite.origin = origin;
                        button->normal.sprite.size = size;
                        button->hover.sprite.origin = origin;
                        button->hover.sprite.size = size;
                        button->active.sprite.origin = origin;
                        button->active.sprite.size = size;
                } else {
                        c_vec2_t origin2, size2;
                        float border;

                        border = (float)i_border.value.n;
                        if (border > size.x / 4)
                                border = size.x / 4;
                        if (border > size.y / 4)
                                border = size.y / 4;
                        origin2 = C_vec2_subf(origin, border);
                        size2 = C_vec2_addf(size, border * 2.f);
                        button->icon_active.origin = origin2;
                        button->icon_active.size = size2;
                        button->icon_hover.origin = origin2;
                        button->icon_hover.size = size2;
                }

                /* Pack the icon left, vertically centered */
                origin.y += size.y / 2;
                if (button->type == I_BT_DECORATED) {
                        origin.x += i_border.value.n;
                        size = C_vec2_subf(size, i_border.value.n * 2.f);
                }
                if (button->icon.texture) {
                        button->icon.origin = C_vec2(origin.x, origin.y -
                                                     button->icon.size.y / 2);
                        width = button->icon.size.x + i_border.value.n;
                        origin.x += width;
                        size.x -= width;
                }

                /* Pack text */
                button->text.origin = origin;
                button->text.origin.y -= button->text.size.y / 2;

                /* If there is some room left, center the icon and text */
                width = (size.x - button->text.size.x) / 2.f;
                if (width > 0.f) {
                        button->icon.origin.x += width;
                        button->text.origin.x += width;
                }

                /* Clamp origins to prevent blurriness */
                button->text.origin = R_pixel_clamp(button->text.origin);

                break;
        case I_EV_CLEANUP:
                R_window_cleanup(&button->normal);
                R_window_cleanup(&button->active);
                R_window_cleanup(&button->hover);
                R_sprite_cleanup(&button->icon_active);
                R_sprite_cleanup(&button->icon_hover);
                R_sprite_cleanup(&button->icon);
                R_sprite_cleanup(&button->text);
                break;
        case I_EV_MOUSE_MOVE:
                if (button->hover_activate)
                        button->widget.state = I_WS_ACTIVE;
                break;
        case I_EV_MOUSE_DOWN:
                if (i_mouse_button == SDL_BUTTON_LEFT)
                        button->widget.state = I_WS_ACTIVE;
                break;
        case I_EV_MOUSE_UP:
                if (button->widget.state == I_WS_ACTIVE) {
                        if (button->on_click)
                                button->on_click(button);
                        if (button->widget.state == I_WS_ACTIVE)
                                button->widget.state = I_WS_READY;
                }
                button->hover_activate = FALSE;
                break;
        case I_EV_RENDER:
                button->icon.modulate.a = button->widget.fade;
                button->text.modulate.a = button->widget.fade;
                button->hover.sprite.modulate.a = button->widget.fade;
                button->active.sprite.modulate.a = button->widget.fade;
                button->normal.sprite.modulate.a = button->widget.fade;
                if (button->widget.state == I_WS_HOVER) {
                        R_window_render(&button->hover);
                        button->icon_hover.modulate.a = button->widget.fade;
                        R_sprite_render(&button->icon_hover);
                } else if (button->widget.state == I_WS_ACTIVE) {
                        R_window_render(&button->active);
                        button->icon_active.modulate.a = button->widget.fade;
                        R_sprite_render(&button->icon_active);
                } else
                        R_window_render(&button->normal);
                R_push_clip();
                R_clip_rect(button->widget.origin, button->widget.size);
                R_sprite_render(&button->icon);
                R_sprite_render(&button->text);
                R_pop_clip();
                break;
        default:
                break;
        }
        return TRUE;
}

/******************************************************************************\
 Configure the button widget. The button must have already been initialized.
 The [icon] path and [text] strings can be NULL and do not need to persist
 after calling the function.
\******************************************************************************/
void I_button_configure(i_button_t *button, const char *icon, const char *text,
                        i_button_type_t type)
{
        button->type = type;
        R_sprite_load(&button->icon, icon);
        C_strncpy_buf(button->buffer, text);
        I_widget_event(&button->widget, I_EV_CONFIGURE);
}

/******************************************************************************\
 Initializes a button widget. The [icon] path and [text] strings can be NULL
 and do not need to persist after calling the function.
\******************************************************************************/
void I_button_init(i_button_t *button, const char *icon, const char *text,
                   i_button_type_t type)
{
        C_zero(button);
        I_widget_init(&button->widget, "Button");
        button->widget.event_func = (i_event_f)I_button_event;
        button->widget.state = I_WS_READY;
        button->on_click = NULL;
        button->type = type;
        R_sprite_load(&button->icon, icon);
        C_strncpy_buf(button->buffer, text);
}

