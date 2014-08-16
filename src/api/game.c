/*****************************************************************************\
 Plutocracy - Copyright (C) 2008 - John Black

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\*****************************************************************************/

#include "../game/g_common.h"

PyObject *g_callbacks;

static PyObject *register_variables(PyObject *self, PyObject *args) {
        G_register_variables();
        Py_RETURN_NONE;
}

static PyObject *init(PyObject *self, PyObject *args) {
        g_callbacks = PyDict_New();
        G_init();
        Py_RETURN_NONE;
}

static PyObject *refresh_servers(PyObject *self, PyObject *args) {
        G_refresh_servers();
        Py_RETURN_NONE;
}

static PyObject *cleanup(PyObject *self, PyObject *args) {
        G_cleanup();
        Py_CLEAR(g_callbacks);
        Py_RETURN_NONE;
}

static PyObject *render_globe(PyObject *self, PyObject *args) {
        G_render_globe();
        Py_RETURN_NONE;
}

static PyObject *render_ships(PyObject *self, PyObject *args) {
        G_render_ships();
        Py_RETURN_NONE;
}

static PyObject *update_host(PyObject *self, PyObject *args) {
        G_update_host();
        Py_RETURN_NONE;
}

static PyObject *render_game_over(PyObject *self, PyObject *args) {
        G_render_game_over();
        Py_RETURN_NONE;
}

static PyObject *update_client(PyObject *self, PyObject *args) {
        G_update_client();
        Py_RETURN_NONE;
}

static PyObject *add_shipclass(PyObject *self, PyObject *args) {
        ShipClass *shipclass;
        if(g_initilized)
                Py_RETURN_NONE;
        if(!PyArg_ParseTuple(args, "O!", &ShipClassType, &shipclass))
        {
                return NULL;
        }
        shipclass->class_id = PyList_GET_SIZE(g_ship_class_list);
        PyList_Append(g_ship_class_list, (PyObject*)shipclass);
        Py_RETURN_NONE;
}

static PyObject *add_buildingclass(PyObject *self, PyObject *args) {
        PyObject *buildingclass;
        if(g_initilized)
                Py_RETURN_NONE;
        if(!PyArg_ParseTuple(args, "O!", &BuildingClassType, &buildingclass))
        {
                return NULL;
        }
        PyList_Append(g_building_class_list, buildingclass);
        Py_RETURN_NONE;
}
static PyObject *get_shipclasses(PyObject *self, PyObject *args) {
        Py_INCREF(g_ship_class_list);
        return g_ship_class_list;
}

static PyObject *get_buildingclasses(PyObject *self, PyObject *args) {
        Py_INCREF(g_building_class_list);
        return g_building_class_list;
}

static PyObject *pay(PyObject *self, PyObject *args) {
        int client_id, tile;
        PyObject *cost_list, *pay;
        g_cost_t cost;
        if(!PyArg_ParseTuple(args, "iiOO", &client_id, &tile, &cost_list,
                             &pay))
        {
                return NULL;
        }
        G_list_to_cost(cost_list, &cost);
        if(G_pay(client_id, tile, &cost, PyObject_IsTrue(pay)))
                Py_RETURN_TRUE;
        else
                Py_RETURN_FALSE;
}

static PyObject *cost_to_string(PyObject *self, PyObject *args) {
        PyObject *cost_list;
        g_cost_t cost;
        if(!PyArg_ParseTuple(args, "O", &cost_list))
        {
                return NULL;
        }
        G_list_to_cost(cost_list, &cost);
        return Py_BuildValue("s", G_cost_to_string(&cost));
}

static PyObject *ship_spawn(PyObject *self, PyObject *args) {
        int index, client, tile;
        g_ship_t *ship;
        ShipClass *shipclass;
        if(!PyArg_ParseTuple(args, "iiiO!", &index, &client, &tile,
                             &ShipClassType, &shipclass))
        {
                return NULL;
        }
        ship = G_ship_spawn(index, client, tile, shipclass->class_id);
        if(!ship)
                Py_RETURN_NONE;
        return (PyObject*)ship;
}

