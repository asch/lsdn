#!/bin/bash

NETNS="lsdn-test"
VETH_PREFIX="ltif-"
MAC_PREFIX="00:00:00:00:00:"
IPV4_PREFIX="192.168.59."

ENDPOINTS="a b c d e f g"
ENDPOINT_CTR=1

MAIN_NS="ip netns exec $NETNS"

function mynamespace() {
	nsinode=$(readlink /proc/$$/ns/net)
	for ns in /var/run/netns/*
	do
		ino=$(stat -c "net:[%i]" $ns)
		if [ "$ino" == "$nsinode" ]; then
			basename "$ns"
			return 0
		fi
	done
	echo "<DEFAULT>"
}

function setUp() {
#	ip netns add $NETNS
	for endpoint in $ENDPOINTS; do
		create_endpoint $endpoint
	done
}

function tearDown() {
	for endpoint in $ENDPOINTS; do
		delete_endpoint $endpoint
	done
#	ip netns delete $NETNS
}

function count_endpoints() {
	count=$(ls -1 /var/run/netns/${NETNS}-sub-* | wc -l)
	(( ENDPOINT_CTR = count + 1 ))
	echo $ENDPOINT_CTR
}

function create_endpoint() {
	NSNAME="${NETNS}-sub-$1"
	SUB_NS="ip netns exec $NSNAME"

	num=$(count_endpoints)

	# create pair of veths
	ip link add "${VETH_PREFIX}$1" type veth peer name "${VETH_PREFIX}peer-$1"
	# push main to test namespace
	ip link set "${VETH_PREFIX}$1" netns "$NETNS"
	# enable
	$MAIN_NS ip link set "${VETH_PREFIX}$1" up
	# push peer to new namespace
	ip netns add "$NSNAME"
	ip link set "${VETH_PREFIX}peer-$1" netns "$NSNAME"
	# within new namespace:
	# rename to default veth0
	$SUB_NS ip link set "${VETH_PREFIX}peer-$1" name veth0
	# set MAC
	$SUB_NS ip link set veth0 address "${MAC_PREFIX}${num}"
	# set IP level config
	$SUB_NS ifconfig veth0 up "${IPV4_PREFIX}${num}"
}

function delete_endpoint() {
	NSNAME="${NETNS}-sub-$1"
	SUB_NS="ip netns exec $NSNAME"
	$SUB_NS ip link delete veth0
	$MAIN_NS ip link delete "${VETH_PREFIX}$1"
	ip netns delete "$NSNAME"
}

case "$1" in
	"setup")
		ip netns add $NETNS
		setUp
		;;
	"teardown")
		tearDown
		ip netns delete $NETNS
		;;
	"add")
		create_endpoint "$2"
		;;
	"delete")
		delete_endpoint "$2"
		;;
	"list")
		ls -1 /var/run/netns/${NETNS}-sub-*
		;;
	*)
		echo "Usage: $0 setup | teardown | add <name> | delete <name> | list"
		;;
esac
