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

static int Store_traverse(g_store_t *self, visitproc visit, void *arg) {
//        Py_VISIT(self->cost);
        return 0;
}

static int Store_clear(g_store_t *self) {
//        Py_XDECREF(self->cost);
        return 0;
}

static void Store_dealloc(g_store_t* self) {
        Store_clear(self);
        self->ob_type->tp_free((PyObject*)self);
}

PyObject *Store_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
        g_store_t *self;
        self = (g_store_t *)type->tp_alloc(type, 0);
        return (PyObject *)self;
}

int Store_init(g_store_t *self , PyObject *args /*, PyObject *kwds */ )
{

        if (!PyArg_ParseTuple(args, "i", &self->capacity))
                return -1;

        return 0;
}

PyObject *Store_add(g_store_t *self , PyObject *args /*, PyObject *kwds */ )
{
        int cargo, amount, ret;
        if (!PyArg_ParseTuple(args, "ii", &cargo, &amount))
                return NULL;

        ret = G_store_add(self, cargo, amount);
        return Py_BuildValue("i", ret);
}

//
static PyMemberDef Store_members[] =
{
// {"cost", T_OBJECT_EX, offsetof(Store, cost), 0, "Ship's costs"},
 {"capacity", T_INT, offsetof(g_store_t, capacity), RO, "Store's capacity"},
// {"name", T_STRING, offsetof(Store, name), RO, "Ship's name"},
// {"speed", T_FLOAT, offsetof(Store, speed), 0, "Ship's speed"},
 {NULL}  /* Sentinel */
};

static PyMethodDef Store_methods[] =
{
 {"add", (PyCFunction)Store_add, METH_VARARGS,
         "Add [amount] of [cargo] to the store"},
 {NULL}  /* Sentinel */
};

static PyGetSetDef Store_getseters[] =
{
 {NULL}  /* Sentinel */
};

//
PyTypeObject StoreType =
{
 PyObject_HEAD_INIT(NULL)
 0,                         /*ob_size*/
 "plutocracy.game.Store",   /*tp_name*/
 sizeof(g_store_t),             /*tp_basicsize*/
 0,                         /*tp_itemsize*/
 (destructor)Store_dealloc, /*tp_dealloc*/
 0,                         /*tp_print*/
 0,                         /*tp_getattr*/
 0,                         /*tp_setattr*/
 0,                         /*tp_compare*/
 0,                         /*tp_repr*/
 0,                         /*tp_as_number*/
 0,                         /*tp_as_sequence*/
 0,                         /*tp_as_mapping*/
 0,                         /*tp_hash */
 0,                         /*tp_call*/
 0,                         /*tp_str*/
 0,                         /*tp_getattro*/
 0,                         /*tp_setattro*/
 0,                         /*tp_as_buffer*/
 Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
 "Ship class",              /* tp_doc */
 (traverseproc)Store_traverse, /* tp_traverse */
 (inquiry)Store_clear,      /* tp_clear */
 0,                         /* tp_richcompare */
 0,                         /* tp_weaklistoffset */
 0,                         /* tp_iter */
 0,                         /* tp_iternext */
 Store_methods,             /* tp_methods */
 Store_members,             /* tp_members */
 Store_getseters,           /* tp_getset */
 0,                         /* tp_base */
 0,                         /* tp_dict */
 0,                         /* tp_descr_get */
 0,                         /* tp_descr_set */
 0,                         /* tp_dictoffset */
 (initproc)Store_init,      /* tp_init */
 0,                         /* tp_alloc */
 Store_new,                 /* tp_new */
};

static int ShipClass_traverse(ShipClass *self, visitproc visit, void *arg) {
        Py_VISIT(self->cost);
        return 0;
}

static int ShipClass_clear(ShipClass *self) {
        Py_XDECREF(self->cost);
        return 0;
}

static void ShipClass_dealloc(ShipClass* self) {
        ShipClass_clear(self);
        self->ob_type->tp_free((PyObject*)self);
}

