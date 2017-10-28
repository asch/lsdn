phys_seq=`seq 40`
phys_names=

function prepare(){
	mk_testnet net
	for p in $phys_seq; do
		mk_phys net p$p ip 172.16.0.$p/24
		mk_virt p$p virt ip 192.168.99.$p/24 mac "00:00:00:00:00:$(printf "%x" $p)"
		phys_names="$phys_names p$p"
	done
	mk_bridge net switch $phys_names
}

function connect(){
	lsctl_in_all_phys parts/large.lsctl
}

function test_ping(){
	# We need significantly longer ping time here to allow for warmup of the infrastructure.
	# For a simple ping, there are two levels of ARP broadcasts which need to be resolved,
	# and there is 40 of them on the physical level.
	qping="ping -c 2 -i 1 -w 5"

	pass in_virt p1 virt $qping 192.168.99.2
	pass in_virt p1 virt $qping 192.168.99.40
	pass in_virt p40 virt $qping 192.168.99.1
}
