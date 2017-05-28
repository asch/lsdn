#!/bin/bash

NSPREFIX=lsdn
IFPREFIX=lsdn

mk_netns(){
	ip netns add "${NSPREFIX}-$1"
}

in_ns(){
	local ns="$1"
	shift
	ip netns exec "${NSPREFIX}-$ns" "$@"
}

mk_veth_pair(){
	local ns1="$1"
	local name1="$2"
	local ns2="$3"
	local name2="$4"

	ip link add "${IFPREFIX}-tmp1" type veth peer name "${IFPREFIX}-tmp2"
	# move one side to the namespace
	ip link set "${IFPREFIX}-tmp1" netns "${NSPREFIX}-$ns1"
	# and rename to the final name
	in_ns "$ns1" ip link set "${IFPREFIX}-tmp1" name "$name1"

	# and the second side ...
	ip link set "${IFPREFIX}-tmp2" netns "${NSPREFIX}-$ns2"
	in_ns "$ns2" ip link set "${IFPREFIX}-tmp2" name "$name2"
}

mk_phys(){
	local net="$1"
	local phys="$2"
	shift 2

	mk_netns "$phys"
	mk_veth_pair "$phys" out "$net" "$phys"
	set_ifattr "$phys" out "$@"
}
in_phys(){
	local ns="$1"
	shift
	in_ns "$ns" "$@"
}

mk_virt(){
	local phys="$1"
	local virt="$2"
	shift 2

	mk_netns "$phys-$virt"
	mk_veth_pair "$phys" "$virt" "$phys-$virt" out
	set_ifattr "$phys-$virt" out "$@"
	in_ns "$phys" ip link set dev "$virt" up
}

mk_bridge(){
	local ns="$1"
	local if="$2"
	shift 2

	in_ns "$ns" ip link add "$if" type bridge
	in_ns "$ns" ip link set dev "$if" up
	for p in $@; do
		in_ns "$ns" ip link set dev "$p" master "$if"
		in_ns "$ns" ip link set dev "$p" up
	done
}

in_virt(){
	local ns="$1-$2"
	shift 2
	in_ns "$ns" "$@"
}

mk_testnet(){
	mk_netns "$1"
}

set_ifattr(){
	local ns="$1"
	local if="$2"
	shift 2

	while [ "$#" -ne 0 ]; do
		local key="$1"
		shift
		case "$key" in
			ip)
				in_ns "$ns" ip addr replace "$1" dev "$if"
				shift
				;;
			*)
				echo "Unknown attr key $key" >&2
				return 1
				;;
		esac
	done
	in_ns "$ns" ip link set "$if" up
}

cleanup() {
	for dev in /sys/class/net/${IFPREFIX}-*; do
		dev="$(basename "${dev}")"
		if [ "$dev" == "${IFPREFIX}-*" ]; then
			break
		fi
		echo Deleting netdev "${dev}"
		ip link delete "${dev}"
	done

	for ns in /var/run/netns/${NSPREFIX}-*; do
		ns="$(basename "${ns}")"
		if [ "$ns" == "${NSPREFIX}-*" ]; then
			break
		fi
		echo Deleting namespace "${ns}"
		ip netns delete "${ns}"
	done
}

qping="ping -c 2 -i 0.2 -w 1"

fail() {
	if trace_command "$@"; then
		test_error
	fi
}

pass() {
	if ! trace_command "$@"; then
		test_error
	fi
}

trace_command() {
	echo "> $@"
	"$@"
	return "$?"
}

test_error() {
	echo "^^^ TEST FAILED ^^^"
	$cleanup_handler
	exit 2
}
