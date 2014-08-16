/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "n_common.h"

c_var_t n_port;

/******************************************************************************\
 Registers the network namespace variables.
\******************************************************************************/
void N_register_variables(void)
{
        C_register_integer(&n_port, "n_port", 32500, "server port");
}

