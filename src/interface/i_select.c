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

/******************************************************************************\
 Find the length of the longest select option. Returns the width of the
 widest option.
\******************************************************************************/
static float select_widest(i_select_t *select)
{
        i_select_option_t *option;
        c_vec2_t size;
        float width;

        /* Numeric widget maximum is assumed to be widest */
        if (!select->options) {
                const char *fmt;
                float max;

                /* Fixed number of digits */
                max = select->max;
                if (select->digits > 0)
                        max = powf(10, select->digits) - 1.f;
                else if (select->digits < 0)
                        max = -powf(10, select->digits) + 1.f;

                if (select->suffix)
                        fmt = C_va("%%.0%df%%s", select->decimals);
                else
                        fmt = C_va("%%.0%df", select->decimals);
                size = R_font_size(select->item.font,
                                   C_va(fmt, max, select->suffix));
                return (size.x + i_border.value.n) / r_scale_2d;
        }

        /* Cycle through each option */
        width = 0.f;
        select->list_len = 0;
        for (option = select->options; option; option = option->next) {
                size = R_font_size(select->item.font, option->string);
                size.x /= r_scale_2d;
                if (size.x > width)
                        width = size.x;
                select->list_len++;
        }
        return width + i_border.value.n;
}

/******************************************************************************\
 Selection widget event function.
\******************************************************************************/
int I_select_event(i_select_t *select, i_event_t event)
{
        if (event == I_EV_CONFIGURE) {
                if (select->index < 0)
                        I_select_change(select, 0);
                select->item.width = select_widest(select);
                select->widget.size.y = R_font_height(R_FONT_GUI) / r_scale_2d;
                I_widget_pack(&select->widget, I_PACK_H, I_FIT_NONE);
                select->widget.size = I_widget_child_bounds(&select->widget);
                return FALSE;
        }
        if (event == I_EV_CLEANUP) {
                i_select_option_t *option, *next;

                option = select->options;
                while (option) {
                        next = option->next;
                        C_free(option);
                        option = next;
                }
                select->options = NULL;
        }
        return TRUE;
}

/******************************************************************************\
 Change the selection widget's item by index.
\******************************************************************************/
void I_select_change(i_select_t *select, int index)
{
        i_select_option_t numeric, *option;
        int i, max;

        /* Numeric widget */
        if (select->list_len <= 0)
                max = (int)((select->max - select->min) /
                            select->increment + 0.5f);
        else
                max = select->list_len - 1;

        /* Range checks */
        if (index <= 0) {
                index = 0;
                select->left.widget.state = I_WS_DISABLED;
        } else if (select->left.widget.state == I_WS_DISABLED) {
                select->left.widget.state = I_WS_READY;
        }
        if (index >= max) {
                index = max;
                select->right.widget.state = I_WS_DISABLED;
        } else if (select->right.widget.state == I_WS_DISABLED)
                select->right.widget.state = I_WS_READY;

        /* Already set? */
        if (select->index == index)
                return;
        select->index = index;

        /* Get the option */
        if (select->list_len > 0) {
                option = select->options;
                for (i = 0; option && i < index; i++)
                        option = option->next;
        }

        /* For numeric options, fake one */
        else {
                float value;
                const char *fmt;

                option = &numeric;
                value = select->min + select->increment * select->index;
                fmt = C_va("%%.0%df%%s", select->decimals);
                snprintf(option->string, sizeof (option->string), fmt, value,
                         select->suffix ? select->suffix : "");
                option->value = value;
        }

        if (select->widget.configured)
                I_label_configure(&select->item, option->string);
        else
                C_strncpy_buf(select->item.buffer, option->string);
        if (select->on_change)
                select->on_change(select);

        /* If an auto-set variable is configured, set it now */
        if (select->variable && option) {
                if (select->variable->type == C_VT_FLOAT)
                        C_var_set(select->variable,
                                  C_va("%g", option->value));
                else if (select->variable->type == C_VT_INTEGER)
                        C_var_set(select->variable,
                                  C_va("%d", (int)(option->value + 0.5f)));
                else
                        C_var_set(select->variable, option->string);
        }
}

/******************************************************************************\
 Get the amount to change an option by and affect it by the keys held.
\******************************************************************************/
int I_key_amount(void)
{
        int amount;

        amount = 1;
        if (i_key_ctrl)
                amount *= 5;
        if (i_key_shift)
                amount *= 10;
        return amount;
}

/******************************************************************************\
 Left select button clicked.
\******************************************************************************/
static void left_arrow_clicked(i_button_t *button)
{
        i_select_t *select;

        select = (i_select_t *)button->data;
        I_select_change(select, select->index - I_key_amount());
}

/******************************************************************************\
 Right select button clicked.
\******************************************************************************/
static void right_arrow_clicked(i_button_t *button)
{
        i_select_t *select;

        select = (i_select_t *)button->data;
        I_select_change(select, select->index + I_key_amount());
}

/******************************************************************************\
 Add a value to the start of the options list.
\******************************************************************************/
static i_select_option_t *select_add(i_select_t *select, const char *string)
{
        i_select_option_t *option;

        option = C_malloc(sizeof (*option));
        C_strncpy_buf(option->string, string);
        option->next = select->options;
        option->value = C_FLOAT_MAX;
        select->options = option;
        select->list_len++;
        return option;
}

