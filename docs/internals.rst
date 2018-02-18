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
live in the shared table. Another function of this module is that it helps us
overcome the 32 actions limit in the kernel for our broadcast rules.

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
    :caption: Components and dependencies
    :align: center

    node [shape=record]
    compound = true

    lsctlc [label = <<i>lsctlc</i> program>]
    lsctld [label = <<i>lsctld</i> program>]
    lsctl [label = <<i>lsctl</i> program>]
    tclext [label = <<i>lsctl-tclext</i> library>]

    subgraph cluster_liblsdn {
        label = <<i>lsdn</i> library>
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

LSDN does not have an official stable extension API, but the network modules are
intended to be mostly separate from the rest of the code. However, there are
still a few places you will need to touch.

To support a new type of network :

 - add your network to the ``lsdn_nettype`` enum (in ``private/lsdn.h``)
 - add the settings for your network to the ``lsdn_settings`` struct (in
   ``private/lsdn.h``). Place the in the anonymous union, where settings for
   other types are placed.
 - declare a function ``lsdn_settings_new_xxx`` (in ``include/lsdn.h``)
 - create a new file ``net_xxx.c`` for all your code and add it to the
   ``CMakeLists.txt``

The **settings_new** function will inform LSDN how to use your network type.
Do not forget to do the following things in your *settings_new* function:

 - allocate new ``lsdn_settings`` structure via malloc
 - initalize the settings using ``lsdn_settings_init_common`` function
 - fill in the:
    - ``nettype`` (as you have added above)
    - ``switch_type`` (static, partialy static, or learning, purely
      informational, has no effect)
    - ``ops`` (*lsdn_net_ops* will be described shortly)
 - return the new settings

Also note that your function will be part of the C API and should use
``ret_err``  to return error codes (instead of plain ``return``), to provide
correct error handling (see :ref:`capi/errors`).

However, the most important part of the *settings* is the **lsdn_net_ops**
structure -- the callbacks invoked by LSDN to let you construct the network.
First let us get a quick look at the structure definition (full commented
definition is in the source code or Doxygen):

.. doxygenstruct:: lsdn_net_ops
    :project: lsdn-full
    :members:
    :outline:

The first callback that will be called is :c:member:`lsdn_net_ops::create_pa`.
PA is a shorthand for phys attachment and the call means that the physical
machine this LSDN is managing has attached to a virtual network. Typically you
will need to prepare a tunnel(s) connecting to the virtual network and a bridge
connecting the tunnel(s) to the virtual machines (that will be connected later).

If your network does all packet routing by itself, use the ``lbridge.c``
module. It will create an ordinary Linux bridge and allow you to connect your
tunnel interface that bridge. We assume your tunnel has a Linux network interface. 
If not, you will have to come up with some other way of connecting it to the
Linux bridge, or use something else than a Linux bridge. In that case, feel
free not to you ``lbridge.c`` and do custom processing in
:c:member:`lsdn_net_ops::create_pa`.

If the routing in your network is static, use :ref:`internals_sbridge`. It will
allow you to setup a set of flower rules for routing the packets, ending in
custom TC actions. In these actions, you will typically set-up the required
routing metadata for the packet and send it of.

After the PA is created, you will receive other callbacks.

The :c:member:`lsdn_net_ops::add_virt` callback is called when a new virtual
machine has connected on the phys your are managing. Typically, you will add the
virtual machine to the bridge you have created previously.

If your network is learning, you are almost done. But if it is static, you will
want to handle :c:member:`lsdn_net_ops::add_remote_pa` and
:c:member:`lsdn_net_ops::add_remote_virt`. These callbacks inform you about the
other physical machines and virtual machines that have joined the virtual
network. If the routing is static, you need to be informed about them to
correctly set-up the routing information (see :ref:`internals_sbridge`).
Depending on the implementation of the tunnels in Linux, you may also need to
create tunnels for each other remote machine. In that case, the
:c:member:`lsdn_net_ops::add_remote_pa` callback is the right place.

Finally, you need to fill in the :c:member:`lsdn_net_ops::type` with the name of
your network type. This will be used as identifier in the JSON dumps. At this
point you might want to decide if your network should be supported in
:ref:`lsctl` and modify ``lsext.c`` accordingly. The network type names in LSCTL
and JSON should match.

The other callbacks are mandatory. Naturally, you will want to implement the
``remove``/``destroy`` callbacks for all your ``add``/``create`` callbacks. There
are also validation callbacks, that allow you to reject invalid network
configuration early (see c:ref:`validation`). Finally, LSDN can check the
uniqueness of the listening IP address/port combiations your tunnels use, if you
provide them using :c:member:`lsdn_net_ops::get_ip` and
:c:member:`lsdn_net_ops::get_port`.


Since example is the best explanation, we encourage you to look at some of the
existing plugins -- *VLAN* (``net_vlan.c``) for learning networks, *Geneve*
(``net_geneve.c``) for static networks.

.. _internals_sbridge:

Static bridge
~~~~~~~~~~~~~

The static-bridge subsystem provides helper functions to help you manage an L2
router built on TC flower rules and actions. The TC implementation means
that it can be integrated with the metadata based Linux tunnels.

Metadata-based tunnels (or sometimes called lightweight IP tunnels) are Linux
tunnels that can choose their tunnel endpoint by looking at a special packet
metadata. This means you do not need to create new network interface for each
endpoint you wan to communicate with, but one shared interface can be used, with
only the metadata changing. In our case, we use TC actions to set these
metadata depending on the destination MAC address (since we now where a virtual
machine with that MAC lives). The setup is illustrated in :numref:`sbridge_fig`.

.. _sbridge_fig:

.. graph:: sbridge
    :caption: Two virtual networks using a static routing (using TC) and shared
              metadata tunnel. Lines illustrate connection of each VM.
    :align: center

    {VM1 VM2} -- sbridge1
    {VM3 VM4} -- sbridge2
    {sbridge1 sbridge2} -- sbridge_phys_if
    {sbridge1 sbridge2} -- sbridge_phys_if
    sbridge_phys_if -- phys_if
    sbridge_phys_if -- phys_if
    sbridge_phys_if -- phys_if
    sbridge_phys_if -- phys_if

    sbridge1 [label=<TC bridge for virtual network 1>]
    sbridge2 [label=<TC bridge for virtual network 2>]
    sbridge_phys_if [label=<Metadata tunnel>]
    phys_if [label=<Physical network interface>]

The static bridge is not a simple implementation of Linux bridge in TC. A bridge
is a virtual interfaces with multiple enslaved interfaces connected to it.
However, the static bridge needs to deal with the tunnel metadata during its
routing. For that, it provides the following C structures.

Struct **lsdn_sbridge** represents the bridge as a whole. Internally, it will
create a helper interface to hold the routing rules.

Struct **lsdn_sbridge_phys_if** represents a Linux network interface connected
to the bridge. This will typically be a virtual machine interface or a tunnel.
Unlike classic bridge, a single interface may be connected to multiple bridges.

Struct **lsdn_sbridge_if** represents the connection of *sbridge_phys_if* to the
bridge. For virtual machines *sbridge_if* and *sbridge_phys_if* will correspond
1:1, since virtual machine can not be connected to multiple bridges. If a
sbridge is shared, you have to provide a criteria spliting up the traffic,
usually by the :ref:`vid`.

Struct **lsdn_sbridge_route** represents a route through given *sbridge_if*. For
a virtual machine, there will be just a single route, but metadata tunnel
interfaces can provide multiple routes, each leading to a different physical
machine. The users of the static-bridge module must provide TC actions to set
the correct metadata for that route.

Struct **lsdn_sbridge_mac** tells to use a given route when sending packets to a
given MAC address. There will be a *sbridge_mac* for each VM on a physical
machine where the route leads.

The structures above need to be created from LSDN callbacks. For a network with
static routing, and metadata tunnels, the correspondence will loook similar to
this:

 ================================================================= ==================================================
 callback                                                          sbridge
 ================================================================= ==================================================
 :c:member:`create_pa <lsdn_net_ops::create_pa>` (first call)      create **phys_if** for tunnel
 :c:member:`create_pa <lsdn_net_ops::create_pa>`                   create **sbridge** and **sbridge_if** for tunnel
 :c:member:`add_virt <lsdn_net_ops::add_virt>`                     create **if**, **route** and **mac**
 :c:member:`add_remote_pa <lsdn_net_ops::add_remote_pa>`           create **route** for the physical machine
 :c:member:`add_remote_virt <lsdn_net_ops::add_remote_virt>`       create **mac** for the route
 ================================================================= ==================================================


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

.. _test_harness:

Test environment
~~~~~~~~~~~~~~~~

.. todo:: describe the test environment
