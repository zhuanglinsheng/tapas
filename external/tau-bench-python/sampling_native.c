/* Weighted product draws, preserving Python Random.choices' RNG stream.
 * No fast math, no independent PRNG, no mutation of candidate values. */
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <math.h>

typedef struct {
    Py_ssize_t dimensions;
    PyObject *specs;
} Draw;
static const char *capsule_name = "sampling.product.v1";

static void destroy_draw(PyObject *capsule) {
    Draw *draw = PyCapsule_GetPointer(capsule, capsule_name);
    if (draw) { Py_DECREF(draw->specs); PyMem_Free(draw); }
}

static PyObject *compile_draw(PyObject *self, PyObject *specs) {
    if (!PyTuple_Check(specs)) {
        PyErr_SetString(PyExc_TypeError, "Expected tuple of immutable draw specifications"); return NULL;
    }
    for (Py_ssize_t i=0; i<PyTuple_GET_SIZE(specs); i++) {
        PyObject *s=PyTuple_GET_ITEM(specs,i);
        if (!PyTuple_Check(s) || PyTuple_GET_SIZE(s)!=2) goto invalid;
        PyObject *d=PyTuple_GET_ITEM(s,0), *c=PyTuple_GET_ITEM(s,1);
        if (!PyTuple_Check(d) || !PyTuple_Check(c) || !PyTuple_GET_SIZE(d)
            || PyTuple_GET_SIZE(d)!=PyTuple_GET_SIZE(c)) goto invalid;
        double previous=0.0;
        for (Py_ssize_t j=0; j<PyTuple_GET_SIZE(c); j++) {
            PyObject *v=PyTuple_GET_ITEM(c,j);
            if (!PyFloat_CheckExact(v)) goto invalid;
            double value=PyFloat_AS_DOUBLE(v);
            if (!isfinite(value) || value<previous) goto invalid;
            previous=value;
        }
        if (previous<=0.0) goto invalid;
    }
    Draw *draw=PyMem_Malloc(sizeof(Draw));
    if (!draw) return PyErr_NoMemory();
    draw->dimensions=PyTuple_GET_SIZE(specs);draw->specs=Py_NewRef(specs);
    PyObject *capsule=PyCapsule_New(draw,capsule_name,destroy_draw);
    if (!capsule) { Py_DECREF(specs);PyMem_Free(draw); }
    return capsule;
invalid:
    PyErr_SetString(PyExc_ValueError,"Invalid draw specification");return NULL;
}

static PyObject *sample_draw(PyObject *self, PyObject *args) {
    PyObject *capsule,*random;
    if (!PyArg_ParseTuple(args,"OO",&capsule,&random)) return NULL;
    Draw *draw=PyCapsule_GetPointer(capsule,capsule_name);
    if (!draw) return NULL;
    PyObject *result=PyTuple_New(draw->dimensions);
    if (!result) return NULL;
    for (Py_ssize_t i=0; i<draw->dimensions; i++) {
        PyObject *s=PyTuple_GET_ITEM(draw->specs,i);
        PyObject *d=PyTuple_GET_ITEM(s,0),*c=PyTuple_GET_ITEM(s,1);
        PyObject *r=PyObject_CallNoArgs(random);
        if (!r) {Py_DECREF(result);return NULL;}
        double u=PyFloat_AsDouble(r);Py_DECREF(r);
        if (PyErr_Occurred()) {Py_DECREF(result);return NULL;}
        if (!isfinite(u) || u<0.0 || u>1.0) {
            Py_DECREF(result);PyErr_SetString(PyExc_ValueError,"Random value outside [0,1]");return NULL;
        }
        Py_ssize_t lo=0,hi=PyTuple_GET_SIZE(d)-1;
        double target=u*PyFloat_AS_DOUBLE(PyTuple_GET_ITEM(c,hi));
        while (lo<hi) {
            Py_ssize_t mid=lo+(hi-lo)/2;
            if (target<PyFloat_AS_DOUBLE(PyTuple_GET_ITEM(c,mid))) hi=mid;
            else lo=mid+1;
        }
        PyTuple_SET_ITEM(result,i,Py_NewRef(PyTuple_GET_ITEM(d,lo)));
    }
    return result;
}
static PyMethodDef methods[]={
    {"compile_draw",compile_draw,METH_O,"Validate immutable sampling tables once."},
    {"draw",sample_draw,METH_VARARGS,"Draw with exactly one Python RNG call per coordinate."},
    {NULL,NULL,0,NULL}
};
static struct PyModuleDef module={PyModuleDef_HEAD_INIT,"_sampling_native",NULL,-1,methods};
PyMODINIT_FUNC PyInit__sampling_native(void) { return PyModule_Create(&module); }
