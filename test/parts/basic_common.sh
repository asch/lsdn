PHYS_LIST="a b c"
function prepare(){
	mk_testnet net
	mk_phys net a ip 172.16.0.1/24
	mk_phys net b ip 172.16.0.2/24
	mk_phys net c ip 172.16.0.3/24

	mk_virt a 1 ip 192.168.99.1/24 mac 00:00:00:00:00:a1
	mk_virt a 2 ip 192.168.99.2/24 mac 00:00:00:00:00:a2
	mk_virt a 3 ip 192.168.99.3/24 mac 00:00:00:00:00:a3
	mk_virt b 1 ip 192.168.99.4/24 mac 00:00:00:00:00:b1
	mk_virt b 2 ip 192.168.99.5/24 mac 00:00:00:00:00:b2
	mk_virt c 1 ip 192.168.99.6/24 mac 00:00:00:00:00:c1
	mk_virt c 2 ip 192.168.99.7/24 mac 00:00:00:00:00:c2
	mk_bridge net switch a b c
}

function test_ping() {
	pass in_virt a 1 $qping 192.168.99.2
	pass in_virt a 1 $qping 192.168.99.4
	fail in_virt a 1 $qping 192.168.99.3
	fail in_virt a 1 $qping 192.168.99.5
}
