.. _quickstart:

============
Quick-Start
============

Let's use LSDN to configure a simple network: four VMs, running on two physical
machines. We will call the physical machines *A* and *B* and the virtual machines
*1*, *2*, *3* and *4*. The virtual machines *1* and *2* are running on physical
machine *A*, virtual machines *3* and *4* are located on physical machine *B*.
The configuration is illustrated in :numref:`qnm`.

.. _qnm:

.. figure:: quickstart.svg
    :align: center

    Network setup. Solid lines are physical machine connections, dashed lines
    denote communication between virtual machines.

VMs *1* and *3* can communicate with each other and so can VMs *2* and *4*. This
means we will create two virtual networks, one for VM *1* and *3*, second for VM
*2* and *4*.

As mentioned in the `intro`, there are two major ways to use LSDN --
`configuration files <quickstart_lsctl>` and `C API <quickstart_c>`. We will
look at both possibilities.

Setting up virtual machines
---------------------------

You are free to use any Virtual Machine Manager you like: bare Qemu/KVM, libvirt
or VirtualBox or run containers (via LXC for example). The only thing LSDN needs
to know is which network interfaces on the host are assigned to the virtual
machines. Typically, this will be a *tap* interface for a VM and a *veth*
interface for a container.

.. _qemu:

Qemu
~~~~

.. index:: Qemu
.. index:: KVM

If you are just trying out LSDN, we suggest you download some live distro (like
`Alpine Linux <https://alpinelinux.org/downloads/>`_) and run Qemu/KVM. First
make sure QEMU is installed and then on physical machine *A* run:

.. code-block:: bash

    sudo qemu-system-x86_64 -enable-kvm -m 256 \
        -cdrom $iso_path.iso \
        -netdev type=tap,ifname=tap0,script=no,downscript=no,id=net0 \
        -device virtio-net-pci,netdev=net0,mac=14:9b:dd:6b:81:71

This will start up the Live ISO. Now login into the VM and setup a simple IP
configuration:

.. code-block:: bash

    ip addr change dev eth0 192.168.0.1/24
    ip link set eth0 up

Do the same for the remaining virtual machines, but with a different MAC and TAP
interface name. There is no need to change the ``net0`` strings:

 - on *A* create VM using ``ifname=tap0``, ``mac=14:9b:dd:6b:81:71``
   and set up IP address as ``192.168.0.1`` (we just did that in example above).
 - on *A* create VM using ``ifname=tap1``, ``mac=92:89:90:93:61:75``
   and set up IP address as ``192.168.0.2``
 - on *B* create VM using ``ifname=tap0``, ``mac=42:94:a5:f9:69:c6``
   and set up IP address as ``192.168.0.3``
 - on *B* create VM using ``ifname=tap1``, ``mac=f2:9b:4f:48:2d:d1``
   and set up IP address as ``192.168.0.4``

Libvirt
~~~~~~~

If you are using Libvirt, set up the virtual machines as usual. Unfortunately,
``virt-manager`` can not be told to leave the VM's networking alone. It will try
to connect it to a network, but that's what LSDN will be used for! It can also
not change an interface MAC address.  Instead, use ``virsh edit`` to manually
change the VM's XML. Change the ``interface`` tag of VM *1* on *A* to look like
this:

.. code-block:: xml

    <interface type='ethernet'>
      <mac address='14:9b:dd:6b:81:71'/>
      <script path='/usr/bin/true'/>
      <target dev='tap0'/>
      <!-- original <model> and <address> -->
     </interface>

Also change the other virtual machines but with different MAC and TAP interface
names (look at the `qemu` section for correct values).

.. _quickstart_lsctl:

Using configuration files
-------------------------

First, create the file ``config.lsctl`` with the following contents:

.. code-block:: tcl

    # Boilerplate
    namespace import lsdn::*
    # Choose the network tunneling technology
    settings geneve

    # Define the two virtual networks we have mentioned
    net 1
    net 2

    # Describe the network
    phys -name A -if eth0 -ip "192.168.10.1" {
        attach 1 2
        virt -name 1 -if tap0 -mac "14:9b:dd:6b:81:71" -net 1
        virt -name 2 -if tap1 -mac "92:89:90:93:61:75" -net 2
    }

    phys -name A -if eth0 -ip "192.168.10.2" {
        attach 1 2
        virt -name 3 -if tap0 -mac "42:94:a5:f9:69:c6" -net 1
        virt -name 4 -if tap1 -mac "f2:9b:4f:48:2d:d1" -net 2
    }

    # Tell LSDN what machine we are configuring right now
    claimLocal [lindex $argv 0]
    # Activate everything
    commit

Naturally, if you are using different IP addresses for your physical machines,
change the configuration file. Also pay attention to the ``-if eth0`` arguments
-- they tell LSDN what interface you use for connecting machines *A* and *B*
together and you may also need to change the interface to reflect your physical
setup.

