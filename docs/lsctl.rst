.. highlight:: tcl

==========================
Lsctl Configuration Files
==========================

LSDN has its own configuration language for describing the network topology. If
you have seen the `quickstart` or `netmodel` sections, you have already seen
examples of the configuration files.

-------
Syntax
-------

The configuration syntax is actually based on the TCL_ language -- but you do
not have to be afraid, this guide is self contained. You might not even guess
TCL is there if we did not tell you. However, it is good to known TCL is there
if you need more advanced stuff, like variable substitution or loops. And if you
know TCL, you will recognize some of the conventions and feel at home.

.. _TCL: https://www.tcl.tk/

The only downside is that you have to include a short boilerplate at top of each
configuration file, to tell TCL you don't want to prefix everything with
``lsdn::``.  Start your configuration file with this line: ::

    namespace import lsdn::*

The configuration file itself is a list of directives that tell LSDN what
objects the network consists of, how they are configured and how they connect.
The directive are terminated by a newline, but other whitespace is not
significant. All available directives are listed in section `dirref`.

All lsdn directives follow the same basic patterns. They start with the
directive name (for example ``net`` or ``settings``) and are folowed with
arguments for that directive. The directive and their arguments are separated by
whitespace. Most directives make use of named arguments (or as they are called in some
languages "keyword arguments"): ::

    directiveWithNamedArgs -opt1 value1 -opt1 value2

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
as the names do not have any direct effect on the network.

Please note that forward references are not allowed, because LSCTL in its core
an imperative language. For example this snippet is illegal: ::

    virt -net test
    net -vid 1 test

The order must be swapped like this: ::

    virt -net test
    net -vid 1 test

--------
Nesting
--------

