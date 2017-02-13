#!/bin/bash

if [ $# -ne 1 ] ; then
    echo "Usage: $0 <dummy_interface>"
    exit 1
fi

dummy="$1"
nsa="dummy_nsa"

function redir(){
    tc filter add dev "$1" parent 1: protocol all u32 \
    match u32 "0x$200" 0xffffffff at 32 \
    match ip dport 4789 0xffff \
    action mirred egress mirror dev "$3"
}

tc qdisc add dev "${dummy}" root handle 1: prio

ip link add name "${nsa}" type dummy
ip link set "${nsa}" up

redir "${dummy}" "000001" "${nsa}"
#redir "${dummy}" "000002" "${nsa}"
