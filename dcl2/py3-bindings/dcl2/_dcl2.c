#include <Python.h>

#include "dcl2.h"


typedef struct {
    PyObject_HEAD
    DeadcomL2 dcl;
} py_DeadcomL2_obj;

static PyTypeObject py_DeadcomL2_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "_dcl2.DeadcomL2",
    .tp_doc       = "Deadcom Layer 2",
    .tp_basicsize = sizeof(py_DeadcomL2_obj),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       = PyType_GenericNew
};

static struct PyModuleDef dcl2_moduledef = {
    .m_base    = PyModuleDef_HEAD_INIT,
    .m_name    = "_dcl2",
    .m_doc     = NULL,
    .m_size    = -1,
    .m_methods = NULL
};

PyMODINIT_FUNC PyInit__dcl2(void) {
    PyObject *module;

    if (PyType_Ready(&py_DeadcomL2_type) < 0) {
        return NULL;
    }

    module = PyModule_Create(&dcl2_moduledef);
    if (module == NULL) {
        return NULL;
    }

    Py_INCREF(&py_DeadcomL2_type);
    PyModule_AddObject(module, "DeadcomL2", (PyObject *) &py_DeadcomL2_type);

    return module;
}
