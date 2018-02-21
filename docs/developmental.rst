Developmental Documentation
===========================

LSDN project focuses on the problem of easily manageable networking setup in the
environment of virtual machines and cloud environment generally. It perfectly
fits to large scale deployment for managing complex virtual networks in data
centers as well as small scale deployment for complete control over containers
in the software developer's virtual environment. Naturally the network interface
providers have to run Linux Kernel as we use it for the real networking work.

Two core goals which LSDN resolved are:

1) Make Linux Kernel Traffic Control (TC) Subsystem usable:

   * LSDN provides library with high-level C API for linking together with
     recent orchestrators.
   * Domain Specific Language (DSL) for standalone configuration is designed and
     can be used as is.

2) Audit the TC subsystem and verify that it is viable for management of complex
   virtual network scenarios as is.

   * Bugs in Linux kernel were found and fixed.
   * TC is viable to be used for complex virtual network management.

Problem Introduction
--------------------

The biggest challenge in the cloud industry today is how to manage enormous
number of operating system instances together in some feasible and transparent
way. No matter if containers or full computer virtualization is used the
virtualization of networking brings several challenges which are not present in
the world of classical physical networks, e.g. the isolation customer's networks
inside of datacenter (multi-tenancy), sharing the bandwidth on top of physical
layer etc. All these problems have to be tackled in a very thoughtful way.
Furthermore it would be nice to build high-level domain specific language (DSL)
for configuring the standalone network as well as C language API for linking and
using with orchestrators.

The networking functionality of such a needed tool is following:
	* Support for Virtual Networks, Switches and Ports.
	* API for management via library.
	* DSL for stand-alone management.
	* Network Overlay.
	* Multi-tenancy.
	* Firewall.
	* QoS support.

Most of the requirements above are barely fulfilled with vast majority of recent
products which is the main motivation for LSDN project.

Current Situation
-----------------

The domination of open-source technologies in the cloud environment is clear.
Thus we do not focus on the cloud based on closed, proprietary technologies such
as cloud services from Microsoft.

In the open-source world the position of Open vSwitch (OVS) is dominant. It is
kernel module providing functionality for managing virtual networks and is used
by all big players in the cloud technology, e.g. RedHat. However Linux Kernel
provides almost identical functionality via it's traffic control (TC) subsystem.
Thus there is code duplicity of TC and OVS and furthermore the code base of OVS
is not as clear as the code base of TC. Hence the effort to eliminate OVS in
favor of TC and focus to improve just one place in the Linux networking world.

Although TC is super featureful it has no documentation (literally zero) and the
error handling of it's calls is most of the time without any additional
information. Hence for correct TC usage one has to read bunch of kernel source
codes. Add bugs in a rare used places and we have very powerful but almost
unusable software. Thus some higher level API is very attractive for everyone
who wants to use more advanced networking features from Linux kernel.

Similar Projects
----------------

There is no direct competition among tools building on top of TC to make it much
more usable (actually to make it just usable). However there are competitors for
TC, which are not that powerful or they are just modules full of hacks and are
taken positively in the Linux mainline.

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

In this section we present all the tools used in our project which are worth
mentioning.

Development Tools
.................

The platform independent builds with all the dependency and version checks are
done thanks to **cmake** in cooperation with **pkgfile**. This is much nicer and
more featureful alternative to autoconf tools.

Furthermore we kept everything since the beginning in the **GIT** repository on
GitHub. We used pretty intensely with all it's features like branches etc. VCS
is a must for any project and GIT is the most common choice.

When we were developing daemon (for migration support) we found library called
**libdaemon** which helps you to write system daemon in a proper way with all
the signal handling, sockets management and elimination of code full of race
conditions.

As a code editor only **VIM** and **ed** were allowed.

Testing Environment
...................

Our testing environment is based on the highly modular complex of **bash
scripts**, where every part which should be tested defines prescribed functions
which are further executed together with other parts. Like this we can create
complex tests just with combination of several parts.

For automatic test execution and it's simplification we used **ctest** tool
which is part of cmake package.

The continuous integration was used through the **Travic-CI** service which
after every code commit executed all the tests and provides automatic email
notification in case of failure.

We have also extensive support for testing on not supported kernels via
**QEMU**. Automatic scripts are able to create minimalistic and up-to-date Arch
Linux root filesystem, boot up-to-date kernel and ran all tests. This method is
also used on Travis-CI, where only LTS versions of Ubuntu are available.

Of course various networking tools like dhcpd, dhcpcd, dhclient, tcpdump,
iproute, ping etc. were used for diagnostics as well as directly in tests.

Note that during tests we were highly dependent on **Linux namespaces**, hence
we were able to simulate several virtual machines without any overhead and speed
up all the tests.

