#!/bin/bash

if [ $# -ne 2 ] ; then
    echo "Usage: $0 <vm> <ip_addr>"
    exit 1
fi

vm="$1"
ip_addr="$2"

vm_peer="${vm}-peer"
ip netns add "${vm}"
ip link add "${vm}" type veth peer name "${vm_peer}"
ip addr add "${ip_addr}" dev "${vm}"
ip link set "${vm}" up

ip link set "${vm_peer}" netns "${vm}"
ip netns exec "${vm}" ip link set "${vm_peer}" name veth0
ip netns exec "${vm}" ip addr add "${ip_addr}" dev veth0
ip netns exec "${vm}" ip link set veth0 up
