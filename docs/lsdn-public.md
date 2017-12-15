@page lsdn-public LSDN Public API
@short Overview of the user-facing LSDN library API.

## Basic concepts

The LSDN library is built upon a model of separate virtual networks connecting
virtual machines distributed over multiple physical hosts. One host can be
part of multiple networks and every network can span many hosts. The virtual
networks have a simple topology: each virtual machine is connected to a central
switching hub which allows it to connect to any other machine within the same
network.

LSDN ensures isolation between networks. Virtual machines never see traffic from
devices that are not part of their network, even if they exist on the same host.
Multiple virtual machines can even have identical MAC addresses, as long as they
are connected to different networks. It is possible to virtualize multiple
existing physical networks and run them without interference in a single hosting
location.

## Network representation

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
knows to act on the local network interfaces.

