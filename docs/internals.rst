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
because we instead opted to share the routing table among all virts connect
through the given phys instead. Since firewall rules are per-virt, the can not
live in the shared table. Another function of this module is that it helps us
overcome the 32 actions limit in the kernel for our broadcast rules.

The *netmodel* core only manages the aspects common to all network types --
life cycle, firewall rules and QoS, but calls back to a concrete network type
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

The network model (in ``lsdn.c``) provides functions that are not specific to
any network type. This includes QoS, firewall rules and basic validation.

Importantly, it also provides the state management needed for implementing the
commit functionality, which is important for the overall ease-of-use of the C
API. The network model layer must keep track of both the current state of the
network model and what is committed. Also it tracks which objects have changed
attributes and need to be update. Finally, it keeps objects that were deleted y
the user, but are still committed alive.

For this, it is important to understand a life-cycle of an object, illustrated
in :numref:`netmodel_states`.

.. _netmodel_states:

.. digraph:: states
    :caption: Object states. Blue lines denote update (attribute change, free),
              green lines commit, orange lines errors during commit, red lines 
              errors where recovery has failed.

    T [shape = point ];
    NEW; RENEW; DELETE; OK; free
    T -> NEW [color = "blue"];
    NEW -> NEW [label = "update", color = "blue"];
    NEW -> free [label = "free", color = "blue"];
    NEW -> OK [label = "commit", color = "green"];
    NEW -> NEW [label = "c. error", color = "orange" ];
    NEW -> FAIL [label = "c. fail", color = "red"];
    OK -> RENEW [label = "update", color = "blue"];
    OK -> DELETE [label = "free", color = "blue"];
    OK -> OK [label = "commit", color = "green"];
    DELETE -> free [label = "commit", color = "green"];
    DELETE -> free [label = "c. fail", color = "red"];
    DELETE -> free [label = "c. error", color = "orange"];
    RENEW -> RENEW [label = "update", color = "blue"];
    RENEW -> DELETE [label = "free", color = "blue"];
    RENEW -> NEW [label = "c. error", color = "orange"];
    RENEW -> FAIL [label = "c. fail", color = "red"];
    RENEW -> OK [label = "commit", color = "green"];
    FAIL -> free [label = "free", color = "blue" ];
    FAIL -> FAIL [label = "update", color = "blue" ];
    FAIL -> FAIL [label = "c. fail", color = "red" ];

The objects alway start in the **NEW** state, indicating that they will be
actually created with the nearest commit.  If they are freed, the `free` call is
actually done immediately. Any update leaves them in the *NEW* state, since
there is nothing to update yet.

Once a *NEW* object is successfully committed, it moves to the **OK** state. A
commit has no effect on such object, since it is up-to-date.

If a *NEW* object is freed, it is moved to the **DELETE** state, but its memory
is retained, until commit is called and the object is deleted from kernel. The
objects in *DELETE* state can not be updated, since they are no longer visible
and should not be used by the user of the API. They also can not be found by
their name.

If a *NEW* object is updated, it is moved to the **RENEW** state. This means
that on the next update, it is removed from the kernel, moved to *NEW* state,
and in the same commit added back to the kernel and moved back to the *OK*
state. Updating the *RENEW* object again does nothing and freeing it moves it to
the *DELETE* state, since that takes precedence.

If a commit for some reason fails, LSDN tries to unroll all operations for that
object and returns the object temporarily to the *ERR* state. After the commit
has ended, it moves all objects from *ERR* state to the *NEW* state.  This means
that on the next commit, the operations will be retried again, unless the user
decides to delete the object.

If even the unrolling fails, the object is moved to the **FAIL** state. The only
possibility for the user is to release its memory. If the object was originally
already deleted, it bypasses the *FAIL* state.

