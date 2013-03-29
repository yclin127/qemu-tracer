#include "tracer/trace_file.h"

#include "tracer/config.h"

#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE
#include "Python.h"

int cache_line_bits = 6;
int cache_set_bits  = 12;
int cache_way_count = 8;

PyObject* py_trace_file_module = NULL;
PyObject* py_trace_file_init = NULL;
PyObject* py_trace_file_begin = NULL;
PyObject* py_trace_file_end = NULL;
PyObject* py_trace_file_log = NULL;

void trace_file_init(void)
{
    atexit(PyErr_Print);
    
    Py_Initialize();
    
    /* setup python argv */
    const char *argv[] = {"tracer_file"};
    PySys_SetArgv(1, (char **)argv);
    
    /* setup python path */
    PyObject *path = PySys_GetObject((char *)"path"); 
    if (path == NULL) exit(0);
    PyList_Append(path, PyString_FromString("tracer"));

    py_trace_file_module = PyImport_Import(PyString_FromString("trace_file"));
    if (!py_trace_file_module) exit(0);
    
    py_trace_file_init = PyObject_GetAttrString(py_trace_file_module, "trace_file_init");
    if (!(py_trace_file_init && PyCallable_Check(py_trace_file_init))) exit(0);
    
    py_trace_file_begin = PyObject_GetAttrString(py_trace_file_module, "trace_file_begin");
    if (!(py_trace_file_begin && PyCallable_Check(py_trace_file_begin))) exit(0);
    
    py_trace_file_end = PyObject_GetAttrString(py_trace_file_module, "trace_file_end");
    if (!(py_trace_file_end && PyCallable_Check(py_trace_file_end))) exit(0);
    
    py_trace_file_log = PyObject_GetAttrString(py_trace_file_module, "trace_file_log");
    if (!(py_trace_file_log && PyCallable_Check(py_trace_file_log))) exit(0);
    
#ifdef CONFIG_CACHE_FILTER
    PyObject* value;
    
    value = PyObject_GetAttrString(py_trace_file_module, "CACHE_LINE_BITS");
    if (!value) exit(0);
    cache_line_bits = PyLong_AsLong(value);
    Py_DECREF(value);
    
    value = PyObject_GetAttrString(py_trace_file_module, "CACHE_SET_BITS");
    if (!value) exit(0);
    cache_set_bits = PyLong_AsLong(value);
    Py_DECREF(value);
    
    value = PyObject_GetAttrString(py_trace_file_module, "CACHE_WAY_COUNT");
    if (!value) exit(0);
    cache_way_count = PyLong_AsLong(value);
    Py_DECREF(value);
#endif
    
    value = PyObject_CallObject(py_trace_file_init, NULL);
    if (value == NULL) exit(0);
    Py_DECREF(value);
}

void trace_file_begin(void)
{
    PyObject* value = PyObject_CallObject(py_trace_file_begin, NULL);
    if (value == NULL) exit(0);
    Py_DECREF(value);
}

void trace_file_end(void)
{
    PyObject* value = PyObject_CallObject(py_trace_file_end, NULL);
    if (value == NULL) exit(0);
    Py_DECREF(value);
}

void trace_file_log(target_ulong vaddr, target_ulong paddr, uint32_t flags, uint64_t icount)
{
    PyObject* arguments = PyTuple_Pack(4,
        PyInt_FromLong(vaddr),
        PyInt_FromLong(paddr),
        PyInt_FromLong(flags),
        PyInt_FromLong(icount)
    );
    PyObject* value = PyObject_CallObject(py_trace_file_log, arguments);
    if (value == NULL) exit(0);
    Py_DECREF(value);
    Py_DECREF(arguments);
}
