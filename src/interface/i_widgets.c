/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Basic widget mechanics and event handling */

#include "i_common.h"

/* Colors in this array are kept up to date when the theme changes */
c_color_t i_colors[I_COLORS];

/* Points to the widget that currently holds keyboard focus */
i_widget_t *i_key_focus;

/* This is the child widget for an I_EV_ADD_CHILD event */
i_widget_t *i_child;

/* The most deeply nested widget that the mouse is hovering over */
i_widget_t *i_mouse_focus;

/* Event parameters */
c_vec2_t i_mouse;
int i_key, i_key_shift, i_key_alt, i_key_ctrl, i_key_unicode,
    i_mouse_button;

static i_widget_t *key_focus, *mouse_focus;
static int widgets;

/******************************************************************************\
 Initializes a widget name field with a unique name that includes the
 widget's class.
\******************************************************************************/
void I_widget_init(i_widget_t *widget, const char *class_name)
{
        C_assert(widget);
        C_zero(widget);
        snprintf(widget->name, sizeof (widget->name),
                 "widget #%u (%s)", widgets++, class_name);
        if (c_mem_check.value.n && i_debug.value.n)
                C_trace("Initialized %s", widget->name);
        widget->shown = TRUE;
}

/******************************************************************************\
 Remove a widget from its parent container and free its memory.
\******************************************************************************/
void I_widget_remove(i_widget_t *widget, int cleanup)
{
        if (!widget)
                return;
        if (widget->parent) {
                i_widget_t *prev;

                prev = widget->parent->child;
                if (prev != widget) {
                        while (prev->next != widget) {
                                if (!prev->next)
                                        C_error("%s is not its parent's child",
                                                widget->name);
                                prev = prev->next;
                        }
                        prev->next = widget->next;
                } else
                        widget->parent->child = widget->next;
        }
        widget->parent = NULL;
        widget->next = NULL;
        if (cleanup)
                I_widget_event(widget, I_EV_CLEANUP);
}

/******************************************************************************\
 Move a widget and all of its children. Will not generate move events if the
 origin did not change.
\******************************************************************************/
void I_widget_move(i_widget_t *widget, c_vec2_t origin)
{
        i_widget_t *child;
        c_vec2_t diff;

        diff = C_vec2_sub(origin, widget->origin);
        if (!diff.x && !diff.y)
                return;
        widget->origin = origin;
        child = widget->child;
        while (child) {
                I_widget_move(child, C_vec2_add(child->origin, diff));
                child = child->next;
        }
        if (widget->event_func)
                widget->event_func(widget, I_EV_MOVED);
}

/******************************************************************************\
 Returns the width and height of a rectangle that would encompass the widget
 including margins.
\******************************************************************************/
c_vec2_t I_widget_bounds(const i_widget_t *widget, i_pack_t pack)
{
        c_vec2_t size;
        float margin_front, margin_rear;

        size = widget->size;
        margin_front = widget->margin_front * i_border.value.n;
        margin_rear = widget->margin_rear * i_border.value.n;
        if (pack == I_PACK_H)
                size.x += margin_front + margin_rear;
        else if (pack == I_PACK_V)
                size.y += margin_front + margin_rear;
        return size;
}

/******************************************************************************\
 Returns TRUE if [child] is a child of [parent].
\******************************************************************************/
bool I_widget_child_of(const i_widget_t *parent, const i_widget_t *child)
{
        const i_widget_t *widget;

        if (!parent || !child)
                return FALSE;
        for (widget = child; widget != parent; widget = widget->parent) {
                if (widget == &i_root)
                        return FALSE;
                if (!widget)
                        C_error("Widget '%s' is not a child of root",
                                child->name);
        }
        return TRUE;
}

/******************************************************************************\
 Propagates an event to all of the widget's children in order. This must be
 called from your event function if you want to propagate events to child
 widgets.
\******************************************************************************/
void I_widget_propagate(i_widget_t *widget, i_event_t event)
{
        i_widget_t *child, *child_next;

        child = widget->child;
        while (child) {
                child_next = child->next;
                I_widget_event(child, event);
                child = child_next;
        }
}

