.. _capi:

================
C API
================

LSDN is primarily designed as a shared library that can be linked to any
program, e.g., a cloud orchestrator service. This guide should give you the
necessary information for using LSDN as a C library.

--------
Overview
--------

Networks, physes and virts are each represented by a corresponding struct. In
addition, ``settings`` are its own type of object represented by a struct. This
is to allow reusing settings between virtual networks. All these structs are
opaque to users of the library and you have to use function calls to modify
them, set attributes, etc.

Every LSDN object is also directly or indirectly associated with a *context*.
The context basically represents the network model as a whole. It allows you to
commit it to kernel tables, configure common behaviors such as handling
out-of-memory conditions, and simplify memory management.

Your application should always have exactly one context. Through it you can
create networks, physes and virts, like you would with ``lsctl``.

You can modify the network model in memory as much as you like. To apply the
changes and install the network model, you need to commit it to kernel tables.
After a commit, you can continue modifying the model; subsequent commits will
apply the changes.

-----------------
Object life-cycle
-----------------

Objects are created by calling ``lsdn_<type>_new``. This requires an argument
through which the new object is linked to the model: context (for settings
and physes), settings (for networks), or network (for virts).

These functions return a pointer to a newly allocated struct of the appropriate
type. You can use this pointer to set attributes (see below), construct child
objects, perform actions like attaching a virt to a phys, etc.

You can also destroy the object by calling ``lsdn_<type>_free``. This ensures
that the object is deinitialized properly and the network model remains in
a consistent state. All child objects are also destroyed.  Because of this
behavior, you don't need to keep track of all created objects in your program.
Specifically, due to everything being ultimately associated with a context,
``lsdn_context_free`` will safely deallocate all memory.

Note that in the current version, it is impossible to walk the context to find
all child objects. If you want to modify an object later, you need keep track of
its reference -- or find it by name.

----------
Attributes
----------

Many objects have configurable attributes, such as IP addresses, MAC addresses
and similar. For each attribute, you get a collection of functions:

::

  lsdn_<type>_get_<attr>
  lsdn_<type>_set_<attr>
  lsdn_<type>_clear_<attr>

In addition, there is a special attribute, ``name``. You can specify a name for
any object, and then look it up by calling ``lsdn_<type>_by_name``. Names must
be unique for a given type of object.

There is no ``clear`` method for names. However, if you need to do that for
whatever reason, you can set the name to ``NULL``.

------------------------
Network model life-cycle
------------------------

The network model representation in memory consists of a context and various
child objects associated with it. As a whole, it represents virtual network topology
with attached virts and their connections through physes. This is all nice, but
in order for the model to *do* something, it must be converted to TC rules and
installed into kernel tables.

Once a model is constructed, you must mark a phys as local, by calling
:c:func:`lsdn_phys_claim_local`. This sets up the viewpoint for rule generation.
Afterwards, calling :c:func:`lsdn_commit` will walk the model, generate rules and
install them into the kernel. There is no real-time connection between memory
representation of the network model and the kernel rules. All changes to the
model are only reflected in the kernel after a call to :c:func:`lsdn_commit`.

After you're done with your program, you have two choices for deleting the model
from memory. A call to :c:func:`lsdn_context_free` will deallocate the model, but keep
rules in kernel as they are. If you want to remove the rules, call
:c:func:`lsdn_context_cleanup`, which will both delete the model and uninstall the
kernel rules -- as if you deleted all objects manually and then performed a
commit with an empty model.

Commits are not atomic, and ``lsdn_commit`` can fail in two distinct ways. In
the better case, an error prevented installing a particular object, and the
kernel rules are a mix of the old and the new network model. This is represented
by :c:member:`LSDNE_COMMIT` error code. You can retry the commit and LSDN will attempt to
apply the remaining changes.

In the current version, there is no way to examine the network model and
directly find out which changes were applied and which were not. However, the
problem callback supplied to ``lsdn_commit`` will be notified of failed objects.

In the worse case, a rule removal will fail and the kernel rules will remain in
an inconsistent state, not corresponding to a valid network model. This is
indicated by :c:member:`LSDNE_INCONSISTENT` error code. It is impossible to recover from
this condition, you need to call :c:func:`lsdn_context_cleanup` and start over.
TODO už to lsdn_context_cleanup umí?

--------
Sections
--------

Learn more about individual kinds of objects and their functions.

.. toctree::

    capi/context.rst
    capi/phys.rst
    capi/network.rst
    capi/virt.rst
    capi/rules.rst
    capi/errors.rst
    capi/misc.rst
