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

/* Cargo widget */
typedef struct cargo_line {
        i_selectable_t sel;
        i_label_t left, label, buy_price, sell_price, right;
        i_image_t selling_icon, buying_icon;
        i_cargo_info_t info;
        bool no_auto, no_empty;
} cargo_line_t;

static cargo_line_t cargo_lines[G_CARGO_TYPES];
static i_label_t title;
static i_info_t space_info, right_info;
static i_image_t control_sep;
static i_select_t minimum, maximum, buy_price, sell_price, buying, selling;
static i_selectable_t *cargo_group;
static i_box_t button_box, control_box, left_box, right_box;
static i_button_t buy_button, sell_button, drop_button;
static int space_used, space_total;
static bool configuring, left_own, right_own, have_partner;

/******************************************************************************\
 Enable/disable price controls depending on buying/selling settings.
\******************************************************************************/
static void enable_price_controls(void)
{
        bool enable;

        enable = buying.widget.state != I_WS_DISABLED;
        if(left_own && right_own)
                enable = FALSE;

        /* Enable/disable control widgets */
        I_widget_enable(&sell_price.widget, enable && selling.index);
        I_widget_enable(&buy_price.widget, enable && buying.index);
        I_widget_enable(&minimum.widget, enable && selling.index);
        I_widget_enable(&maximum.widget, enable && buying.index);

        /* Show/hide price/quantity indicators */
        buying.item.widget.shown = enable;
        selling.item.widget.shown = enable;
        minimum.item.widget.shown = enable;
        maximum.item.widget.shown = enable;
        buy_price.item.widget.shown = enable;
        sell_price.item.widget.shown = enable;
}

/******************************************************************************\
 Configures the control widgets.
\******************************************************************************/
static void configure_controls(cargo_line_t *cargo)
{
        bool enable;

        /* Whether to enable the control widgets */
        enable = cargo && left_own && !cargo->no_auto && !g_game_over;
        if(left_own && right_own)
                enable = FALSE;

        /* Enable/disable control widgets */
        I_widget_enable(&buying.widget, enable);
        I_widget_enable(&selling.widget, enable);
        enable_price_controls();

        /* Cannot be bought or sold automatically */
        if (cargo->no_auto)
                goto skip_controls;
        /* Trading with own ship */
        if (left_own && right_own) {
                goto skip_controls;
        }

        /* Don't setup controls if they're disabled */
        if (!enable)
                return;
        configuring = TRUE;

        /* Set buying/selling */
        I_select_change(&buying, cargo->info.auto_buy);
        I_select_change(&selling, cargo->info.auto_sell);

        /* Setup control ranges */
        I_select_range(&maximum, 0.f, 1.f, (float)space_total);
        I_select_range(&minimum, cargo->no_empty ? 1.f : 0.f, 1.f,
                       (float)space_total);

        /* Setup nearest values */
        I_select_change(&buying, cargo->info.auto_buy);
        I_select_change(&selling, cargo->info.auto_sell);
        I_select_nearest(&maximum, cargo->info.maximum);
        I_select_nearest(&minimum, cargo->info.minimum);
        I_select_nearest(&buy_price, cargo->info.buy_price);
        I_select_nearest(&sell_price, cargo->info.sell_price);

skip_controls:
        configuring = FALSE;

        /* Enable/disable buttons */
        I_widget_enable(&buy_button.widget,
                        have_partner && cargo->info.p_amount > 0 &&
                        cargo->info.p_buy_price >= 0);
        I_widget_enable(&sell_button.widget,
                        have_partner && cargo->info.amount > 0 &&
                        cargo->info.p_sell_price >= 0);
        I_widget_enable(&drop_button.widget, cargo->info.amount > 0);

        /* Change buy/sell to take/give depending on price */
        if (cargo->info.p_buy_price == 0 || (left_own && right_own))
                I_button_configure(&buy_button, NULL,
                                   C_str("i-cargo-take", "Take"),
                                   I_BT_DECORATED);
        else
                I_button_configure(&buy_button, NULL,
                                   C_str("i-cargo-buy", "Buy"),
                                   I_BT_DECORATED);
        if (cargo->info.p_sell_price == 0 || (left_own && right_own))
                I_button_configure(&sell_button, NULL,
                                   C_str("i-cargo-give", "Give"),
                                   I_BT_DECORATED);
        else
                I_button_configure(&sell_button, NULL,
                                   C_str("i-cargo-sell", "Sell"),
                                   I_BT_DECORATED);
}

/******************************************************************************\
 Configures the control widgets for the currently selected cargo line.
\******************************************************************************/
static void configure_selected(void)
{
        if (!cargo_group)
                return;
        configure_controls((cargo_line_t *)cargo_group);
}