/******************************************************************************\
 Returns the top-most container of the widget. Possibly the widget itself if
 its parent is root.
\******************************************************************************/
i_widget_t *I_widget_top_level(i_widget_t *widget)
{
        if (!widget)
                return NULL;
        while (widget->parent != &i_root) {
                if (!widget->parent)
                        C_error("Widget '%s' is not a child of root",
                                widget->name);
                widget = widget->parent;
        }
        return widget;
}

/******************************************************************************\
 Returns a statically allocated string containing the name of an event token.
\******************************************************************************/
const char *I_event_to_string(i_event_t event)
{
        switch (event) {
        case I_EV_NONE:
                return "I_EV_NONE";
        case I_EV_ADD_CHILD:
                return "I_EV_ADD_CHILD";
        case I_EV_CLEANUP:
                return "I_EV_CLEANUP";
        case I_EV_CONFIGURE:
                return "I_EV_CONFIGURE";
        case I_EV_GRAB_FOCUS:
                return "I_EV_GRAB_FOCUS";
        case I_EV_HIDE:
                return "I_EV_HIDE";
        case I_EV_KEY_DOWN:
                return "I_EV_KEY_DOWN";
        case I_EV_KEY_UP:
                return "I_EV_KEY_UP";
        case I_EV_KEY_FOCUS:
                return "I_EV_KEY_FOCUS";
        case I_EV_MOUSE_IN:
                return "I_EV_MOUSE_IN";
        case I_EV_MOUSE_OUT:
                return "I_EV_MOUSE_OUT";
        case I_EV_MOUSE_MOVE:
                return "I_EV_MOUSE_MOVE";
        case I_EV_MOUSE_DOWN:
                return "I_EV_MOUSE_DOWN";
        case I_EV_MOUSE_UP:
                return "I_EV_MOUSE_UP";
        case I_EV_MOUSE_FOCUS:
                return "I_EV_MOUSE_FOCUS";
        case I_EV_MOVED:
                return "I_EV_MOVED";
        case I_EV_RENDER:
                return "I_EV_RENDER";
        case I_EV_SHOW:
                return "I_EV_SHOW";
        default:
                if (event >= 0 && event < I_EVENTS)
                        C_warning("Forgot I_event_to_string() entry for event %d",
                                  event);
                return "I_EV_INVALID";
        }
}

/******************************************************************************\
 Returns TRUE if [widget] meets criteria for mouse focus.
\******************************************************************************/
static bool can_mouse_focus(i_widget_t *widget)
{
        return widget->state != I_WS_NO_FOCUS &&
               widget->state != I_WS_DISABLED && widget->shown &&
               C_rect_contains(widget->origin, widget->size, i_mouse);
}

/******************************************************************************\
 Call on a widget in hover or active state to check if the widget has mouse
 focus this frame. Will generate I_EV_MOUSE_OUT when applicable. Returns
 TRUE if the widget has mouse focus.
\******************************************************************************/
static bool check_mouse_focus(i_widget_t *widget)
{
        if (!widget)
                return FALSE;
        if (can_mouse_focus(widget)) {
                mouse_focus = widget;
                return TRUE;
        }
        while (widget->state == I_WS_HOVER || widget->state == I_WS_ACTIVE) {
                I_widget_event(widget, I_EV_MOUSE_OUT);
                if (!widget->parent || can_mouse_focus(widget->parent))
                        break;
                widget = widget->parent;
        }
        return FALSE;
}

/******************************************************************************\
 When a widget is hidden, propagates up to the widget's parent and then to its
 parent and so on until mouse focus is claimed. Does nothing if the mouse is
 not within widget bounds.
\******************************************************************************/
static void focus_parent(i_widget_t *widget)
{
        i_widget_t *p;

        /* Propagate key focus up */
        if (I_widget_child_of(widget, key_focus)) {
                p = widget->parent;
                while (p) {
                        if (p->entry) {
                                key_focus = p;
                                break;
                        }
                        p = p->parent;
                }
        }

        /* Propagate mouse out events */
        if (!I_widget_child_of(widget, mouse_focus))
                return;
        p = mouse_focus;
        while (p != widget) {
                I_widget_event(p, I_EV_MOUSE_OUT);
                p = p->parent;
        }
        I_widget_event(p, I_EV_MOUSE_OUT);

        /* Propagate focus up */
        for (p = widget->parent; p; p = p->parent)
                if (check_mouse_focus(p))
                        return;
        mouse_focus = NULL;
}

