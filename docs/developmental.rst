Developmental Documentation
===========================

LSDN project focuses on the problem of easily manageable networking setup in the environment of
virtual machines and cloud environment generally. It perfectly fits to large scale deployment for
managing complex virtual networks in datacenters as well as small scale deployment for complete
control over containers in the software developer's virtual environment. Naturally the network
interface providers have to run Linux Kernel as we use it for the real networking work.

Two core goals which LSDN resolved are:
	1) Make Linux Kernel Traffic Control (TC) Subsystem usable:
		* LSDN provides library with high-level C API for linking together with recent
		  orchestrators.
		* Domain Specific Language (DSL) for standalone configuration is designed and can be used as
		  is.

	2) Audit the TC subsystem and verify that it is viable for management of complex virtual network scenarios as is.
		* Bugs in Linux kernel were found and fixed.
		* TC is viable to be used for complex virtual network management.

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

The networking functionality of such a needed tool is following:
	* Support for Virtual Networks, Switches and Ports.
	* API for management via library.
	* DSL for stand-alone management.
	* Network Overlay.
	* Multi-tenancy.
	* Firewall.
	* QoS support.

Most of the requirements above are barely fulfilled with vast majority of recent products which is
the main motivation for LSDN project.

Current Situation
-----------------

The domination of open-source technologies in the cloud environment is clear. Thus we do not focus
on the cloud based on closed, proprietary technologies such as cloud services from Microsoft.

In the open-source world the position of Open vSwitch (OVS) is dominant. It is kernel module
providing functionality for managing virtual networks and is used by all big players in the cloud
technology, e.g. RedHat. However Linux Kernel provides almost identical functionality via it's
traffic control (TC) subsystem. Thus there is code duplicity of TC and OVS and furthermore the
codebase of OVS is not as clear as the codebase of TC. Hence the effort to eliminate OVS in favor of
TC and focus to improve just one place in the Linux networking world.

Although TC is super featureful it has no documentation (literally zero) and the error handling of
it's calls is most of the time without any additional information. Hence for correct TC usage one
has to read bunch of kernel source codes. Add bugs in a rare used places and we have very powerful
but almost unusable software. Thus some higher level API is very attractive for everyone who wants
to use more advanced networking features from Linux kernel.

Similar Projects
----------------

There is no direct competition among tools building on top of TC to make it much more usable
(actually to make it just usable). However there are competitors for TC, which are not that powerful
or they are just modules full of hacks and are taken positively in the Linux mainline.

open vSwitch
............

	* Similar level of functionality to TC.
	* Complex, hardly maintainable code and code duplicity with respect to TC.
	* External module without any chance to be accepted to kernel mainline.
	* Slightly more user convenient configuration than TC.

vSphere Distributed Switch
..........................

	* Out-of-the game because it is not open-source.
	* Not that featureful as TC. E.g. no firewall, geneve etc.
	* Closed-source product of VMware.
	* Hardly applicable to heterogeneous open-source environment.

Hyper-V Virtual Switch
......................

	* Out-of-the game because it is not open-source.
	* Not that featureful as TC. E.g. no firewall, geneve etc.
	* Closed-source product of Microsoft.
	* Hardly applicable to heterogeneous open-source environment.

Development Environment
-----------------------

In this section we present all the tools used in our project which are worth mentioning.

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

The project has fairly nice documentation architecture. C source codes including API are commented
with **Doxygen**, which is a standard way how to this kind of task. Then the Doxygen output is used
and enhanced with tons of various documentations (user, developmental...) and processed with Sphinx.

**Sphinx** is a tool for creating really nice documentations and supports various outputs. Like this
we are able to have HTML and PDF documentation synced and both formats look fabulous.

Furthermore we use **readthedocs.io** for automatic generation of documentation after every
documentation commit. This also means that we have always up-to-date documentation online in
browsable HTML version as well as downloadable and printable PDF version. Note that PDF generation
uses LaTeX as a typesetting system, thus the printed documentation looks great.

The whole documentation source is written in **reStructuredText** (rst) markup language which greatly
simplified the whole process of creation such a comprehensive documentation.

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
