#!/bin/bash

if [ $# -ne 5 ] ; then
    echo "Usage: $0 <src_vm> <dst_vm> <vni> <local_vxlan_IP_addr> <rem>"
    exit 1
fi

src_vm="$1"
dst_vm="$2"
vni="$3"
loc="$4"
rem="$5"

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
ip netns exec "${dst_vm}" ip link add "${vxlan}" type vxlan id "${vni}" dstport 4789 dev veth0
ip netns exec "${dst_vm}" ip link set "${vxlan}" master "${br}"
ip netns exec "${dst_vm}" bridge fdb append to ff:ff:ff:ff:ff:ff dst "${rem}" dev "${vxlan}"
ip netns exec "${dst_vm}" ip addr add "${loc}" dev "${vxlan}"
ip netns exec "${dst_vm}" ip link set "${vxlan}" up
