function dhcp_server(){
	cat << EOF > /tmp/dhcpd.conf
default-lease-time 600;
max-lease-time 7200;

subnet 192.168.99.0 netmask 255.255.255.0 {
	option subnet-mask 255.255.255.0;
	range 192.168.99.2 192.168.99.250;
}
EOF
	echo > /tmp/dhcpd.leases
	in_virt $1 $2 dhcpd -cf /tmp/dhcpd.conf -lf /tmp/dhcpd.leases --no-pid $3
}

function dhcp_client(){
	in_virt $1 $2 dhcpcd $3

	# Ugly. I don't know how to force dhcpcd to
	# store pid files elsewhere or ignore them completely
	# (probably wasn't designed with namespaces in mind).
	rm /run/dhcpcd*.pid
}

function prepare(){
	mk_testnet net
	mk_phys net a ip 172.16.0.1/24
	mk_phys net b ip 172.16.0.2/24
	mk_phys net c ip 172.16.0.3/24

	mk_virt a 1 ip 192.168.99.1/24
	mk_virt b 1
	mk_virt b 2
	mk_virt b 3
	mk_virt c 1
	mk_virt c 2

	set_up_if a-1 lo
	set_up_if b-1 lo
	set_up_if b-2 lo
	set_up_if b-3 lo
	set_up_if c-1 lo
	set_up_if c-2 lo

	mk_bridge net switch a b c
}

function connect(){
	in_phys a $test 0
	in_phys b $test 1
	in_phys c $test 2

	dhcp_server a 1 out

	dhcp_client b 1 out
	dhcp_client b 2 out
	dhcp_client c 1 out
}

function test(){
	pass in_virt b 1 $qping 192.168.99.1
	pass in_virt b 1 $qping 192.168.99.2
	pass in_virt b 1 $qping 192.168.99.3
	pass in_virt b 1 $qping 192.168.99.4
	fail in_virt b 1 $qping 192.168.99.5
	fail in_virt b 3 $qping 192.168.99.2
}
