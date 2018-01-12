NETCONF="firewall"
PHYS_LIST="a"

function prepare(){
	mk_testnet net
	mk_phys net a ip 172.16.0.1/24

	mk_virt a 1 ip 192.168.99.1/24 mac 00:00:00:00:00:a1
	in_virt a 1 ip addr add dev out-1 192.168.98.1/24

	mk_virt a 2 ip 192.168.99.2/24 mac 00:00:00:00:00:a2
	in_virt a 2 ip addr add dev out-1 192.168.99.3/24
	in_virt a 2 ip addr add dev out-1 192.168.99.4/24
	in_virt a 2 ip addr add dev out-1 192.168.98.2/24
}

function test() {
	fail in_virt a 1 $qping 192.168.99.2
	fail in_virt a 1 $qping 192.168.99.3
	pass in_virt a 1 $qping 192.168.99.4
	fail in_virt a 1 $qping 192.168.98.2
}


function connect(){
	lsctl_in_all_phys parts/firewall.lsctl
}