static PyObject *ShipClass_new(PyTypeObject *type, PyObject *args,
                               PyObject *kwds  )
{
        ShipClass *self;
        self = (ShipClass *)type->tp_alloc(type, 0);
        if (self != NULL) {
                int i;
                self->cost = PyList_New(G_CARGO_TYPES);
                if (self->cost == NULL)
                {
                        Py_DECREF(self);
                        return NULL;
                }
                for (i=0; i < G_CARGO_TYPES; i++)
                        PyList_SetItem(self->cost, i, Py_BuildValue("i", 0));
        }
        return (PyObject *)self;
}

static int ShipClass_init(ShipClass *self , PyObject *args )
{
        if (!PyArg_ParseTuple(args, "sss", &self->name, &self->model_path,
                              &self->ring_icon))
                return -1;

        return 0;
}
//
static PyMemberDef ShipClass_members[] =
{
 {"cost", T_OBJECT_EX, offsetof(ShipClass, cost), 0, "Ship's costs"},
 {"model_path", T_STRING, offsetof(ShipClass, model_path), RO,
  "Ship's model path"},
 {"ring_icon", T_STRING, offsetof(ShipClass, ring_icon), RO,
  "Ship's ring icon"},
 {"ring_id", T_INT, offsetof(ShipClass, ring_id), RO,
  "Ship class ring icon id"},
 {"name", T_STRING, offsetof(ShipClass, name), RO, "Ship's name"},
 {"speed", T_FLOAT, offsetof(ShipClass, speed), 0, "Ship's speed"},
 {"max_health", T_INT, offsetof(ShipClass, health), 0, "Ship's health"},
 {"cargo", T_INT, offsetof(ShipClass, cargo), 0,
  "Amount of cargo ship can hold"},
 {NULL}  /* Sentinel */
};

static PyMethodDef ShipClass_methods[] =
{
{NULL}  /* Sentinel */
};

PyTypeObject ShipClassType =
{
 PyObject_HEAD_INIT(NULL)
 0,                         /*ob_size*/
 "plutocracy.game.ShipClass", /*tp_name*/
 sizeof(ShipClass),         /*tp_basicsize*/
 0,                         /*tp_itemsize*/
 (destructor)ShipClass_dealloc, /*tp_dealloc*/
 0,                         /*tp_print*/
 0,                         /*tp_getattr*/
 0,                         /*tp_setattr*/
 0,                         /*tp_compare*/
 0,                         /*tp_repr*/
 0,                         /*tp_as_number*/
 0,                         /*tp_as_sequence*/
 0,                         /*tp_as_mapping*/
 0,                         /*tp_hash */
 0,                         /*tp_call*/
 0,                         /*tp_str*/
 0,                         /*tp_getattro*/
 0,                         /*tp_setattro*/
 0,                         /*tp_as_buffer*/
 Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
 "Ship class",              /* tp_doc */
 (traverseproc)ShipClass_traverse, /* tp_traverse */
 (inquiry)ShipClass_clear,  /* tp_clear */
 0,                         /* tp_richcompare */
 0,                         /* tp_weaklistoffset */
 0,                         /* tp_iter */
 0,                         /* tp_iternext */
 ShipClass_methods,         /* tp_methods */
 ShipClass_members,         /* tp_members */
 0,                         /* tp_getset */
 0,                         /* tp_base */
 0,                         /* tp_dict */
 0,                         /* tp_descr_get */
 0,                         /* tp_descr_set */
 0,                         /* tp_dictoffset */
 (initproc)ShipClass_init,  /* tp_init */
 0,                         /* tp_alloc */
 ShipClass_new,             /* tp_new */
};

static int Ship_traverse(g_ship_t *self, visitproc visit, void *arg) {
        Py_VISIT(self->boarding_ship);
        Py_VISIT(self->target_ship);
        Py_VISIT(self->store);
        Py_VISIT(self->class);
        return 0;
}

static int Ship_clear(g_ship_t *self) {
        Py_CLEAR(self->boarding_ship);
        Py_CLEAR(self->target_ship);
        Py_CLEAR(self->store);
        Py_CLEAR(self->class);
        return 0;
}