/******************************************************************************\
 Discover the new mouse and key focus widgets.
\******************************************************************************/
static void find_focus(void)
{
        check_mouse_focus(mouse_focus);
        I_widget_event(&i_root, I_EV_MOUSE_FOCUS);
        if (!mouse_focus)
                return;

        /* Change keyboard focus */
        if (mouse_focus->state != I_WS_DISABLED && mouse_focus->entry)
                I_widget_event(mouse_focus, I_EV_KEY_FOCUS);
}

/******************************************************************************\
 Dispatches basic widget events and does some checks to see if the event
 applies to this widget. Cleans up resources on I_EV_CLEANUP and propagates
 the event to all child widgets.

 Key focus follows the mouse. Only one widget can have keyboard focus at any
 time and that is the last widget that the mouse cursor passed over that
 could take keyboard input.

 Events filter down through containers to child widgets. The ordering will call
 all of a widget's parent containers before calling the widget and will call
 all of the children of a widget before calling a sibling.

 Always call this function instead of a widget's class event function in order
 to ensure correct event filtering and propagation.
\******************************************************************************/
void I_widget_event(i_widget_t *widget, i_event_t event)
{
        if (!widget)
                return;
        if (!widget->name[0] || !widget->event_func) {
                if (event == I_EV_CLEANUP)
                        return;
                C_error("Propagated %s to uninitialized widget, parent is %s",
                        I_event_to_string(event),
                        widget->parent ? widget->parent->name : "NULL");
        }

        /* The only event an unconfigured widget can handle is I_EV_CONFIGURE */
        if (!widget->configured && event != I_EV_CONFIGURE) {
                if (widget->auto_configure)
                        I_widget_event(widget, I_EV_CONFIGURE);
                if (!widget->configured)
                        C_error("Propagated %s to unconfigured %s",
                                I_event_to_string(event), widget->name);
        }

        /* Print out the event in debug mode */
        if (i_debug.value.n >= 2)
                switch (event) {
                default:
                        C_trace("%s --> %s", I_event_to_string(event),
                                widget->name);
                case I_EV_RENDER:
                case I_EV_MOUSE_MOVE:
                case I_EV_MOUSE_FOCUS:
                case I_EV_GRAB_FOCUS:
                case I_EV_KEY_UP:
                case I_EV_MOUSE_UP:
                        break;
                }

        /* Before handling and propagating event to children */
        switch (event) {
        case I_EV_CLEANUP:
                if (c_mem_check.value.n)
                        C_trace("Freeing %s", widget->name);
                break;
        case I_EV_CONFIGURE:
                if (c_mem_check.value.n)
                        C_trace("Configuring %s", widget->name);
                break;
        case I_EV_GRAB_FOCUS:
                if (widget->entry && widget->shown &&
                    widget->state != I_WS_DISABLED)
                        key_focus = widget;
                break;
        case I_EV_HIDE:
                if (!widget->shown)
                        return;
                widget->shown = FALSE;
                widget->event_func(widget, event);
                focus_parent(widget);
                return;
        case I_EV_KEY_DOWN:
                if (widget->steal_keys &&
                    widget->state != I_WS_DISABLED && widget->shown &&
                    widget->state != I_WS_NO_FOCUS)
                        widget->event_func(widget, event);
                return;
        case I_EV_MOUSE_IN:
                if (widget->state == I_WS_READY)
                        widget->state = I_WS_HOVER;
                widget->event_func(widget, event);
                return;
        case I_EV_MOUSE_OUT:
                if (widget->state == I_WS_HOVER || widget->state == I_WS_ACTIVE)
                        widget->state = I_WS_READY;
                widget->event_func(widget, event);
                return;
        case I_EV_MOUSE_DOWN:
                widget->event_func(widget, event);
                return;
        case I_EV_MOUSE_FOCUS:
                if (!check_mouse_focus(widget))
                        return;
                break;
        case I_EV_MOUSE_MOVE:
                if (widget->state == I_WS_READY)
                        I_widget_event(widget, I_EV_MOUSE_IN);
                widget->event_func(widget, event);
                return;
        case I_EV_KEY_FOCUS:
                key_focus = mouse_focus;
                widget->event_func(widget, event);
                return;
        case I_EV_MOVED:
                C_error("I_EV_MOVED should only be generated by "
                        "I_widget_move()");
        case I_EV_RENDER:
                if (!i_debug.value.n && widget->parent &&
                    !C_rect_intersect(widget->origin, widget->size,
                                      widget->parent->origin,
                                      widget->parent->size))
                        return;
                if (i_fade.value.f > 0.f) {
                        float target, rate;

                        target = 0.f;

                        /* Widgets inherit the parent widget's fade */
                        if (widget->shown) {
                                target = 1.f;
                                if (widget->parent)
                                        target = widget->parent->fade;
                        }

                        rate = i_fade.value.f * c_frame_sec;
                        if (widget->state == I_WS_DISABLED) {
                                target *= 0.25f;
                                if (widget->fade <= 0.25f)
                                        rate *= 0.25f;
                        }
                        if (widget->fade < target) {
                                widget->fade += rate;
                                if (widget->fade > target)
                                        widget->fade = target;
                        } else if (widget->fade > target) {
                                widget->fade -= rate;
                                if (widget->fade < target)
                                        widget->fade = target;
                        }

                        /* Widget should never be less faded than its parent */
                        if (widget->parent &&
                            widget->parent->fade < widget->fade)
                                widget->fade = widget->parent->fade;

                        if (widget->fade <= 0.f)
                                return;
                } else if (!widget->shown)
                        return;
                break;
        case I_EV_SHOW:
                if (widget->shown)
                        return;
                widget->shown = TRUE;
                widget->event_func(widget, event);
                find_focus();
                return;
        default:
                break;
        }

        /* Call widget-specific event handler and propagate event down */
        if (widget->event_func(widget, event))
                I_widget_propagate(widget, event);

        /* After handling and propagation to children */
        switch (event) {
        case I_EV_CONFIGURE:
                widget->configured = TRUE;
                break;
        case I_EV_CLEANUP:
                focus_parent(widget);
                if (i_mouse_focus == widget)
                        i_mouse_focus = NULL;
                if (i_key_focus == widget)
                        i_key_focus = NULL;
                if (widget->heap)
                        C_free(widget);
                else
                        C_zero(widget);
                break;
        case I_EV_MOUSE_DOWN:
        case I_EV_MOUSE_UP:
                if (mouse_focus == widget && widget->state == I_WS_READY)
                        widget->state = I_WS_HOVER;
                break;
        default:
                break;
        }
}

