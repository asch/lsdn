.. _ex:

========
Examples
========

Now is the right time to describe through a couple of examples how LSDN can
actually be used. The following are very simple (but complete nonetheless)
examples of virtual networks that can be set up through LSDN. They should be
descriptive enough to get you started with your own usecases.

.. _ex1:

---------
Example 1
---------

In the first example let's imagine we have three computers, *A*, *B* and *C*,
and that we are managing the networking infrastructure of two local businesses -
a bookstore and a bakery. The local businesses have their own software running
inside virtual machines (VMs) hosted by our three computers *A*, *B* and *C*.
It is expected that the virtual machines of the bookstore will be able to
communicate with each other on the network and likewise the VMs of the bakery.

It is also very desirable for the network traffic sent between the VMs of the
bookstore not to be seen by the VMs of the bakery and vice versa.
The three computers hosting the two companies are perhaps connected to the same
LAN, but it may not necessarily be the case as they quite as well might each be
on a different continent. We don't really care. We only assume that *A*, *B*
and *C* are able to send messages to each other. Without any further ado let's
present the first complete LSDN network configuration file:

.. code-block:: tcl
    :linenos:

    settings vxlan/static -name vxlan
    settings vlan -name vlan

    phys -if eth0 -name A -ip 172.16.0.1
    phys -if eth0 -name B -ip 172.16.0.2
    phys -if eth0 -name C -ip 172.16.0.3

    net -vid 1 -settings vxlan Bookstore {
        attach A B C
        virt -phys A -if 1 -mac 00:00:00:00:00:a1
        virt -phys A -if 2 -mac 00:00:00:00:00:a2
        virt -phys B -if 1 -mac 00:00:00:00:00:b1
        virt -phys C -if 1 -mac 00:00:00:00:00:c1
    }

    net -vid 1 -settings vlan Bakery {
        attach A B
        virt -phys A -if 3 -mac 00:00:00:00:00:a3
        virt -phys B -if 2 -mac 00:00:00:00:00:b2
    }

    claimLocal [lindex $argv 1]
    commit
    free

Before we describe line by line what is going on in this example, it might be
handy to actually see in picture how our networking infrastructure should look
like once we're done: (TODO fix)

.. graphviz::

    digraph G {
        subgraph cluster_0 {
            style=filled;
            color=lightgrey;
            node [style=filled,color=white];
            label = "Machine A";
            subgraph cluster_1 {
                style=filled;
                color=yellow;
                label = "VM 2";
                if1 [ shape=square label="out" ]
            }
            eth1 [ shape=square label="eth0"]
            subgraph cluster_2 {
                style=filled;
                color=yellow;
                label = "VM 1";
                if2 [ shape=square label="out" ]
            }
            subgraph cluster_3 {
                style=filled;
                color=green;
                label = "VM 3";
                if3 [ shape=square label="out" ]
            }
        }
        subgraph cluster_4 {
            style=filled;
            color=lightgrey;
            node [style=filled,color=white];
            label = "Machine B";
            subgraph cluster_5 {
                style=filled;
                color=yellow;
                label = "VM 4";
                if4 [ shape=square label="out" ]
            }
            eth2 [ shape=square label="eth0"]
            subgraph cluster_6 {
                style=filled;
                color=green;
                label = "VM 3";
                if5 [ shape=square label="out" ]
            }
        }
        subgraph cluster_7 {
            style=filled;
            color=lightgrey;
            node [style=filled,color=white];
            label = "Machine B";
            subgraph cluster_8 {
                style=filled;
                color=yellow;
                label = "VM 4";
                if6 [ shape=square label="out" ]
            }
            eth3 [ shape=square label="eth0"]
            subgraph cluster_9 {
                style=filled;
                color=green;
                label = "VM 3";
                if7 [ shape=square label="out" ]
            }
        }
    }

On the first line we create a VXLAN static virtual network settings, named
*vxlan*. As we haven't specified any port for this network settings, the
standard VXLAN UDP port 4789 will be used. On the second line we create another
virtual network settings, but this time of the VLAN network type, named *vlan*.

On lines 4 to 6 we describe three physical machines, or rather just three
physical interfaces present on our three machines. We have given each interface
a name, which corresponds with the name of the interface on the respective
machines. And finally we have also set an IPv4 address for those three
interfaces. It should be noted that it is expected that the physical interfaces
have already been assigned an IP address and that they have been brought up as
well. Maybe you're asking yourself why did we bother to specify the IP addresses
of the interfaces in the configuration file then? That's because this
information is needed when we are using the VXLAN tunnels.

Lines 8 through 14 describe a virtual network we are going to set up for the
bookstore. We will call this virtual network conveniently just *Bookstore*. The
*Bookstore* network will be tunneled through the VXLAN tunnels. We have assigned
the network a virtual network identifier *1*. The network will span all the
machines *A*, *B* and *C* - that's what we've written on line 9. The next line
describes a virtual machine that will reside on machine *A*, it will connect via
an interface which is simply called *1*. We have also assigned a MAC address to
this virtual machine. Again, LSDN expects that an interface called *1* is
already present on the physical machine *A* and that it is assigned the same MAC
address we have given it in the configuration file. Similarly, the next three
lines describe three other virtual machines inside the *Bookstore* network.

