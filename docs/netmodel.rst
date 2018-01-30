.. _netmodel:

======================
Network representation
======================

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

