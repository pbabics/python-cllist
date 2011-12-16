#include <Python.h>
#include <structmember.h>

/* DLListNode */

staticforward PyTypeObject DLListNodeType;

typedef struct
{
    PyObject_HEAD
    PyObject* value;
    PyObject* prev;
    PyObject* next;
    PyObject* list_weakref;
} DLListNodeObject;

/* Convenience function for creating list nodes.
 * Automatically update pointers in neigbours.
 */
static DLListNodeObject* dllistnode_create(PyObject* prev,
                                           PyObject* next,
                                           PyObject* value,
                                           PyObject* owner_list)
{
    DLListNodeObject *node;

    assert(value != NULL);
    assert(owner_list != NULL);
    assert(owner_list != Py_None);

    node = (DLListNodeObject*)PyObject_CallObject(
        (PyObject*)&DLListNodeType, NULL);

    /* prev is initialized to Py_None by default
     * (by dllistnode_new) */
    if (prev != NULL && prev != Py_None)
    {
        node->prev = prev;
        ((DLListNodeObject*)prev)->next = (PyObject*)node;
    }

    /* next is initialized to Py_None by default
     * (by dllistnode_new) */
    if (next != NULL && next != Py_None)
    {
        node->next = next;
        ((DLListNodeObject*)next)->prev = (PyObject*)node;
    }

    assert(node->value == Py_None);

    Py_INCREF(value);
    node->value = value;

    node->list_weakref = PyWeakref_NewRef(owner_list, NULL);

    return node;
}

/* Convenience function for creating list nodes.
 * Automatically updates pointers in neigbours.
 */
static void dllistnode_delete(DLListNodeObject* node)
{
    if (node->prev != Py_None)
    {
        DLListNodeObject* prev = (DLListNodeObject*)node->prev;
        prev->next = node->next;
    }

    if (node->next != Py_None)
    {
        DLListNodeObject* next = (DLListNodeObject*)node->next;
        next->prev = node->prev;
    }

    Py_DECREF((PyObject*)node);
}

static void dllistnode_dealloc(DLListNodeObject* self)
{
    Py_DECREF(Py_None);
    Py_DECREF(self->value);

    self->ob_type->tp_free((PyObject*)self);
}

static PyObject* dllistnode_new(PyTypeObject* type,
                                PyObject* args,
                                PyObject* kwds)
{
    DLListNodeObject* self;

    self = (DLListNodeObject*)type->tp_alloc(type, 0);
    if (self == NULL)
        return NULL;

    /* A single reference to Py_None is held for the whole
     * lifetime of a node. */
    Py_INCREF(Py_None);

    self->value = Py_None;
    self->prev = Py_None;
    self->next = Py_None;
    self->list_weakref = Py_None;

    return (PyObject*)self;
}

static PyObject* dllistnode_call(PyObject* self,
                                 PyObject* args,
                                 PyObject* kw)
{
    DLListNodeObject* node = (DLListNodeObject*)self;

    Py_INCREF(node->value);
    return node->value;
}

static PyMemberDef DLListNodeMembers[] =
{
    { "value", T_OBJECT_EX, offsetof(DLListNodeObject, value), 0,
      "Value stored in node" },
    { "prev", T_OBJECT_EX, offsetof(DLListNodeObject, prev), READONLY,
      "Previous node" },
    { "next", T_OBJECT_EX, offsetof(DLListNodeObject, next), READONLY,
      "Next node" },
    { NULL },   /* sentinel */
};

static PyTypeObject DLListNodeType =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "llist.DLListNode",             /* tp_name */
    sizeof(DLListNodeObject),       /* tp_basicsize */
    0,                              /* tp_itemsize */
    (destructor)dllistnode_dealloc, /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_compare */
    0,                              /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    dllistnode_call,                /* tp_call */
    0,                              /* tp_str */
    0,                              /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,             /* tp_flags */
    "Doubly linked list node",      /* tp_doc */
    0,                              /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    0,                              /* tp_iter */
    0,                              /* tp_iternext */
    0,                              /* tp_methods */
    DLListNodeMembers,              /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    0,                              /* tp_alloc */
    dllistnode_new,                 /* tp_new */
};


/* DLList */

typedef struct
{
    PyObject_HEAD
    PyObject* first;
    PyObject* last;
    Py_ssize_t size;
} DLListObject;

static DLListNodeObject* dllist_get_node_at(DLListObject* self,
                                            Py_ssize_t index)
{
    long i;
    DLListNodeObject* node;

    if (index >= self->size || index < 0)
    {
        PyErr_SetString(PyExc_IndexError, "No such index");
        return NULL;
    }

    if (index <= self->size / 2)
    {
        node = (DLListNodeObject*)self->first;
        assert((PyObject*)node != Py_None);
        for (i = 0; i < index; ++i)
            node = (DLListNodeObject*)node->next;
    }
    else
    {
        node = (DLListNodeObject*)self->last;
        assert((PyObject*)node != Py_None);
        for (i = self->size - 1; i > index; --i)
            node = (DLListNodeObject*)node->prev;
    }

    return node;
}