.. note::

    If validation fails, commit is not performed at all and object states
    do not change at all.

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
 - initialize the settings using ``lsdn_settings_init_common`` function
 - fill in the:
    - ``nettype`` (as you have added above)
    - ``switch_type`` (static, partially static, or learning, purely
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
uniqueness of the listening IP address/port combinations your tunnels use, if you
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
sbridge is shared, you have to provide a criteria splitting up the traffic,
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
static routing, and metadata tunnels, the correspondence will look similar to
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
language. Although it might seem as a strange choice, it provides a bigger
flexibility for creating DSLs than let's say JSON or YAML. Basically, TCL
enforces just a single syntactic rule:``{}`` and ``[]`` parentheses.

Originally, we had a YAML configuration parser, but the project has changed its
heading very significantly and the parser was left behind. A TCL bindings were
done as a quick experiment and since have aged quiete well. The YAML parser was
later dropped instead of updating it.

Naturally, there are advantages to JSON/YAML too. Since our language is
Turing complete, it is not as easily analyzed by machines. However, it is always
possible to just run the configuration scripts and then examine the network
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

Test Environment
~~~~~~~~~~~~~~~~

Our test environment is highly modular, extremely powerful, easy to use and
without any complex dependencies. Thus it is easily extensible even for
outsiders and people beginning with the project. 

CTest
.....

The core of the environment is ``CTest`` testing tool from ``CMake``. It
provides very nice way how to define all the tests in the modular way. We create
test parts which can be combined together for one complex test. This means that
you can for example say that you want to use ``geneve`` as a backend for the
network, you want to test ``migrate`` which means that the migration of virtual
machines will be tested and as a verifier use ``ping``. ``CTest`` configuration
file is called ``CMakeLists.txt`` and tests composed from parts can be added
with ``test_parts(...)`` command. Examples follow, starting with example
described above: ::

	test_parts(geneve migrate ping)

For ``vlan`` and ``dhcp`` test: ::

	test_parts(vlan dhcp)

For backend without tunnelling, migration with daemon's help keeping the state
in memory and ping: ::

	test_parts(direct migrate-daemon ping)

For complete list of all tests see ``CMakeLists.txt`` in the ``test`` directory
and all parts usable to create complex test are in ``test/parts``. To run all
the tests inside the ``CTest`` testing tool just go to ``test`` folder and run ::

	ctest

Parts
.....

In the previous section we described the big picture of tests execution. Now we
will describe what *part* is and how to define it. *Part* is a simple bash
script defining functions according to prescribed API for our test environment.

Function ``prepare()`` is used for establishing the physical network environment
unrelated to the virtual network we would like to manage. These are "wires" we
will use for our virtual networking.

``connect()`` is the main phase for setting the virtual network environment.
LSDN is usually used in this function for configuring all the virtual interfaces
and virtual network appliances.

For testing if the applied configuration is working, e.g. has the expected
behaviour, function ``test()`` is used. Most often ``ping`` is used here, but
you can use anything for testing the functionality.

If you want to do some special cleanup you can use ``cleanup()`` function.

Back to *part* primitive - you can combine various parts together but every
rational test should define all the described functions no matter how many parts
are used.

``CTest`` is pretty good at automated execution of complete tests but if you
want to debug the test or execute just part of it there is a ``run`` script.
This script allows you to execute just selected stages and combine parts in a
comfortable way. It's usage is self-explanatory: ::

	Usage:
		./run -xpctz [parts]
	  -x  trace all commands
	  -p  run the prepare stage
	  -c  run the connect stage
	  -t  run the test stage
	  -z  run the cleanup stage

Thus for running test for the example from the beginning but use just
``connect`` and ``prepare`` stage you can call: ::

	./run -pc geneve migrate ping

QEMU
....

Because we are dependent on fairly new version of Linux Kernel we provide
scripts for executing tests in virtualized environment. This is useful when you
use some traditional Linux distribution like Ubuntu with older kernel and you do
not want to compile or install custom recent kernel.

As a hypervizor we use QEMU with Arch Linux userspace. Here are several steps
you need to follow for execution in QEMU:

    1. Download actual Linux Kernel to ``$linux-path``.
    2. Run ``./create_kernel.sh $linux-path``. This will generate valid kernel
       with our custom ``.config`` file.
    3. Run ``./create_rootfs.sh`` which will create the userspace for virtual
	   machine with all dependencies. Here you need ``pacman`` for downloading
	   all the packages.
    4. Run ``./run-qemu $kernel-path $userspace-path all`` which will execute
       all tests and shut down.

``run-qemu`` script is much more powerful and you can run all the examples
described above together with debugging in the shell inside that virtual
machine. The usage is following: ::

	usage: run-qemu [--help] [--kvm] [--gdb] kernel rootfs guest-command

	Available guest commands: shell, raw-shell, all.

``shell`` will execute just a shell and leave the test execution up to you and
``raw-shell`` is just for debugging the virtual machine userspace because it
will not mount needed directories for tests. ``all`` executes all the tests as
we have already shown above.
