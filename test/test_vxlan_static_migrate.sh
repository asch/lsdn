phys_seq=`seq 40`
phys_names=

function prepare(){
	mk_testnet net
	mk_phys net a ip 172.16.0.1/24
	mk_phys net b ip 172.16.0.2/24
	mk_phys net c ip 172.16.0.3/24

	mk_virt a 1 ip 192.168.99.1/24 mac "00:00:00:00:00:01"
	# The first two virts have wrong IP intentionally
	mk_virt a 2 ip 192.168.99.3/24 mac "00:00:00:00:00:02"
	mk_virt b 2 ip 192.168.99.3/24 mac "00:00:00:00:00:02"
	mk_virt c 2 ip 192.168.99.2/24 mac "00:00:00:00:00:02"

	mk_bridge net switch a b c
}

function connect(){
	in_phys a $lsctl -d localPhys=a $config
	in_phys b $lsctl -d localPhys=b $config
	in_phys c $lsctl -d localPhys=c $config
}

function test(){
	pass in_virt a 1 $qping 192.168.99.2
}
