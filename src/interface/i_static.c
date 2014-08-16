/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Simple widgets that don't interact with the user */

#include "i_common.h"

static r_texture_t *separator_tex[2], *select_tex[3];

/******************************************************************************\
 Theme images for static widgets.
\******************************************************************************/
void I_theme_statics(void)
{
        I_theme_texture(separator_tex, "sep_horizontal");
        I_theme_texture(separator_tex + 1, "sep_vertical");
        I_theme_texture(select_tex, "select_on");
        I_theme_texture(select_tex + 1, "select_off");
        I_theme_texture(select_tex + 2, "select_hover");
}

/******************************************************************************\
 Label widget event function.
\******************************************************************************/
int I_label_event(i_label_t *label, i_event_t event)
{
        switch (event) {
        case I_EV_CONFIGURE:

                /* Get fixed width from the sample text (unwrapped) */
                if (label->width_sample)
                        label->width = R_font_size(label->font,
                                                   label->width_sample).x /
                                       r_scale_2d;

                if (label->width > 0.f)
                        label->widget.size.x = label->width;
                R_text_configure(&label->text, label->font,
                                 label->widget.size.x, i_shadow.value.f,
                                 FALSE, label->buffer);
                label->widget.size.y = label->text.sprite.size.y;
                if (!label->widget.size.x)
                        label->widget.size.x = label->text.sprite.size.x;
        case I_EV_MOVED:
                label->text.sprite.origin = label->widget.origin;

                /* Justify widget text */
                if (label->text.sprite.size.x < label->widget.size.x &&
                    label->justify != I_JUSTIFY_LEFT) {
                        float space;

                        space = label->widget.size.x -
                                label->text.sprite.size.x;
                        if (label->justify == I_JUSTIFY_RIGHT)
                                label->text.sprite.origin.x += space;
                        else if (label->justify == I_JUSTIFY_CENTER)
                                label->text.sprite.origin.x += space / 2.f;
                }

                label->text.sprite.origin =
                        R_pixel_clamp(label->text.sprite.origin);
                break;
        case I_EV_CLEANUP:
                R_text_cleanup(&label->text);
                break;
        case I_EV_RENDER:
                label->text.sprite.modulate = i_colors[label->color];
                label->text.sprite.modulate.a *= label->widget.fade;
                R_text_render(&label->text);
                break;
        default:
                break;
        }
        return TRUE;
}

/******************************************************************************\
 Change a label's text.
\******************************************************************************/
void I_label_configure(i_label_t *label, const char *text)
{
        C_strncpy_buf(label->buffer, text);
        I_widget_event(&label->widget, I_EV_CONFIGURE);
}

/******************************************************************************\
 Initializes a label widget.
\******************************************************************************/
void I_label_init(i_label_t *label, const char *text)
{
        if (!label)
                return;
        C_zero(label);
        I_widget_init(&label->widget, "Label");
        R_text_init(&label->text);
        label->widget.event_func = (i_event_f)I_label_event;
        label->widget.state = I_WS_READY;
        label->font = R_FONT_GUI;
        C_strncpy_buf(label->buffer, text);
}

/******************************************************************************\
 Dynamically allocates a label widget.
\******************************************************************************/
i_label_t *I_label_alloc(const char *text)
{
        i_label_t *label;

        label = C_malloc(sizeof (*label));
        I_label_init(label, text);
        label->widget.heap = TRUE;
        return label;
}

/******************************************************************************\
 Box widget event function.
\******************************************************************************/
int I_box_event(i_box_t *box, i_event_t event)
{
        if (event != I_EV_CONFIGURE)
                return TRUE;

        /* If the non-packing width was specified ahead of time, set it now */
        if (box->width > 0.f) {
                if (box->pack_children == I_PACK_H)
                        box->widget.size.y = box->width;
                else if (box->pack_children == I_PACK_V)
                        box->widget.size.x = box->width;
        }

        I_widget_pack(&box->widget, box->pack_children, box->fit);

        /* Otherwise we can get it from looking at the size of our children */
        if (box->width <= 0.f) {
                c_vec2_t bounds;

                bounds = I_widget_child_bounds(&box->widget);
                if (box->pack_children == I_PACK_H) {
                        box->widget.size.y = bounds.y;
                        if (!box->widget.size.x)
                                box->widget.size.x = bounds.x;
                } else if (box->pack_children == I_PACK_H) {
                        box->widget.size.x = bounds.x;
                        if (!box->widget.size.y)
                                box->widget.size.y = bounds.y;
                }
        }

        return FALSE;
}