static void Ship_dealloc(g_ship_t* self) {
        Ship_clear(self);
        self->ob_type->tp_free((PyObject*)self);
}

PyObject *Ship_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
        g_ship_t *self;
        self = (g_ship_t *)type->tp_alloc(type, 0);
        return (PyObject *)self;
}

int Ship_init(g_ship_t *self , PyObject *args /*, PyObject *kwds */ )
{
        return 0;
}

static PyMemberDef Ship_members[] =
{
 {"class", T_OBJECT_EX, offsetof(g_ship_t, class), RO, "Ship's class"},
 {"store", T_OBJECT_EX, offsetof(g_ship_t, store), RO, "Ship's cargo store"},
 {"model", T_OBJECT_EX, offsetof(g_ship_t, model), 0, ""},
 {"progress", T_FLOAT, offsetof(g_ship_t, progress), RO,
  "Ship's current movement progress"},
 {"health", T_INT, offsetof(g_ship_t, health), 0, "Ship's current health"},
 {NULL}  /* Sentinel */
};

static PyMethodDef Ship_methods[] =
{
 {NULL}  /* Sentinel */
};

static PyGetSetDef Ship_getseters[] =
{
 {NULL}  /* Sentinel */
};

PyTypeObject ShipType =
{
 PyObject_HEAD_INIT(NULL)
 0,                         /*ob_size*/
 "plutocracy.game.Ship",    /*tp_name*/
 sizeof(g_ship_t),              /*tp_basicsize*/
 0,                         /*tp_itemsize*/
 (destructor)Ship_dealloc,  /*tp_dealloc*/
 0,                         /*tp_print*/
 0,                         /*tp_getattr*/
 0,                         /*tp_setattr*/
 0,                         /*tp_compare*/
 0,                         /*tp_repr*/
 0,                         /*tp_as_number*/
 0,                         /*tp_as_sequence*/
 0,                         /*tp_as_mapping*/
 0,                         /*tp_hash */
 0,                         /*tp_call*/
 0,                         /*tp_str*/
 0,                         /*tp_getattro*/
 0,                         /*tp_setattro*/
 0,                         /*tp_as_buffer*/
 Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
 "Ship",                    /* tp_doc */
 (traverseproc)Ship_traverse, /* tp_traverse */
 (inquiry)Ship_clear,       /* tp_clear */
 0,                         /* tp_richcompare */
 0,                         /* tp_weaklistoffset */
 0,                         /* tp_iter */
 0,                         /* tp_iternext */
 Ship_methods,              /* tp_methods */
 Ship_members,              /* tp_members */
 Ship_getseters,            /* tp_getset */
 0,                         /* tp_base */
 0,                         /* tp_dict */
 0,                         /* tp_descr_get */
 0,                         /* tp_descr_set */
 0,                         /* tp_dictoffset */
 (initproc)Ship_init,  /* tp_init */
 0,                         /* tp_alloc */
 Ship_new,         /* tp_new */
};

static int BuildingClass_traverse(BuildingClass *self, visitproc visit,
                                  void *arg) {
        Py_VISIT(self->cost);
        return 0;
}

static int BuildingClass_clear(BuildingClass *self) {
        Py_XDECREF(self->cost);
        return 0;
}

static void BuildingClass_dealloc(BuildingClass* self) {
        BuildingClass_clear(self);
        self->ob_type->tp_free((PyObject*)self);
}

static PyObject *BuildingClass_new(PyTypeObject *type, PyObject *args,
                                   PyObject *kwds  )
{
        BuildingClass *self;
        self = (BuildingClass *)type->tp_alloc(type, 0);
        if (self != NULL) {
                int i;
                self->cost = PyList_New(G_CARGO_TYPES);
                if (self->cost == NULL)
                {
                        Py_DECREF(self);
                        return NULL;
                }
                for (i=0; i < G_CARGO_TYPES; i++)
                        PyList_SetItem(self->cost, i, Py_BuildValue("i", 0));
                self->callbacks = PyDict_New();
                if (self->callbacks == NULL)
                {
                        Py_DECREF(self);
                        return NULL;
                }
        }
        return (PyObject *)self;
}

