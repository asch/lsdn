==================
Introduction
==================

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

.. todo:: Explain the motivation and introduce the various technologies.
