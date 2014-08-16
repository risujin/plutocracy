/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - John Black

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

#include "../render/r_common.h"
#include "../game/g_shared.h"

static PyObject *register_variables(PyObject *self, PyObject *args) {
        R_register_variables();
        Py_RETURN_NONE;
}

/******************************************************************************\
 Initialize SDL.
\******************************************************************************/
static PyObject *init_sdl(PyObject *self, PyObject *args) {
        SDL_version compiled;
        const SDL_version *linked;

        SDL_VERSION(&compiled);
        C_debug("Compiled with SDL %d.%d.%d",
                compiled.major, compiled.minor, compiled.patch);
        linked = SDL_Linked_Version();
        if (!linked)
                C_error("Failed to get SDL linked version");
        C_debug("Linked with SDL %d.%d.%d",
                linked->major, linked->minor, linked->patch);
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
                C_error("Failed to initialize SDL: %s", SDL_GetError());
        SDL_WM_SetCaption(PACKAGE_STRING, PACKAGE);
        Py_RETURN_NONE;
}

static PyObject *init(PyObject *self, PyObject *args) {
//        init_sdl();
        R_init();
        Py_RETURN_NONE;
}

static PyObject *load_test_assets(PyObject *self, PyObject *args) {
        R_load_test_assets();
        Py_RETURN_NONE;
}

static PyObject *cleanup(PyObject *self, PyObject *args) {
        R_free_test_assets();
        R_cleanup();
        Py_RETURN_NONE;
}

static PyObject *start_frame(PyObject *self, PyObject *args) {
        R_start_frame();
        Py_RETURN_NONE;
}

static PyObject *finish_frame(PyObject *self, PyObject *args) {
        R_finish_frame();
        Py_RETURN_NONE;
}

static PyObject *start_globe(PyObject *self, PyObject *args) {
        R_start_globe();
        Py_RETURN_NONE;
}

static PyObject *finish_globe(PyObject *self, PyObject *args) {
        R_finish_globe();
        Py_RETURN_NONE;
}

/******************************************************************************\
 Renders the status text on the screen. This function uses the throttled
 counter for interval timing. Counters are reset periodically.
\******************************************************************************/
static PyObject *render_status(PyObject *self, PyObject *args) {
        R_render_status();
        Py_RETURN_NONE;
}

static PyObject *render_border(PyObject *self, PyObject *args) {
        PyObject *dashed;
        int tile;
        if(!PyArg_ParseTuple(args, "iO", &tile, &dashed))
        {
                return NULL;
        }
        R_render_border(tile, g_nations[G_NN_GREEN].color,
                        PyObject_IsTrue(dashed));
        Py_RETURN_NONE;
}

static PyMethodDef module_methods[] =
{
 { "register_variables", register_variables, METH_NOARGS, ""},
 { "init_sdl", init_sdl, METH_NOARGS, ""},
 { "init", init, METH_NOARGS, ""},
 { "load_test_assets", load_test_assets, METH_NOARGS, ""},
 //  { "main", run_main, METH_NOARGS, ""},
 { "cleanup", cleanup, METH_NOARGS, ""},
 { "start_frame", start_frame, METH_NOARGS, ""},
 { "finish_frame", finish_frame, METH_NOARGS, ""},
 { "start_globe", start_globe, METH_NOARGS, ""},
 { "finish_globe", finish_globe, METH_NOARGS, ""},
 { "render_status", render_status, METH_NOARGS, ""},
 { "render_border", render_border, METH_VARARGS, ""},
 {NULL}  /* Sentinel */
};

PyObject *init_render_api(void)
{
        PyObject* m;

        m = Py_InitModule3("api.render", module_methods, "");

        PyType_Ready(&R_model_type);
        Py_INCREF(&R_model_type);
        PyModule_AddObject(m, "Model", (PyObject *)&R_model_type);

        PyType_Ready(&R_tile_wrapper_type);
        Py_INCREF(&R_tile_wrapper_type);
        PyModule_AddObject(m, "Model", (PyObject *)&R_tile_wrapper_type);

        return m;
}