/******************************************************************************\
 Returns the string representation of a keycode. The string it stored in a
 C_va() buffer and should be considered temporary.
\******************************************************************************/
const char *I_key_string(int sym)
{
        if (sym >= ' ' && sym < 0x7f)
                return C_va("%c", sym);
        return C_va("0x%x", sym);
}

/******************************************************************************\
 Propagates an event up through its parents.
\******************************************************************************/
static void propagate_up(i_widget_t *widget, i_event_t event)
{
        for (; widget && widget->event_func; widget = widget->parent)
                if (widget->shown && widget->state != I_WS_DISABLED)
                        I_widget_event(widget, event);
}

/******************************************************************************\
 Redirect focus from one widget to another. Note that focus only updates from
 the next event.
\******************************************************************************/
void I_widget_focus(i_widget_t *widget, bool key, bool mouse)
{
        if (key)
                key_focus = widget;
        if (mouse)
                mouse_focus = widget;
}

/******************************************************************************\
 Fade a widget and its children to a specific opacity.
\******************************************************************************/
void I_widget_fade(i_widget_t *widget, float fade)
{
        if (!widget)
                return;
        widget->fade = fade;
        widget = widget->child;
        while (widget) {
                I_widget_fade(widget, fade);
                widget = widget->next;
        }
}

