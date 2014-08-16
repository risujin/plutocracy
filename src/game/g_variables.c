/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "g_common.h"

/* Game testing */
c_var_t g_debug_net, g_test_globe;

/* Globe variables */
c_var_t g_forest, g_globe_seed, g_globe_subdiv4, g_island_num, g_island_size,
        g_island_variance;

/* Nation colors */
c_var_t g_nation_colors[G_NATION_NAMES];

/* Player settings */
c_var_t g_draw_distance, g_name;

/* Server settings */
c_var_t g_players, g_time_limit, g_victory_gold;
c_var_t g_player_ship_limit, g_player_building_limit, g_echo_rate;

/* Master server */
c_var_t g_master, g_master_url;

/******************************************************************************\
 Registers the game namespace variables.
\******************************************************************************/
void G_register_variables(void)
{
        /* Game testing */
        C_register_integer(&g_test_globe, "g_test_globe", FALSE,
                           "test globe tile click detection");
        g_test_globe.edit = C_VE_ANYTIME;
        C_register_integer(&g_debug_net, "g_debug_net", FALSE,
                           "log network messages");
        g_debug_net.edit = C_VE_ANYTIME;

        /* Globe variables */
        C_register_integer(&g_globe_seed, "g_globe_seed", C_rand(),
                           "seed for globe terrain generator");
        g_globe_seed.archive = FALSE;
        C_register_integer(&g_globe_subdiv4, "g_globe_subdiv4", 4,
                           "globe subdivision iterations, 0-5");
        C_register_integer(&g_island_num, "g_islands", 0,
                           "number of islands, 0 for default");
        C_register_integer(&g_island_size, "g_island_size", 0,
                           "maximum size of islands, 0 for default");
        C_register_float(&g_island_variance, "g_island_variance", -1.f,
                           "proportion of island size to randomize");
        C_register_float(&g_forest, "g_forest", 0.8f,
                           "proportion of tiles that have trees");

        /* Nation colors */
        C_register_string(g_nation_colors + G_NN_RED, "g_color_red",
                          "#80ef2929", "red national color");
        C_register_string(g_nation_colors + G_NN_GREEN, "g_color_green",
                          "#808ae234", "green national color");
        C_register_string(g_nation_colors + G_NN_BLUE, "g_color_blue",
                          "#80729fcf", "blue national color");
        C_register_string(g_nation_colors + G_NN_PIRATE, "g_color_pirate",
                          "#80a0a0a0", "pirate color");

        /* Player settings */
        C_register_string(&g_name, "g_name", "Newbie", "player name");
        C_register_float(&g_draw_distance, "g_draw_distance", 15.f,
                         "model drawing distance from surface");
        g_draw_distance.edit = C_VE_ANYTIME;

        /* Server settings */
        C_register_integer(&g_players, "g_players", 12,
                           "maximum number of players");
        C_register_integer(&g_time_limit, "g_time_limit", 45,
                           "minutes after which game ends");
        C_register_integer(&g_victory_gold, "g_victory_gold", 30000,
                           "gold a team needs to win the game");
        C_register_integer(&g_player_ship_limit, "g_player_ship_limit", 1000,
                           "max number of ships per player");
        g_player_ship_limit.edit = C_VE_ANYTIME;
        C_register_integer(&g_player_building_limit, "g_player_building_limit",
                           1000, "max number of buildings per player");
        g_player_building_limit.edit = C_VE_ANYTIME;
        C_register_integer(&g_echo_rate, "g_echo_rate", 10000,
                           "number of milliseconds between sending echo "
                           "requests, set to 0 to disable");
        g_echo_rate.edit = C_VE_ANYTIME;

        /* Master server */
        C_register_string(&g_master, "g_master", "master.plutocracy.ca",
                          "address of the master server");
        C_register_string(&g_master_url, "g_master_url", "/",
                          "URL path to master server script");
}

