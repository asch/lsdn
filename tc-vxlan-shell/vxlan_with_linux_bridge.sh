#!/bin/bash

if [ $# -ne 1 ] ; then
    echo "Usage: $0 <dummy_interface>"
    exit 1
fi

dummy="$1"

ip link add name "${dummy}" type dummy
ip link set "${dummy}" up

function create_tunnel(){
    ip link add name "bridge$1" type bridge
    ip link set "bridge$1" up
    ip link add "vxlan$1" type vxlan id $2 group 239.1.1.$3 dstport 4789 dev "${dummy}"
    ip link set "vxlan$1" up
    ip link set "vxlan$1" master "bridge$1"
}

vni=1
mcastgroup=1

for name in a b ; do
    create_tunnel "${name}" "${vni}" "${mcastgroup}"
    ((vni++))
    ((mcastgroup++))
done
