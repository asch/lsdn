.. highlight:: tcl

.. _lsctl:

==========================
Lsctl Configuration Files
==========================

LSDN has its own configuration language for describing the network topology. If
you saw the `quickstart` or `netmodel` sections, you have already seen examples
of the configuration files.

-------
Syntax
-------

The configuration syntax is actually based on the TCL_ language -- but you do
not have to be afraid, this guide is self-contained. You might not even guess
TCL is there if we did not tell you. However, it is good to know TCL is there
if you need more advanced stuff, like variable substitution or loops. And if you
know TCL, you will recognize some of the conventions and feel at home.

.. _TCL: https://www.tcl.tk/

The only downside is that you have to include a short boilerplate at the top of each
configuration file to tell TCL that you don't want to prefix everything with
``lsdn::``.  Start your configuration file with this line: ::

    namespace import lsdn::*

The configuration file itself is a list of directives that tell LSDN what
objects the network consists of, how they are configured and how they connect.
The directives are terminated by a newline. Other white-space is not
significant. All available directives are listed in section `dirref`.

All LSDN directives follow the same basic patterns. They start with the
directive name (for example ``net`` or ``settings``) and are followed with
argument for that directive. Directives and their arguments are separated by
white-space. Some directives go without an argument. Other directives make use of
named arguments (or as they are called in some languages "keyword arguments"): ::

    directiveWithNamedArgs -opt1 value1 -opt2 value2 -opt3WithoutAnyValue

The directives may combine named and regular arguments. In that case, consult
the documentation for the particular directive, if the regular (non-keyword)
arguments should be placed before or after the keyword ones.

If you need the directive to span multiple lines, use the backslash ``\``
continuation character as you do in shell:

.. code-block:: none

    virt -name critical_server_with_unknown_purpose -if enps0 \
        -mac F9:9B:81:C2:66:F9 -net service_net

-----
Names
-----

Objects (physical machines, virtual machines etc.) in LSCTL are given names, to
allow you to refer to them later. The name must be given when a configuration
directive is used to create an object. You are free to choose any name you like,
as the names do not have any direct impact on the network.

Please note that forward references are not allowed, because in its core LSCTL
is a procedural language. For example, this snippet will not work: ::

    virt -net test
    net -vid 1 test {}

Any directive referencing an undefined object will return an error like this: ::

    can not find network

The order must be swapped like this: ::

    net -vid 1 test {}
    virt -net test

If a directive references an undefined object, it will print a stack-trace and
the script execution will end.

--------
Nesting
--------

