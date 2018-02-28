.. _intro:

==================
Introduction
==================

LSDN is a tool which allows you to easily configure networks with virtual
machines (or containers) in Linux. It let's you configure network tunnels
(`ovl_vxlan`, `ovl_geneve` ...) for separating groups of VMs into their own
virtual networks.

Each virtual network behaves (from the perspective of a VM) as if all the
computers were connected to a simple switch and were on the same LAN.

LSDN ensures isolation between networks using the existing network tunneling
technologies. Virtual machines never see traffic from devices that are not part
of their virtual network, even if they exist on the same host.  Multiple virtual
machines can even have identical MAC addresses, as long as they are connected to
different virtual networks. Thus, it is possible to virtualize multiple existing
physical networks and run them without interference in a single hosting
location.

Intended usage
~~~~~~~~~~~~~~

LSDN provides a `configuration language <lsctl>`, that allows you to describe
the desired network configuration (we call it a `network model <netmodel>` or
*netmodel* for short): the `virtual networks <net>`, `physical machines <phys>`
and `virtual machines <virt>` and their relationships. It can also be driven
programmatically, using a `capi`.

You run LSDN on each physical machine and provide it with the same netmodel,
either by passing the same configuration file (you can use our :c:func:`dumping
<lsdn_dump_context_tcl>` mechanism) or calling the same C API calls. LSDN then
takes care of the configuration so that the VMs in the same virtual network can
correctly talk to each other even if on different computers.

If you run a static ZOO of VMs, you can simply copy over the configuration file
to all the physical machines. If you have more complex virtualization setup, you
are likely to have an orchestrator on each physical machine. In that case, you
can modify your orchestrator to use LSDN as a backend.

.. rubric:: Open vSwitch

LSDN intentionally does not use `Open vSwitch <http://www.openvswitch.org/>`_ to
configure the tunnels, but only basic Linux networking (TC + flower classifier)
to show that this is possible and can be made reasonably convenient.

Configuring let's say VLANs in this way is not very difficult, but it can be
daunting if done for Geneve or static VXLANs.