static void dllist_dealloc(DLListObject* self)
{
    Py_XDECREF(self->last);
    Py_XDECREF(self->first);

    self->ob_type->tp_free((PyObject*)self);
}

static PyObject* dllist_new(PyTypeObject* type,
                            PyObject* args,
                            PyObject* kwds)
{
    DLListObject* self;

    self = (DLListObject*)type->tp_alloc(type, 0);
    if (self == NULL)
        return NULL;

    /* A single reference to Py_None is held for the whole
     * lifetime of a list. */
    Py_INCREF(Py_None);

    self->first = Py_None;
    self->last = Py_None;
    self->size = 0;

    return (PyObject*)self;
}

static PyObject* dllist_appendleft(DLListObject* self, PyObject* arg)
{
    DLListNodeObject* new_node;

    new_node = dllistnode_create(NULL, self->first, arg, (PyObject*)self);

    self->first = (PyObject*)new_node;

    if (self->last == Py_None)
        self->last = (PyObject*)new_node;

    ++self->size;

    Py_INCREF((PyObject*)new_node);
    return (PyObject*)new_node;
}

static PyObject* dllist_appendright(DLListObject* self, PyObject* arg)
{
    DLListNodeObject* new_node;

    new_node = dllistnode_create(self->last, NULL, arg, (PyObject*)self);

    self->last = (PyObject*)new_node;

    if (self->first == Py_None)
        self->first = (PyObject*)new_node;

    ++self->size;

    Py_INCREF((PyObject*)new_node);
    return (PyObject*)new_node;
}

static PyObject* dllist_insert(DLListObject* self, PyObject* args)
{
    PyObject* val = NULL;
    PyObject* ref_node = NULL;
    DLListNodeObject* new_node;

    if (!PyArg_UnpackTuple(args, "insert", 1, 2, &val, ref_node))
    {
        PyErr_SetString(PyExc_RuntimeError, "Invalid arguments");
        return NULL;
    }

    if (PyObject_TypeCheck(val, &DLListNodeType))
        val = ((DLListNodeObject*)val)->value;

    if (ref_node == NULL || ref_node == Py_None)
    {
        /* append item at the end of the list */
        new_node = dllistnode_create(self->last, NULL, val, (PyObject*)self);

        self->last = (PyObject*)new_node;

        if (self->first == Py_None)
            self->first = (PyObject*)new_node;
    }
    else
    {
        /* insert item before ref_node */
        new_node = dllistnode_create(NULL, ref_node, val, (PyObject*)self);

        if (ref_node = self->first)
            self->first = (PyObject*)new_node;

        if (self->last == Py_None)
            self->last = (PyObject*)new_node;
    }

    ++self->size;

    Py_INCREF((PyObject*)new_node);
    return (PyObject*)new_node;
}

static PyObject* dllist_popleft(DLListObject* self)
{
    DLListNodeObject* del_node;

    if (self->first == Py_None)
    {
        PyErr_SetString(PyExc_RuntimeError, "List is empty");
        return NULL;
    }

    del_node = (DLListNodeObject*)self->first;

    self->first = del_node->next;
    if (self->last == (PyObject*)del_node)
        self->last = Py_None;

    --self->size;

    dllistnode_delete(del_node);

    Py_RETURN_NONE;
}

static PyObject* dllist_popright(DLListObject* self)
{
    DLListNodeObject* del_node;

    if (self->last == Py_None)
    {
        PyErr_SetString(PyExc_RuntimeError, "List is empty");
        return NULL;
    }

    del_node = (DLListNodeObject*)self->last;

    self->last = del_node->prev;
    if (self->first == (PyObject*)del_node)
        self->first = Py_None;

    --self->size;

    dllistnode_delete(del_node);

    Py_RETURN_NONE;
}

static PyObject* dllist_remove(DLListObject* self, PyObject* arg)
{
    DLListNodeObject* del_node;
    PyObject* list_ref;

    if (!PyObject_TypeCheck(arg, &DLListNodeType))
    {
        PyErr_SetString(PyExc_TypeError, "Argument must be a DLListNode");
        return NULL;
    }

    if (self->first == Py_None)
    {
        PyErr_SetString(PyExc_RuntimeError, "List is empty");
        return NULL;
    }

    del_node = (DLListNodeObject*)arg;

    list_ref = PyWeakref_GetObject(del_node->list_weakref);
    if (list_ref != (PyObject*)self)
    {
        PyErr_SetString(PyExc_RuntimeError,
            "DLListNode belongs to another list");
        return NULL;
    }

    if (self->first == arg)
        self->first = del_node->next;
    if (self->last == arg)
        self->last = del_node->prev;

    --self->size;

    dllistnode_delete(del_node);

    Py_RETURN_NONE;
}

