/*****************************************************************************\
 Plutocracy - Copyright (C) 2008 - John Black

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\*****************************************************************************/

#include "../common/c_shared.h"

static PyObject *register_variables(PyObject *self, PyObject *args) {
        C_register_variables();
        Py_RETURN_NONE;
}

static PyObject *parse_config_file(PyObject *self, PyObject *args) {
        const char *s;
        int ret;
        if(!PyArg_ParseTuple(args, "s", &s))
        {
                return NULL;
        }
        ret = C_parse_config_file(s);
        return Py_BuildValue("i", ret);
}

static PyObject *open_log_file(PyObject *self, PyObject *args) {
        C_open_log_file();
        Py_RETURN_NONE;
}

static PyObject *endian_check(PyObject *self, PyObject *args) {
        C_endian_check();
        Py_RETURN_NONE;
}

static PyObject *test_mem_check(PyObject *self, PyObject *args) {
        C_test_mem_check();
        Py_RETURN_NONE;
}

static PyObject *init_lang(PyObject *self, PyObject *args) {
        C_init_lang();
        Py_RETURN_NONE;
}

static PyObject *translate_vars(PyObject *self, PyObject *args) {
        C_translate_vars();
        Py_RETURN_NONE;
}

static PyObject *init(PyObject *self, PyObject *args) {
        /* Seed the system random number generator */
        srand((unsigned int)time(NULL));

        /* Initialize */
        Py_RETURN_NONE;
}

static PyObject *cleanup(PyObject *self, PyObject *args) {
        SDL_Quit();
        C_cleanup_lang();
        C_check_leaks();
        Py_RETURN_NONE;
}

static PyObject *time_update(PyObject *self, PyObject *args) {
        C_time_update();
        Py_RETURN_NONE;
}

static PyObject *throttle_fps(PyObject *self, PyObject *args) {
        C_throttle_fps();
        Py_RETURN_NONE;
}

static PyObject *write_autogen(PyObject *self, PyObject *args) {
        C_write_autogen();
        Py_RETURN_NONE;
}

static PyMethodDef module_methods[] = 
{
 { "register_variables", register_variables, METH_NOARGS, ""},
 { "parse_config_file", parse_config_file, METH_VARARGS, ""},
 { "open_log_file", open_log_file, METH_NOARGS, ""},
 { "endian_check", endian_check, METH_NOARGS, ""},
 { "test_mem_check", test_mem_check, METH_NOARGS, ""},
 { "init_lang", init_lang, METH_NOARGS, ""},
 { "translate_vars", translate_vars, METH_NOARGS, ""},
 { "init", init, METH_NOARGS, ""},
 { "cleanup", cleanup, METH_NOARGS, ""},
 { "time_update", time_update, METH_NOARGS, ""},
 { "throttle_fps", throttle_fps, METH_NOARGS, ""},
 { "write_autogen", write_autogen, METH_NOARGS, ""},
 {NULL}  /* Sentinel */
};

PyObject *init_common_api(void) 
{
        PyObject* m;

        m = Py_InitModule3("api.common", module_methods, "");

        return m;
}
