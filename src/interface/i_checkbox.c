/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin
              Copyright (C) 2008 - John Black


 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "i_common.h"

static r_texture_t *checkbox_texture, *checkmark, *active, *hover;

/******************************************************************************\
 Called when the checkmark theme assets need to be initialized.
\******************************************************************************/
void I_theme_checkboxs(void)
{
        /* Decorated buttons */
        I_theme_texture(&checkbox_texture, "checkbox");
        I_theme_texture(&checkmark, "checkmark");
        I_theme_texture(&active, "checkbox_active");
        I_theme_texture(&hover, "checkbox_hover");
}

/******************************************************************************\
 Button widget event function.
\******************************************************************************/
int I_checkbox_event(i_checkbox_t *checkbox, i_event_t event)
{
        c_vec2_t origin, size;
        float width;

        switch (event) {
        case I_EV_CONFIGURE:

                /* Setup decorations */
                R_window_cleanup(&checkbox->normal);
                R_sprite_cleanup(&checkbox->checkmark);
                R_sprite_cleanup(&checkbox->hover);
                R_sprite_cleanup(&checkbox->active);

                R_window_init(&checkbox->normal, checkbox_texture);
                R_sprite_init(&checkbox->checkmark, checkmark);
                R_sprite_init(&checkbox->hover, hover);
                R_sprite_init(&checkbox->active, active);

//                /* Setup text */
//                R_sprite_cleanup(&checkbox->text);
//                R_sprite_init_text(&checkbox->text, R_FONT_GUI, 0.f,
//                                   i_shadow.value.f, FALSE, checkbox->buffer);
//                checkbox->text.modulate = i_colors[I_COLOR];

                checkbox->widget.size.y = checkbox->normal.sprite.size.y;
                checkbox->widget.size.x = checkbox->normal.sprite.size.x;

//                /* Size requisition */
//                if (checkbox->widget.size.y < 1) {
//                        checkbox->widget.size.y = checkbox->text.size.y;
//                        if (checkbox->icon.texture &&
//                            checkbox->icon.size.y > checkbox->widget.size.y)
//                                checkbox->widget.size.y = checkbox->icon.size.y;
//                        if (checkbox->type == I_BT_DECORATED)
//                                checkbox->widget.size.y += i_border.value.n * 2;
//                }
//                if (checkbox->widget.size.x < 1) {
//                        checkbox->widget.size.x = checkbox->text.size.x +
//                                                checkbox->icon.size.x;
//                        if (checkbox->type == I_BT_DECORATED)
//                                checkbox->widget.size.x += i_border.value.n * 2;
//                        if (checkbox->icon.texture && checkbox->buffer[0])
//                                checkbox->widget.size.x += i_border.value.n;
//                }

        case I_EV_MOVED:

                /* Setup decorations (for each state) to cover full area */
                origin = checkbox->widget.origin;
                size = checkbox->widget.size;
//                if (checkbox->type == I_BT_DECORATED) {
                        checkbox->normal.sprite.origin = origin;
                        checkbox->normal.sprite.size = size;
                        checkbox->checkmark.origin = origin;
                        checkbox->checkmark.size = size;
                        checkbox->hover.origin = origin;
                        checkbox->hover.size = size;
                        checkbox->active.origin = origin;
                        checkbox->active.size = size;
//                } else {
//                        c_vec2_t origin2, size2;
//                        float border;
//
//                        border = (float)i_border.value.n;
//                        if (border > size.x / 4)
//                                border = size.x / 4;
//                        if (border > size.y / 4)
//                                border = size.y / 4;
//                        origin2 = C_vec2_subf(origin, border);
//                        size2 = C_vec2_addf(size, border * 2.f);
//                        checkbox->icon_active.origin = origin2;
//                        checkbox->icon_active.size = size2;
//                        checkbox->icon_hover.origin = origin2;
//                        checkbox->icon_hover.size = size2;
//                }

//                /* Pack the icon left, vertically centered */
//                origin.y += size.y / 2;
//                if (checkbox->type == I_BT_DECORATED) {
//                        origin.x += i_border.value.n;
//                        size = C_vec2_subf(size, i_border.value.n * 2.f);
//                }
//                if (checkbox->icon.texture) {
//                        checkbox->icon.origin = C_vec2(origin.x, origin.y -
//                                                     checkbox->icon.size.y / 2);
//                        width = checkbox->icon.size.x + i_border.value.n;
//                        origin.x += width;
//                        size.x -= width;
//                }
//
//                /* Pack text */
//                checkbox->text.origin = origin;
//                checkbox->text.origin.y -= checkbox->text.size.y / 2;
//
//                /* If there is some room left, center the icon and text */
//                width = (size.x - checkbox->text.size.x) / 2.f;
//                if (width > 0.f) {
//                        checkbox->icon.origin.x += width;
//                        checkbox->text.origin.x += width;
//                }
//
//                /* Clamp origins to prevent blurriness */
//                checkbox->text.origin = R_pixel_clamp(checkbox->text.origin);

                break;
        case I_EV_CLEANUP:
                R_window_cleanup(&checkbox->normal);
                R_sprite_cleanup(&checkbox->checkmark);
                R_sprite_cleanup(&checkbox->hover);
                R_sprite_cleanup(&checkbox->active);
                break;
        case I_EV_MOUSE_MOVE:
                if (checkbox->hover_activate)
                        checkbox->widget.state = I_WS_ACTIVE;
                break;
        case I_EV_MOUSE_DOWN:
                if (i_mouse_button == SDL_BUTTON_LEFT)
                        checkbox->widget.state = I_WS_ACTIVE;
                break;
        case I_EV_MOUSE_UP:
                if (checkbox->widget.state == I_WS_ACTIVE) {
                        if (checkbox->on_change)
                                checkbox->on_change(checkbox);
                        checkbox->checkbox_checked =
                                (checkbox->checkbox_checked) ? FALSE : TRUE;
                        if (checkbox->widget.state == I_WS_ACTIVE)
                                checkbox->widget.state = I_WS_READY;
                }
                checkbox->hover_activate = FALSE;
                break;
        case I_EV_RENDER:
//                checkbox->checkbox.modulate.a = checkbox->widget.fade;
//                checkbox->text.modulate.a = checkbox->widget.fade;
                checkbox->normal.sprite.modulate.a = checkbox->widget.fade;
                checkbox->checkmark.modulate.a = checkbox->widget.fade;
                R_window_render(&checkbox->normal);
                if (checkbox->widget.state == I_WS_HOVER) {
//                        R_window_render(&checkbox->hover);
//                        checkbox->icon_hover.modulate.a = checkbox->widget.fade;
                        R_sprite_render(&checkbox->hover);
                } else if (checkbox->widget.state == I_WS_ACTIVE) {
//                        R_window_render(&checkbox->active);
//                        checkbox->icon_active.modulate.a = checkbox->widget.fade;
                        R_sprite_render(&checkbox->active);
                }
                if(checkbox->checkbox_checked)
                        R_sprite_render(&checkbox->checkmark);
                R_push_clip();
                R_clip_rect(checkbox->widget.origin, checkbox->widget.size);
//                R_sprite_render(&checkbox->icon);
//                R_sprite_render(&checkbox->text);
                R_pop_clip();
                break;
        default:
                break;
        }
        return TRUE;
}

///******************************************************************************\
// Configure the button widget. The button must have already been initialized.
// The [icon] path and [text] strings can be NULL and do not need to persist
// after calling the function.
//\******************************************************************************/
//void I_checkbox_configure(i_checkbox_t *button, const char *icon,
//                          const char *text,
//                        i_button_type_t type)
//{
//        button->type = type;
//        R_sprite_load(&button->icon, icon);
//        C_strncpy_buf(button->buffer, text);
//        I_widget_event(&button->widget, I_EV_CONFIGURE);
//}

/******************************************************************************\
 Initializes a button widget. The [icon] path and [text] strings can be NULL
 and do not need to persist after calling the function.
\******************************************************************************/
void I_checkbox_init(i_checkbox_t *checkbox)
{
        C_zero(checkbox);
        I_widget_init(&checkbox->widget, "Checkbox");
        checkbox->widget.event_func = (i_event_f)I_checkbox_event;
        checkbox->widget.state = I_WS_READY;
        checkbox->on_change = NULL;
//        checkbox->type = type;
//        R_sprite_load(&button->icon, icon);
//        C_strncpy_buf(button->buffer, text);
}