static PyObject *ship_class_from_ring_id(PyObject *self, PyObject *args) {
        int id;
        ShipClass *shipclass;
        if(!PyArg_ParseTuple(args, "i", &id))
        {
                return NULL;
        }
        shipclass = G_ship_class_from_ring_id(id);
        if(!shipclass)
        {
                Py_RETURN_NONE;
        }
        Py_INCREF(shipclass);
        return (PyObject*)shipclass;
}

static PyObject *connect(PyObject *self, PyObject *args) {
        char *signal_name;
        PyObject *callback;
        if(!PyArg_ParseTuple(args, "sO", &signal_name, &callback))
        {
                return NULL;
        }
        if(callback == Py_None)
        {
                if(PyDict_DelItemString(g_callbacks, signal_name) == -1)
                        PyErr_Clear(); /* No error if key doesn't exist */
                Py_RETURN_NONE;
        }
        if (!PyCallable_Check(callback))
        {
                PyErr_SetString(PyExc_StandardError,
                                "callback must be callable");
                return NULL;
        }
        C_debug("connecting %p to %s", callback, signal_name);
        PyDict_SetItemString(g_callbacks, signal_name, callback);
        Py_RETURN_NONE;
}

static PyMethodDef module_methods[] =
{
 { "register_variables", register_variables, METH_NOARGS, ""},
 { "init", init, METH_NOARGS, ""},
 { "refresh_servers", refresh_servers, METH_NOARGS, ""},
 { "cleanup", cleanup, METH_NOARGS, ""},
 { "render_globe", render_globe, METH_NOARGS, ""},
 { "render_ships", render_ships, METH_NOARGS, ""},
 { "render_game_over", render_game_over, METH_NOARGS, ""},
 { "update_host", update_host, METH_NOARGS, ""},
 { "update_client", update_client, METH_NOARGS, ""},
 { "add_shipclass", add_shipclass, METH_VARARGS, ""},
 { "get_shipclasses", get_shipclasses, METH_NOARGS, ""},
 { "add_buildingclass", add_buildingclass, METH_VARARGS, ""},
 { "get_buildingclasses", get_buildingclasses, METH_NOARGS, ""},
 { "pay", pay, METH_VARARGS, ""},
 { "cost_to_string", cost_to_string, METH_VARARGS, ""},
 { "ship_spawn", ship_spawn, METH_VARARGS, ""},
 { "ship_class_from_ring_id", ship_class_from_ring_id, METH_VARARGS, ""},
 { "connect", connect, METH_VARARGS, ""},
 {NULL}  /* Sentinel */
};

PyObject *init_game_api(void)
{
        PyObject* m;

        g_ship_class_list = PyList_New(0);
        g_building_class_list = PyList_New(0);

        m = Py_InitModule3("api.game", module_methods, "");

        PyModule_AddIntConstant(m, "G_PROTOCOL", G_PROTOCOL);
        PyModule_AddIntConstant(m, "G_CT_GOLD", G_CT_GOLD);
        PyModule_AddIntConstant(m, "G_CT_CREW", G_CT_CREW);
        PyModule_AddIntConstant(m, "G_CT_RATIONS", G_CT_RATIONS);
        PyModule_AddIntConstant(m, "G_CT_WOOD", G_CT_WOOD);
        PyModule_AddIntConstant(m, "G_CT_IRON", G_CT_IRON);

        PyModule_AddObject(m, "tile_ring_callback",
                           PyCObject_FromVoidPtrAndDesc(G_tile_ring_callback,
                                                        (void*)I_RING_CALLBACK,
                                                        NULL));

        PyType_Ready(&ShipClassType);
        Py_INCREF(&ShipClassType);
        PyModule_AddObject(m, "ShipClass", (PyObject *)&ShipClassType);

        PyType_Ready(&ShipType);
        Py_INCREF(&ShipType);
        PyModule_AddObject(m, "Ship", (PyObject *)&ShipType);

        PyType_Ready(&BuildingClassType);
        Py_INCREF(&BuildingClassType);
        PyModule_AddObject(m, "BuildingClass", (PyObject *)&BuildingClassType);

        PyType_Ready(&G_building_type);
        Py_INCREF(&G_building_type);
        PyModule_AddObject(m, "Building", (PyObject *)&G_building_type);

        PyType_Ready(&StoreType);
        Py_INCREF(&StoreType);
        PyModule_AddObject(m, "Store", (PyObject *)&StoreType);


        if (m == NULL)
                return NULL;
        return m;
}
