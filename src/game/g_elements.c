/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Initializes, cleans up, and manipulates gameplay elements */

#include "g_common.h"

/* Ship base classes */
PyObject *g_ship_class_list;

/* Array of game nations */
g_nation_t g_nations[G_NATION_NAMES];

/* Array of building base classes */
PyObject *g_building_class_list;
//g_building_class_t g_building_classes[G_BUILDING_TYPES];

/* Array of cargo names */
const char *g_cargo_names[G_CARGO_TYPES];

/* TRUE if the game has ended */
bool g_game_over;

/******************************************************************************\
 Update function for a national color.
\******************************************************************************/
static int nation_color_update(c_var_t *var, c_var_value_t value)
{
        C_color_update(var, value);
        I_update_colors();
        return TRUE;
}

/******************************************************************************\
 Convert a nation to a color index.
\******************************************************************************/
i_color_t G_nation_to_color(g_nation_name_t nation)
{
        if (nation == G_NN_RED)
                return I_COLOR_RED;
        else if (nation == G_NN_GREEN)
                return I_COLOR_GREEN;
        else if (nation == G_NN_BLUE)
                return I_COLOR_BLUE;
        else if (nation == G_NN_PIRATE)
                return I_COLOR_PIRATE;
        return I_COLOR_ALT;
}

/******************************************************************************\
 Milliseconds one crew member can survive after eating one unit of [cargo].
\******************************************************************************/
int G_cargo_nutritional_value(g_cargo_type_t cargo)
{
        switch (cargo) {
        case G_CT_CREW:
                return 200000;
        case G_CT_RATIONS:
                return 400000;
        default:
                return 0;
        }
}

/******************************************************************************\
 Initialize game elements.
\******************************************************************************/
void G_init_elements(void)
{
        int i;

        C_status("Initializing game elements");

        /* Check our constants */
        C_assert(G_CLIENT_MESSAGES < 256);
        C_assert(G_SERVER_MESSAGES < 256);

        /* Nation constants */
        g_nations[G_NN_NONE].short_name = "";
        g_nations[G_NN_NONE].long_name = "";
        g_nations[G_NN_RED].short_name = "red";
        g_nations[G_NN_RED].long_name = C_str("g-nation-red", "Ruby");
        g_nations[G_NN_GREEN].short_name = "green";
        g_nations[G_NN_GREEN].long_name = C_str("g-nation-green", "Emerald");
        g_nations[G_NN_BLUE].short_name = "blue";
        g_nations[G_NN_BLUE].long_name = C_str("g-nation-blue", "Sapphire");
        g_nations[G_NN_PIRATE].short_name = "pirate";
        g_nations[G_NN_PIRATE].long_name = C_str("g-nation-pirate", "Pirate");

        /* Initialize nation window and color variables */
        for (i = G_NN_NONE + 1; i < G_NATION_NAMES; i++)
                C_var_update_data(g_nation_colors + i, nation_color_update,
                                  &g_nations[i].color);

        /* Special cargo */
        g_cargo_names[G_CT_GOLD] = C_str("g-cargo-gold", "Gold");
        g_cargo_names[G_CT_CREW] = C_str("g-cargo-crew", "Crew");

        /* Tech preview cargo */
        g_cargo_names[G_CT_RATIONS] = C_str("g-cargo-rations", "Rations");
        g_cargo_names[G_CT_WOOD] = C_str("g-cargo-wood", "Wood");
        g_cargo_names[G_CT_IRON] = C_str("g-cargo-iron", "Iron");

}

/******************************************************************************\
 Converts a cost to a build time.
\******************************************************************************/
int G_build_time(const g_cost_t *cost)
{
        g_cost_t delay;
        int i, time;

        if (!cost)
                return 0;
        delay = *cost;
        delay.cargo[G_CT_GOLD] /= 100;
        for (time = i = 0; i < G_CARGO_TYPES; i++)
                time += delay.cargo[i] * 100;
        return time;
}

/******************************************************************************\
 Reset game structures.
\******************************************************************************/
void G_reset_elements(void)
{
        int i;

        g_host_inited = FALSE;
        g_game_over = FALSE;
        G_cleanup_ships();
        G_cleanup_tiles();

        /* Reset clients, keeping names */
        for (i = 0; i < N_CLIENTS_MAX; i++)
                g_clients[i].nation = G_NN_NONE;

        /* The server "client" has fixed information */
        g_clients[N_SERVER_ID].nation = G_NN_PIRATE;

        /* We can reuse names now */
        G_reset_name_counts();

        /* Start out without a selected ship */
        G_ship_select(NULL);

        /* Not hovering over anything */
        Py_CLEAR(g_hover_ship);
        g_hover_tile = -1;
}

