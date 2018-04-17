/*
 * Note! Documentation for this module is maintained manually in `docs/dcl2/py-api.rst`. Don't
 * forget to update it when changing user-facing API!
 */

#include <Python.h>
#include "_dcl2_type.h"
#include "_dcl2_type_methods.h"


static PyObject* method_connect(Dc_Py_ObjDeadcomL2 *self) {
    PyObject *ret = NULL;

    assert(!PyErr_Occurred());
    assert(self);
    Py_INCREF(self);

    DeadcomL2Result r = dcConnect(&(self->dcl2));
    switch(r) {
        case DC_OK:
            ret = Py_True;
            Py_INCREF(ret);
            break;
        case DC_NOT_CONNECTED:
            ret = Py_False;
            Py_INCREF(ret);
            break;
        case DC_FAILURE:
            if (PyErr_Occurred()) {goto except;}
            // fallthrough intentional
        default:
            PyErr_SetString(PyExc_RuntimeError, "C func dcConnect returned unexpected error!");
            goto except;
    }
    goto finally;
except:
    Py_XDECREF(ret);
    assert(PyErr_Occurred());
    ret = NULL;
finally:
    Py_DECREF(self);
    return ret;
}


static PyObject* method_disconnect(Dc_Py_ObjDeadcomL2 *self) {
    PyObject *ret = NULL;

    assert(!PyErr_Occurred());
    assert(self);
    Py_INCREF(self);

    DeadcomL2Result r = dcDisconnect(&(self->dcl2));
    ret = Py_None;
    Py_INCREF(ret);
    if (r == DC_FAILURE && PyErr_Occurred()) {goto except;}
    if (r != DC_OK) {
        PyErr_SetString(PyExc_RuntimeError, "C func dcDisconnect returned unexpected error!");
        goto except;
    }
    goto finally;
except:
    Py_XDECREF(ret);
    assert(PyErr_Occurred());
    ret = NULL;
finally:
    Py_DECREF(self);
    return ret;
}


static PyObject* method_sendMessage(Dc_Py_ObjDeadcomL2 *self, PyObject *args) {
    PyObject *ret = NULL;
    PyObject *data = NULL;

    assert(!PyErr_Occurred());
    assert(self);
    assert(args);
    Py_INCREF(self);
    Py_INCREF(args);

    if (!PyArg_ParseTuple(args, "S", &data)) {
        goto except;
    }
    Py_INCREF(data);
    Py_ssize_t py_size = PyBytes_Size(data);
    if (py_size < 0) {
        PyErr_SetString(PyExc_ValueError, "size of passed bytes is negative");
        goto except;
    }
    size_t size = py_size;
    uint8_t *data_bytes = (uint8_t *) PyBytes_AsString(data);
    DeadcomL2Result r = dcSendMessage(&(self->dcl2), data_bytes, size);
    switch(r) {
        case DC_OK:
            ret = Py_None;
            Py_INCREF(ret);
            break;
        case DC_NOT_CONNECTED:
            PyErr_SetString(PyExc_BrokenPipeError, "link is not connected");
            goto except;
        case DC_LINK_RESET:
            PyErr_SetString(PyExc_ConnectionError, "other station did not acked, link reset");
            goto except;
        case DC_FAILURE:
            if (PyErr_Occurred()) {goto except;}
            // fallthrough intentional
        default:
            PyErr_SetString(PyExc_RuntimeError, "C func dcSendMessage returned unexpected error!");
            goto except;
    }
    goto finally;
except:
    Py_XDECREF(ret);
    assert(PyErr_Occurred());
    ret = NULL;
finally:
    Py_DECREF(self);
    Py_XDECREF(data);
    Py_DECREF(args);
    return ret;
}


