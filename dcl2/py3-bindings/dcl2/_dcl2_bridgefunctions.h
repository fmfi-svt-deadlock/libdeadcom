/**
 * dcl2 library is expecting the user to implement certain facilities it requires, in particular
 * synchronization-related functions and function for transmitting bytes. All of these functions
 * take void* parameter to a context they operate on (mutex context, condvar context and
 * transmission context). These context are provided to dcl2 during object initialization.
 *
 * These bindings use the following strategy: mutex context, condvar context and  transmission
 * context are actualy a single PyObject*, an instance of DeadcomL2 Python object.
 * This object contains python versions of these callbacks and python versions of parameters
 * these python callbacks expect.
 *
 * These functions then cast their context parameter to Dc_Py_ObjDeadcomL2 and invoke the
 * appropriate Python function with correct parameter.
 */

#pragma once
#include <Python.h>

bool Dc_Py_BridgeTransmit(const uint8_t *bytes, size_t size, void *context);

extern DeadcomL2ThreadingMethods Dc_Py_BridgeThreadingMethods;
