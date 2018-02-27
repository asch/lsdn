.. _netmodel:

======================
Network representation
======================

The public API (either :ref:`capi` or :ref:`lsctl`) gives you tools to build a 
model of your virtual networks, which LSDN will then realize on top of the
physical network, using various tunneling technologies. You will need to tell LSDN
both about the virtual networks and the physical network they will be using.

There are three core concepts (objects) LSDN operates with: **virtual
machines**, **physical machines** and **virtual networks**. In the rest of the
guide (and in the source code) we abbreviate them as **virts**, **physes** and
**nets**. If you are wondering if there are any physical networks, then no, LSDN
just expects that the physical machines are connected together when needed and
that is all.

The terminology is derived from the most common use case, but that does not mean
that *virts* really have to be virtual machines and *physes* must really be
physical machines. For example the *virts* could be Linux containers and
*physes* could be virtual machines running those containers.

The *virts*, *physes* and *nets* have the following relationships:

 - *virts* always belong to one *net* (they can not be moved between *nets*)
 - *virts* are connected at one of the *physes* (however, they can be
   reconnected at a different *phys*, in other words, they can **migrate**)
 - *physes* attach to a *net* -- this tells LSDN that the *phys* will have
   *virts* connecting to the network [#fattach]_.

Each of these objects can also have attributes -- for example *physes* can have an
IP address (some network tunneling technologies require this information) and *virts*
can have a MAC address (network tunnels not supporting MAC learning require
this information).

One of the attributes common to all objects is a **name**. A *name* does not
have impact on the functionality of the network, but you can use it to keep track
of the object. If you are using :ref:`lsctl`, it is more or less mandatory,
because it is the only way to refer to an object if you want to change it at a
later point (for example when you want to migrate a *virt*). If you do not
specify a name, one will be generated for you. This ensures that the
:c:func:`the export/dump mechanism <lsdn_dump_context_json>` will always be able
to create cross-references.

Collectively, the model is represented by a LSDN **context**, which contains all
the *physes*, *virts* and *nets*. *Context* is a well known concept in C
libraries, which essentially replaces global variables and ensures that the
library can be safely used by multiple clients in the same process.

.. note::

    LSDN is not thread-safe. It assumes that a given context is never accessed
    concurrently. Different context can be accessed concurrently.

.. rubric:: Footnotes

.. [#fattach] In theory LSDN could figure out if a *phys* should be attached to a
    *net* just by checking if any of its *virts* are attached to that *net*.
    But we have decided to make this explicit. LSDN checks if *physes* connected
    to the same *net* have certain properties (for example their IP address must
    use the same IP version) and we did not want to make these checks implicit.
    A switch may be provided in future versions, though.

.. _net:
.. _vid:

---------------------------
Networks and their settings
---------------------------

**LSCTL:** :lsctl:cmd:`net`, :lsctl:cmd:`settings`

**C API:** :c:func:`lsdn_net_new` and various ``lsdn_settings_*``.

Virtual networks are defined by their *virtual network identifier* (**VID**) and
the settings for the tunneling technology they should use. The *VID* is a numeric
identifier used to separate one virtual network from another and is mapped to VLAN
IDs, VXLAN IDs or similar identifiers. The allowed range of the *VID* is defined
by the used tunneling technology and must be unique among all networks of the
same type [#funique]_.

The used networking overlay technology (and any options related to that, like
VXLAN port) is encapsulated in the **settings** object, which serves as a
template for the new networks (with only the *VID* changing each time). A list
of supported networking technologies is in the chapter :ref:`ovl`, including the
additional options they support.

Like other objects, networks can have a name. However, they do not have any
other attributes, since everything important for their functioning is part of the
*settings*. *Settings* can have names and *lsctl* reserves a the name ``default``
for unnamed settings.

.. rubric:: Footnotes

.. [#funique] In theory, they could overlap if the *nets* are always connected to
    different *physes* (and so there are is no ambiguity), but LSDN still checks
    that they are globally unique.

.. _virt:
.. _attr_mac:

-----
Virts
-----

**LSCTL:** :lsctl:cmd:`virt`

**C API:** :c:func:`lsdn_virt_new`, :c:func:`lsdn_virt_connect`,
:c:func:`lsdn_virt_set_mac`

*virts* are the computers/virtual machines that are going to connect to the
virtual network. From LSDN's standpoint, they are just network interfaces that
exist on a *phys* (usually ``tap`` for a virtual machine or ``veth`` for a
container). LSDN does not care what is on the other end.

When creating a *virt* you have to specify which virtual network it is going to
be part of. This can not be changed later. If you remove the network, all it's
*virts* will be removed as well.

A *virt* also can not be part of multiple virtual networks. The recommended
solution in that case is to simply create one *virt* for each virtual network
you are going to connect to. In this sense *virt* can be described not as a
virtual machine, but as a network interface of a virtual machine.

Once created, you can specify which *phys* this *virt* will connect at and how
is its network interface named on that phys. If you are using LSCTL, just run
:lsctl:cmd:`virt` with a new ``-phys`` argument. In C API use
:c:func:`lsdn_virt_connect`. If the *virt* was already connected, it will be
reconnected (migrated) to the new phys (you want to do this in sync with the
final stage of the migration of the virtual machine itself).

Like other objects, *virts* can have names for your convenience. The names do
not have to be unique globally, but just inside of a single *net*.

Depending on the :ref:`networking technology <ovl>` used, you may also need to
inform LSDN about the virtual machine's MAC address (currently only one MAC
address can be assigned, this may change in future versions). LSDN will use this
MAC address for routing network packets to the machine.

.. _rules:

Firewall rules
--------------
**LSCTL:** :lsctl:cmd:`rule`

**C API:** :c:func:`lsdn_vr_new` and other functions (see :ref:`capi/rules`)

You can filter out specific packets based on their source/destination IP address
range and source/destination MAC address range. The filtering can be done
independently on ingress and egress traffic.

The filtering rules are organized by their priority. All rules inside a given
priority must match against the same target (a target is a masked part of an IP
or MAC address -- for example first octet of the IP address) and must be unique.
This restriction exists to ensure that only deterministic rules can be defined.

Unfortunately, currently there is no way to ``ACCEPT`` packets early, as is
common in e.g. ``iptables``.

.. _rates:

QoS
---

**LSCTL:** :lsctl:cmd:`rate`

**C API:** :c:func:`lsdn_virt_set_rate_in`, :c:func:`lsdn_virt_set_rate_out`

You can limit the amount of traffic going in or out of the *virt* for each
direction. There are three settings:

 - *avg_rate* provides the basic bandwidth limit
 - *burst_size* allows the traffic to overshoot the limit for certain number of
   bytes
 - *burst_rate* (optional) absolute bandwidth limit applied even if traffic is
   allowed to overshoot *avg_rate*

If you do not want to allow any bursting, specify *burst_rate* equal to the
maximum size of a single packet (the MTU). Setting *burst_rate* to zero will not
work.

.. _attr_ip:
.. _phys:

------
Physes
------
**LSCTL:** :lsctl:cmd:`phys`, :lsctl:cmd:`attach`, :lsctl:cmd:`claimLocal`

**C API:** :c:func:`lsdn_phys_new`, :c:func:`lsdn_phys_set_ip`, :c:func:`lsdn_phys_claim_local`

*physes* are used to described the underlying physical machines that will run
your virtual machines.

You will tell LSDN which machine it is currently running on (using
:lsctl:cmd:`claimLocal` or :c:func:`lsdn_phys_claim_local`). LSDN will then make
sure that the *virts* running on this machine are connected to the rest of the
*virts* running on the other machines.

If your machine has multiple separate network interfaces (not bonded), you will
want to create a new *phys* for each network interface on that machine and claim
all such *physes* as local. In this sense, a *phys* is not a physical machine
but a network interface of a physical machine.

This use-case is not meant for a case where both network interfaces are
connected to the same physical network and you just want to choose which one
will be used. LSDN does not support two physes claimed as local connecting to
the same virtual network for technical reasons, so it will not work.

Like other objects, *physes* can have names. They can also have and *ip*
attribute, specifying IP address for the network overlay technologies that
require it.

.. _validation:

----------
Validation
----------
**LSCTL:** :lsctl:cmd:`validate`

**C API:** :c:func:`lsdn_validate`

The validation step in LSDN serves to validate the network model. There are
several reasons why the validation step is present in LSDN. One reason is that
when a network model is being gradually built up using the :ref:`capi` the user
does not have to worry too much about the order in which network objects are
being created as long as the final netmodel is valid. The intermediate steps are
not being checked on-the-fly. For example when creating a virtual machine its
MAC attribute may be specified just before `committing <commit>` the network
model even though for a particular network type this information may be
mandatory (this is specified for each network type in
:ref:`networking technology <ovl>`).

Another advantage of this approach is that when there are problems detected
during the validation phase they will all get reported one by one. LSDN
conveniently provides a :c:func:`lsdn_problem_stderr_handler` function which
will report every detected problem on the standard error output. It is also
possible to invoke the :c:func:`lsdn_validate` step with a different error
handler. This error handler must have the same function signature as
:c:func:`lsdn_problem_stderr_handler`.

This way you can try some network scenario and if the validation reports to you
some problems it has detected in the network model you may fix all these
issues at once and perhaps the next network validation phase will succeed.

Every host participating in a network must share a compatible network
representation. This usually means that all hosts have the same model,
presumably read from a common configuration file or installed through a single
orchestrator. It is then necessary to :lsctl:cmd:`claim` (or
:c:func:`lsdn_phys_claim_local`) a *phys* as local, so that LSDN knows on which
machine it is running. Several restrictions also apply to the creation of
networks in LSDN.

Fixing all the issues present in your network model in the validation step
greatly reduces the risk of creating inconsistent network models in the kernel
and it also alleviates the complexity of the creation of the individual 
network objects in the right order inside the kernel.

The validation phase will ensure the network model does not violate any of the
restrictions listed in `restricts`.

.. _commit:

------
Commit
------
**LSCTL:** :lsctl:cmd:`commit`

**C API:** :c:func:`lsdn_commit`

Commiting a network model means telling LSDN to actually set-up the network
inside Linux kernel.

When we commit a network model the first thing LSDN does it `validates
<validation>` the whole network model. Only if the validation phase succeeds,
the commit phase may proceed. This way the user does not even need to be aware of the
validation phase involved and can only commit the netmodel when appropriate.
This often eliminates the possibility of getting the network in some undesirable
state.

We need to be able to distinguish among network objects already created and
committed in the kernel and network objects newly created, but not yet
committed. LSDN will keep track of the state of each network object. Basically
what we need to do is to remember which objects are already present in the
kernel in their most up-to-date state and which objects have been newly created
or updated since the last time they have been committed (if ever) and which
objects have been deleted. Each attribute you add, remove from or change on a
network object is considered as an update of this object.

If you want to know more about LSDN state management and also to view a diagram
of all states and transitions between these states have a look at the
:ref:`internals_netmodel` section.

It is important to note that any updates exercised on the kernel data structures
representing our network objects are only performed on local objects, where:

 - *phys* is local iff it has been claimed local (either with
   :lsctl:cmd:`claimLocal` or :c:func:`lsdn_phys_claim_local`),
 - *virt* is local iff it is connected at a local *phys*.

However, local objects may sometimes need to be updated as a result of a non
local network object being added, updated or removed. E.g. when a MAC address of
a non local *virt* changes inside a network where this information is mandatory
(such as in `static VXLAN <ovl_vxlan_static>` networks) then local routing
information in the kernel must be updated.

Also, there are transitive dependencies among the network objects. In
particular, when:

 - *virt* is deleted then all its `rules` and `rates` are deleted as well,
 - *net* is deleted then all its *virts* are deleted as well,
 - *phys* is deleted then all *virts* attached to this *phys* are deleted as
   well,
 - *settings* are deleted then all *nets* of this type are deleted as well.

After the initial validation step is completed, LSDN will then proceed with the
actual commit phase which is further subdivided into two subphases:

 - *decommit*
 - *recommit*

In the *decommit* subphase LSDN will consider all the network objects that need
to be either updated or deleted and it will delete both of these objects from
the kernel data structures. However, LSDN will keep track of those objects which
have been initially updated, but not deleted, as they will need to be committed
back again in the next subphase.

The second subphase is the *recommit* phase in which LSDN will iterate over all
local *phys* objects and commit any new or updated *virts* residing on this
*phys*.

You can perhaps think of the whole commit phase as finding the smallest possible
delta between the objects ready to be committed and those already committed. In
the special case of committing for the very first time we can imagine we have
only committed an empty network model (which, by the way, is also possible to
do).

Unfortunately, things can go wrong in the commit phase even when the network
model passes the validation phase. Depending on the phase at which an error
occured we may or may not be able to keep the network model consistent.

If an error occurs in the *recommit* phase, a limited rollback is performed and
the kernel rules remain in mixed state. Some objects may have been successfully
committed, others might still be in the old state because the commit failed. In
such cases the user can retry the commit to install the remaining objects.

If an error occurs in the *decommit* phase, however, there is no safe way to
recover. Given that kernel rules are not installed atomically and there are
usually several rules tied to an object, LSDN can't know what is the installed
state after rule removal fails. In this case the model is considered to be in an
inconsistent state. The only way to proceed is to tear down the whole model and
reconstruct it from scratch.

.. _error_handling:

--------------
Error Handling
--------------
**C API:** :c:func:`lsdn_context_set_nomem_callback`, :c:func:`lsdn_context_abort_on_nomem`, :c:type:`lsdn_err_t`

During construction of the network model there are several things that can go
wrong. LSDN will report these errors to the user of the :ref:`capi`. All the
possible error types are grouped in :c:type:`lsdn_err_t`.

A successful operation will return the :c:member:`LSDNE_OK` error code.

When parsing an IP address of a *phys* or when parsing a MAC attribute of a
*virt* the operation may fail if the provided address is invalid. In that case
LSDN will report this as a :c:member:`LSDNE_PARSE` error.

When assigning a name to a network object (such as *virt*, *phys* or *net*) the
assignment may fail with the :c:member:`LSDNE_DUPLICATE` error code if an object
of the same type with this name already exists.

A :c:member:`LSDNE_NOIF` error code will be returned when querying the
recommended MTU for a *virt* if the given *virt* has no locally assigned
interface (see :c:func:`lsdn_virt_get_recommended_mtu`).

A :c:member:`LSDNE_NETLINK` error code is returned when LSDN is unable to
establish a netlink socket for communicating with the kernel.

:c:member:`LSDNE_VALIDATE` is returned when the network model validation failed.
This can happen while validating the network with :lsctl:cmd:`validate` or
:c:func:`lsdn_validate`. It can also happen when committing the network model
with :lsctl:cmd:`commit` or :c:func:`lsdn_commit`, because the network model is
always validated first. In the latter case of committing the network model, the
current network model will stay in effect.

The :c:member:`LSDNE_COMMIT` error code means a network model commit failed and
a mix of old, new and dysfunctional objects are in effect. You may retry the
commit and see if the error was only temporary.

:c:member:`LSDNE_INCONSISTENT` is more serious than the :c:member:`LSDNE_COMMIT`
failure, since the commit operation can not be successfully retried. The only
operation possible is to rebuild the whole model again.

You may also encounter a :c:member:`LSDNE_NOMEM` error. LSDN deals with
out-of-memory errors in the following fashion: whenever it fails to allocate
dynamic memory it will call a registered callback (if any) that may deal with
this error as it sees fit. The callback is registered with the
:c:func:`lsdn_context_set_nomem_callback` function. It is possible to set a
default handler using  :c:func:`lsdn_context_abort_on_nomem` function provided
by LSDN. This error handler will simply print an error message on the standard
error output and will immediately abort the program should any dynamic memory
allocation fail. Of course, you may register your own out-of-memory callback as
long as the function signature of the callback is that of
:c:func:`lsdn_context_abort_on_nomem`. You can also use the callback to
implement a ``setjmp/longjmp`` error handling scheme.

If no nomem callback is registered (the default), the :c:member:`LSDNE_NOMEM`
error is simply returned to the caller.

---------
Debugging
---------

The LSDN library and the *lsctl* tool both respect the ``LSDN_DEBUG``
environment variable. If you have any problem when committing a model, try
setting ``LSDN_DEBUG=nlerr`` to print extended netlink messages. Alternatively,
you can try ``LSDN_DEBUG=all`` for very verbose output.

``LSDN_DEBUG`` accepts a comma separated list of the following message
categories:

=========== ================================================================
Category    Description
=========== ================================================================
netops      High-level network commit operations (add virt, phys etc.)
rules       Creation and deletion of TC flower rules.
nlerr       Errors returned from kernel (mostly netlink).
all         All of the above
=========== ================================================================

.. _ovl:

--------------------------------
Supported tunneling technologies
--------------------------------

Currently LSDN supports three network tunneling technologies: `ovl_vlan`,
`ovl_vxlan` (in three variants) and `ovl_geneve`. They are all configured the
same in LSDN (only the `settings <net>` differ), but it is important to realize
what technology you are using and what restrictions it has.

Theoretically, you should be able to define your network model once and then
switch the networking technologies as you wish. But in practice some
technologies may need more detailed network models than others. For example,
``ovl_vxlan_mcast`` does not need to known the MAC addresses of the virtual
machines and ``ovl_vlan`` does not need to know the IP addresses of the physical
machines nor the MAC addresses of the virtual machines.

.. index::
    single: VLAN

.. _ovl_vlan:

VLAN
----
**Available as**: :lsctl:cmd:`settings vlan` (lsctl),
:c:func:`lsdn_settings_new_vlan` (C API).

Also known as *802.1Q*, VLAN is a Layer-2 tagging technology, that extends the
Ethernet frame with a 12-bit VLAN tag. LSDN needs no additional information to
setup this type of network, as it relies on the networking equipment along the
way to route packets (typically using MAC learning).

If either the physical network already uses VLAN tagging (the physical computers
are connected to a VLAN segment) or the virtual network will be using tagging,
then the networking equipment along the way must support this. The support is
called *802.1ad* or sometimes *QinQ*.

**Restrictions:**
 - 12 bit `vid <vid>`
 - Physical nodes in the same virtual network must by located on the same
   Ethernet network
 - Care must be taken when nesting

.. index::
    single: VXLAN

.. _ovl_vxlan:

VXLAN
-----

VXLAN is a Layer-3 UDP-based tunneling protocol. It is available in three
variants in LSDN, depending on the routing method used. All of the variants
need the connected participating physical machines to have the
`IP attribute <attr_ip>` set and they must all see each other on the IP network
directly (no NAT).

VXLAN tags have 24 bits (16 million networks). VXLANs by default use UDP port
*4789*, but this is configurable and could in theory be used to expand the
`vid <vid>` space. LSDN currently does not do this.

.. note::

    VXLANs support IPv6 addresses, but they can not be mixed with IPv4. All
    physical nodes must use the same IP version and the version of multicast
    address for `ovl_vxlan_mcast` VXLAN must be the same. This does not prevent
    you from using both IPv6 and IPv4 on the same physical node for other
    purposes than LSDN, you just have to choose one version for the *phys* `IP
    attribute <attr_ip>`.

.. _ovl_vxlan_mcast:

Multicast
~~~~~~~~~
**Available as**: :lsctl:cmd:`settings vxlan/mcast` (lsctl),
:c:func:`lsdn_settings_new_vxlan_mcast` (C API).

This is a self configuring variant of VXLANs. No further information for any
machine needs to be provided, because the VXLAN routes all unknown and broadcast
packets to a designated multicast IP address and the VXLAN iteratively learns
the source IP addresses.  Hence the only additional information is the multicast
group IP address.

**Restrictions**:
 - 24 bit `vid <vid>`
 - Physical nodes in the same virtual network must be reachable on the IP layer
 - UDP and IP header overhead
 - Requires multicast support

.. _ovl_vxlan_e2e:

Endpoint-to-Endpoint
~~~~~~~~~~~~~~~~~~~~
**Available as**: :lsctl:cmd:`settings vxlan/e2e` (lsctl),
:c:func:`lsdn_settings_new_vxlan_e2e` (C API).

Partially self-configuring variant of VXLANs. LSDN must be informed
about the IP address of each physical machine participating in the network using
the `IP attribute <attr_ip>`. All unknown and broadcast packets are sent to all
the physical machines and the VXLAN iteratively learns the IP address - MAC
address mapping.

**Restrictions**:
 - 24 bit `vid <vid>`
 - Physical nodes in the same virtual network must be reachable on the IP layer
 - UDP and IP header overhead
 - Unknown and broadcast packets are duplicated for each physical machine

.. _ovl_vxlan_static:

Fully static
~~~~~~~~~~~~
**Available as**: :lsctl:cmd:`settings vxlan/static` (lsctl),
:c:func:`lsdn_settings_new_vxlan_static` (C API).

VXLAN with fully static packet routing. LSDN must be informed about the
`IP address <attr_ip>` of each physical machine and the `MAC address <attr_mac>`
of each virtual machine participating in the network. LSDN then constructs a
routing table from this information. Broadcast packets are duplicated and sent
to all machines.

**Restrictions**:
 - 24 bit `vid <vid>`
 - Physical nodes in the same virtual network must be reachable on the IP layer
 - UDP and IP header overhead
 - Unknown and broadcast packets are duplicated for each physical machine
 - The virtual network is not fully opaque (MAC addresses of virtual machines
   must be known).


.. index::
    single: Geneve

.. _ovl_geneve:

Geneve
------
**Available as**: :lsctl:cmd:`settings geneve` (lsctl),
:c:func:`lsdn_settings_new_geneve` (C API).

Geneve is a Layer-3 UDP-based tunneling protocol. All participating physical
machines must see each other on the IP network directly (no NAT).

Geneve uses fully static routing. LSDN must be informed about the IP address of
each physical machine (using `IP attribute <attr_ip>`) and
`MAC address <attr_mac>` of each virtual machine participating in the network.

**Restrictions**:
  - 24 bit `vid <vid>`
  - Physical nodes in the same virtual network must be reachable on the IP layer
  - UDP and IP header overhead
  - Unknown and broadcast packets are duplicated for each physical machine
  - The virtual network is not fully opaque (MAC addresses of virtual machines
    must be known).

.. _ovl_direct:

No tunneling
------------
**Available as**: :lsctl:cmd:`settings direct` (lsctl), :c:func:`lsdn_settings_new_direct` (C API).

No separation between the networks. You can use this type of network for
corner cases, like connecting a VM serving as an internet gateway to a dedicated
interface. In this case no separation is needed nor desired.

.. _restricts:

Network Restrictions
--------------------
Certain restrictions apply to the set of possible networks and their
configurations that can be created using LSDN. Anywhere where the keyword
**mandatory** is written in the following list with regards to a network type,
please refer to :ref:`ovl` to see if the rule applies to a given network type:

- You can not assign the same MAC address to two different *virts* that are part
  of the same *net*,
- Any two *nets* of the same network type must not be assigned the same virtual
  network identifier,
- Any two VXLAN networks sharing the same phys, where one network is of type
  :ref:`ovl_vxlan_static` and the other is either of type
  :ref:`ovl_vxlan_e2e` or :ref:`ovl_vxlan_mcast`, must use different UDP ports,
- A *virt* must be explicitly assigned a MAC address when this is **mandatory**
  for a given network type,
- IP address has been specified for a *phys* if it hosts a *net* where this
  information is **mandatory**,
- No duplicate IP addresses were specified for any two *phys*,
- All *phys* attached to the same *net* have the same IP versions of their IP
  addresses,
- While trying to connect a *virt* to a *net* on *phys*, the *phys* is attached
  to *net*,
- Interface specified for *virt* exists,
- No duplicate MAC addresses were specified for any two *virts* connected to the
  same *net* if this attribute is **mandatory** for a given network type,
- Any two *nets* created on the same *phys* have compatible network types,
- The virtual network identifier is within the allowed range for a given
  network type where this is **mandatory**,
- No two *nets* of the same network type have the same virtual network
  identifier,
- No two rules on the same *virt* sharing the same priority have different match
  targets or masks,
- Two rules on the same *virt* sharing the same priority are not equal,
- QoS rates specified for a *virt* are within the allowed range
  (:lsctl:cmd:`rate`).
