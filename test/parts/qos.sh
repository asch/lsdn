NETCONF="firewall"
PHYS_LIST="a"

function prepare(){
	mk_testnet net
	mk_phys net a ip 172.16.0.1/24

	mk_virt a 1 ip 192.168.99.1/24 mac 00:00:00:00:00:a1
	mk_virt a 2 ip 192.168.99.2/24 mac 00:00:00:00:00:a2
}

function test() {
	pass in_virt a 1 $qping 192.168.99.2
	# There is no actual speed test yet, we just check that things connect
}


function connect(){
	lsctl_in_all_phys parts/qos.lsctl
}