Some directives may also contain other directives. In that case, the nested
directives are enclosed in curly-braces ``{}`` following the parent directive
like this [#f1]_: ::

    phys -name p
    net 42 {
        virt -name a -phys p
        attach p
    }

Nested directives are used to simplify definition of related elements. The example
above specifies a virtual machine *a* connected to network *42*. The connection
is implied from the nesting. The nesting is quite flexible and you can decide
to use a style that most suits you and that reflects organization of your
network. An equivalent way to write the example above would be: ::

    phys -name p
    net 42
    virt -name a -net 42 -phys p
    attach -phys p -net 42

Or: ::

    net 42
    phys -name p {
        virt -name a -net 42
    }
    attach -phys p -net 42

Or: ::

    net 42 {
        phys -name p {
            virt -name a
        }
    }

Note that there is no need for :lsctl:cmd:`attach` in the last example, since
nesting took care of it for us.

In general, nesting can be used anywhere you would otherwise have to specify a
relationship using arguments. Other nestings are disallowed. The supported
nestings are:

- *virt* in *net* = *virt* will be connected to the *net*
- *virt* in *phys* = *virt* will be connected at this *phys*
- *net* in *phys* = *phys* will be attached to the *net*
- *phys* in *net* = *phys* will be attached to the *net*
- *attach* in *net* = *net* will be attached to phys given as argument
- *attach* in *phys* = nets given as arguments will be attached to *phys*
- *detach* follows the same rules
- *claimLocal* in *phys* = *phys* will be claimed as local

Some directives are only settings for one object (and do not imply any
relationship). These are the ``rate`` (for virt QoS) and ``rules`` (for virt
firewall) directives. They **must** be nested under a ``virt`` directive.

.. rubric:: Footnotes

.. [#f1] If you are familiar with TCL, you will recognize this is how TCL
    control-flow commands work.

---------------
Argument types
---------------

.. lsctl:type:: int

    An integer number, given as string of digits optionally prefixed with a
    sign. LSCTL recognizes the ``0x`` prefix for hexadecimal and ``0`` for octal
    integers.

.. lsctl:type:: string

    String arguments in LSCTL are given the same way as in shell - they don't
    need to be quoted. Mostly they are used for names, so there is no need to
    give string arguments containing spaces.

    If you want to give a directive an argument containing spaces, newlines or
    curly brackets, simply enclose the argument in double-quotes.  If you want
    the argument to contain double-quotes, backslash or dollar sign, precede the
    character with backslash: ::

        virt -name "really\$bad\\idea
        on so many levels"

    If you need the full syntax definition, refer to ``man tcl.n``
    on your system.

.. lsctl:type:: direction

    Either ``in`` or ``out``. ``in`` is for packets entering the
    virtual machine, ``out`` is for packets leaving the virtual machine.

.. lsctl:type:: ip

    IP address, either IPv6 or IPv4. Common IPv6 and IPv4 formats are supported.

    For exact specification, refer to ``inet_pton`` function in C library.

    Examples:

    .. code-block:: none

        2a00:1028:8380:f86::2
        192.168.56.1

.. lsctl:type:: subNet

    IP address optionally followed by ``/`` and prefix size. If the prefix size
    is not given, it is equivalent to 128 for IPv6 and 32 for IPv4, that is
    subnet containing the single IP address.

    Instead of writing a prefix after the ``/``, a network mask can be given,
    using the same format as for the IP address.

    Examples:

    .. code-block:: none

        2a00:1028:8380:f86::2
        2a00:1028:8380:f86::0/64
        192.168.56.0/24
        192.168.56.0/255.255.255.0

.. lsctl:type:: mac

    MAC address in octal format. Both addresses with colons and without colons
    are supported, as long as the colons are consistent. Case-insensitive

    .. code-block:: none

        9F:1A:C1:4C:EE:0B
        9f1ac14cee0b

.. lsctl:type:: size

    An unsigned decimal integer specifying a number of bytes. Suffices ``kb``, ``mb``, ``gb``
    and ``bit``, ``kbit``, ``mbit``, ``gbit`` can be given to change the unit.
    All units are 1024-base (not 1000), despite their `SI
    <https://en.wikipedia.org/wiki/International_System_of_Units>`_ names. This
    is for compatibility with the ``tc`` tool from ``iproute`` package, which
    uses the same units.

.. lsctl:type:: speed

    An unsigned decimal integer specifying a number of bytes per second.

    Supported units are the same as for :lsctl:type:`size`.

.. _dirref:

-------------------
Directive reference
-------------------

.. default-role:: lsctl:cmd

.. lsctl:cmd:: net | name -vid -settings -phys -remove { ... }

    Define new virtual network or change an existing one.

    **C API equivalents:** :c:func:`lsdn_net_new`, :c:func:`lsdn_net_by_name`.

    :param int vid:
        Virtual network identifier. Network technologies like VXLANs or VLANs
        use these numbers to separate different networks. The ID must be unique
        among all networks of the same network type. The parameter is forbidden
        if network already exists. The permissible range of network identifiers
        differs for individual network types (see :ref:`netmodel`).
    :param string name:
        Name of the network. Does not change network behavior, only used by the
        configuration to refer to the network. However, if the ``-vid`` argument
        is not specified, this ``name`` argument will also specify the ``vid``.
    :param string phys:
        Optional name of a `phys` you want to attach to this network.  Shorthand
        for using the `attach` directive. Can not be used when nested inside
        `phys` directive.
    :param string settings:
        Optional name of a previously defined `settings`, specifying the network
        overlay type (VLAN, VXLAN etc.). If not given, the ``default`` settings
        will be used. Settings of existing net can not be changed.
    :param remove:
        Optional, remove the network. This will effectively also remove any
        child object (e.g. any `virt` inside this network).
    :scope none:
        This directive can appear at root level.
    :scope phys:
        Automatically attaches the parent phys to this network. Shorthand for
        using the `attach` directive.

.. lsctl:cmd:: phys | -name -if -ip -net -remove -ifClear -ipClear

    Define a new physical machine or change an existing one.

    **C API equivalents:** :c:func:`lsdn_phys_new`, :c:func:`lsdn_phys_by_name`.

    :param string name:
        Optional, name of the physical machine. Does not change network
        behavior, only used by the configuration to refer to the phys.
    :param string if:
        Optional, set the network interface name this phys uses to communicate
        with the physical network.
    :param ip ip:
        Optional, set the IP address assigned to the phys on the physical
        network.
    :param string net:
        Optional, name of a `net` you want this phys to attach to. Shorthand for
        using the `attach` directive. Can not be used when nested inside `net`
        directive.
    :param remove:
        Optional, remove the physical machine. This will effectively also
        disconnect any `virt` residing on this machine.
    :param ifClear:
        Optional, clear the machine's interface name, if any.
    :param ipClear:
        Optional, clear the IP address of the `phys`, if any.
    :scope none:
        This directive can appear at root level.
    :scope net:
        Automatically attaches this phys to the parent network. Shorthand for
        using the `attach` directive.

.. lsctl:cmd:: virt | -net -name -mac -phys -if -remove -macClear

    Define a new virtual machine or change an existing one.

    **C API equivalents:** :c:func:`lsdn_virt_new`, :c:func:`lsdn_virt_by_name`.

    :param string net:
        The virtual network this virt should be part of. Mandatory if creating
        new virt, forbidden if changing an existing one. Forbidden if nested
        inside `net`.
    :param string name:
        Optional, name of the virtual machine. Does not change network behavior,
        only used by the configuration to refer to this virt.
    :param mac mac:
        Optional, MAC address used by the virtual machine.
    :param string phys:
        Optional, connect (or migrate, if already connected) at a given `phys`.
    :param string if:
        Set the network interface used by the virtual machine to connect at the
        phys. Mandatory, if ``-phys`` argument was used.
    :param remove:
        Optional, remove the virtual machine.
    :param macClear:
        Optional, clear the virtual machine's MAC address, if any.
    :scope none:
        This directive can appear at root level.
    :scope net:
        Equivalent with giving the ``-net`` parameter and thus mutually
        exclusive.
    :scope phys:
        Equivalent with giving the ``-phys`` parameter and thus mutually
        exclusive

.. lsctl:cmd:: attach | -phys -net
.. lsctl:cmd:: attach | -phys netlist
.. lsctl:cmd:: attach | -net physlist

    Attaches a given physical machine(s) to a virtual network(s). The command
    can either attach a single phys to a single net (using the ``-phys`` and
    ``-net`` arguments) or to multiple nets at once (using the ``-phys``
    argument and positional arguments) or attach multiple physes to a single
    network (using the ``-net`` argument and positional arguments).

    If scoped, the ``-net`` or ``-phys`` arguments are implicit, so you can
    easily attach a phys to multiple nets like this: ::

        phys test {
            attach net1 net2
        }

    :scope root:
        This directive can appear at root level.
    :scope net:
        Equivalent with giving the ``-net`` parameter and thus mutually
        exclusive.
    :scope phys:
        Equivalent with giving the ``-phys`` parameter and thus mutually
        exclusive

.. lsctl:cmd:: detach | -phys -net
.. lsctl:cmd:: detach | -phys netlist
.. lsctl:cmd:: detach | -net physlist

    Detaches the virtual networks from physical machines. See `attach` for
    syntax of the command.

.. lsctl:cmd:: rule | direction prio action -srcIp -dstIp -srcMac -dstMac

    Add a new firewall rule for a given virt. The rule applies if all the
    matches specified by the arguments are satisfied.

    **C API equivalents:** :c:func:`lsdn_vr_new` and other functions (see :ref:`capi/rules`)


    :param direction direction: Direction of the packets.
    :param int prio:
        Priority of the rule. Rules with lower numbers are matched first.
    :param string action:
        Currently only drop action is supported.
    :param subNet srcIp:
        Match if the source IP address of the packet is in the given subnet.
    :param subNet dstIp:
        Match if the destination IP address of the packet is in the given subnet.
    :param mac srcMac:
        Match if the source MAC address of the packet is equal to the given one.
    :param mac dstMac:
        Match if the source MAC address of the packet is equal to the given one.

    :scope virt: Only allowed in a virt scope.

.. lsctl:cmd:: flushVr |

    Remove all virt firewall rules defined by `rule` previously.

    :scope virt: Only allowed in a virt scope.

.. lsctl:cmd:: rate | direction -avg -burst -burstRate

    Limit bandwidth in a given direction. If no arguments are given, all limits
    are lifted.

    **C API equivalents:** :c:func:`lsdn_virt_set_rate_in`, :c:func:`lsdn_virt_set_rate_out`, :c:func:`lsdn_virt_clear_rate_in`, :c:func:`lsdn_virt_clear_rate_out`.

    :param direction direction: Direction to limit.
    :param speed avg: Average speed allowed.
    :param speed burstRate: Higher speed allowed during short bursts.
    :param size burst: Size of the burst during which higher speeds are allowed.
    :scope virt: Only allowed in a virt scope.

.. lsctl:cmd:: claimLocal | -phys

    Inform LSDN that it is running on this physical machine.

    You might want to distribute the same configuration file to all physical
    machines, just with different physical machines claimed as local. You can
    use the following command to allow the control of the local phys using the 
    first command-line argument to the script: ::

        claimLocal [lindex $argv 0]

    After that, invoke :ref:`lsctl <prog_lsctl>` like this:

    .. code-block:: none

        lsctl <your script> <local phys>

    **C API equivalents:** :c:func:`lsdn_phys_claim_local`.

    :param string phys: The phys to mark as local.
    :scope none: This directive can appear at root level.
    :scope phys: Equivalent to specifying the ``-phys`` parameter.


.. |sname_docs| replace::
    Optional, creates a non-default named setting. Use the `net` ``-setting``
    argument to select.
.. lsctl:cmd:: settings | type

    Set a network overlay type for newly defined networks. Use one of the
    concrete overloads below.

.. lsctl:cmd:: settings direct | -name

    Do not use any network separation.

    See :ref:`ovl_direct` for more details.

    :param string name: |sname_docs|
    :scope none: This directive can only appear at root level.

.. lsctl:cmd:: settings vlan | -name

    Use VLAN tagging to separate networks.

    See :ref:`ovl_vlan` for more details.

    :param string name: |sname_docs|
    :scope none: This directive can only appear at root level.

.. lsctl:cmd:: settings vxlan/mcast | -name -mcastIp -port

    Use VXLAN tunnelling with automatic setup using multicast.

    See :ref:`ovl_vxlan_mcast` VXLAN for more details.

    :param string name: |sname_docs|
    :param ip mcastIp:
        Mandatory, the IP address used for VXLAN broadcast communication. Must
        be a valid multicast IP address.
    :param int port:
        Optional, the UDP port used for VXLAN communication.
    :scope none: This directive can only appear at root level.


.. lsctl:cmd:: settings vxlan/e2e | -name -port

    Use VXLAN tunnelling with endpoint-to-endpoint communication and MAC
    learning.

    See :ref:`ovl_vxlan_e2e` VXLAN for more details.

    :param string name: |sname_docs|
    :param int port:
        Optional, the UDP port used for VXLAN communication.
    :scope none: This directive can only appear at root level.

.. lsctl:cmd:: settings vxlan/static | -name -port

    Use VXLAN tunnelling with fully static setup.

    See :ref:`ovl_vxlan_static` VXLAN for more details.

    :param string name: |sname_docs|
    :param int port:
        Optional, the UDP port used for VXLAN communication.
    :scope none: This directive can only appear at root level.

.. lsctl:cmd:: settings geneve | -name -port

    Use Geneve tunnelling with fully static setup.

    See :ref:`ovl_geneve` for more details.

    :param string name: |sname_docs|
    :param int port:
        Optional, the UDP port used for Geneve communication.
    :scope none: This directive can only appear at root level.

.. lsctl:cmd:: commit |

    Apply all changes done so far. This will usually be located at the end of
    each LSCTL script.

    If the validation or commit fails, the errors will be printed to stderr and
    the directive will end with an error. The script will be terminated.

    **C API equivalents:** :c:func:`lsdn_commit`

    :scope none: This directive can only appear at root level.

.. lsctl:cmd:: validate |

    Check the network model for errors.

    If the validation fails, the errors will be printed to stderr and
    the directive will end with an error. The script will be terminated.

    **C API equivalents:** :c:func:`lsdn_validate`

    :scope none: This directive can only appear at root level.

.. lsctl:cmd:: cleanup |

    Revert all changes done so far.

    If the cleanup fails, the errors will be printed to stderr and
    the directive will end with an error. The script will be terminated.

    **C API equivalents:** :c:func:`lsdn_context_cleanup`

    :scope none: This directive can only appear at root level.

.. lsctl:cmd:: show | -tcl -json

    Show the network model so far. Shows even changes that are not yet commited.

    :param tcl: Dump the network model in LSCTL format.
    :param json: Dump the network model in JSON format.

    **C API equivalents:** :c:func:`lsdn_dump_context_tcl`,
    :c:func:`lsdn_dump_context_json`.

    :scope none: This directive can only appear at root level.

.. lsctl:cmd:: free |

    Free all the resources used by LSDN, but do not revert the changes. This is
    useful for memory leak debugging (Valgrind etc.).

    **C API equivalents:** :c:func:`lsdn_context_free`

    :scope none: This directive can only appear at root level.

------------------
Command-line tools
------------------

.. default-role:: ref

The LSCTL configuration language is accepted by the command-line tools
`lsctl <prog_lsctl>` and `lsctld <prog_lsctld>`. The one you should choose
depends on your use-case. `lsctl <prog_lsctl>` is used for simple run-and-forget
configuration, while `lsctld <prog_lsctld>` runs in the background and supports
virtual machine migration and other types of network evolution.

.. _prog_lsctl:

Using lsctl
-----------

Run ``lsctl`` with the name of your configuration script like this:

.. code-block:: bash

    lsctl my_configuration.lsctl

You can also pass additional arguments to lsctl, which will be all available in
the ``$argv`` variable. See :lsctl:cmd:`claimLocal` for an example use.

If you run ``lsctl`` without arguments, you will receive an interactive shell,
where you can enter directives one after another.

.. _prog_lsctld:

Using lsctld and lsctlc
-----------------------

If you want to migrate machines, you have to keep a ``lsctld`` daemon running in
the background, so that it can remember the current state of the network and
make changes appropriately. You can send new configuration directives to the
daemon using the ``lsctlc`` command.

First, let's decide on the location of the control socket for ``lsctld``.
``lsctld`` uses a regular Unix socket that can be located anywhere on the
file-system, so let's use ``/var/run/lsdn``:

.. code-block:: bash

    lsctld -s /var/run/lsdn

Afterwards, commands can be sent to ``lsctld`` using ``lsctlc``. Either pass
them on standard input:

.. code-block:: bash

    cat my_configuration.lsctl | lsctlc /var/run/lsdn

Or directly on the command-line:

.. code-block:: bash

    lsctlc /var/run/lsdn virt -name vm1 -phys b -net customer
    lsctlc /var/run/lsdn commit

``lsctld`` can be controlled with the following options:

.. program:: lsctld
.. option:: --socket, -s

    Specify the location of the Unix control socket (mandatory).

.. option:: --pidfile, -p

    Specify the location of the PID file. ``lsctld`` will use the PID file to
    prevent multiple instances from running and it can be used for daemon
    management.

    If the option is not specified, no PID file will be created.

.. option:: -f

    Run in foreground, do not daemonize.

TCL extension (tclsh)
----------------------

Instead of using the `lsctl <prog_lsctl>` command-line tool, you can use TCL
directly and load LSDN as an extension. This will allow you to combine LSDN with
larger TCL programs and run it using ``tclsh``. This can be done using the
regular TCL means: ::

    package require lsdn
    namespace import lsdn::*

    net test { ... }