static Py_ssize_t dllist_len(PyObject* self)
{
    DLListObject* list = (DLListObject*)self;
    return list->size;
}

static PyObject* dllist_get_item(PyObject* self, Py_ssize_t index)
{
    DLListNodeObject* node;

    node = dllist_get_node_at((DLListObject*)self, index);
    Py_XINCREF(node);

    return (PyObject*)node;
}

static int dllist_set_item(PyObject* self, Py_ssize_t index, PyObject* val)
{
    DLListNodeObject* node;

    if (PyObject_TypeCheck(val, &DLListNodeType))
        val = ((DLListNodeObject*)val)->value;

    node = dllist_get_node_at((DLListObject*)self, index);
    if (node != NULL)
    {
        PyObject* oldval = node->value;

        Py_INCREF(val);
        node->value = val;
        Py_DECREF(oldval);

        return 0;
    }

    return -1;
}

static PyMethodDef DLListMethods[] =
{
    { "appendleft", (PyCFunction)dllist_appendleft, METH_O,
      "Append element at the beginning of the list" },
    { "append", (PyCFunction)dllist_appendright, METH_O,
      "Append element at the end of the list" },
    { "appendright", (PyCFunction)dllist_appendright, METH_O,
      "Append element at the end of the list" },
    { "insert", (PyCFunction)dllist_insert, METH_VARARGS,
      "Inserts element before node" },
    { "popleft", (PyCFunction)dllist_popleft, METH_NOARGS,
      "Remove first element from the list and return it" },
    { "pop", (PyCFunction)dllist_popright, METH_NOARGS,
      "Remove last element from the list and return it" },
    { "popright", (PyCFunction)dllist_popright, METH_NOARGS,
      "Remove last element from the list and return it" },
    { "remove", (PyCFunction)dllist_remove, METH_O,
      "Remove element from the list" },
    { NULL },   /* sentinel */
};

static PyMemberDef DLListMembers[] =
{
    { "first", T_OBJECT_EX, offsetof(DLListObject, first), READONLY,
      "First node" },
    { "last", T_OBJECT_EX, offsetof(DLListObject, last), READONLY,
      "Next node" },
    { NULL },   /* sentinel */
};

static PySequenceMethods DLListSequenceMethods[] =
{
    dllist_len,                 /* sq_length */
    0,                          /* sq_concat */
    0,                          /* sq_repeat */
    dllist_get_item,            /* sq_item */
    0,                          /* sq_slice */
    dllist_set_item,            /* sq_ass_item */
    0                           /* sq_ass_slice */
};

static PyTypeObject DLListType =
{
    PyObject_HEAD_INIT(NULL)
    0,                          /* ob_size */
    "llist.DLList",             /* tp_name */
    sizeof(DLListObject),       /* tp_basicsize */
    0,                          /* tp_itemsize */
    (destructor)dllist_dealloc, /* tp_dealloc */
    0,                          /* tp_print */
    0,                          /* tp_getattr */
    0,                          /* tp_setattr */
    0,                          /* tp_compare */
    0,                          /* tp_repr */
    0,                          /* tp_as_number */
    DLListSequenceMethods,      /* tp_as_sequence */
    0,                          /* tp_as_mapping */
    0,                          /* tp_hash */
    0,                          /* tp_call */
    0,                          /* tp_str */
    0,                          /* tp_getattro */
    0,                          /* tp_setattro */
    0,                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,         /* tp_flags */
    "Doubly linked list",       /* tp_doc */
    0,                          /* tp_traverse */
    0,                          /* tp_clear */
    0,                          /* tp_richcompare */
    0,                          /* tp_weaklistoffset */
    0,                          /* tp_iter */
    0,                          /* tp_iternext */
    DLListMethods,              /* tp_methods */
    DLListMembers,              /* tp_members */
    0,                          /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    0,                          /* tp_init */
    0,                          /* tp_alloc */
    dllist_new,                 /* tp_new */
};

int dllist_init_type()
{
    DLListType.tp_new = PyType_GenericNew;
    DLListNodeType.tp_new = PyType_GenericNew;

    return
        ((PyType_Ready(&DLListType) == 0) &&
         (PyType_Ready(&DLListNodeType) == 0))
        ? 1 : 0;
}

void dllist_register(PyObject* module)
{
    Py_INCREF(&DLListType);
    PyModule_AddObject(module, "DLList", (PyObject*)&DLListType);
}
