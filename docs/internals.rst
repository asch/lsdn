.. _internals:

Internals
---------

If you plan on hacking LSCTL, this chapter is for you. It will describe the
available internal APIs and how they interact.

Project organization (components)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The core of LSDN is the **lsdn** library (``liblsdn.so``), which implements all
of the C API -- the netmodel handling and the individual network types. The
library itself relies on *libmnl* library for netlink communication helpers,
*libjson* for its ability to dump netmodel into JSON and *uthash* for hash
tables.

The command-line tools (:ref:`lsctl <prog_lsctl>` and :ref:`lsctld
<prog_lsctld>`) are built upon our **lsdn-tclext** library, which provides the
lsctl language engine and is layered on the C API. For more info, see
:ref:`internals_cmdline`.

The *lsdn* library itself is composed from several layers/components (see
:numref:`layering` for illustration). At the bottom layer, we have several
mostly independent utility components:
 - ``nettypes.c`` manipulates, parses and prints IP addresses and MAC addresses
 - ``nl.c`` provides functions do to more complex netlink tasks than *libmnl*
   provides - create interfaces, manipulate QDiscs, filters etc.
 - ``names.c`` provides naming tables for netmodel objects, so that we can find
   physes, virts etc. by name
 - ``log.c`` simple logging to stderr governed by the ``LSDN_DEBUG`` environment
   variable
 - ``errors.c`` contains :c:type:`lsdn_err_t` error codes and
   infrastructure for reporting commit problems (which do not use simple
   :c:type:`lsdn_err_t` errors). The problem reporting actual relies on the
   netmodel :c:type:`lsdn_context`.
 - ``list.h`` embedded linked-list implementation (every C project needs its own
   :) )

The **netmodel** core (in ``net.c`` and ``lsdn.c``) is responsible for
maintaining the network model and managing its lifecycle (more info in
:ref:`internals_netmodel`).

For this, it relies on the **rules** (in ``rules.c``) system, which helps you
manage a chain of TC flower filters and their rules. The system also allows the
firewall rules (given by the user) and the routing rules (defined by the virtual network
topology) to share the same flower table. However, the sharing is currently not done,
beacause we instead opted to share the routing table among all virts connect
through the given phys instead. Since firewall rules are per-virt, the can not
live in the shared table.

The *netmodel* core only manages the aspects common to all network types --
lifecycle, firewall rules and QoS, but calls back to a concrete network type
plugin for constructing the virtual network. This is done through the
:c:type:`lsdn_netops` structure and described more thoroughly in
:ref:`internals_netops`.

The currently supported network types are in ``net_direct.c``, ``net_vlan.c``,
``net_vxlan.c`` (all types of VXLANs) and ``net_geneve.c``. Depending on the
type of the network (learning vs static), the network implementations rely on
either ``lbridge.c``, a Linux learning bridge, or ``sbridge.c``, a static bridge
constructed from TC rules. The Linux bridge is pretty self-explanatory, but you
can read out more about the TC rule madness in :ref:`internals_sbridge`.

Finally, *liblsdn* also has support for dumping the netmodel in LSCTL and JSON
formats, to be either used as configuration files or consumed by other
applications (in ``dump.c``).

.. _layering:

.. digraph:: layering
    :caption: Organization

    node [shape=record]
    compound = true

    lsctlc [label = <<i>lsctlc</i> program>]
    lsctld [label = <<i>lsctld</i> program>]
    lsctl [label = <<i>lsctl</i> program>]
    tclext [label = <<i>lsctl-tclext</i> library>]

    subgraph cluster_liblsdn {
        label = <<b>lsdn</b> library>
        color = black

        json_dump [label = "JSON dump"]
        lsctl_dump [label = "lsctl dump"]
        netmodel
        vlan
        vxlan_static [label = "static vxlan"]
        vxlan_e2e [label = "e2e vxlan"]
        vxlan_mcast [label = "mcast vxlan"]
        geneve
        direct
        sbridge
        lbridge
        rules
        subgraph cluster_util {
            label = <utility modules>;
            list
            error
            log
            names
            nl
            nettypes
            list
        }
    }

    lsctl_dump -> json_dump
    json_dump -> netmodel
    lsctld -> tclext
    lsctl -> tclext
    tclext -> netmodel
    netmodel -> {vlan vxlan_static vxlan_e2e vxlan_mcast geneve direct}
    {vlan vxlan_e2e vxlan_mcast} -> lbridge
    {vxlan_static geneve} -> sbridge
    sbridge -> rules
    netmodel -> rules


    # Layout hacks

    # Needed not to render tools parallel with subgraph in parallel
    tclext -> lsctl_dump [style=invis]

    rules -> list [style=invis ltail=cluster_util]

.. _internals_netmodel:

Netmodel implementation
~~~~~~~~~~~~~~~~~~~~~~~

.. _internals_netops:

How to support a new network type
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. _internals_sbridge:

Static bridge (sbridge)
~~~~~~~~~~~~~~~~~~~~~~~

.. _internals_cmdline:

Command-line
~~~~~~~~~~~~

The :ref:`lsctl` are interpreted by the *lsdn-tclext* library.
We have chosen to use the TCL language as a basic for our configuration
language. Althought it might seem as a strange choice, it provides a bigger
flexibility for creating DSLs than let's say JSON or YAML. Basically, TCL
enforces just a single syntactic rule:``{}`` and ``[]`` parentheses.

Originally, we had a YAML configuration parser, but the project has changed its
heading very significantly and the parser was left behind. A TCL bindings were
done as a quick experiment and since have aged quiete well. The YAML parser was
later dropped instead of updating it.

Naturally, there are advantages to JSON/YAML too. Since our language is
turing complete, it is not as easily analyzed by machines. However, it is always
possible to just run the configuraiton scripts and then examine the network
model afterwards. The TCL approach also brings a lot of features for free:
conditional compilation, variables, loops etc.

*lsdn-tclext* library is a collection of TCL commands. One way to use it
is in a custom host program (that is :ref:`lsctl <prog_lsctl>` and  :ref:`lsctld
<prog_lsctld>`). The program will use *libtcl* to create a TCL interpreter and
then call *lsdn-tclext* to register the LSDN specific commands.

:ref:`lsctld <prog_lsctld>` creates the interpreter, registers the LSDN
commands, binds to a Unix domain socket and listens for commands. The commands
(received as plain strings) are fed to the interpreter and *stdout* and *stderr*
is sent back.

:ref:`lsctlc <prog_lsctld>` does not depend on TCL or ``lsdn-tclext``, since it
is a simple netcat-like program that simply pipes its input to the running
``lsctld`` instance and receives script output back.

:ref:`lsctl <prog_lsctl>` is just a few lines, since it uses the ``Tcl_Main``
call. ``Tcl_Main`` is provided by TCL for building a custom TCL interpreter
quickly and does argument parsing and interpreter setup (``tclsh`` is actually
just ``Tcl_Main`` call).

The other way to use *lsdn-tclext* is as a regular TCL extension, from ``tclsh``.
``pkgIndex.tcl`` is provided by LSDN and so LSDN can be loaded using the
``require`` command.

Test environment
~~~~~~~~~~~~~~~~
