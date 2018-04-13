#include <Python.h>
#include "_dcl2_type.h"
#include "_dcl2_type_methods.h"
#include "_dcl2_bridgefunctions.h"


#define Dc_Py_Helper_PresetToPyNone(x)  (x) = Py_None; Py_INCREF(Py_None);
#define Dc_Py_Helper_InitAssignArgIfAny(dst, src)  if ((dst)) { \
    PyObject *tmp; \
    tmp = (dst); \
    Py_INCREF((src)); \
    (dst) = (src); \
    Py_XDECREF(tmp); \
}
#define Dc_Py_Helper_RaiseIfNotCallable(obj, arg_no)  if (!PyCallable_Check((obj))) { \
    PyErr_Format(PyExc_TypeError, "Argument number %d must be callable!", (arg_no)); \
    return -1; \
}


static void Dc_Py_DeallocDeadcomL2(Dc_Py_ObjDeadcomL2 *self) {
    Py_XDECREF(self->mutex);
    Py_XDECREF(self->condvar);
    Py_XDECREF(self->transmitFunction);
    Py_XDECREF(self->tvmt_mutexInit);
    Py_XDECREF(self->tvmt_mutexLock);
    Py_XDECREF(self->tvmt_mutexUnlock);
    Py_XDECREF(self->tvmt_condvarInit);
    Py_XDECREF(self->tvmt_condvarWait);
    Py_XDECREF(self->tvmt_condvarSignal);
    Py_TYPE(self)->tp_free((PyObject *) self);
}


static PyObject* Dc_Py_NewDeadcomL2(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    Dc_Py_ObjDeadcomL2 *self;
    self = (Dc_Py_ObjDeadcomL2 *) type->tp_alloc(type, 0);
    if (self != NULL) {
        Dc_Py_Helper_PresetToPyNone(self->mutex);
        Dc_Py_Helper_PresetToPyNone(self->condvar);
        Dc_Py_Helper_PresetToPyNone(self->transmitFunction);
        Dc_Py_Helper_PresetToPyNone(self->tvmt_mutexInit);
        Dc_Py_Helper_PresetToPyNone(self->tvmt_mutexLock);
        Dc_Py_Helper_PresetToPyNone(self->tvmt_mutexUnlock);
        Dc_Py_Helper_PresetToPyNone(self->tvmt_condvarInit);
        Dc_Py_Helper_PresetToPyNone(self->tvmt_condvarWait);
        Dc_Py_Helper_PresetToPyNone(self->tvmt_condvarSignal);
    }
    return (PyObject*) self;
}


static int Dc_Py_InitDeadcomL2(Dc_Py_ObjDeadcomL2 *self, PyObject *args, PyObject *kwds) {
    PyObject *mutex, *condvar, *tvmt_mutexInit, *tvmt_mutexLock, *tvmt_mutexUnlock,
             *tvmt_condvarInit, *tvmt_condvarWait, *tvmt_condvarSignal, *transmitFunction;

    if (!PyArg_ParseTuple(args, "OOOOOOOOO", &mutex, &condvar, &tvmt_mutexInit, &tvmt_mutexLock,
                          &tvmt_mutexUnlock, &tvmt_condvarInit, &tvmt_condvarWait,
                          &tvmt_condvarSignal, &transmitFunction)) {
        return -1;
    }

    Dc_Py_Helper_RaiseIfNotCallable(tvmt_mutexInit, 3);
    Dc_Py_Helper_RaiseIfNotCallable(tvmt_mutexLock, 4);
    Dc_Py_Helper_RaiseIfNotCallable(tvmt_mutexUnlock, 5);
    Dc_Py_Helper_RaiseIfNotCallable(tvmt_condvarInit, 6);
    Dc_Py_Helper_RaiseIfNotCallable(tvmt_condvarWait, 7);
    Dc_Py_Helper_RaiseIfNotCallable(tvmt_condvarSignal, 8);
    Dc_Py_Helper_RaiseIfNotCallable(transmitFunction, 9);

    Dc_Py_Helper_InitAssignArgIfAny(self->mutex, mutex);
    Dc_Py_Helper_InitAssignArgIfAny(self->condvar, condvar);
    Dc_Py_Helper_InitAssignArgIfAny(self->transmitFunction, transmitFunction);
    Dc_Py_Helper_InitAssignArgIfAny(self->tvmt_mutexInit, tvmt_mutexInit);
    Dc_Py_Helper_InitAssignArgIfAny(self->tvmt_mutexLock, tvmt_mutexLock);
    Dc_Py_Helper_InitAssignArgIfAny(self->tvmt_mutexUnlock, tvmt_mutexUnlock);
    Dc_Py_Helper_InitAssignArgIfAny(self->tvmt_condvarInit, tvmt_condvarInit);
    Dc_Py_Helper_InitAssignArgIfAny(self->tvmt_condvarWait, tvmt_condvarWait);
    Dc_Py_Helper_InitAssignArgIfAny(self->tvmt_condvarSignal, tvmt_condvarSignal);

    DeadcomL2Result r = dcInit(&(self->dcl2), self, self, &Dc_Py_BridgeThreadingMethods,
                               Dc_Py_BridgeTransmit, self);
    if (r == DC_FAILURE && PyErr_Occurred()) {
        return -1;
    }
    if (r != DC_OK) {
        PyErr_SetString(PyExc_RuntimeError, "C func dcInit returned unexpected error!");
        return -1;
    }

    return 0;
}


PyTypeObject Dc_Py_TypeDeadcomL2 = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "_dcl2.DeadcomL2",
    .tp_doc       = "Deadcom Layer 2",
    .tp_basicsize = sizeof(Dc_Py_ObjDeadcomL2),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new       = Dc_Py_NewDeadcomL2,
    .tp_init      = (initproc) Dc_Py_InitDeadcomL2,
    .tp_dealloc   = (destructor) Dc_Py_DeallocDeadcomL2,
    .tp_methods   = Dc_Py_MethodsDeadcomL2
};