static int BuildingClass_init(BuildingClass *self , PyObject *args )
{
        if (!PyArg_ParseTuple(args, "sss", &self->name, &self->model_path,
                              &self->ring_icon))
                return -1;

        return 0;
}

PyDoc_STRVAR(building_buildable_doc,
             "Set to something true if building can be built by a player");

static PyObject *BC_get_buildable(BuildingClass *self, void *closure)
{
    if(self->buildable)
            Py_RETURN_TRUE;
    else
            Py_RETURN_FALSE;
}


static int BC_set_buildable(BuildingClass *self, PyObject *value,
                            void *closure)
{
        if(value == NULL)
                self->buildable = FALSE;
        if(PyObject_IsTrue(value))
                self->buildable = TRUE;
        else
                self->buildable = FALSE;
        return 0;
}

static PyObject *BC_connect(BuildingClass *self, PyObject *args)
{
        char *signal_name;
        PyObject *callback;
        if (!PyArg_ParseTuple(args, "sO", &signal_name, &callback))
                return NULL;
        if(callback == Py_None)
        {
                if(PyDict_DelItemString(self->callbacks, signal_name) == -1)
                        PyErr_Clear(); /* No error if key doesn't exist */
                Py_RETURN_NONE;
        }
        if (!PyCallable_Check(callback))
        {
                PyErr_SetString(PyExc_StandardError,
                                "callback must be callable");
                return NULL;
        }
        PyDict_SetItemString(self->callbacks, signal_name, callback);
        Py_RETURN_NONE;
}

/* Member doc strings */
PyDoc_STRVAR(building_cargo_doc,
          "Amount of cargo the building can store (0 if it can't store any).");

#define BC BuildingClass /* Make member def a bit shorter */

static PyMemberDef BuildingClass_members[] =
{
 {"cost", T_OBJECT_EX, offsetof(BC, cost), 0, "Building class cost"},
 {"model_path", T_STRING, offsetof(BC, model_path), RO,
  "Building class model path"},
 {"ring_icon", T_STRING, offsetof(BC, ring_icon), RO,
  "Building class ring icon"},
 {"ring_id", T_INT, offsetof(BC, ring_id), RO, "Building class ring icon id"},
 {"name", T_STRING, offsetof(BC, name), RO, "Building class name"},
 {"health", T_INT, offsetof(BC, health), 0, "Building class health"},
 {"cargo", T_INT, offsetof(BC, cargo), 0, building_cargo_doc},
 {NULL}  /* Sentinel */
};

static PyMethodDef BuildingClass_methods[] =
{
 {"connect", (PyCFunction)BC_connect, METH_VARARGS,
  "connect a callback to a signal" },
 {NULL}  /* Sentinel */
};

static PyGetSetDef BuildingClass_getseters[] =
{
 {"buildable", (getter)BC_get_buildable, (setter)BC_set_buildable,
  building_buildable_doc, NULL},
};