/******************************************************************************\
 Disable the trade window.
\******************************************************************************/
void I_disable_trade(void)
{
        I_toolbar_enable(&i_right_toolbar, i_trade_button, FALSE);
}

/******************************************************************************\
 Enable or disable the trade window.
\******************************************************************************/
void I_enable_trade(bool left, bool right, const char *right_name, int used,
                    int capacity)
{
        I_toolbar_enable(&i_right_toolbar, i_trade_button, TRUE);
        left_own = left;
        right_own = right;

        /* Trading partner */
        I_info_configure(&right_info, right_name);
        have_partner = right_name && *right_name;

        /* Cargo space */
        I_info_configure(&space_info, C_va("%d/%d", used, capacity));
        space_used = used;
        space_total = capacity;

        /* The window may need repacking */
        configuring = TRUE;
        I_widget_event(I_widget_top_level(&right_info.widget), I_EV_CONFIGURE);
        configuring = FALSE;
}

/******************************************************************************\
 Configures a cargo line.
\******************************************************************************/
static void cargo_line_configure(cargo_line_t *cargo)
{
        int value;
        bool valid;

        /* Left amount */
        if ((cargo->left.widget.shown = cargo->info.amount >= 0))
                I_label_configure(&cargo->left,
                                  C_va("%d", cargo->info.amount));

        /* Right amount */
        cargo->right.widget.shown = (value = cargo->info.p_amount) >= 0;
        if (cargo->right.widget.shown)
                I_label_configure(&cargo->right, C_va("%d", value));

        /* Cannot be bought or sold automatically 
         * or if trading with your own ship */
        if (cargo->no_auto || (left_own && right_own)) {
                cargo->buying_icon.widget.shown = FALSE;
                cargo->selling_icon.widget.shown = FALSE;
                cargo->buy_price.widget.shown = FALSE;
                cargo->sell_price.widget.shown = FALSE;
                return;
        }

        /* Automatic-buy/sell indicator icons */
        I_widget_enable(&cargo->buying_icon.widget,
                        left_own && cargo->info.auto_buy);
        I_widget_enable(&cargo->selling_icon.widget,
                        left_own && cargo->info.auto_sell);

        /* In-line prices */
        valid = cargo->info.p_buy_price >= 0 &&
                cargo->info.p_amount < cargo->info.p_maximum;
        if ((cargo->buy_price.widget.shown = valid))
                I_label_configure(&cargo->buy_price,
                                  C_va("%dg", cargo->info.p_buy_price));
        valid = cargo->info.p_sell_price >= 0 &&
                cargo->info.p_amount > cargo->info.p_minimum;
        if ((cargo->sell_price.widget.shown = valid))
                I_label_configure(&cargo->sell_price,
                                  C_va("%dg", cargo->info.p_sell_price));
}

/******************************************************************************\
 Configures a cargo line on the trade window.
\******************************************************************************/
void I_configure_cargo(int i, const i_cargo_info_t *info)
{
        C_assert(i >= 0 && i < G_CARGO_TYPES);
        cargo_lines[i].info = *info;

        /* Update display line */
        cargo_line_configure(cargo_lines + i);

        /* Update controls */
        if (cargo_group == &cargo_lines[i].sel)
                configure_controls(cargo_lines + i);
}

/******************************************************************************\
 Updates the game namespace's trade control parameters.
\******************************************************************************/
static void controls_changed(void)
{
        cargo_line_t *cargo;
        int buy_gold, sell_gold;

        /* User didn't actually change the select widget */
        if (configuring || !left_own)
                return;

        C_assert(cargo_group);
        cargo = (cargo_line_t *)cargo_group;

        /* Update our stored values */
        cargo->info.auto_buy = buying.index;
        cargo->info.auto_sell = selling.index;
        cargo->info.buy_price = (int)I_select_value(&buy_price);
        cargo->info.sell_price = (int)I_select_value(&sell_price);
        cargo->info.minimum = (int)I_select_value(&minimum);
        cargo->info.maximum = (int)I_select_value(&maximum);
        cargo_line_configure(cargo);

        /* Pass update back to the game namespace */
        sell_gold = buy_gold = -1;
        if (cargo->info.auto_buy)
                buy_gold = cargo->info.buy_price;
        if (cargo->info.auto_sell)
                sell_gold = cargo->info.sell_price;
        G_trade_params((g_cargo_type_t)(cargo - cargo_lines),
                       buy_gold, sell_gold,
                       cargo->info.minimum, cargo->info.maximum);
}

