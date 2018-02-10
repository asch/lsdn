.. _ex:

========
Examples
========

Now is the right time to describe through a couple of examples how LSDN can
actually be used. The following are very simple (but complete nonetheless)
examples of virtual networks that can be set up through LSDN. They should be
descriptive enough to get you started with your own usecases.

.. _ex1:

----------------------------
Example 1 - Basic Principles
----------------------------

In the first example let's imagine we have three computers, **A**, **B** and **C**,
and that we are managing the networking infrastructure of two local businesses -
a bookstore and a bakery. The local businesses have their own software running
inside virtual machines (VMs) hosted by our three computers **A**, **B** and **C**.
It is expected that the virtual machines of the bookstore will be able to
communicate with each other on the network and likewise the VMs of the bakery.

It is also very desirable for the network traffic sent between the VMs of the
bookstore not to be seen by the VMs of the bakery and vice versa.

The three computers hosting the two companies are perhaps connected to the same
LAN, but it may not necessarily be the case as they quite as well might each be
on a different continent. We don't really care. We only assume that **A**, **B**
and **C** are able to send messages to each other. Without any further ado let's
present the first complete LSDN network configuration file. Afterwards, we will
split the example into smaller chunks and explain each section in detail.

.. code-block:: tcl

    settings vxlan/static -name vxlan

    phys -if eth0 -name A -ip 172.16.0.1
    phys -if eth0 -name B -ip 172.16.0.2
    phys -if eth0 -name C -ip 172.16.0.3

    net -vid 1 Bookstore {
        attach A B C
        virt -phys A -if 1 -mac 00:00:00:00:00:a1
        virt -phys A -if 2 -mac 00:00:00:00:00:a2
        virt -phys B -if 1 -mac 00:00:00:00:00:b1
        virt -phys C -if 1 -mac 00:00:00:00:00:c1
    }

    net -vid 2 Bakery {
        attach A B
        virt -phys A -if 3 -mac 00:00:00:00:00:a3
        virt -phys B -if 2 -mac 00:00:00:00:00:b2
    }

    claimLocal [lindex $argv 1]
    commit
    free

It might also be handy to actually see in picture how our networking
infrastructure will look like once we're done:

.. figure:: Example1.pdf
    :scale: 50 %

    Virtual networks in Example 1

    On three physical machines we host 6 VMs, pertaining to two different virtual networks.

Now let's step through our configuration file, starting with:

.. code-block:: tcl

    settings vxlan/static -name vxlan

This line will create a VXLAN static virtual network settings, named
*vxlan*. As we haven't specified any port for this network settings, the
default VXLAN UDP port will be used (`VXLAN <ovl_vxlan>`).

The following lines

.. code-block:: tcl

    phys -if eth0 -name A -ip 172.16.0.1
    phys -if eth0 -name B -ip 172.16.0.2
    phys -if eth0 -name C -ip 172.16.0.3

describe three physical machines, or rather just three physical interfaces
present on our three machines. We have given each interface a name, which
corresponds to the name of the interface on the respective machines. And finally
we have also set an IPv4 address for those three interfaces. It should be noted
that it is expected that the physical interfaces have already been assigned an
IP address and that they have been brought up as well. Maybe you're asking
yourself why did we bother to specify the IP addresses of the interfaces in the
configuration file then? That's because this information is needed when we are
using the VXLAN tunnels.

.. code-block:: tcl

    net -vid 1 Bookstore {
        attach A B C
        virt -phys A -if 1 -mac 00:00:00:00:00:a1
        virt -phys A -if 2 -mac 00:00:00:00:00:a2
        virt -phys B -if 1 -mac 00:00:00:00:00:b1
        virt -phys C -if 1 -mac 00:00:00:00:00:c1
    }

Afterwards we describe a virtual network we are going to set up for the
bookstore. We will call this virtual network conveniently just **Bookstore**.
The **Bookstore** network will be tunneled through the VXLAN tunnels. We have
assigned the network a virtual network identifier *1*. The network will span all
the machines **A**, **B** and **C** - that's what we've written with the :lsctl:cmd:`attach`
statement. The next line describes a virtual machine that will reside on machine
**A**. It will connect via an interface which is simply called *1*. We have also
assigned a MAC address to this virtual machine. Again, LSDN expects that an
interface called *1* is already present on the physical machine **A** and that it
is assigned the same MAC address we have given it in the configuration file.
Similarly, the next three lines describe three other virtual machines inside the
**Bookstore** network.

In a very similar fashion we've created a *Bakery* virtual network:

.. code-block:: tcl

    net -vid 2 Bakery {
        attach A B
        virt -phys A -if 3 -mac 00:00:00:00:00:a3
        virt -phys B -if 2 -mac 00:00:00:00:00:b2
    }

