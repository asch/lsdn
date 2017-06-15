function prepare(){
	mk_testnet net
	mk_phys net a
	mk_phys net b
	mk_phys net c

	mk_virt a 1 ip 192.168.99.1/24
	mk_virt a 2 ip 192.168.99.2/24
	mk_virt a 3 ip 192.168.99.3/24
	mk_virt b 1 ip 192.168.99.4/24
	mk_virt b 2 ip 192.168.99.5/24
	mk_virt c 1 ip 192.168.99.6/24
	mk_virt c 2 ip 192.168.99.7/24
	mk_bridge net switch a b c
}

function connect(){
	in_phys a $test 0
	in_phys b $test 1
	in_phys c $test 2
}

function test(){
	pass in_virt a 1 $qping 192.168.99.2
	pass in_virt a 1 $qping 192.168.99.4
	pass in_virt a 1 $qping 192.168.99.6
	fail in_virt a 1 $qping 192.168.99.3
	fail in_virt a 1 $qping 192.168.99.5
	fail in_virt a 1 $qping 192.168.99.7
}
