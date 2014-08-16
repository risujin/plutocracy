/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Functions for widgets that contain other widgets */

#include "i_common.h"

/******************************************************************************\
 Attaches a child widget to another widget to the end of the parent's children
 linked list. Sends the I_EV_ADD_CHILD event.
\******************************************************************************/
void I_widget_add(i_widget_t *parent, i_widget_t *child)
{
        i_widget_t *sibling;

        if (!child)
                return;
        if (!parent->name[0])
                C_error("Parent widget uninitialized");
        if (!child->name[0])
                C_error("Child widget uninitialized");
        if (!parent)
                C_error("Tried to add %s to a null pointer", child->name);
        if (child->parent)
                C_error("Cannot add %s to %s, already has a parent",
                        child->name, parent->name);
        sibling = parent->child;
        if (sibling) {
                while (sibling->next)
                        sibling = sibling->next;
                sibling->next = child;
        } else
                parent->child = child;
        child->next = NULL;
        child->parent = parent;
        i_child = child;
        if (parent->event_func)
                parent->event_func(parent, I_EV_ADD_CHILD);
}

/******************************************************************************\
 Remove all of the widget's children.
\******************************************************************************/
void I_widget_remove_children(i_widget_t *widget, int cleanup)
{
        i_widget_t *child, *child_next;

        if (!widget)
                return;
        for (child = widget->child; child; child = child_next) {
                child_next = child->next;
                child->parent = NULL;
                child->next = NULL;
                if (cleanup)
                        I_widget_event(child, I_EV_CLEANUP);
        }
        widget->child = NULL;
}

/******************************************************************************\
 Gives extra space to expanding child widgets.
\******************************************************************************/
static void expand_children(i_widget_t *widget, c_vec2_t size, float expand)
{
        c_vec2_t offset, share;
        i_widget_t *child;
        bool expand_up;

        if (!expand)
                return;
        size = C_vec2_divf(size, expand);
        offset = C_vec2(0.f, 0.f);
        expand_up = size.x > 0.f || size.y > 0.f;
        for (child = widget->child; child; child = child->next) {
                if (child->pack_skip)
                        continue;
                if (!child->expand || (child->shrink_only && expand_up)) {
                        I_widget_move(child, C_vec2_add(child->origin, offset));
                        continue;
                }
                share = C_vec2_scalef(size, child->expand);
                child->size = C_vec2_add(child->size, share);
                child->origin = C_vec2_add(child->origin, offset);
                I_widget_event(child, I_EV_CONFIGURE);
                offset = C_vec2_add(offset, share);
        }
}

/******************************************************************************\
 Packs all of a widget's children.
\******************************************************************************/
void I_widget_pack(i_widget_t *widget, i_pack_t pack, i_fit_t fit)
{
        i_widget_t *child;
        c_vec2_t origin, size;
        float expand;

        /* First, let every widget claim the minimum space it requires */
        size = C_vec2_subf(widget->size,
                           widget->padding * i_border.value.n * 2.f);
        origin = C_vec2_addf(widget->origin,
                             widget->padding * i_border.value.n);
        expand = 0.f;
        for (child = widget->child; child; child = child->next) {
                float margin_front, margin_rear;

                /* Skip children if they don't want to be packed */
                if (child->pack_skip) {
                        I_widget_event(child, I_EV_CONFIGURE);
                        continue;
                }

                margin_front = child->margin_front * i_border.value.n;
                margin_rear = child->margin_rear * i_border.value.n;
                if (pack == I_PACK_H) {
                        origin.x += margin_front;
                        child->origin = origin;
                        child->size = C_vec2(0.f, size.y);
                        I_widget_event(child, I_EV_CONFIGURE);
                        origin.x += child->size.x + margin_rear;
                        size.x -= child->size.x + margin_front + margin_rear;
                } else if (pack == I_PACK_V) {
                        origin.y += margin_front;
                        child->origin = origin;
                        child->size = C_vec2(size.x, 0.f);
                        I_widget_event(child, I_EV_CONFIGURE);
                        origin.y += child->size.y + margin_rear;
                        size.y -= child->size.y + margin_front + margin_rear;
                }
                if (child->expand > 0)
                        expand += child->expand;
        }

        /* If there is left over space and we are not collapsing, assign it
           to any expandable widgets */
        if (fit == I_FIT_NONE) {
                if (pack == I_PACK_H && expand) {
                        size.y = 0.f;
                        expand_children(widget, size, expand);
                } else if (pack == I_PACK_V && expand) {
                        size.x = 0.f;
                        expand_children(widget, size, expand);
                }
                return;
        }

        /* This is a fitting widget fit the widget to its contents */
        if (pack == I_PACK_H) {
                widget->size.x -= size.x;
                if (fit == I_FIT_TOP) {
                        origin = widget->origin;
                        origin.x += size.x;
                        I_widget_move(widget, origin);
                }
        } else if (pack == I_PACK_V) {
                widget->size.y -= size.y;
                if (fit == I_FIT_TOP) {
                        origin = widget->origin;
                        origin.y += size.y;
                        I_widget_move(widget, origin);
                }
        }
}

