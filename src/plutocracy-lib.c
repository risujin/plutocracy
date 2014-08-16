/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - John Black

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "common/c_shared.h"
#include "network/n_shared.h"
#include "render/r_shared.h"
#include "interface/i_shared.h"
#include "game/g_shared.h"
#include "plutocracy-lib.h"

static PyObject *c_update(PyObject *pArgs, PyObject *kArgs) {
        SDL_Event ev;
                
        R_start_frame();
        while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT)
                {
                        c_exit = TRUE;
                        Py_RETURN_NONE;
                }
                I_dispatch(&ev);
        }
        R_start_globe();
        G_render_globe();
        R_finish_globe();
        G_render_ships();
        G_render_game_over();
        I_render();
        R_render_status();
        R_finish_frame();
        C_time_update();
        C_throttle_fps();

        /* Update the game after rendering everything */
        G_update_host();
        G_update_client();
        return Py_BuildValue("i", 0);
}

static PyObject *check_exit(PyObject *pArgs, PyObject *kArgs)
{
        if(c_exit)
                Py_RETURN_TRUE;
        else
                Py_RETURN_FALSE;
}

static PyMethodDef module_methods[] = 
{
  { "c_update", c_update, METH_NOARGS, ""},
  { "check_exit", check_exit, METH_NOARGS, ""},
  {NULL}  /* Sentinel */
};

#ifndef PyMODINIT_FUNC  /* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC initapi(void) 
{
  PyObject* m;
  
  m = Py_InitModule3("api", module_methods, "");
  
  if (m == NULL)
    return;
  
  PyModule_AddObject( m, "common", (PyObject*)init_common_api() );
  PyModule_AddObject( m, "network", (PyObject*)init_network_api() );
  PyModule_AddObject( m, "render", (PyObject*)init_render_api() );
  PyModule_AddObject( m, "interface", (PyObject*)init_interface_api() );
  PyModule_AddObject( m, "game", (PyObject*)init_game_api() );
}
