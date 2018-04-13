#include "Python.h"
#include "dcl2.h"
#include "_dcl2_bridgefunctions.h"
#include "_dcl2_type.h"


bool pyBridge_mutexInit(void *mutex_p) {
    if (PyErr_Occurred()) {return false;}
    Dc_Py_ObjDeadcomL2 *o = (Dc_Py_ObjDeadcomL2*) mutex_p;
    PyObject *r = PyObject_CallFunctionObjArgs(o->tvmt_mutexInit, o->mutex, NULL);
    if (r == NULL) {
        return false;
    }
    Py_DECREF(r);
    return true;
}

bool pyBridge_mutexLock(void *mutex_p) {
    if (PyErr_Occurred()) {return false;}
    Dc_Py_ObjDeadcomL2 *o = (Dc_Py_ObjDeadcomL2*) mutex_p;
    PyObject *r = PyObject_CallFunctionObjArgs(o->tvmt_mutexLock, o->mutex, NULL);
    if (r == NULL) {
        return false;
    }
    Py_DECREF(r);
    return true;
}

bool pyBridge_mutexUnock(void *mutex_p) {
    if (PyErr_Occurred()) {return false;}
    Dc_Py_ObjDeadcomL2 *o = (Dc_Py_ObjDeadcomL2*) mutex_p;
    PyObject *r = PyObject_CallFunctionObjArgs(o->tvmt_mutexUnlock, o->mutex,  NULL);
    if (r == NULL) {
        return false;
    }
    Py_DECREF(r);
    return true;
}

bool pyBridge_condvarInit(void *condvar_p) {
    if (PyErr_Occurred()) {return false;}
    Dc_Py_ObjDeadcomL2 *o = (Dc_Py_ObjDeadcomL2*) condvar_p;
    PyObject *r = PyObject_CallFunctionObjArgs(o->tvmt_condvarInit, o->condvar, NULL);
    if (r == NULL) {
        return false;
    }
    Py_DECREF(r);
    return true;
}

bool pyBridge_condvarWait(void *condvar_p, uint32_t milliseconds, bool *timed_out) {
    if (PyErr_Occurred()) {return false;}
    Dc_Py_ObjDeadcomL2 *o = (Dc_Py_ObjDeadcomL2*) condvar_p;
    Py_INCREF(o);
    Py_INCREF(o->condvar);
    PyObject *r = PyObject_CallFunction(o->tvmt_condvarWait, "Oi", o->condvar, milliseconds);
    if (r == NULL) {
        return false;
    }
    *timed_out = PyObject_IsTrue(r);
    Py_DECREF(r);
    return true;
}

bool pyBridge_condvarSignal(void *condvar_p) {
    if (PyErr_Occurred()) {return false;}
    Dc_Py_ObjDeadcomL2 *o = (Dc_Py_ObjDeadcomL2*) condvar_p;
    PyObject *r = PyObject_CallFunctionObjArgs(o->tvmt_condvarSignal, o->condvar, NULL);
    if (r == NULL) {
        return false;
    }
    Py_DECREF(r);
    return true;
}

bool Dc_Py_BridgeTransmit(const uint8_t *bytes, size_t size, void *context) {
    if (PyErr_Occurred()) {return false;}
    Dc_Py_ObjDeadcomL2 *o = (Dc_Py_ObjDeadcomL2*) context;
    Py_ssize_t py_size = size;
    PyObject *b = PyBytes_FromStringAndSize((const char *)bytes, py_size);
    if (b == NULL) {
        return false;
    }
    PyObject *r = PyObject_CallFunctionObjArgs(o->transmitFunction, b, NULL);
    if (r == NULL) {
        return false;
    }
    return true;
}

DeadcomL2ThreadingMethods Dc_Py_BridgeThreadingMethods = {
    .mutexInit = pyBridge_mutexInit,
    .mutexLock = pyBridge_mutexLock,
    .mutexUnlock = pyBridge_mutexUnock,
    .condvarInit = pyBridge_condvarInit,
    .condvarWait = pyBridge_condvarWait,
    .condvarSignal = pyBridge_condvarSignal
};
