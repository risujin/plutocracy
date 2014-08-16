/*****************************************************************************\
 Plutocracy - Copyright (C) 2008 - John Black

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\*****************************************************************************/

#include "../interface/i_common.h"

static PyObject *parse_config(PyObject *self, PyObject *args) {
        I_parse_config();
        Py_RETURN_NONE;
}

static PyObject *register_variables(PyObject *self, PyObject *args) {
        I_register_variables();
        Py_RETURN_NONE;
}

static PyObject *init(PyObject *self, PyObject *args) {
        I_init();
        Py_RETURN_NONE;
}

static PyObject *cleanup(PyObject *self, PyObject *args) {
        I_cleanup();
        Py_RETURN_NONE;
}

static PyObject *check_events(PyObject *self, PyObject *args) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT)
                {
                        c_exit = TRUE;
                        Py_RETURN_NONE;
                }
                I_dispatch(&ev);
        }
        Py_RETURN_NONE;
}

static PyObject *render(PyObject *self, PyObject *args) {
        I_render();
        Py_RETURN_NONE;
}

PyDoc_STRVAR(quick_info_close_doc,
             "Close the quick info window. The window will need to be " \
             "initialized again the next time it is shown.");

static PyObject *quick_info_close(PyObject *self, PyObject *args) {
        I_quick_info_close();
        Py_RETURN_NONE;
}
PyDoc_STRVAR(quick_info_add_doc,
             "Add an information item to the quick info window.");

static PyObject *quick_info_add(PyObject *self, PyObject *args) {
        const char *label, *value;
        int colour;
        if(!PyArg_ParseTuple(args, "ss", &label, &value, &colour))
        {
                return NULL;
        }
        I_quick_info_add(label, value);
        Py_RETURN_NONE;
}

PyDoc_STRVAR(quick_info_add_colour_doc,
             "Add a coloured information item to the quick info window.");

PyDoc_STRVAR(quick_info_add_color_doc,
             "Add a colored information item to the quick info window.");

static PyObject *quick_info_add_colour(PyObject *self, PyObject *args) {
        const char *label, *value;
        int colour;
        if(!PyArg_ParseTuple(args, "ssi", &label, &value, &colour))
        {
                return NULL;
        }
        I_quick_info_add_color(label, value, colour);
        Py_RETURN_NONE;
}

PyDoc_STRVAR(reset_ring_doc,
             " Reset ring contents.");

static PyObject *reset_ring(PyObject *self, PyObject *args) {
        I_reset_ring();
        Py_RETURN_NONE;
}

PyDoc_STRVAR(add_to_ring_doc,
             "add_to_ring( button_id, enabled, title, sub_title) \n" \
             "Add a button to the ring.");

static PyObject *add_to_ring(PyObject *self, PyObject *args) {
        const char *title, *sub;
        int icon;
        PyObject *enabled;
        if(!PyArg_ParseTuple(args, "iOss", &icon, &enabled, &title, &sub))
        {
                return NULL;
        }
        I_add_to_ring(icon, PyObject_IsTrue(enabled), title, sub);
        Py_RETURN_NONE;
}

static PyObject *show_ring(PyObject *self, PyObject *args) {
        PyObject *callback;
        if(!PyArg_ParseTuple(args, "O!", &PyCObject_Type, &callback))
        {
                return NULL;
        }
        if ((int)PyCObject_GetDesc(callback) != I_RING_CALLBACK)
        {
                PyErr_SetString(PyExc_StandardError, "Invalid callback");
                return NULL;
        }
        I_show_ring((i_ring_f)PyCObject_AsVoidPtr(callback));
        Py_RETURN_NONE;
}

PyDoc_STRVAR(reset_servers_doc,
             "reset_servers() \n" \
             "Clear the game server list and enables the Refresh button");
static PyObject *reset_servers(PyObject *self, PyObject *args) {
        I_reset_servers();
        Py_RETURN_NONE;
}

PyDoc_STRVAR(add_server_doc,
             "add_server(name, alt, address, compatible) \n" \
             "Add a server to the game server list.");
static PyObject *add_server(PyObject *self, PyObject *args) {
        const char *name, *alt, *address;
        PyObject *compatible;
        if(!PyArg_ParseTuple(args, "sssO", &name, &alt, &address, &compatible))
        {
                return NULL;
        }
        I_add_server(name, alt, address, PyObject_IsTrue(compatible));
        Py_RETURN_NONE;
}
static PyMethodDef module_methods[] =
{
 { "parse_config", parse_config, METH_NOARGS, ""},
 { "register_variables", register_variables, METH_NOARGS, ""},
 { "init", init, METH_NOARGS, ""},
 { "cleanup", cleanup, METH_NOARGS, ""},
 { "check_events", check_events, METH_NOARGS, ""},
 { "render", render, METH_NOARGS, ""},
 { "quick_info_close", quick_info_close, METH_NOARGS, quick_info_close_doc},
 { "quick_info_add", quick_info_add, METH_VARARGS, quick_info_add_doc},
 { "quick_info_add_colour", quick_info_add_colour, METH_VARARGS,
   quick_info_add_colour_doc},
 { "quick_info_add_color", quick_info_add_colour, METH_VARARGS,
   quick_info_add_color_doc},
 { "quick_info_add_color", quick_info_add_colour, METH_VARARGS,
   quick_info_add_color_doc},
 { "reset_ring", reset_ring, METH_NOARGS, reset_ring_doc},
 { "add_to_ring", add_to_ring, METH_VARARGS, add_to_ring_doc},
 { "show_ring", show_ring, METH_VARARGS, add_to_ring_doc},
 { "reset_servers", reset_servers, METH_NOARGS, reset_servers_doc},
 { "add_server", add_server, METH_VARARGS, add_server_doc},
 {NULL}  /* Sentinel */
};

PyObject *init_interface_api(void)
{
        PyObject* m;

        m = Py_InitModule3("api.interface", module_methods, "");

        if (m == NULL)
                return NULL;
        return m;
}