/******************************************************************************\
 Initializes a box widget.
\******************************************************************************/
void I_box_init(i_box_t *box, i_pack_t pack, float width)
{
        if (!box)
                return;
        C_zero(box);
        I_widget_init(&box->widget, "Box");
        box->widget.event_func = (i_event_f)I_box_event;
        box->widget.state = I_WS_READY;
        box->pack_children = pack;
        box->fit = I_FIT_NONE;
        box->width = width;
}

/******************************************************************************\
 Image event function.
\******************************************************************************/
int I_image_event(i_image_t *image, i_event_t event)
{
        switch (event) {
        case I_EV_CONFIGURE:
                if (image->theme_texture) {
                        R_sprite_cleanup(&image->sprite);
                        R_sprite_init(&image->sprite, *image->theme_texture);
                        image->original_size = image->sprite.size;
                }
                if (image->resize && image->widget.size.x) {
                        image->sprite.size.x = image->widget.size.x;
                        image->widget.size.y = image->original_size.y;
                } else if (image->resize && image->widget.size.y) {
                        image->sprite.size.y = image->widget.size.y;
                        image->widget.size.x = image->original_size.x;
                } else
                        image->widget.size = image->sprite.size;
        case I_EV_MOVED:
                image->sprite.origin = image->widget.origin;
                break;
        case I_EV_RENDER:
                image->sprite.modulate.a = image->widget.fade;
                R_sprite_render(&image->sprite);
                break;
        case I_EV_CLEANUP:
                R_sprite_cleanup(&image->sprite);
                break;
        default:
                break;
        }
        return TRUE;
}

/******************************************************************************\
 Initializes an image widget.
\******************************************************************************/
void I_image_init(i_image_t *image, const char *icon)
{
        if (!image)
                return;
        C_zero(image);
        I_widget_init(&image->widget, "Image");
        image->widget.event_func = (i_event_f)I_image_event;
        image->widget.state = I_WS_READY;
        if (icon) {
                R_sprite_load(&image->sprite, icon);
                image->original_size = image->sprite.size;
        }
}

/******************************************************************************\
 Initializes an image widget with a pointer to a theme-updated texture
 variable.
\******************************************************************************/
void I_image_init_themed(i_image_t *image, r_texture_t **pptex)
{
        if (!image)
                return;
        I_image_init(image, NULL);
        image->theme_texture = pptex;
}

/******************************************************************************\
 Initializes an image widget with a pointer to a separator.
\******************************************************************************/
void I_image_init_sep(i_image_t *image, bool vertical)
{
        if (!image)
                return;
        I_image_init_themed(image, vertical ?
                                   separator_tex + 1 : separator_tex);
        image->resize = TRUE;
}

/******************************************************************************\
 Info widget event function.
\******************************************************************************/
int I_info_event(i_info_t *info, i_event_t event)
{
        if (event != I_EV_CONFIGURE)
                return TRUE;
        I_widget_pack(&info->widget, I_PACK_H, I_FIT_NONE);
        info->widget.size = I_widget_child_bounds(&info->widget);
        return FALSE;
}

/******************************************************************************\
 Initializes an info widget.
\******************************************************************************/
void I_info_init(i_info_t *info, const char *left, const char *right)
{
        if (!info)
                return;
        C_zero(info);
        I_widget_init(&info->widget, "Info");
        info->widget.event_func = (i_event_f)I_info_event;
        info->widget.state = I_WS_READY;

        /* Label */
        I_label_init(&info->left, left);
        I_widget_add(&info->widget, &info->left.widget);

        /* Info */
        I_label_init(&info->right, right);
        info->right.widget.expand = TRUE;
        info->right.justify = I_JUSTIFY_RIGHT;
        info->right.color = I_COLOR_ALT;
        I_widget_add(&info->widget, &info->right.widget);
}

