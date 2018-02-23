.. _netmodel:

======================
Network representation
======================

The public API (either :ref:`capi` or :ref:`lsctl`) gives you tools to build a a
model of your virtual networks, which LSDN will then realize on top of the
physical network, using various overlay technologies. You will need to tell LSDN
both abouth the virtual networks and the physical network they will be using.

There are three core concepts LSDN operates with: **virtual machines**,
**physical machines** and **virtual networks**. In the rest of the guide (and in
the source code) we abbreviate them as **virts**, **physes** and **nets**. If
you are wondering if there are any physical networks, then no, LSDN just expects
that the physical machines are connected together when needed.

The terminology is derived from the most common use case, but that does not mean
that *virts* really have to be virtual machines and *physes* must really be
physical machines. Maybe the *virts* are Linux containers and *physes* are
virtual machines running those containers?

The *virts*, *physes* and *nets* have the following relationships:
 - *virts* always belong to one *net* (they can not be moved)
 - *virts* are connected at one of the *physes* (they can connect at different
   phys, in other words, they can **migrate**)
 - *physes* attach to a *net* -- this tells LSDN that the *phys* will have virts
   connecting to the network [#f1]_.

.. rubric:: Footnotes

.. [#f1] In theory LSDN could figure out if a *phys* should be attached to a
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

.. _virt:
.. _attr_mac:

-----
Virts
-----

.. _phys:
.. _attr_ip:

------
Physes
------

.. _validation:

----------------------
Validation and Commit
----------------------

Every host participating in a network must share a compatible network
representation. This usually means that all hosts have the same model,
presumably read from a common configuration file or installed through a single
orchestrator. It is then necessary to claim a *phys* as local, so that LSDN
knows on which machines it is running. Several restrictions also apply
to the creation of networks in LSDN. For details refer to section `restricts`.

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

Partially self-cofiguring variant of VXLANs. LSDN must be informed
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
