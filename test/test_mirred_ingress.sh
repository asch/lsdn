#!/bin/bash

ip link add name ltif3 type veth peer name ltif2
ip link add name ltif1 type dummy

# Now we will connect ltif1 and ltif2 using TC rules. ltif2 ingress will be forwarded
# to ltif1 ingress and ltif1 egress to ltif2 egress.

./test_mirred_ingress

ip netns add a
ip link set ltif3 netns a
ip netns exec a ip addr add 192.168.59.1/24 dev ltif3
ip netns exec a ip link set up dev ltif3

ip addr add 192.168.59.2/24 dev ltif1
ip link set up dev ltif1
# ARP is disabled by default on dummy interfaces, probably because
# they were intended only to provide fake routes
ip link set dev ltif1 arp on
ip link set up dev ltif2

ip netns exec a ping 192.168.59.2
