Developmental Documentation
===========================

LSDN project focuses on the problem of easily manageable networking setup in the environment of
virtual machines. It perfectly fits to large scale deployment for managing complex virtual networks
in datacenters as well as small scale deployment for complete control over containers in the
software developer's virtual environment. Naturally the network interface providers have to run
Linux Kernel as we use it for the real networking work.

Problem Introduction
--------------------

The biggest challenge in the cloud industry today is how to manage enormous number of operating
system instances together in some feasible and transparent way. No matter if containers or full
computer virtualization is used the virtualization of networking brings several challenges which are
not present in the world of classical physical networks, e.g. the isolation customer's networks
inside of datacenter (multi-tenancy), sharing the bandwidth on top of physical layer etc. All these
problems have to be tackled in a very thoughtful way. Furthermore it would be nice to build
high-level domain specific language (DSL) for configuring the standalone network as well as C
language API for linking and using with orchestrators.

The networking functionality of the tool is following:
	* Support for Virtual Networks, Switches and Ports.
	* API for management via library.
	* DSL for stand-alone management.
	* Network Overlay.
	* Multi-tenancy.
	* Firewall.
	* QoS support.

Current Situation
-----------------

The domination of open-source technologies in the cloud environment is clear. Thus we do not focus
on the cloud based on closed, proprietary technologies such as cloud services from Microsoft.

In the open-source world the position of Open vSwitch (OVS) is dominant. It is kernel module
providing functionality for managing virtual networks and is used by all big players, e.g. RedHat.
However Linux Kernel provides almost identical functionality via it's traffic control (TC)
subsystem. Thus there is code duplicity of TC and OVS and furthermore the codebase of OVS is not
clear as the codebase of TC. Hence the effort to eliminate OVS in favor of TC and focus to improve
just one place in the Linux networking world.

Although TC is super featureful it has no documentation (literally zero) and the error handling of
it's calls is most of the time without any additional information. Hence for correct TC usage one
has to read bunch of kernel source codes. Add bugs in very rare used places and we have almost
unusable software. Thus some higher level API is very attractive for everyone who wants to use more
advanced networking features from Linux kernel.

Similar Projects
----------------

open vSwitch
............

vSphere Distributed Switch
..........................

Hyper-V Virtual Switch
......................

Development Environment
-----------------------

In this section we present all the tools used in our project which are worth mentioning. TODO: add
description and nice story-telling for every tool

Development Tools
.................

cmake, libdaemon, git(hub), linux namespaces, vim, packages, archlinux userspace,

Testing Environment
...................

qemu, ctest, travis-ci

Communication Tools
...................

mailman, irc

Documentation Tools
...................

sphinx, doxygen, breathe, latex, rtfd.io

Project Timeline
----------------

Github logs, graphs, topics per name.

Team Members
------------

Vojtěch Aschenbrenner
Roman Kápl
Jan Matějek
Adam Vyškovský

Conclusion and Future Work
--------------------------

It was done, it can be extended. It works in both virtual environment as well as on physical
machines.