PyTypeObject BuildingClassType =
{
 PyObject_HEAD_INIT(NULL)
 0,                         /*ob_size*/
 "plutocracy.game.BuildingClass", /*tp_name*/
 sizeof(BuildingClass),     /*tp_basicsize*/
 0,                         /*tp_itemsize*/
 (destructor)BuildingClass_dealloc, /*tp_dealloc*/
 0,                         /*tp_print*/
 0,                         /*tp_getattr*/
 0,                         /*tp_setattr*/
 0,                         /*tp_compare*/
 0,                         /*tp_repr*/
 0,                         /*tp_as_number*/
 0,                         /*tp_as_sequence*/
 0,                         /*tp_as_mapping*/
 0,                         /*tp_hash */
 0,                         /*tp_call*/
 0,                         /*tp_str*/
 0,                         /*tp_getattro*/
 0,                         /*tp_setattro*/
 0,                         /*tp_as_buffer*/
 Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
 "Building class",              /* tp_doc */
 (traverseproc)BuildingClass_traverse, /* tp_traverse */
 (inquiry)BuildingClass_clear,  /* tp_clear */
 0,                         /* tp_richcompare */
 0,                         /* tp_weaklistoffset */
 0,                         /* tp_iter */
 0,                         /* tp_iternext */
 BuildingClass_methods,     /* tp_methods */
 BuildingClass_members,     /* tp_members */
 BuildingClass_getseters,   /* tp_getset */
 0,                         /* tp_base */
 0,                         /* tp_dict */
 0,                         /* tp_descr_get */
 0,                         /* tp_descr_set */
 0,                         /* tp_dictoffset */
 (initproc)BuildingClass_init,  /* tp_init */
 0,                         /* tp_alloc */
 BuildingClass_new,         /* tp_new */
};

static int G_building_traverse(g_building_t *self, visitproc visit, void *arg) {
        Py_VISIT(self->store);
        Py_VISIT(self->class);
        return 0;
}

static int G_building_clear(g_building_t *self) {
        Py_CLEAR(self->store);
        Py_CLEAR(self->class);
        Py_CLEAR(self->model);
        return 0;
}

static void G_building_dealloc(g_building_t* self) {
        G_building_clear(self);
        self->ob_type->tp_free((PyObject*)self);
}

PyObject *G_building_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
        g_building_t *self;
        self = (g_building_t *)type->tp_alloc(type, 0);
        return (PyObject *)self;
}

int G_building_init(g_building_t *self , PyObject *args /*, PyObject *kwds */ )
{
        return 0;
}

static PyMemberDef G_building_members[] =
{
 {"class", T_OBJECT_EX, offsetof(g_building_t, class), RO, "Building's class"},
 {"store", T_OBJECT_EX, offsetof(g_building_t, store), RO,
  "Building's cargo store"},
 {"health", T_INT, offsetof(g_building_t, health), 0, "Building's current health"},
 {NULL}  /* Sentinel */
};

static PyMethodDef G_building_methods[] =
{
 {NULL}  /* Sentinel */
};

static PyGetSetDef G_building_getseters[] =
{
 {NULL}  /* Sentinel */
};

PyTypeObject G_building_type =
{
 PyObject_HEAD_INIT(NULL)
 0,                         /*ob_size*/
 "plutocracy.game.Building",    /*tp_name*/
 sizeof(g_building_t),              /*tp_basicsize*/
 0,                         /*tp_itemsize*/
 (destructor)G_building_dealloc,  /*tp_dealloc*/
 0,                         /*tp_print*/
 0,                         /*tp_getattr*/
 0,                         /*tp_setattr*/
 0,                         /*tp_compare*/
 0,                         /*tp_repr*/
 0,                         /*tp_as_number*/
 0,                         /*tp_as_sequence*/
 0,                         /*tp_as_mapping*/
 0,                         /*tp_hash */
 0,                         /*tp_call*/
 0,                         /*tp_str*/
 0,                         /*tp_getattro*/
 0,                         /*tp_setattro*/
 0,                         /*tp_as_buffer*/
 Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
 "Building",                    /* tp_doc */
 (traverseproc)G_building_traverse, /* tp_traverse */
 (inquiry)G_building_clear,       /* tp_clear */
 0,                         /* tp_richcompare */
 0,                         /* tp_weaklistoffset */
 0,                         /* tp_iter */
 0,                         /* tp_iternext */
 G_building_methods,              /* tp_methods */
 G_building_members,              /* tp_members */
 G_building_getseters,            /* tp_getset */
 0,                         /* tp_base */
 0,                         /* tp_dict */
 0,                         /* tp_descr_get */
 0,                         /* tp_descr_set */
 0,                         /* tp_dictoffset */
 (initproc)G_building_init,  /* tp_init */
 0,                         /* tp_alloc */
 G_building_new,         /* tp_new */
};
