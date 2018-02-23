.. _netmodel:

======================
Network representation
======================

The public API (either :ref:`capi` or :ref:`lsctl`) gives you tools to build a a
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
physical machines. Maybe the *virts* are Linux containers and *physes* are
virtual machines running those containers?

The *virts*, *physes* and *nets* have the following relationships:
 - *virts* always belong to one *net* (they can not be moved)
 - *virts* are connected at one of the *physes* (they can connect at different
   phys, in other words, they can **migrate**)
 - *physes* attach to a *net* -- this tells LSDN that the *phys* will have virts
   connecting to the network [#fattach]_.

Each of the object can also have attributes -- for example *physes* can have an
IP address (some network tunneling technologies require this information) and *virts*
can have a MAC address (network tunnels not supporting MAC learning require
this information).

One of the attributes common to all objects is a **name**. A *name* does not
have impact on the functioning of the network, but you can use it to keep track
of the object. If you are using :ref:`lsctl`, it is more or less mandatory,
because it is the only way to refer to an object if you want to change it at a
later point (for example when you want to migrate a *virt*).

Collectively, the model is represented by a LSDN **context**, which contains all
the *physes*, *virts* and *nets*. *Context* is a well known concept in C
libraries, which essentially replaces global variables and ensures that the
library can be safely used by multiple clients in the same process.

.. note::

    LSDN is not thread-safe. It assumes that a given context is never accessed
    concurrently. Different context may be accessed concurrently.

.. rubric:: Footnotes

.. [#fattach] In theory LSDN could figure out if a *phys* should be attached to a
    *net* just be looking at if any of its *virts* are attached to that *net*.
    But we have decided to make this explicit. LSDN checks if *physes* connected
    to the same *net* have certain properties (for example the same IP version)
    and we did not want to make these checks implicit. A switch may be provided
    in future versions, though.

.. _net:
.. _vid:

---------------------------
Networks and their settings
---------------------------

**LSCTL:** :lsctl:cmd:`net`, :lsctl:cmd:`settings`

**C API:** :c:func:`lsdn_net_new` and various ``lsdn_settings_*``.

Virtual networks are defined by their *virtual network identifier* (**VID**) and
the settings for the tunneling technology they should use. The *VID* is a numeric
identifier used to separate one virtual network from other and is mapped to VLAN
IDs, VXLAN IDs or similar identifiers. The allowed range of the *VID* is defined
by the used tunneling technology and the must be unique among all networks
[#funique]_.

The used networking overlay technology (and any options related to that, like
VXLAN port) is encapsulated in the **settings** object, which serves as a template
for the new networks (with only the *VID* changing each time). If you remove the
template, the networks will be removed to. A list of supported networking
technologies is in the chapter :ref:`ovl`, including the additional options they
support.

Like other objects, networks can have a name. However, they do not have any
other attributes, since everything important to their functioning is part of the
*settings*. *Settings* can have names to and *lsctl* reserves a name ``default``
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
virtual network. From LSDN standpoint, they are just a network interfaces that
exists on a *phys* (usually ``tap`` for a virtual machine or ``veth`` for a
container). LSDN does not care what is on the other end.

When creating a *virt* you have to specify, which virtual network it is going to
be part of. This can not be changed later. If you remove the network, all it's
*virts* will be also removed.

A *virt* also can not be part of multiple virtual networks. The intended
solution is to simply create one *virt* for each virtual network you are going
to connect to. LSDN does not need to know, they are connected to the same
virtual machine/container on the other end. In this sense  *virt* can be
described not as a virtual machine, but as a network interface of a virtual machine.

Once created, you can specify which *phys* this *virt* will connect at and how
is its network interface named on that phys. If you are using LSCTL, just run
:lsctl:cmd:`virt` with a new ``-phys`` argument. In C API use
:c:func:`lsdn_virt_connect`. If the *virt* was already connected, it will be
reconnected (migrated) to the new phys (you want to do this in sync with the
final stage of the migration of the virtual machine itself).

Like other objects, *virts* can have names for your convenience. The names do
not have to be unique globally, but just inside of a single *net*.

Depending on the :ref:`networking technology <ovl>` used, you may need to inform
LSDN about the virtual machine's MAC address (currently only one MAC address can
be given). LSDN will use this MAC address for routing the packets to the
machine.

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

.. _qos:

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

**C API:** :c:func:`lsdn_phys_new`, :c:func:`lsdn_phys_set_ip`,
    :c:func:`lsdn_phys_claim_local`

*physes* are used to described the underlying physical machines that will run
your virtual machines.

You will tell LSDN which machine it is currently running on (using
:lsctl:cmd:`claimLocal` or :c:func:`lsdn_phys_claim_local`). LSDN will then make
sure that the *virts* running on this machine are connected to the rest of the
*virts* running on the other machines.

If you your machine has multiple separate network interfaces (not bonded), you
will want to create a new *phys* for each network interface on that machine and
claim all such *physes* as local. In this sense, a *phys* is not a physical
machine but a network interfaces of a physical machine.

This use-case is not meant for a case where both network interfaces are
connected to the same physical network and you just want to choose where data
will flow. LSDN does not support two physes claimed as local connecting to the
same virtual network, for technical reasons, so it will not work.

Like other objects, *physes* can have names. They can also have and *ip*
attribute, specifying IP address for the network overlay technologies that
require it.

.. _validation:

-------------------------------------
Validation, Commit and Error Handling
-------------------------------------

Every host participating in a network must share a compatible network
representation. This usually means that all hosts have the same model,
presumably read from a common configuration file or installed through a single
orchestrator. It is then necessary to claim a *phys* as local, so that LSDN
knows on which machines it is running. Several restrictions also apply
to the creation of networks in LSDN. For details refer to section `restricts`.

---------
Debugging
---------

.. _ovl:

--------------------------------
Supported tunneling technologies
--------------------------------

Currently LSDN supports three network tunneling technologies: `ovl_vlan`,
`ovl_vxlan` (in three variants) and `ovl_geneve`. They all use the same basic
networking model in LSDN, but it is important to realize what technology you are
using and what restrictions it has.

Theoretically, you should be able to define your network model once and then
switch the networking technologies as you wish. But in practice some
technologies may need more detailed network model than others. For example,
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
 - 12 bit `vid`
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
need the connected participating physical machines to have the `attr_ip` set
and they must all see each other on the IP network directly (no NAT).

VXLAN tags have 24 bits (16 million networks). VXLANs by default use UDP port
*4789*, but this is configurable and could in theory be used to expand the
`vid` space. LSDN currently does not do this.

**IPv6 note**: VXLANs support IPv6 addresses, but they can not be mixed. All
physical nodes must use the same IP version and the version of multicast address
for ``ovl_vlan_mcast`` VXLAN must be the same. This does not prevent you from
using both IPv6 and IPv4 on the same physical node, you just have to choose one
version for the LSDN `attr_ip`.

.. _ovl_vxlan_mcast:

Multicast
~~~~~~~~~
**Available as**: :lsctl:cmd:`settings vxlan/mcast` (lsctl),
:c:func:`lsdn_settings_new_vxlan_mcast` (C API).

This is a self configuring variant of VXLAN. No further information for each
machine needs to be provided, because the VXLAN routes all unknown and broadcast
packets to a designated multicast IP address and the VXLAN iteratively learns
the source IP addresses.  Hence the only additional information is the multicast
group IP address.

**Restrictions**:
 - 24 bit `vid`
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
the `attr_ip`. All unknown and broadcast packets are sent to all the physical
machines and the VXLAN iteratively learns the IP address - MAC address mapping.

**Restrictions**:
 - 24 bit `vid`
 - Physical nodes in the same virtual network must be reachable on the IP layer
 - UDP and IP header overhead
 - Unknown and broadcast packets are duplicated for each physical machine

.. _ovl_vxlan_static:

Fully static
~~~~~~~~~~~~
**Available as**: :lsctl:cmd:`settings vxlan/static` (lsctl),
:c:func:`lsdn_settings_new_vxlan_static` (C API).

VXLAN with fully static packet routing. LSDN must be informed about the IP
address of each physical machine (using `attr_ip`) and MAC address of each
virtual machine (using `attr_mac`) participating in the network. LSDN then
constructs a routing table from this information. Broadcast packets are
duplicated and sent to all machines.

**Restrictions**:
 - 24 bit `vid`
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
each physical machine (using `attr_ip`) and MAC address of each virtual machine
(using `attr_mac`) participating in the network.

**Restrictions**:
  - 24 bit `vid`
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
configurations that can be created using LSDN. All the restrictions we are
going to describe in a moment shall be familiar to anyone who has some
experience with computer networks.

- You can not assign the same MAC address to two different virts that are
  part of the same virtual network.
- Any two virtual networks of the same network type must not be assigned the
  same virtual network identifier.
- Any two VXLAN networks sharing the same phys, where one network is of type
  :ref:`ovl_vxlan_static` and the other is either of type
  :ref:`ovl_vxlan_e2e` or :ref:`ovl_vxlan_mcast`, must use different UDP
  ports.
- Any virt inside a :ref:`ovl_vxlan_static` VXLAN network must be explicitly
  assigned a unique MAC address.
- All virts inside the same network must by assigned an unique IP address.
  Moreover, all IP addresses assigned to virts in the same network must be
  be of the same IP version (both IPv4 and IPv6 versions are supported by LSDN).

.. todo:

    Go through the various network types and describe their functioning and
    limitations. 