/******************************************************************************\
 Initialize a cargo line widget.
\******************************************************************************/
static void cargo_line_init(cargo_line_t *cargo, const char *name)
{
        I_selectable_init(&cargo->sel, &cargo_group, 0.f);
        cargo->sel.on_select = (i_callback_f)configure_selected;

        /* Left amount */
        I_label_init(&cargo->left, NULL);
        cargo->left.width_sample = "99999";
        cargo->left.color = I_COLOR_ALT;
        cargo->left.justify = I_JUSTIFY_RIGHT;
        I_widget_add(&cargo->sel.widget, &cargo->left.widget);

        /* Buying icon */
        I_image_init(&cargo->buying_icon, "gui/icons/buying.png");
        cargo->buying_icon.widget.margin_front = 0.5f;
        cargo->buying_icon.widget.margin_rear = 0.5f;
        I_widget_add(&cargo->sel.widget, &cargo->buying_icon.widget);

        /* Buy price */
        I_label_init(&cargo->buy_price, NULL);
        cargo->buy_price.width_sample = "999g";
        cargo->buy_price.color = I_COLOR_ALT;
        I_widget_add(&cargo->sel.widget, &cargo->buy_price.widget);

        /* Label */
        I_label_init(&cargo->label, name);
        cargo->label.widget.expand = TRUE;
        cargo->label.justify = I_JUSTIFY_CENTER;
        I_widget_add(&cargo->sel.widget, &cargo->label.widget);

        /* Sell price */
        I_label_init(&cargo->sell_price, NULL);
        cargo->sell_price.width_sample = "999g";
        cargo->sell_price.color = I_COLOR_ALT;
        I_widget_add(&cargo->sel.widget, &cargo->sell_price.widget);

        /* Selling icon */
        I_image_init(&cargo->selling_icon, "gui/icons/selling.png");
        cargo->selling_icon.widget.margin_front = 0.5f;
        cargo->selling_icon.widget.margin_rear = 0.5f;
        I_widget_add(&cargo->sel.widget, &cargo->selling_icon.widget);

        /* Right amount */
        I_label_init(&cargo->right, NULL);
        cargo->right.width_sample = cargo->left.width_sample;
        cargo->right.color = I_COLOR_ALT;
        I_widget_add(&cargo->sel.widget, &cargo->right.widget);
}

/******************************************************************************\
 Interface button to buy the cargo was pressed.
\******************************************************************************/
static void buy_cargo(const i_button_t *button)
{
        g_cargo_type_t cargo;
        int amount;

        if (!cargo_group)
                return;
        amount = I_key_amount();
        cargo = (g_cargo_type_t)((cargo_line_t *)cargo_group - cargo_lines);
        G_buy_cargo(cargo, amount);
}

/******************************************************************************\
 Interface button to sell the cargo was pressed.
\******************************************************************************/
static void sell_cargo(const i_button_t *button)
{
        g_cargo_type_t cargo;
        int amount;

        if (!cargo_group)
                return;
        amount = I_key_amount();
        cargo = (g_cargo_type_t)((cargo_line_t *)cargo_group - cargo_lines);
        G_buy_cargo(cargo, -amount);
}

/******************************************************************************\
 Interface button to drop the cargo was pressed.
\******************************************************************************/
static void drop_cargo(const i_button_t *button)
{
        g_cargo_type_t cargo;
        int amount;

        if (!cargo_group)
                return;
        amount = I_key_amount();
        cargo = (g_cargo_type_t)((cargo_line_t *)cargo_group - cargo_lines);
        G_drop_cargo(cargo, amount);
}

/******************************************************************************\
 Buying/selling toggled.
\******************************************************************************/
static void mode_changed(void)
{
        controls_changed();
        enable_price_controls();
}

