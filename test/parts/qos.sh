NETCONF="firewall"
PHYS_LIST="a"

function prepare(){
	mk_testnet net
	mk_phys net a ip 172.16.0.1/24

	mk_virt a 1 ip 192.168.99.1/24 mac 00:00:00:00:00:a1
	mk_virt a 2 ip 192.168.99.2/24 mac 00:00:00:00:00:a2
	mk_virt a 3 ip 192.168.99.3/24 mac 00:00:00:00:00:a3
	mk_virt a 4 ip 192.168.99.4/24 mac 00:00:00:00:00:a4
}

function test() {
	pass in_virt a 1 $qping 192.168.99.2
	# This is a very naive QoS test
	# If we limit the bandwidth a lot, then even ping fails ...
	fail in_virt a 3 $qping 192.168.99.4
}


function connect(){
	lsctl_in_all_phys parts/qos.lsctl
}
