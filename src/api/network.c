/*****************************************************************************\
 Plutocracy - Copyright (C) 2008 - John Black

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\*****************************************************************************/

#include "../network/n_common.h"

static PyObject *register_variables(PyObject *self, PyObject *args) {
        N_register_variables();
        Py_RETURN_NONE;
}

static PyObject *init(PyObject *self, PyObject *args) {
        N_init();
        Py_RETURN_NONE;
}

static PyObject *cleanup(PyObject *self, PyObject *args) {
        N_cleanup();
        Py_RETURN_NONE;
}

static PyMethodDef module_methods[] = 
{
 { "register_variables", register_variables, METH_NOARGS, ""},
 { "init", init, METH_NOARGS, ""}, 
 { "cleanup", cleanup, METH_NOARGS, ""}, 
 {NULL}  /* Sentinel */
};

PyObject *init_network_api(void) 
{
        PyObject* m;

        m = Py_InitModule3("api.network", module_methods, "");

        return m;
}
