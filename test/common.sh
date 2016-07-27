#!/bin/bash

NETNS="lsdn-test"
VETH_PREFIX="ltif"
MAC_PREFIX="00:00:00:00:00:"
IPV4_PREFIX="192.168.59."

ENDPOINTS="1 2 3 4 5 6 7"

MAIN_NS="ip netns exec $NETNS"

function IN_NS() {
	id=$1
	shift
	NSNAME="${NETNS}-sub$id"
	ip netns exec $NSNAME "$@"
}

function ping_from() {
	src=$1
	dst=$2
	IN_NS $src ping -c 1 -w 1 $IPV4_PREFIX$dst
}
