#include <Python.h>
#include <structmember.h>

#include "_dcl2_type.h"


static struct PyModuleDef Dc_Py_ModdefDCL2 = {
    .m_base    = PyModuleDef_HEAD_INIT,
    .m_name    = "_dcl2",
    .m_doc     = NULL,
    .m_size    = -1,
    .m_methods = NULL
};

PyMODINIT_FUNC PyInit__dcl2(void) {
    PyObject *module;

    if (PyType_Ready(&Dc_Py_TypeDeadcomL2) < 0) {
        return NULL;
    }

    module = PyModule_Create(&Dc_Py_ModdefDCL2);
    if (module == NULL) {
        return NULL;
    }

    Py_INCREF(&Dc_Py_TypeDeadcomL2);
    PyModule_AddObject(module, "DeadcomL2", (PyObject *) &Dc_Py_TypeDeadcomL2);

    return module;
}
