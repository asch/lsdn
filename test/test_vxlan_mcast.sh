function prepare(){
	mk_testnet net
	mk_phys net a ip 172.16.0.1/24
	mk_phys net b ip 172.16.0.2/24

	mk_virt a 1 ip 192.168.99.1/24
	mk_virt a 2 ip 192.168.99.2/24
	mk_virt a 3 ip 192.168.99.3/24
	mk_virt b 1 ip 192.168.99.4/24
	mk_virt b 2 ip 192.168.99.5/24
	mk_bridge net switch a b
}

function connect(){
	in_phys a $lsctl -d localPhys=a $config
	in_phys b $lsctl -d localPhys=b $config
}

function test(){
	pass in_virt a 1 $qping 192.168.99.2
	pass in_virt a 1 $qping 192.168.99.4
	fail in_virt a 1 $qping 192.168.99.3
	fail in_virt a 1 $qping 192.168.99.5
}