static PyObject* method_getMessage(Dc_Py_ObjDeadcomL2 *self) {
    PyObject *ret = NULL;

    assert(!PyErr_Occurred());
    assert(self);
    Py_INCREF(self);

    size_t msg_len;
    switch (dcGetReceivedMsg(&(self->dcl2), NULL, &msg_len)) {
        case DC_LINK_RESET:
            ret = Py_None;
            Py_INCREF(ret);
            goto finally;
        case DC_FAILURE:
            if (PyErr_Occurred()) {goto except;}
            PyErr_SetString(PyExc_RuntimeError, "C func dcGetReceivedMsg returned unexpected error!");
            goto except;
        default:
            break; //pass
    }
    if (msg_len == 0) {
        ret = Py_None;
        Py_INCREF(ret);
        goto finally;
    }


    PyObject *b;
    {
        // An explicit scope for buf[msg_len] varable, so that we don't jump into it with
        // goto except;
        uint8_t buf[msg_len];
        switch (dcGetReceivedMsg(&(self->dcl2), buf, &msg_len)) {
            case DC_LINK_RESET:
                // There was a message when we first checked, but the link was reset in the meantime
                ret = Py_None;
                Py_INCREF(ret);
                goto finally;
            case DC_OK: {
                b = PyBytes_FromStringAndSize((const char *)buf, (Py_ssize_t) msg_len);
                ret = b;
                Py_INCREF(ret);
                break;
            }
            case DC_FAILURE:
                if (PyErr_Occurred()) {goto except;}
                // fallthrough intentional
            default:
                PyErr_SetString(PyExc_RuntimeError, "C func dcGetReceivedMsg returned unexpected error!");
                goto except;
        }
    }

    goto finally;
except:
    Py_XDECREF(ret);
    assert(PyErr_Occurred());
    ret = NULL;
finally:
    Py_DECREF(self);
    return ret;
}


static PyObject* method_processData(Dc_Py_ObjDeadcomL2 *self, PyObject *args) {
    PyObject *ret = NULL;
    PyObject *data = NULL;

    assert(!PyErr_Occurred());
    assert(self);
    Py_INCREF(self);

    if (!PyArg_ParseTuple(args, "S", &data)) {
        goto except;
    }
    Py_INCREF(data);
    Py_ssize_t py_size = PyBytes_Size(data);
    if (py_size < 0) {
        PyErr_SetString(PyExc_TypeError, "size of passed bytes is negative");
        goto except;
    }
    if (py_size == 0) {
        // dcl2 requires processing of non-zero bytes for processing. Let's make this a no-op
        ret = Py_None;
        Py_INCREF(ret);
        goto finally;
    }
    size_t size = py_size;
    uint8_t *data_bytes = (uint8_t *) PyBytes_AsString(data);
    DeadcomL2Result r = dcProcessData(&(self->dcl2), data_bytes, size);
    if (r == DC_FAILURE && PyErr_Occurred()) {goto except;}
    if (r != DC_OK) {
        PyErr_SetString(PyExc_TypeError, "C func dcProcessData returned unexpected error!");
        goto except;
    }
    ret = Py_None;
    Py_INCREF(ret);
    goto finally;
except:
    Py_XDECREF(ret);
    assert(PyErr_Occurred());
    ret = NULL;
finally:
    Py_DECREF(data);
    Py_DECREF(args);
    Py_DECREF(self);
    return ret;
}


PyMethodDef Dc_Py_MethodsDeadcomL2[] = {
    {
        .ml_name  = "connect",
        .ml_meth  = (PyCFunction) method_connect,
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Try to establish a connection with the other size"
    },
    {
        .ml_name  = "disconnect",
        .ml_meth  = (PyCFunction) method_disconnect,
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Disconnect link"
    },
    {
        .ml_name  = "sendMessage",
        .ml_meth  = (PyCFunction) method_sendMessage,
        .ml_flags = METH_VARARGS,
        .ml_doc   = "Send a message over open link"
    },
    {
        .ml_name  = "getMessage",
        .ml_meth  = (PyCFunction) method_getMessage,
        .ml_flags = METH_NOARGS,
        .ml_doc   = "Get received message"
    },
    {
        .ml_name  = "processData",
        .ml_meth  = (PyCFunction) method_processData,
        .ml_flags = METH_VARARGS,
        .ml_doc   = "Process incoming data"
    },
    {NULL}
};
