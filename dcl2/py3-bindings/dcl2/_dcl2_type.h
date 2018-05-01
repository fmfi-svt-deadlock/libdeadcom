#pragma once
#include <Python.h>
#include "dcl2.h"

// TODO don't forget cyclic garbage collection, if necessary
typedef struct {
    PyObject_HEAD
    DeadcomL2 dcl2;
    PyObject *mutex;
    PyObject *condvar;
    PyObject *transmitFunction;
    PyObject *tvmt_mutexInit;
    PyObject *tvmt_mutexLock;
    PyObject *tvmt_mutexUnlock;
    PyObject *tvmt_condvarInit;
    PyObject *tvmt_condvarWait;
    PyObject *tvmt_condvarSignal;
} Dc_Py_ObjDeadcomL2;

extern PyTypeObject Dc_Py_TypeDeadcomL2;
