#!/bin/bash

function create_bridge(){
	ip link add name "$1" type bridge
}

function create_veth_pair(){
	ip link add dev "$1" type veth peer name "$2"
}

function add_to_bridge(){
	ip link set "$1" master "$2"
}

function set_up_if(){
	ip link set dev "$1" up
}

function prepare(){
	create_bridge switch

	set_up_if switch

	create_veth_pair vetha vetha_switch
	create_veth_pair vethb vethb_switch
	create_veth_pair vethc vethc_switch

	add_to_bridge vetha_switch switch
	add_to_bridge vethb_switch switch
	add_to_bridge vethc_switch switch

	ip addr replace 172.16.0.1/24 dev vetha
	ip addr replace 172.16.0.2/24 dev vethb
	ip addr replace 172.16.0.3/24 dev vethc

	set_up_if vetha
	set_up_if vethb
	set_up_if vethc

	set_up_if vetha_switch
	set_up_if vethb_switch
	set_up_if vethc_switch
}

function connect(){
    ./dhcp_in_vms
}

function test(){
	# TODO automate
	echo "$ virsh console CLIENT-B-1"
	echo "$ ping 192.168.99.1"
	echo "$ ping 192.168.99.2"
	echo "$ ping 192.168.99.3"
	echo "$ ping 192.168.99.4"
	echo "$ ping 192.168.99.5"
	# TODO cleanup the interfaces
	echo "Cleanup the interfaces: ..."
}

prepare
connect
test
