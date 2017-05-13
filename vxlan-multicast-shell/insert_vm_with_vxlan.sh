#!/bin/bash

if [ $# -ne 3 ] ; then
    echo "Usage: $0 <src_vm> <dst_vm> <vni>"
    exit 1
fi

src_vm="$1"
dst_vm="$2"
vni="$3"

br="br_${src_vm}"
vxlan="vxlan_${src_vm}"

# Move src interface into dst namespace
ip link set "${src_vm}" netns "${dst_vm}"
ip netns exec "${dst_vm}" ip link set "${src_vm}" up

# Create bridge
ip netns exec "${dst_vm}" ip link add "${br}" type bridge
ip netns exec "${dst_vm}" ip link set "${br}" up
ip netns exec "${dst_vm}" ip link set "${src_vm}" master "${br}"

# Create vxlan
ip netns exec "${dst_vm}" ip link add "${vxlan}" type vxlan id "${vni}" group 239.1.1.10 dstport 4789 dev veth0
ip netns exec "${dst_vm}" ip link set "${vxlan}" master "${br}"
ip netns exec "${dst_vm}" ip link set "${vxlan}" up