void I_select_add_string(i_select_t *select, const char *string)
{
        select_add(select, string);
}

void I_select_add_float(i_select_t *select, float f, const char *override)
{
        i_select_option_t *option;
        const char *fmt;

        if (override)
                option = select_add(select, override);
        else if (select->suffix && select->suffix[0]) {
                fmt = C_va("%%.0%df%%s", select->decimals);
                option = select_add(select, C_va(fmt, f, select->suffix));
        } else {
                fmt = C_va("%%.0%df", select->decimals);
                option = select_add(select, C_va(fmt, f, select->suffix));
        }
        option->value = f;
}

void I_select_add_int(i_select_t *select, int n, const char *override)
{
        i_select_option_t *option;

        if (override)
                option = select_add(select, override);
        else if (select->suffix && select->suffix[0])
                option = select_add(select, C_va("%d%s", n, select->suffix));
        else
                option = select_add(select, C_va("%d", n, select->suffix));
        option->value = (float)n;
}

/******************************************************************************\
 Change to the index of the closest numerical value.
\******************************************************************************/
void I_select_nearest(i_select_t *select, float value)
{
        float diff, best_diff;
        int best = -1;

        /* Numeric widgets don't need to cycle */
        if (select->list_len <= 0) {
                if (value < select->min)
                        value = select->min;
                if (value > select->max)
                        value = select->max;
                best = (int)((value - select->min) / select->increment + 0.5f);
        }

        /* Cycle through options to find the closest one */
        else {
                i_select_option_t *option;
                int i;

                best_diff = C_FLOAT_MAX;
                option = select->options;
                for (i = 0; option; option = option->next, i++) {
                        diff = value - option->value;
                        if (diff < 0.f)
                                diff = -diff;
                        if (diff < best_diff) {
                                best = i;
                                if (!diff)
                                        break;
                                best_diff = diff;
                        }
                }
        }

        I_select_change(select, best);
}

/******************************************************************************\
 Update the selection widget with the nearest value to the variable we are
 trying to set.
\******************************************************************************/
void I_select_update(i_select_t *select)
{
        if (!select->variable)
                return;

        /* For numerics just compute the nearest value */
        if (select->list_len <= 0) {
                float value = 0.0;

                if (select->variable->type == C_VT_FLOAT)
                        value = select->variable->value.f;
                else if (select->variable->type == C_VT_INTEGER)
                        value = (float)select->variable->value.n;
                else
                        C_error("Invalid variable type %d",
                                select->variable->type);
                I_select_nearest(select, value);
                return;
        }

        /* Go through the options list and find the closest value */
        if (select->variable->type == C_VT_FLOAT)
                I_select_nearest(select, select->variable->value.f);
        else if (select->variable->type == C_VT_INTEGER)
                I_select_nearest(select, (float)select->variable->value.n);
        else
                C_error("Invalid variable type %d", select->variable->type);
}

/******************************************************************************\
 Returns the floating-point value of the current selection.
\******************************************************************************/
float I_select_value(const i_select_t *select)
{
        const i_select_option_t *option;
        int i;

        /* Numerics don't have options */
        if (select->list_len <= 0)
                return select->min + select->index * select->increment;

        /* Cycle through to find our option */
        option = select->options;
        for (i = 0; option; i++, option = option->next)
                if (i == select->index)
                        return option->value;

        return 0.f;
}

/******************************************************************************\
 Change the range/increment of the select widget after initialization.
\******************************************************************************/
void I_select_range(i_select_t *select, float min, float inc, float max)
{
        float value;

        if (select->min == min && select->max == max &&
            select->increment == inc)
                return;
        value = I_select_value(select);
        select->min = min;
        select->max = max;
        select->increment = inc;
        select->index = -1;
        I_select_nearest(select, value);
}

/******************************************************************************\
 Initialize a selection widget.
\******************************************************************************/
void I_select_init(i_select_t *select, const char *label, const char *suffix)
{
        if (!select)
                return;
        C_zero(select);
        I_widget_init(&select->widget, "Select");
        select->widget.event_func = (i_event_f)I_select_event;
        select->widget.state = I_WS_READY;
        select->suffix = suffix;
        select->decimals = 2;
        select->index = -1;
        select->increment = 1.f;

        /* Description label */
        I_label_init(&select->label, label);
        select->label.widget.expand = TRUE;
        select->label.widget.margin_rear = 0.5f;
        I_widget_add(&select->widget, &select->label.widget);

        /* Left button */
        I_button_init(&select->left, "gui/icons/arrow-left.png", NULL,
                      I_BT_ROUND);
        select->left.widget.state = I_WS_DISABLED;
        select->left.on_click = (i_callback_f)left_arrow_clicked;
        select->left.data = select;
        I_widget_add(&select->widget, &select->left.widget);

        /* Selected item label */
        I_label_init(&select->item, NULL);
        select->item.color = I_COLOR_ALT;
        select->item.justify = I_JUSTIFY_CENTER;
        I_widget_add(&select->widget, &select->item.widget);

        /* Right button */
        I_button_init(&select->right, "gui/icons/arrow-right.png", NULL,
                      I_BT_ROUND);
        select->right.widget.state = I_WS_DISABLED;
        select->right.on_click = (i_callback_f)right_arrow_clicked;
        select->right.data = select;
        I_widget_add(&select->widget, &select->right.widget);
}

