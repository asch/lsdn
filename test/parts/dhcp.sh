PHYS_LIST="a b c"

function dhcp_server(){
	cat << EOF > /tmp/dhcpd.conf
default-lease-time 600;
max-lease-time 7200;

subnet 192.168.99.0 netmask 255.255.255.0 {
	option subnet-mask 255.255.255.0;
	range 192.168.99.2 192.168.99.250;
}
# Having a statically assigned addresses prevent the DHCP server from doing ARP probes
# and speeds up the whole process
host b1 { hardware ethernet 00:00:00:00:00:b1; fixed-address 192.168.99.2; }
host b2 { hardware ethernet 00:00:00:00:00:b2; fixed-address 192.168.99.3; }
host b3 { hardware ethernet 00:00:00:00:00:b3; fixed-address 192.168.99.5; }
host c1 { hardware ethernet 00:00:00:00:00:c1; fixed-address 192.168.99.4; }
EOF
	echo > /tmp/dhcpd.leases
	in_virt $1 $2 dhcpd -4 -cf /tmp/dhcpd.conf -lf /tmp/dhcpd.leases --no-pid $3
}

function dhcp_client(){
	rm /var/lib/dhcpcd/dhcpcd-*.lease || true
	# -A disables ARP probes and makes the lease quicker
	in_virt $1 $2 dhcpcd -A -4 --oneshot -t 5 $3
}

function prepare(){
	mk_testnet net
	mk_phys net a ip 172.16.0.1/24
	mk_phys net b ip 172.16.0.2/24
	mk_phys net c ip 172.16.0.3/24

	mk_virt a 1 ip 192.168.99.1/24 mac 00:00:00:00:00:a1
	mk_virt b 1 mac 00:00:00:00:00:b1
	mk_virt b 2 mac 00:00:00:00:00:b2
	mk_virt b 3 mac 00:00:00:00:00:b3
	mk_virt c 1 mac 00:00:00:00:00:c1

	mk_bridge net switch a b c
}

function connect(){
	lsctl_in_all_phys parts/dhcp.lsctl

	dhcp_server a 1 out-1

	pass dhcp_client b 1 out-1
	pass dhcp_client b 2 out-1
	fail dhcp_client b 3 out-1
	pass dhcp_client c 1 out-1

}

function test(){
	pass in_virt b 1 $qping 192.168.99.1
	pass in_virt b 1 $qping 192.168.99.2
	pass in_virt b 1 $qping 192.168.99.3
	pass in_virt b 1 $qping 192.168.99.4
	fail in_virt b 1 $qping 192.168.99.5
	fail in_virt b 3 $qping 192.168.99.2
}
