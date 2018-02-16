.. _internals:

Internals
---------

If you plan on hacking LSCTL, this chapter is for you. It will describe the
available internal APIs and how they interact.

Project organization (components)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The core of LSDN is the **lsdn** library (``liblsdn.so``), which implements all of
the C API -- the netmodel handling and the individual network types.

.. digraph:: layering
    :caption: Organization

    node [shape=record]
    compound = true

    lsctlc [label = <<i>lsctlc</i> program>]
    lsctld [label = <<i>lsctld</i> program>]
    lsctl [label = <<i>lsctl</i> program>]
    tclext [label = <<i>lsctl-tclext</i> library>]

    subgraph cluster_liblsdn {
        label = <<b>lsdn</b> library>
        color = black

        json_dump [label = "JSON dump"]
        lsctl_dump [label = "lsctl dump"]
        netmodel
        vlan
        vxlan_static [label = "static vxlan"]
        vxlan_e2e [label = "e2e vxlan"]
        vxlan_mcast [label = "mcast vxlan"]
        geneve
        direct
        sbridge
        lbridge
        rules
        subgraph cluster_util {
            label = <utility modules>;
            list
            error
            log
            names
            nl
            nettypes
            list
        }
    }

    lsctl_dump -> json_dump
    json_dump -> netmodel
    lsctld -> tclext
    lsctl -> tclext
    tclext -> netmodel
    netmodel -> {vlan vxlan_static vxlan_e2e vxlan_mcast geneve direct}
    {vlan vxlan_e2e vxlan_mcast} -> lbridge
    {vxlan_static geneve} -> sbridge
    sbridge -> rules
    netmodel -> rules


    # Layout hacks

    # Needed not to render tools parallel with subgraph in parallel
    tclext -> lsctl_dump [style=invis]

    rules -> list [style=invis ltail=cluster_util]

Netmodel implementation
~~~~~~~~~~~~~~~~~~~~~~~

How to support a new network type
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Test environment
~~~~~~~~~~~~~~~~