In a very similar fashion (lines 16 through 20) we've created a *Bakery* virtual
network with two virtual machines, but this time the virtual network spans only
the physical machines *A* and *B*. Note that the *Bakery* virtual network is
going to be tunneled inside a VLAN.

Line 22 will instruct LSDN which machine it should consider as being local. How
this command exactly works is described in .... TODO

If we don't want to perform just a dry run then we'd better tell LSDN to take
the network model it has constructed up to this point parsing the configuration
file and write (or commit in LSDN terminology) the model into the appropriate
kernel data structures. That's exactly what's being done with the single command
on line 23.

The last line instructs LSDN to clean up it's internal network model stored in
memory. Especially note this does not delete the networks stored in the kernel.

That was our first complete example. Now it remains to distribute this
configuration file (let's name it *example1.lsctl*) to our three computers *A*,
*B* and *C*. You may be wondering whether we didn't forget to show you two other
configuration files so that we would have three files that we could then
distribute to our three machines. In a moment you will see why it's not actually
needed.

On machine *A* type:

.. code-block:: bash

    lsctl example1.lsctl A

Similarly on machine *B*:

.. code-block:: bash

    lsctl example1.lsctl B

and on machine *C*:

.. code-block:: bash

    lsctl example1.lsctl C

By passing the command line parameter *A*, *B* or *C* to *lsctl* on the
appropriate nodes LSDN will be able to distinguish which machines are local.

That's it. Now your customers should be able to communicate inside the virtual
networks we have just created.

Keeping all our networking configuration in a single file will hopefully make it
easier for us to keep the networks in sync. But it is by no means the only way
how to configure your networks using LSDN. You may perhaps prefer to keep and
edit a configuration file on each physical machine separately; or you may have a
separate configuration file for each virtual network. The possibilities are
plentiful.

.. _ex2:

---------
Example 2
---------

In the second example we will focus on one very important aspect of virtual
networking - the problem of virtual machine migration. There are many reasons
why we might want to migrate virtual machines between physical machines hosting
them. For example we would like to do some planned maintenance on one of the
physical machines so we need to take all the VMs hosted on this machine and
migrate them (seamlessly if possible) to a different host in our infrastructure.

Let's jump right in and list the configuration file (*example2-1.lsctl*) of our
second example:

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

    claimLocal [lindex $argv 1]
    commit

If you're not recognizing any of the syntax used in this configuration file,
please refer to `ex1`.

We will run the following commands on node *A*: TODO how does claimLocal
actually work here?:

.. code-block:: bash

    lsctld -s /var/run/lsdn/example2.sock
    lsctlc /var/run/lsdn/example2.sock < example2-1.lsctl A

and similarly on node *B*:

.. code-block:: bash

    lsctld -s /var/run/lsdn/example2.sock
    lsctlc /var/run/lsdn/example2.sock < example2-1.lsctl B

and node *C*:

.. code-block:: bash

    lsctld -s /var/run/lsdn/example2.sock
    lsctlc /var/run/lsdn/example2.sock < example2-1.lsctl C

Again, the VMs inside the virtual network should now be able to reach each other
on the network.

Maybe after some time we realized it would be better to move the *migrator*
VM from node *A* to node *B*. We instruct LSDN to migrate this virtual machine
with the following commands in *example2-2.lsctl*:

.. code-block:: tcl

    net 1 {
        virt -phys B -if 2 -name migrator
    }

    commit

which we will again send to `lsctld` on node *A*:

.. code-block:: bash

    lsctlc /var/run/lsdn/example2.sock < example2-2.lsctl A

node *B*:

.. code-block:: bash

    lsctlc /var/run/lsdn/example2.sock < example2-2.lsctl B

and node *C*:

.. code-block:: bash

    lsctlc /var/run/lsdn/example2.sock < example2-2.lsctl C

What effectively happened is the *migrator* VM was disconnected from the virtual
network on node *A* and reconnected back again on node *B*.

It is important to note we have to perform this update on all nodes *A*, *B* and
*C*. Had we decided to create for example a vlan virtual network then we
wouldn't have to update the LSDN netmodel on machine *C*.

.. _ex3:

---------
Example 3
---------

In this example we are going to build on the `ex1`, but this time we are going
to demonstrate ways how we can shape the network traffic inside out virtual
networks. We will shape the traffic with firewall and QoS rules. These rules
will be specified for individual VMs.

.. code-block:: tcl
    :linenos:

    settings vxlan/e2e

    phys -if eth0 -name A -ip 172.16.0.1
    phys -if eth0 -name B -ip 172.16.0.2

    net -vid 1 {
        attach A B
        virt -phys A -if 1 -mac 00:00:00:00:00:a1 {
            rule out 1 drop -dstIp 192.168.99.2
        }
        virt -phys A -if 2 -mac 00:00:00:00:00:a2 {
            rule out 1 drop -dstIp 192.168.98.0/24
            rate out { -avg 100kbit -burst 600kbit -burstRate 200kbit }
        }
        virt -phys B -if 1 -mac 00:00:00:00:00:b1 {
            rule in 1 drop -dstIp 192.168.98.2
            rule out 2 drop -dstIp 192.168.98.1
            rate in { -avg 50kbit -burst 300kbit -burstRate 100kbit }
        }
    }

    claimLocal [lindex $argv 1]
    commit
    free

TODO description.

.. todo:: make references throughtout the examples to other parts of the doc
.. todo:: add e.g. a dhcp, gateway example?
