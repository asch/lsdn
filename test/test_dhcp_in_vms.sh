# TODO add to lib/common.sh
function create_bridge(){
	ip link add name "$1" type bridge
}

# TODO add to lib/common.sh
function create_veth_pair(){
	ip link add dev "$1" type veth peer name "$2"
}

# TODO add to lib/common.sh
function add_to_bridge(){
	ip link set "$1" master "$2"
}

function prepare(){
	mk_phys net a 
	mk_phys net b ip 172.16.0.2/24
	mk_phys net c ip 172.16.0.3/24

	create_bridge switch
	create_bridge bra
	create_bridge brb
	create_bridge brc

	create_veth_pair vetha_bra vetha_switch
	create_veth_pair vethb_brb vethb_switch
	create_veth_pair vethc_brc vethc_switch

	add_to_bridge vetha_bra bra
	add_to_bridge vetha_switch switch

	add_to_bridge vethb_brb brb
	add_to_bridge vethb_switch switch

	add_to_bridge vethc_brc brc
	add_to_bridge vethc_switch switch

	#mk_virt a 1 ip 192.168.99.1/24
	#mk_virt b 1
	#mk_virt b 2
	#mk_virt b 3
	#mk_virt c 1
	#mk_virt c 2

	#set_up_if a-1 lo
	#set_up_if b-1 lo
	#set_up_if b-2 lo
	#set_up_if b-3 lo
	#set_up_if c-1 lo
	#set_up_if c-2 lo
}

function connect(){
	# TODO run test
	#$test
}

function test(){
	# test manually
	# $ virsh console CLIENT-B-1
	# $ ping 192.168.99.1
	# $ ping 192.168.99.2
	# $ ping 192.168.99.3
	# $ ping 192.168.99.4

	# TODO automate
	#pass in_vm b 1 $qping 192.168.99.1
	#pass in_vm b 1 $qping 192.168.99.2
	#pass in_vm b 1 $qping 192.168.99.3
	#pass in_vm b 1 $qping 192.168.99.4
	#fail in_vm b 1 $qping 192.168.99.5
	#fail in_vm b 3 $qping 192.168.99.2
}
