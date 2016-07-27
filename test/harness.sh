#!/bin/bash

myexec=$(realpath $0)
cd $(dirname $myexec)
source common.sh

function setUp() {
	ip netns add $NETNS
	for endpoint in $ENDPOINTS; do
		create_endpoint $endpoint
	done
}

function tearDown() {
	for endpoint in $ENDPOINTS; do
		delete_endpoint $endpoint
	done
	ip netns delete $NETNS
}

function create_endpoint() {
	NSNAME="${NETNS}-sub$1"
	SUB_NS="IN_NS $1"

	# create pair of veths
	ip link add "${VETH_PREFIX}$1" type veth peer name "${VETH_PREFIX}peer$1"
	# push main to test namespace
	ip link set "${VETH_PREFIX}$1" netns "$NETNS"
	# enable
	$MAIN_NS ip link set "${VETH_PREFIX}$1" up
	# push peer to new namespace
	ip netns add "$NSNAME"
	ip link set "${VETH_PREFIX}peer$1" netns "$NSNAME"
	# within new namespace:
	# rename to default veth0
	$SUB_NS ip link set "${VETH_PREFIX}peer$1" name veth0
	# set MAC
	$SUB_NS ip link set veth0 address "${MAC_PREFIX}$1"
	# set IP level config
	$SUB_NS ip addr add "${IPV4_PREFIX}$1/24" dev veth0
	$SUB_NS ip link set veth0 up
}

function delete_endpoint() {
	SUB_NS="IN_NS $1"
	$SUB_NS ip link delete veth0
#	$MAIN_NS ip link delete "${VETH_PREFIX}$1"
	ip netns delete "$NSNAME"
}

case "$1" in
	"setup")
		setUp
		;;
	"teardown")
		tearDown
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
	"run-test")
		setUp
		shift
		name=$1
		shift
		ip netns exec $NETNS test_$name.sh "$@"
		tearDown
		;;
	*)
		echo "Usage: $0 setup | teardown | add <name> | delete <name> | list"
		;;
esac
