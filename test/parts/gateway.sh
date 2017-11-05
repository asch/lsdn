function prepare(){
	mk_testnet net
	mk_phys net a ip 172.16.0.1/24
	mk_phys net b ip 172.16.0.2/24
	mk_phys net c ip 172.16.0.3/24

	mk_virt a 1 ip 192.168.99.1/24 mac 00:00:00:00:00:a1 next 2 ip 192.168.100.1/24 mac 00:00:00:00:00:a2
	mk_virt b 1 ip 192.168.99.2/24 mac 00:00:00:00:00:b1
	mk_virt b 2 ip 192.168.99.3/24 mac 00:00:00:00:00:b2
	mk_virt c 1 ip 192.168.99.4/24 mac 00:00:00:00:00:c1
	mk_virt c 2 ip 192.168.100.2/24 mac 00:00:00:00:00:c2

	mk_bridge net switch a b c
}

function connect(){
	lsctl_in_all_phys parts/gateway.lsctl

	enable_ipv4_forwarding a 1

	add_default_route b 1 192.168.99.1 out-1
	add_default_route b 2 192.168.99.1 out-1
	add_default_route c 1 192.168.99.1 out-1
	add_default_route c 2 192.168.100.1 out-1
}

function test_ping(){
	pass in_virt b 1 $qping 192.168.99.1
	pass in_virt b 1 $qping 192.168.99.2
	pass in_virt b 1 $qping 192.168.99.3
	pass in_virt b 1 $qping 192.168.99.4
	pass in_virt b 1 $qping 192.168.100.1
	pass in_virt b 1 $qping 192.168.100.2
}
