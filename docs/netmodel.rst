.. _netmodel:

======================
Network representation
======================

.. todo:: A bit slower introduction with more context.

The public API gives you tools to build a memory model of your networks. This
must describe the shape of the physical network as well as all virtual networks.

On the physical side, LSDN needs to know about every participating host machine.
Each such machine is called a _phys_ and is identified by its IP address. LSDN
assumes that all _phys_es are reachable by each other.

On the virtual side, each virtual _network_ is a collection of virtual machines,
called _virt_s. A _virt_ is tied to its network and identified by a MAC address.
In order to start communicating, it must _connect_ through a given Linux network
interface on a given _phys_. While the network is running, a _virt_ can migrate
-- disconnect and reconnect via a different interface and/or on a different
_phys_.

Single _phys_ can of course host multiple _virt_s at the same time. A single
_virt_, that is a single network inteface, can only be part of one virtual
network. However, the same virtual machine can act as multiple _virt_s if it
has multiple distinct network interfaces, and so participate in multiple
networks at the same time.
XXX Tohle nevím. Kontroluje se to někde, že se nepřipojí dva virty na stejném
interface? Přišlo mi že ne. Možná by mělo? Nebo explicitně řekneme že se to smí
a že to funguje?

Every host participating in a network must share a compatible network
representation. This usually means that all hosts have the same model,
presumably read from a common configuration file or installed through a single
orchestrator. It is then necessary to claim a _phys_ as local, so that LSDN
knows to act on the local network interfaces. Several restrictions also apply
to the creation of networks in LSDN. For details refer to section `restricts`.

--------------------------------
Supported tunneling technologies
--------------------------------

Currently LSDN supports three network tunneling technologies: `ovl_vlan`,
`ovl_vxlan` (in three variants) and `ovl_geneve`. They all use the same basic
networking model in LSDN, but is important to realize what technology are you
using and what restrictions it has.

Theoretically, you should be able to define your network model once and then
switch the networking technologies as you like. But in practice some
technologies may need more detailed network model than others. For example,
``ovl_vxlan_mcast`` does not need to known MAC address of the virtual machines
and ``ovl_vlan`` does not need to know IP adresses of the physical machines
neither the MAC addresses of the virtual ones.


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

If you either the physical network already uses VLAN tagging (the physical
computers are connected to a VLAN segment) or the virtual network will be using
tagging, the networking equipment along the way must support this. The support
is called *802.1ad* or sometimes *QinQ*.

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
needs the connected participating physical machines to have the `attr_ip` set
and they must all see each other on the IP network directly (no NAT).

VXLAN tags have 16 bits (16 million networks). VXLANs by default use UDP port
*4789*, but this is configurable and could in theory be used to expand the
`vid` space (but LSDN currently does not do this.

**IPv6 note**: VXLANs support IPv6 addreses, but they can not be mixed. All
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
 - 16 bit `vid`
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
 - 16 bit `vid`
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
 - 16 bit `vid`
 - Physical nodes in the same virtual network must be reachable on the IP layer
 - UDP and IP header overhead
 - Unknown and broadcast packets are duplicated for each physical machine
 - The virtual network is not fully opaque (MAC addresses of virtual machines
   must be knownn).


.. index::
    single: Geneve

.. _ovl_geneve:

Geneve
------
**Available as**: :lsctl:cmd:`settings geneve` (lsctl),
:c:func:`lsdn_settings_new_genve` (C API).

.. _ovl_direct:

No tunneling
------------
**Available as**: :lsctl:cmd:`settings direct` (lsctl), :c:func:`lsdn_settings_new_direct` (C API).

.. _restricts:

--------------------
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
- Any virt inside a :ref:`ovl_vxlan_static` network must be explicitly
  assigned a unique MAC address.
- All virts inside the same network must by assigned an unique IP address.
  Moreover, all IP addresses assigned to virts in the same network must be
  be of the same IP version (both IPv4 and IPv6 versions are supported by LSDN).

.. todo:

    Go through the various network types and describe their functioning and
    limitations. 