/******************************************************************************\
 Allocate a new info widget on the heap.
\******************************************************************************/
i_info_t *I_info_alloc(const char *left, const char *right)
{
        i_info_t *info;

        info = C_malloc(sizeof (*info));
        I_info_init(info, left, right);
        info->widget.heap = TRUE;
        return info;
}

/******************************************************************************\
 Configures an info widget.
\******************************************************************************/
void I_info_configure(i_info_t *info, const char *right)
{
        C_strncpy_buf(info->right.buffer, right);
        I_widget_event(&info->widget, I_EV_CONFIGURE);
}

/******************************************************************************\
 Deactivate a selectable widget.
\******************************************************************************/
void I_selectable_off(i_selectable_t *sel)
{
        if (!sel || !sel->group || *sel->group != sel)
                return;
        *sel->group = NULL;
}

/******************************************************************************\
 Activate a selectable widget. This will deactivate the currently selected
 member of the group, if it is not already this widget.
\******************************************************************************/
void I_selectable_on(i_selectable_t *sel)
{
        if (!sel || !sel->group || *sel->group == sel)
                return;
        *sel->group = sel;
        if (sel->on_select)
                sel->on_select(sel);
}

/******************************************************************************\
 Selectable widget event handler.
\******************************************************************************/
int I_selectable_event(i_selectable_t *sel, i_event_t event)
{
        switch (event) {
        case I_EV_CONFIGURE:
                if (sel->height > 0.f)
                        sel->widget.size.y = sel->height;
                R_window_cleanup(&sel->on);
                R_window_cleanup(&sel->off);
                R_window_cleanup(&sel->hover);
                R_window_init(&sel->on, select_tex[0]);
                R_window_init(&sel->off, select_tex[1]);
                R_window_init(&sel->hover, select_tex[2]);
                I_widget_pack(&sel->widget, I_PACK_H, I_FIT_NONE);
                if (!sel->height) {
                        c_vec2_t bounds;

                        bounds = I_widget_child_bounds(&sel->widget);
                        sel->widget.size.y = bounds.y;
                }
        case I_EV_MOVED:
                sel->on.sprite.origin = sel->widget.origin;
                sel->on.sprite.size = sel->widget.size;
                sel->on.sprite.size.y -= 1;
                sel->off.sprite.origin = sel->on.sprite.origin;
                sel->off.sprite.size = sel->on.sprite.size;
                sel->hover.sprite.origin = sel->on.sprite.origin;
                sel->hover.sprite.size = sel->on.sprite.size;
                return FALSE;
        case I_EV_CLEANUP:
                R_window_cleanup(&sel->on);
                R_window_cleanup(&sel->off);
                R_window_cleanup(&sel->hover);
                break;
        case I_EV_MOUSE_DOWN:
                I_selectable_on(sel);
                break;
        case I_EV_RENDER:
                if (sel->group && *sel->group == sel) {
                        sel->on.sprite.modulate.a = sel->widget.fade;
                        R_window_render(&sel->on);
                } else if (sel->widget.state == I_WS_HOVER) {
                        sel->hover.sprite.modulate.a = sel->widget.fade;
                        R_window_render(&sel->hover);
                } else {
                        sel->off.sprite.modulate.a = sel->widget.fade;
                        R_window_render(&sel->off);
                }
                break;
        default:
                break;
        }
        return TRUE;
}

/******************************************************************************\
 Initializes a selectable widget.
\******************************************************************************/
void I_selectable_init(i_selectable_t *sel, i_selectable_t **group,
                       float height)
{
        if (!sel)
                return;
        C_zero(sel);
        I_widget_init(&sel->widget, "Selectable");
        sel->widget.event_func = (i_event_f)I_selectable_event;
        sel->widget.state = I_WS_READY;
        sel->group = group;
        sel->height = height;
}