It has two virtual machines, but this time the virtual network spans only the
physical machines **A** and **B**. Note that the **Bakery** virtual network is again
going to be tunneled inside a VXLAN tunnel, only with a different network
idetifier *2*.

This line:

.. code-block:: tcl

    claimLocal [lindex $argv 1]

will instruct LSDN which machine it should consider as being local. How this
command exactly works is described in :lsctl:cmd:`claimLocal`.

If we don't want to perform just a dry run then we'd better tell LSDN to take
the network model it has constructed up to this point parsing the configuration
file and write (or :lsctl:cmd:`commit` in LSDN terminology) the model into the
appropriate kernel data structures. That's exactly what's being done with the
single command:

.. code-block:: tcl

    commit

The last line:

.. code-block:: tcl

    free

instructs LSDN to clean up it's internal network model stored in memory. For
details consult :lsctl:cmd:`free`. Especially note this does not delete the
networks stored in the kernel.

That was our first complete example. Now it remains to distribute this
configuration file (let's name it *example1.lsctl*) to our three computers **A**,
**B** and **C**. You may be wondering whether we didn't forget to show you two other
configuration files so that we would have three files that we could then
distribute to our three machines. In a moment you will see why it's not actually
needed.

On machine **A** type:

.. code-block:: bash

    lsctl example1.lsctl A

Similarly on machine **B**:

.. code-block:: bash

    lsctl example1.lsctl B

and on machine *C*:

.. code-block:: bash

    lsctl example1.lsctl C

By passing the command line parameter *A*, *B* or *C* to `lsctl <prog_lsctl>`
on the appropriate nodes, LSDN will be able to distinguish which machines are
local.

That's it. Now your customers should be able to communicate inside the virtual
networks we have just created.

Keeping all our networking configuration in a single file will hopefully make it
easier for us to keep the networks in sync. But it is by no means the only way
how to configure your networks using LSDN. You may perhaps prefer to keep and
edit a configuration file on each physical machine separately; or you may have a
separate configuration file for each virtual network. The possibilities are
plentiful.

.. _ex2:

------------------------
Example 2 - VM Migration
------------------------

In the second example we will focus on one very important aspect of virtual
networking - the problem of virtual machine migration. There are many reasons
why we might want to migrate virtual machines between physical machines hosting
them. For example we would like to do some planned maintenance on one of the
physical machines so we need to take all the VMs hosted on this machine and
migrate them (seamlessly if possible) to a different host in our infrastructure.

Let's jump right in and list the contents of the second configuration file which
we're going to name *example2-1.lsctl*:

.. code-block:: tcl

    settings vxlan/static

    phys -if eth0 -name A -ip 172.16.0.1
    phys -if eth0 -name B -ip 172.16.0.2
    phys -if eth0 -name C -ip 172.16.0.3

    net -vid 1 {
        attach A B
        virt -phys A -if 1 -mac 00:00:00:00:00:a1 -name migrator
        virt -phys A -if 2 -mac 00:00:00:00:00:a2
        virt -phys B -if 1 -mac 00:00:00:00:00:b1
        virt -phys C -if 1 -mac 00:00:00:00:00:c1
    }

If you're not recognizing any of the syntax used in this configuration file,
please refer to :ref:`ex1`.

We will run the following commands on node **A**:

.. code-block:: bash

    lsctld -s /var/run/lsdn/example2.sock
    lsctlc /var/run/lsdn/example2.sock < example2-1.lsctl
    lsctlc /var/run/lsdn/example2.sock claimLocal A
    lsctlc /var/run/lsdn/example2.sock commit

and similarly on node **B**:

.. code-block:: bash

    lsctld -s /var/run/lsdn/example2.sock
    lsctlc /var/run/lsdn/example2.sock < example2-1.lsctl
    lsctlc /var/run/lsdn/example2.sock claimLocal B
    lsctlc /var/run/lsdn/example2.sock commit

and node **C**:

.. code-block:: bash

    lsctld -s /var/run/lsdn/example2.sock
    lsctlc /var/run/lsdn/example2.sock < example2-1.lsctl
    lsctlc /var/run/lsdn/example2.sock claimLocal C
    lsctlc /var/run/lsdn/example2.sock commit

Again, the VMs inside the virtual network should now be able to reach each other
on the network.

Maybe after some time we realize it would be better to move the *migrator*
VM from node **A** to node **B**. We instruct LSDN to migrate this virtual machine
with the following commands run on each of the machines **A**, **B** and **C**:

.. code-block:: bash

    lsctlc /var/run/lsdn/example2.sock virt -phys B -if 2 -name migrator -net 1
    lsctlc /var/run/lsdn/example2.sock commit

What effectively happened is the *migrator* VM was disconnected from the virtual
network on node **A** and reconnected back again on node **B**.

It is important to note we have to perform this update on all nodes **A**, **B** and
**C**. Had we decided to create for example a vlan virtual network then we
wouldn't have to update the LSDN netmodel on machine **C**. Regardless of the
network settings type (e.g. VXLAN, GENEVE) created for out virtual networks, it
is always safe to run the same updates on all physical machines hosting the
virtual networks even if some nodes might not be impacted by any of the
performed change.

.. _ex3:

---------------------------
Example 3 - Traffic Shaping
---------------------------

In this example we are going to build on the :ref:`ex1`, but this time we are going
to demonstrate ways how we can shape the network traffic inside out virtual
networks. We will shape the traffic with firewall and Quality of Service (QoS
for short) rules. These rules will be specified for individual VMs. It will be
somewhat of a contrived example, but it will demonstrate the concepts well.
There will be just one virtual network with four VMs (**A**, **B**, **C** and
**D**). Schematically the scenario will look like this:

.. graphviz::

    digraph {
        rankdir=LR;

        A [ label="A\n------------\n40kb"; ]
        B [ label="B\n------------\n20kb"; ]
        C [ label="C\n------------\n10kb"; ]
        D [ label="D\n------------\n20kb"; ]

        A -> B
        B -> C
        C -> D
        D -> A
        B -> D

        { rank=same; B, D }
    }

The VMs will be able to send network packets only along the edges in the figure
above. The virtual network is also shaping the outgoing network bandwidth of
each VM (allocated bandwidth is depicted inside each node).

A transcription of this network setup with LSDN:

.. code-block:: tcl

    settings vlan

    phys -if eth0 -name a

    net 1 {
        attach a
        virt -phys a -if 1 -name A {
            rule out 1 drop -dstIp 192.168.0.3
            rule out 2 drop -dstIp 192.168.0.4
            rule in 3 drop -srcIp 192.168.0.2
            rule in 4 drop -srcIp 192.168.0.3

            rate out -avg 40kb -burstRate 40kb -burst 40kb
        }
        virt -phys a -if 2 -name B {
            rule out 1 drop -dstIp 192.168.0.1
            rule in 2 drop -srcIp 192.168.0.3
            rule in 3 drop -srcIp 192.168.0.4

            rate out -avg 20kb -burstRate 20kb -burst 20kb
        }
        virt -phys a -if 3 -name C {
            rule out 1 drop -dstIp 192.168.0.1
            rule out 2 drop -dstIp 192.168.0.2
            rule in 3 drop -srcIp 192.168.0.1
            rule in 4 drop -srcIp 192.168.0.4

            rate out -avg 10kb -burstRate 10kb -burst 10kb
        }
        virt -phys a -if 4 -name D {
            rule out 1 drop -dstIp 192.168.0.2
            rule out 2 drop -dstIp 192.168.0.3
            rule in 1 drop -srcIp 192.168.0.1

            rate out -avg 20kb -burstRate 20kb -burst 20kb
        }
    }

    claimLocal [lindex $argv 1]
    commit
    free

Let's have a look at all the firewall and QoS rules of one of the virtual
machines:

.. code-block:: tcl

    virt -phys a -if 2 -name B {
        rule out 1 drop -dstIp 192.168.0.1
        rule in 2 drop -srcIp 192.168.0.3
        rule in 3 drop -srcIp 192.168.0.4

        rate out -avg 20kb -burstRate 20kb -burst 20kb
    }

The first rule will drop any outgoing traffic with destination IP address
192.168.0.1. The next two rules will drop any traffic incoming from IP
addresses 192.168.0.3 or 192.168.0.4. If you take a look at the diagram of our
virtual network these are exactly the firewall rules that will ensure that
**VM B** will be able to send packets to **VM C** and **VM D**, but not to
**VM A** and will be able receive packets only from **VM A**. The last rule
installs a QoS rule. It sets the bandwidth for **VM B** with an average rate,
burst rate and burst all set to *20kb*. All the rate parameters are described
in :lsctl:cmd:`rate`

Similarly you can check the rules for **VM A**, **VM C** and **VM D** and see
for yourself they match with our intentation from the sketch above.

You should already be comfortable with the rest of the instructions in the
configuration file. If not, please start with :ref:`ex1`.

It's a fun exercise to build distributed software that keeps broadcasting a
single (UDP) packet with content "A" from within **VM A** to all other VMs in
the virtual network at the maximum rate possible. Each other VM upon reception
of a packet will append it's own name to the contents of the packet and
broadcast this amended packet to all other VMs in the virtual network. **VM A**
upon reception of a packet will dump this packet in a log file and drop this
packet. What patterns do you expect to see in this log file after some time?

.. todo:: make references throughtout the examples to other parts of the doc
.. todo:: add e.g. a dhcp, gateway example?
.. todo:: choose better parameters for the third example
.. todo:: rerun all the examples and make sure there is no mistake