Then make sure the file is available on both physical machines *A* and *B* and
run the following commands:

 - on *A*: ``$ lsctl config.lsctl A``
 - on *B*: ``$ lsctl config.lsctl B``

Congratulations, your network is set-up. Try it:

 - in VM *1*: ``$ ping 192.168.0.3``
 - in VM *2*: ``$ ping 192.168.0.4``

And they are correctly isolated too ``$ ping 192.168.0.2`` won't work in VM *1*.

.. _quickstart_c:

Using the C API
---------------

The equivalent network setup created using the LSDN `C API <capi>`:

.. code-block:: C

    #include <assert.h>
    #include <stdlib.h>
    #include <string.h>
    #include <stdint.h>

    #include <lsdn/lsdn.h>

    /* Use the default GENEVE port */
    static uint16_t geneve_port = 6081;

    static struct lsdn_context *ctx;
    static struct lsdn_settings *settings;
    static struct lsdn_net *net1, *net2;
    static struct lsdn_phys *machine1, *machine2;
    static struct lsdn_virt *VM1, *VM2, *VM3, *VM4;

    int main(int argc, const char* argv[])
    {
        /* On the command line pass in the machine name on which the program
         * is being run. In our case the names will be either A or B. */
        assert(argc == 2);

        /* Create a new LSDN context */
        ctx = lsdn_context_new("quickstart");
        lsdn_context_abort_on_nomem(ctx);

        /* Create new GENEVE network settings */
        settings = lsdn_settings_new_geneve(ctx, geneve_port);

        /* Create Machine 1 */
        machine1 = lsdn_phys_new(ctx);
        lsdn_phys_set_ip(machine1, LSDN_MK_IPV4(192, 168, 10, 1));
        lsdn_phys_set_iface(machine1, "eth0");
        lsdn_phys_set_name(machine1, "A");

        /* Create Machine 2 */
        machine2 = lsdn_phys_new(ctx);
        lsdn_phys_set_ip(machine2, LSDN_MK_IPV4(192, 168, 10, 2));
        lsdn_phys_set_iface(machine2, "eth0");
        lsdn_phys_set_name(machine2, "B");

        /* Create net1 */
        net1 = lsdn_net_new(settings, 1);

        /* Attach net1 */
        lsdn_phys_attach(machine1, net1);
        lsdn_phys_attach(machine2, net1);

        /* Create net2 */
        net2 = lsdn_net_new(settings, 2);

        /* Attach net2 */
        lsdn_phys_attach(machine1, net2);
        lsdn_phys_attach(machine2, net2);

        /* Create VM1 */
        VM1 = lsdn_virt_new(net1);
        lsdn_virt_connect(VM1, machine1, "tap0");
        lsdn_virt_set_mac(VM1, LSDN_MK_MAC(0x14,0x9b,0xdd,0x6b,0x81,0x71));
        lsdn_virt_set_name(VM1, "1");

        /* Create VM2 */
        VM2 = lsdn_virt_new(net2);
        lsdn_virt_connect(VM2, machine1, "tap1");
        lsdn_virt_set_mac(VM2, LSDN_MK_MAC(0x92,0x89,0x90,0x93,0x61,0x75));
        lsdn_virt_set_name(VM2, "2");

        /* Create VM3 */
        VM3 = lsdn_virt_new(net1);
        lsdn_virt_connect(VM3, machine2, "tap0");
        lsdn_virt_set_mac(VM3, LSDN_MK_MAC(0x42,0x94,0xa5,0xf9,0x69,0xc6));
        lsdn_virt_set_name(VM3, "3");

        /* Create VM4 */
        VM4 = lsdn_virt_new(net2);
        lsdn_virt_connect(VM4, machine2, "tap1");
        lsdn_virt_set_mac(VM4, LSDN_MK_MAC(0xf2,0x9b,0x4f,0x48,0x2d,0xd1));
        lsdn_virt_set_name(VM4, "4");

        /* Claim local A or B */
        struct lsdn_phys *local = lsdn_phys_by_name(ctx, argv[1]);
        assert(local != NULL);
        lsdn_phys_claim_local(local);

        /* Commit the created netmodel */
        lsdn_commit(ctx, lsdn_problem_stderr_handler, NULL);

        lsdn_context_free(ctx);
        return 0;
    }

Afterwards compile the program for machines *A* and *B* and link them together
with the LSDN library. Call the resulting executables ``quickstart`` and run the
respective executables on the two machines:

 - on *A*: ``$ ./quickstart A``
 - on *B*: ``$ ./quickstart B``

Your network is now set-up using the C API. Try:

 - in VM *1*: ``$ ping 192.168.0.3``
 - in VM *2*: ``$ ping 192.168.0.4``

And they are correctly isolated too ``$ ping 192.168.0.2`` won't work in VM *1*.