Communication Tools
...................

Communication among all team members and leaders was performed via old-school
mailing lists and IRC combo. We used our own self-hosted **mailman** instance
for several mailing lists:

	* lsdn-general for general talk, organization, communication with leaders
	  and all important decisions.
	* lsdn-travis for automatic reports from Travis-CI notifying us about
	  commits which break the correct functionality.
	* lsdn-commits for summary of every commit we made. This was highly
	  motivation element in our setup, because seeing your colleague committing
	  for the whole day can make you feel really bad. Furthermore discussion
	  about particular commit were done in the same thread, which enhances the
	  organization of decisions we made and why.

For real-time communication we used **IRC** channel #lsdn on Freenode. This is
useful especially for flame-wars and arguing about future design of the tool.

Documentation Tools
...................

The project has fairly nice documentation architecture. C source codes including
API are commented with **Doxygen**, which is a standard way how to this kind of
task. Then the Doxygen output is used and enhanced with tons of various
documentations (user, developmental...) and processed with Sphinx.

**Sphinx** is a tool for creating really nice documentations and supports
various outputs. Like this we are able to have HTML and PDF documentation synced
and both formats look fabulous.

Furthermore we use **readthedocs.io** for automatic generation of documentation
after every documentation commit. This also means that we have always up-to-date
documentation online in browsable HTML version as well as downloadable and
printable PDF version. Note that PDF generation uses LaTeX as a typesetting
system, thus the printed documentation looks great.

The whole documentation source is written in **reStructuredText** (rst) markup
language which greatly simplified the whole process of creation such a
comprehensive documentation.

Open-source contributions
.........................

We have identified a few bugs in the Linux kernel during our development. We
believe this is mainly because of the unusual setups we excersise and new kernel
features (such as goto chain, lwtunnels) we use. Following bugs were patched or
at least reported:

 - `net: don't call update_pmtu unconditionally <https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=f15ca723c1ebe6c1a06bc95fda6b62cd87b44559>`_
   (reported)
 - `net: sched: crash on blocks with goto chain action <https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=a60b3f515d30d0fe8537c64671926879a3548103>`_
 - `net: sched: fix crash when deleting secondary chains <https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=d7aa04a5e82b4f254d306926c81eae8df69e5200>`_
 - `v9fs over btrfs <https://sourceforge.net/p/v9fs/mailman/message/36130692/>`_
   (mailing list dead, not merged)

Naturally, our tooling also has problems, so we also fixed a bug in
`sphinx <https://github.com/sphinx-doc/sphinx/pull/4627>`_ and
`breathe <https://github.com/michaeljones/breathe/pull/365>`_.

Project Timeline
----------------

The project came from an idea of Jiri Benc (Linux Kernel Networking Developer)
from Red Hat Czech who wanted to create a proof-of-concept tool which will try
to replace Open vSwitch with purely Linux Kernel functionality and find all the
missing functionality or bugs in Linux Kernel which would block or slow down the
effort to eliminate Open vSwitch.

These days Vojtech Aschenbrenner was an intern in Jiri's team and also a student
who was looking for challenging Software Project topic from Systems field, which
was a mandatory part of studies at Charles University. Hence the topic arose.

Formation of the team was not that straightforward. In the beginning the team
was composed from 7 people. They were people with Systems interests and also
great computer scientists. The property of excellency was actually the biggest
problem of the team. In the beginning part of the implementation phase 3 people
left the studies and also team because of much better offer. It was two times
because of Google and one time because of Showmax. Thus 4 people left in the
team which was still manageable.

However another personal problems came with studies in the US and jobs of the
remaining members. Vojtech Aschenbrenner left to the University of Rochester and
have almost no time to work on a project for a lot of weeks. Similar situation
came to Adam Vyškovský who left to Paris because of a dream job in an aviation.
Jan Matějek still had full-time job in SUSE and it looked like the project has a
huge problems and will most probably fail. However Roman Kápl showed his true
determination and saved the project although he has also part-time job in a
systems company. It is for sure, that the project would fail without his
knowledge, skills in system programming and diligence. When all the remaining
members who were still part of the team saw how he is continuously working on
the project they came back from abroad and decided to finish the project as well
as their master studies instead of continuing they career elsewhere. All of the
team members believe that Roman influenced our future life in a positive way.

After this we managed to do hackatons quite often and do the majority of the
work in a several months. Because the problematic part of the project where a
lot of people left was before the official start the official timeline of the
project was according to the plan and we were able to fulfill our deadlines
which were following:

 - Month 1:

   * Analysis of the requirements of cloud environments for software defined
     networking.
   * Analysis and introduction to Linux Kernel networking features, especially
     traffic control framework and networking layer of the Linux Kernel.
   * Description of detailed use-cases which will be implemented.

 - Month 2:

   * API design.

 - Months 3 - 7:

   * Implementation of the complete functionality of the project. This was the
     main developing part.

 - Months 8 - 9:

   * Finalization.
   * Debugging.
   * Documentation.
   * Presentation preparation.
   * (The most intense part)