/******************************************************************************\
 Returns the width and height of a rectangle from the widget's origin that
 would encompass it's child widgets.
\******************************************************************************/
c_vec2_t I_widget_child_bounds(const i_widget_t *widget)
{
        c_vec2_t bounds, corner;
        i_widget_t *child;

        bounds = C_vec2(0.f, 0.f);
        for (child = widget->child; child; child = child->next) {
                if (child->pack_skip)
                        continue;
                corner = C_vec2_add(C_vec2_sub(child->origin, widget->origin),
                                    child->size);
                if (bounds.x < corner.x)
                        bounds.x = corner.x;
                if (bounds.y < corner.y)
                        bounds.y = corner.y;
        }
        return bounds;
}

/******************************************************************************\
 Add a widget to a container and pack it. Requires the container to have been
 configured already. Only the child widget is configured. This function is
 largely only used for adding widgets to the ends of boxes.
\******************************************************************************/
void I_widget_add_pack(i_widget_t *parent, i_widget_t *widget,
                       i_pack_t pack, i_fit_t fit)
{
        c_vec2_t origin, space, bounds;
        float margin_rear, margin_front, padding;

        I_widget_add(parent, widget);

        /* Find the size of the previous widgets */
        padding = parent->padding * i_border.value.n;
        bounds = I_widget_child_bounds(parent);
        if (!bounds.x)
                bounds.x = padding;
        if (!bounds.y)
                bounds.y = padding;

        /* Actual margin sizes */
        margin_rear = widget->margin_rear * i_border.value.n;
        margin_front = widget->margin_front * i_border.value.n;

        /* Find the widgets new position and size */
        space = C_vec2_subf(parent->size, padding);
        widget->origin = parent->origin;
        widget->size = C_vec2(space.x - bounds.x, space.y - bounds.y);
        if (pack == I_PACK_H) {
                widget->origin.x += bounds.x + margin_front;
                widget->origin.y += padding;
                if (!widget->expand)
                        widget->size.x = 0.f;
        } else if (pack == I_PACK_V) {
                widget->origin.x += padding;
                widget->origin.y += bounds.y + margin_front;
                if (!widget->expand)
                        widget->size.y = 0.f;
        }

        I_widget_event(widget, I_EV_CONFIGURE);

        /* Count widget's size */
        bounds = C_vec2_sub(C_vec2_add(widget->origin, widget->size),
                            parent->origin);
        if (pack == I_PACK_H)
                bounds.x += margin_rear;
        else if (pack == I_PACK_V)
                bounds.y += margin_rear;

        /* If there is left over space and we are not collapsing, assign it
           to any expandable widgets */
        if (fit == I_FIT_NONE) {
                float expand;

                /* Count expanders */
                expand = 0.f;
                for (widget = parent->child; widget; widget = widget->next)
                        if (widget->expand > 0)
                                expand += widget->expand;

                if (pack == I_PACK_H && expand) {
                        space = C_vec2(space.x - bounds.x, 0.f);
                        expand_children(widget, space, expand);
                } else if (pack == I_PACK_V && expand) {
                        space = C_vec2(0.f, space.y - bounds.y);
                        expand_children(widget, space, expand);
                }
                return;
        }

        /* Fit the parent widget */
        origin = parent->origin;
        if (pack == I_PACK_H) {
                parent->size.x = bounds.x;
                if (fit == I_FIT_TOP) {
                        origin.x -= widget->size.x + margin_front + margin_rear;
                        I_widget_move(parent, origin);
                }
        } else if (pack == I_PACK_V) {
                parent->size.y = bounds.y;
                if (fit == I_FIT_TOP) {
                        origin.y -= widget->size.y + margin_front + margin_rear;
                        I_widget_move(parent, origin);
                }
        }
}