/******************************************************************************\
 Convenience function that sets a widget's state to I_WS_READY only if it is
 disabled (as opposed to hovering, active etc).
\******************************************************************************/
void I_widget_enable(i_widget_t *widget, bool enable)
{
        if (enable) {
                if (widget->state == I_WS_DISABLED)
                        widget->state = I_WS_READY;
                return;
        }
        widget->state = I_WS_DISABLED;
}

/******************************************************************************\
 Handle an SDL event. Does not allow multiple keys or mouse buttons to be
 pressed at the same time.
\******************************************************************************/
void I_dispatch(const SDL_Event *ev)
{
        SDLMod mod;
        i_event_t event;

        /* Update modifiers */
        mod = SDL_GetModState();
        i_key_shift = mod & KMOD_SHIFT;
        i_key_alt = mod & KMOD_ALT;
        i_key_ctrl = mod & KMOD_CTRL;

        /* Before dispatch */
        switch (ev->type) {
        case SDL_ACTIVEEVENT:

                /* If the mouse focus was lost, reset the cursor position */
                if (!ev->active.gain &&
                    (ev->active.state & SDL_APPMOUSEFOCUS))
                        i_mouse = C_vec2(-1.f, -1.f);

                return;
        case SDL_KEYDOWN:
                event = I_EV_KEY_DOWN;
                i_key = ev->key.keysym.sym;
                i_key_unicode = ev->key.keysym.unicode;
                if (i_debug.value.n > 0)
                        C_trace("SDL_KEYDOWN (%s%s)",
                                (i_key_shift ? "shift + " : ""),
                                I_key_string(i_key_unicode));
                if (!i_key) {
                        C_warning("SDL sent zero keysym");
                        return;
                }
                break;
        case SDL_KEYUP:
                event = I_EV_KEY_UP;
                i_key = ev->key.keysym.sym;
                if (i_debug.value.n > 0)
                        C_trace("SDL_KEYUP (%s%s)",
                                (i_key_shift ? "shift + " : ""),
                                I_key_string(i_key_unicode));
                break;
        case SDL_MOUSEMOTION:
                event = I_EV_MOUSE_MOVE;
                i_mouse.x = ev->motion.x / r_scale_2d;
                i_mouse.y = ev->motion.y / r_scale_2d;
                find_focus();
                break;
        case SDL_MOUSEBUTTONDOWN:
                event = I_EV_MOUSE_DOWN;
                i_mouse_button = ev->button.button;
                if (i_debug.value.n > 0)
                        C_trace("SDL_MOUSEBUTTONDOWN (%d)", i_mouse_button);
                check_mouse_focus(mouse_focus);
                break;
        case SDL_MOUSEBUTTONUP:
                event = I_EV_MOUSE_UP;
                i_mouse_button = ev->button.button;
                if (i_debug.value.n > 0)
                        C_trace("SDL_MOUSEBUTTONUP (%d)", i_mouse_button);
                check_mouse_focus(mouse_focus);
                break;
        default:
                return;
        }

        /* Update focus widgets for this event */
        i_key_focus = key_focus;
        i_mouse_focus = mouse_focus;

        /* Key and mouse down events are propagated back from the focused
           widget up the hierarchy */
        if (event == I_EV_KEY_DOWN) {
                I_global_key();
                if (i_key_focus) {
                        i_key_focus->event_func(i_key_focus, event);
                        propagate_up(i_key_focus->parent, event);
                }
        } else if (event == I_EV_MOUSE_DOWN || event == I_EV_MOUSE_MOVE)
                propagate_up(mouse_focus, event);

        /* Normal event propagation up from the root widget */
        else
                I_widget_event(&i_root, event);

        /* After dispatch */
        switch (ev->type) {
        case SDL_MOUSEBUTTONUP:
                i_mouse_button = 0;
                break;
        case SDL_KEYUP:
                i_key = 0;
                break;
        default:
                break;
        }
}