Team Members
------------

The project was originally started with people who are no longer in the team
from various of reasons. We would like to honorably mention them, because the
initial project topics brainstorming were done with them.

	* *Martin Pelikán* left to Google Sydney few weeks after the project was
	  started. Although he is a non-sleeper which can work on several projects
	  together he was not able to find a spare time for this one. This was a big
	  loss because his thesis was about TC. 
	* *David Krška* left to Google London few weeks after his bachelor studies
	  graduation.
	* *David Čepelík* left to Showmax one semester after his bachelor studies
	  graduation.

The rest of the people who started the project were able to stay as a part of
the team and finish it.

	* *Vojtěch Aschenbrenner* established the team and tried to lead the
	  project. He also created, hosted and managed the mailing list platform and
	  officially communicated with authorities from the University as well as
	  mediated the communication inside the team. He created the lsdn daemon and
	  how the way how it communicates with the client. He also worked on the
	  testing environment's scripts, developmental documentation and maintain
	  the Arch Linux package.
	* *Roman Kápl* is the hero of the project. He saved the project when nobody
	  was able to work on it. He has also the deepest understanding of Linux
	  Kernel features related to our project. He was the main technical
	  architect of the project as well as the most intense worker of the team.
	  There is no place in the project without Roman's touch. He also fixed
	  several bugs in the Linux Kernel related to our project, bugs in the
	  tooling and created deb based packages.
	* *Jan Matějek* is the code review and program documentation master. He
	  reviewed every piece of the code and documented it. He created the
	  documentation for public API and linked together the documentation
	  generated by Doxygen with our documentation system. During the
	  documentation process he fixed several bugs and provided great feedback
	  about wrong logic of the program in several places. He also created rpm
	  based packages.
	* *Adam Vyškovský* is another kernel hacker in the team and low level system
	  programmer who was together with Roman the main author of the most low
	  level core functionality which was touching the kernel. He spent enormous
	  time with debugging the netlink communication with the kernel. 

At this place Jiri Benc, the official leader from Red Hat Czech, should be
mentioned because discussion with him was always full of knowledge and his
overview of the Linux Kernel and open-source world is enormous. He always found
a spare time to arrange a meeting with us and was also willing to help us move
forward and motivate us.

Conclusion, Contribution and Future Work
----------------------------------------

The project was able to fulfill all the requirements set in the beginning and
also follow the plan created in the beginning. This means that all the requested
functionality was implemented and properly tested. Furthermore it was documented
all through from both programmers view and also from user (API) view. Also
detailed use cases with the quickstart guide were described. Especially the
quickstart guide showed how easy it is to create complex virtual networking
scenario in a few steps with very minimal configuration files.

At the end the whole project was all through tested in both, virtual setups,
physical setups as well as hybrid setups. Finally the demo presentation showing
the power of LSDN was created. This part of work showed how capable LSDN (and TC
framework) is in terms of replacing Open vSwitch -- it is capable and the
direction of TC framework development goes in the right way of replacing Open
vSwitch in the future.

Another big success of the project was patching the upstream of Linux Kernel as
well as patching the tooling as Sphinx and Breathe. Also several bugs were
reported. This was the secondary and optional target of the project which was
also fulfilled.

LSDN has the ambition to become the only tool using the extremely powerful TC
framework in Linux Kernel and use it in very user convenient way with very
minimal additional dependencies for creation complex virtual network scenarios.
Also the core of the tool is written efficiently in C, thus there is no
performance impact of using LSDN. Furthermore we were able to push LSDN
installation packages to user repositories of Linux distributions or at least
create the packages. This means that the comfort of installation is maximal
which helps to fulfill the main goal of creating easy to use management tool for
complex networks.

Because of the very promising future of the tool, the LSDN team is willing to
continue in supporting the project as well as integrate future enhancements in
the TC framework, fix bugs found in the production as well as customize the
project according to the future needs of virtual networks.

The next challenging step is to integrate LSDN into most popular virtualization
orchestrators and eliminate Open vSwitch. This would attract more developers and
make the project part of the state of the art cloud ecosystem - this is the real
goal!

Well, on a more realistic note, there are some features that we consider useful
and could be improved upon straigh away. Some of them rely on tings that the
kernel learned to do in the last months of the project, or that we have
discovered recently - the ``egress`` qdisc or better default disciplines (CoDEL
was suggested). We would also like to improve the firewall (rewrite the rule
engine and add support for ACCEPT actions).