/******************************************************************************\
 Initialize trade window.
\******************************************************************************/
void I_init_trade(i_window_t *window)
{
        int i, seps;

        I_window_init(window);
        window->widget.size = C_vec2(260.f, 0.f);
        window->fit = I_FIT_TOP;

        /* Label */
        I_label_init(&title, "Trade");
        title.font = R_FONT_TITLE;
        I_widget_add(&window->widget, &title.widget);

        /* Cargo space */
        I_info_init(&right_info,
                    C_str("i-cargo-trading", "Trading with:"), NULL);
        I_widget_add(&window->widget, &right_info.widget);

        /* Cargo space */
        I_info_init(&space_info, C_str("i-cargo", "Cargo space:"), "0/0");
        space_info.widget.margin_rear = 0.5f;
        I_widget_add(&window->widget, &space_info.widget);

        /* Cargo items */
        cargo_group = &cargo_lines[0].sel;
        for (i = 0, seps = 1; i < G_CARGO_TYPES; i++) {
                cargo_line_init(cargo_lines + i, g_cargo_names[i]);
                I_widget_add(&window->widget, &cargo_lines[i].sel.widget);
                if (!((i - 2) % 4))
                        cargo_lines[i].sel.widget.margin_front = 0.5f;
        }

        /* Gold can't be auto-bought/sold */
        cargo_lines[G_CT_GOLD].no_auto = TRUE;

        /* Don't get rid of all of your crew */
        cargo_lines[G_CT_CREW].no_empty = TRUE;

        /* Boxes for controls */
        I_box_init(&control_box, I_PACK_H, 50.f);
        control_box.widget.margin_front = 0.5f;
        I_widget_add(&window->widget, &control_box.widget);

        /* Left control box */
        I_box_init(&left_box, I_PACK_V, 120.f);
        left_box.widget.expand = TRUE;
        I_widget_add(&control_box.widget, &left_box.widget);

        /* Vertical separator */
        I_image_init_sep(&control_sep, TRUE);
        control_sep.widget.margin_front = 1.0f;
        control_sep.widget.margin_rear = 0.5f;
        I_widget_add(&control_box.widget, &control_sep.widget);

        /* Right control box */
        I_box_init(&right_box, I_PACK_V, 120.f);
        right_box.widget.expand = TRUE;
        I_widget_add(&control_box.widget, &right_box.widget);

        /* Buying toggle */
        I_select_init(&buying, C_str("i-buy", "Buy:"), NULL);
        I_select_add_string(&buying, C_str("c-yes", "Yes"));
        I_select_add_string(&buying, C_str("c-no", "No"));
        buying.on_change = (i_callback_f)mode_changed;
        I_widget_add(&left_box.widget, &buying.widget);

        /* Selling toggle */
        I_select_init(&selling, C_str("i-buy", "Sell:"), NULL);
        I_select_add_string(&selling, C_str("c-yes", "Yes"));
        I_select_add_string(&selling, C_str("c-no", "No"));
        selling.on_change = (i_callback_f)mode_changed;
        I_widget_add(&right_box.widget, &selling.widget);

        /* Buy price*/
        I_select_init(&buy_price, C_str("i-cargo-price", "For:"), NULL);
        buy_price.min = 0;
        buy_price.max = 999;
        buy_price.increment = 1;
        buy_price.suffix = "g";
        buy_price.digits = 3;
        buy_price.decimals = 0;
        buy_price.on_change = (i_callback_f)controls_changed;
        I_widget_add(&left_box.widget, &buy_price.widget);

        /* Sell price*/
        I_select_init(&sell_price, C_str("i-cargo-price", "For:"), NULL);
        sell_price.min = 0;
        sell_price.max = 999;
        sell_price.increment = 1;
        sell_price.suffix = "g";
        sell_price.digits = 3;
        sell_price.decimals = 0;
        sell_price.on_change = (i_callback_f)controls_changed;
        I_widget_add(&right_box.widget, &sell_price.widget);

        /* Maximum quantity */
        I_select_init(&maximum, C_str("i-cargo-max", "Max:"), NULL);
        maximum.increment = 1;
        maximum.digits = 3;
        maximum.decimals = 0;
        maximum.on_change = (i_callback_f)controls_changed;
        I_widget_add(&left_box.widget, &maximum.widget);

        /* Minimum quantity */
        I_select_init(&minimum, C_str("i-cargo-min", "Min:"), NULL);
        minimum.increment = 1;
        minimum.digits = 3;
        minimum.decimals = 0;
        minimum.on_change = (i_callback_f)controls_changed;
        I_widget_add(&right_box.widget, &minimum.widget);

        /* Button box */
        I_box_init(&button_box, I_PACK_H, 0.f);
        button_box.widget.margin_front = 0.5f;
        I_widget_add(&window->widget, &button_box.widget);

        /* Buy button */
        I_button_init(&buy_button, NULL, C_str("i-cargo-buy", "Buy"),
                      I_BT_DECORATED);
        buy_button.on_click = (i_callback_f)buy_cargo;
        buy_button.data = (void *)(size_t)1;
        buy_button.widget.expand = 2;
        I_widget_add(&button_box.widget, &buy_button.widget);

        /* Sell button */
        I_button_init(&sell_button, NULL, C_str("i-cargo-sell", "Sell"),
                      I_BT_DECORATED);
        sell_button.on_click = (i_callback_f)sell_cargo;
        sell_button.data = (void *)(size_t)1;
        sell_button.widget.expand = 2;
        I_widget_add(&button_box.widget, &sell_button.widget);

        /* Drop button */
        I_button_init(&drop_button, NULL, C_str("i-cargo-drop", "Drop"),
                      I_BT_DECORATED);
        drop_button.on_click = (i_callback_f)drop_cargo;
        drop_button.data = (void *)(size_t)1;
        drop_button.widget.expand = 1;
        I_widget_add(&button_box.widget, &drop_button.widget);
}