Some directives may also contain other directives. In that case, the nested
directives are enclosed in curly-braces ``{}`` block following the parent
directive, like this [#f1]_: ::

    phys -name p
    net 42 {
        virt -name a -phys p
        attach p
    }

Nested directives are used to simplify definition of related elements. The example
above specifies a virtual machine *a* connected to network *42*. The connection
is implied from the nesting. The nesting is quiete flexible and you can decide
to use a style that most suites you and that reflects organization of your
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

Note that there is no need for lsctl:cmd:`attach` in the last example, since
nesting took care of it for us.

In general nesting can be used where you would otherwise have to specify a
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

.. [#f1] If you are familiar with TCL, you will recognize this how TCL
    control-flow commands work.

---------------
Argument types
---------------

.. lsctl:type:: int

    An integer number, given as string of digits prefixed with optional sign.
    LSCTL recognizes the ``0x`` prefix for hexadecimal and ``0`` for octal
    integers.

.. lsctl:type:: string

    String arguments in LSCTL are given the same way as in shell - they don't
    need to be quoted. Mostly they are used for names, so there is no need to
    give string argument containing spaces.

    if you want to give a directive an argument containing space, newline or
    curly brackets, simply enclose the argument in double-quotes.  If you want
    the argument to contain double-quotes, backslash or dollar sign, precede the
    character with backslash: ::

        virt -name "really\$bad\\idea
        on so many levels"

    If you need the full syntax definition, refer to ``man tcl.n``
    on your system.

.. lsctl:type:: direction

    Either ``in`` or ``out``. ``in`` is for packets entering the
    virtual machine ``out`` is for packets leaving the virtual machine.

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

    Examples:
.. code-block:: none

    2a00:1028:8380:f86::2
    2a00:1028:8380:f86::0/64
    192.168.56.0/24

.. lsctl:type:: mac

    MAC address in octal format. Both addresses with colons and wihtout colons
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

    Supported units are the same as for :lsctl:type:``size``.

.. _dirref:

-------------------
Directive reference
-------------------

.. default-role:: lsctl:cmd

.. lsctl:cmd:: net | name -vid -settings -phys { ... }

    Define new virtual network or change an existing one.

    C API equivalents: :c:func:`lsdn_net_new`, :c:func:`lsdn_net_by_name`.

    :param int vid:
        Virtual network identifier. Network technologies like VXLANs or VLANs
        use these number to separate different networks. The ID must be unique
        among all networks. The parameter is forbidden if network already
        exists.
    :param string name:
        Name of the network. Does not change network behavior, only used by the
        configuration to refer to the network. However, if the ``-vid`` argument
        is not specified, this ``name`` argument will also specify the ``vid``.
    :param string phys:
        Optional name of a `phys` you want to attach to this network.  Shorthand
        for using the `attach` directive. Can not be used when nested inside
        `phys` directive.
    :param string settings:
        Optional name of a previously defined `settings`, specifing the network
        overlay type (VLAN, VXLAN etc.). If not given, the ``default`` settings
        will be used. Settings of existing net can not be changed.
    :scope none:
        This directive can appear at root level.
    :scope phys:
        Automatically attaches the parent phys to this network. Shorthand for
        using the `attach` directive.

.. lsctl:cmd:: phys | -name -if -ip -net

    Define a new physical machine or change an existing one.

    C API equivalents: :c:func:`lsdn_phys_new`, :c:func:`lsdn_phys_by_name`.

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
        Optional, name of a `net` you want thys phys to attach to. Shorthand for
        using the `attach` directive. can not be used when nested inside `net`
        directive.
    :scope none:
        This directive can appear at root level.
    :scope net:
        Automatically attaches this phys to the parent network. Shorthand for
        using the `attach` directive

.. lsctl:cmd:: virt | -net -name -mac -phys -if

    Define a new virtual machine or change an existing one.

    C API equivalents: :c:func:`lsdn_virt_new`, :c:func:`lsdn_virt_by_name`.

    :param string net:
        The virtual network this virt should be part of. Mandatory if creating
        new virt, forbidden if changing an existing one. Forbidden if nested
        inside `net`.
    :param string name:
        Optional, name of the virtual machine. Does not change network behavior,
        only used byt eh confiruation to refer to this virt.
    :param mac mac:
        Optional, MAC address used by the virtual machine.
    :param string phys:
        Optional, connect (or migrate, if already connected) at a given `phys`.
    :param string if:
        Set the network interface used by the virtual machine to connect at the
        phys. Mandatory, if ``-phys`` argument was used.
    :scope none:
        This directive can appear at root level.
    :scope net:
        Equivalent with giving the ``-net`` parameter and thus mutually
        exclusive.
    :scope phys:
        Equivalent with giving the ``-phys`` parameter and thus mutually
        exclusive

.. lsctl:cmd:: rule | direction prio action -srcIp -dstIp -srcMac -dstMac

    Add a new firewall rule for a given virt. The rule applies if all the
    matches specified by the arguments are satisfied.

    C API equivalents: 

    .. todo:: Fill in once the respective section is completed.


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

    Limit bandwith flowing in a given direction. If no arguments are given, all
    limits are lifted.

    C API equivalents:
    .. todo:: Link to the attributes once documented.

    :param direction direction: Direction to limit.
    :param speed avg: Average allowed speed.
    :param speed burstRate: Higher speed allowed during short bursts.
    :param size burst: Size of the burst during which higher speeds are allowed.
    :scope virt: Only allowed in a virt scope.

.. lsctl:cmd:: claimLocal | -phys

    Inform LSDN that lsdn is running on this physical machine.

    You might want to distribute the same configuration to all physical
    machines, just with different physical machines claimed as local. You can
    use the following command to allow the control of the local phys using the 
    first commandline argument to the script: ::

        claimLocal [lindex $argv 1]

    After that, invoke :ref:`lsctl <prog_lsctl>` like this:

    .. code-block:: none

        lsctl <your script> <local phys>

    C API equivalents: :c:func:`lsdn_phys_claim_local`.

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
    :scope none: This directive can only appear at root level.

    :param string name: |sname_docs|

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

    See :ref:`<ovl_vxlan_static>` VXLAN for more details.

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

    Apply all changes done so far. This will usually be at the end of each LSCTL
    script.

    C API equivalents: :c:func:`lsdn_commit`

    :scope none: This directive can only appear at root level.

.. lsctl:cmd:: validate |

    Check the changes done so far for errors.

    C API equivalents: :c:func:`lsdn_validate`

    :scope none: This directive can only appear at root level.

.. lsctl:cmd:: cleanup |

    Revert all changes done so far.

    C API equivalents: :c:func:`lsdn_context_cleanup`

    :scope none: This directive can only appear at root level.

.. lsctl:cmd:: free |

    Free all the resources used by LSDN, but do not revert the changes. This is
    useful for memory leak debugging (Valgrind etc.).

    C API equivalents: :c:func:`lsdn_context_free`

    :scope none: This directive can only appear at root level.

------------------
Command-line tools
------------------

.. default-role:: ref

The LSCTL configuration language is accepted by the command-line tools:
`lsctl <prog_lsctl>` and `lsctld <prog_lsctld>`. The one you should choose
depends on your use-case. `lsctl <prog_lsctl>` is used for simple run-and-forget
configuration, while `lsctld <prog_lsctld>` runs in the background and supports
virtual machine migration and other types of network evolution.

.. _prog_lsctl:

Using lsctl
-----------

Run ``lsctl`` with the name of you configuration script like this:

.. code-block:: bash

    lsctl my_configuration.lsctl

You can also pass additional arguments to lsctl, which will be all available in
the ``$argv`` variable. See :lsctl:cmd:`claimLocal` for usage example.

If you run ``lsctl`` without arguments, you will receive an interactive shell,
where you can enter direcives one after another.

.. _prog_lsctld:

Using lsctld and lsctlc
-----------------------

If you want to use migrations, you have to keep a ``lsctld`` daemon running in the
background, so that it can remember the current state of the network and make
changes appropriately. You can send new configuration directives to the daemon
using the ``lsctlc`` command.

First, let's decide on the location of the control socket for ``lsctld``.
``lsctld`` uses a regular Unix socket that can be located anywhere on the
file-system, so let's use ``/var/run/lsdn``:

.. code-block:: bash

    lsctld -s /var/run/lsdn

After that, commands can be send to ``lsctld`` using ``lsctlc``. Either pass
them on standard input:

.. code-block:: bash

    cat my_configuration.lsctl | lsctlc /var/run/lsdn

Or directly on the command-line:

.. code-block:: bash

    lsctlc /var/run/lsdn virt vm1 -phys b
    lsctlc /var/run/lsdn commit

``lsctld`` can be controled with the following options:

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

Instead of using the `lsctl <prog_lsctl>` command-line tool, you can use TCL directly
and load LSDN as an extension. This will allow you to combine LSDN with larger
TCL programs and run it using ``tclsh``. This can be done using the regular TCL
means: ::

    package require lsdn
    namespace import lsdn::*

    net test { ... }
