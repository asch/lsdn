GLOB_NET_PREF=1001:0db8:ac10:fe01

ipa=${GLOB_NET_PREF}:0000:0000:0000:000a
ipb=${GLOB_NET_PREF}:0000:0000:0000:000b
ipc=${GLOB_NET_PREF}:0000:0000:0000:000c


NET_PREF=2001:0db8:ac10:fe01

ipa1=${NET_PREF}:0000:0000:0000:00a1
ipa2=${NET_PREF}:0000:0000:0000:00a2
ipa3=${NET_PREF}:0000:0000:0000:00a3
ipb1=${NET_PREF}:0000:0000:0000:00b1
ipb2=${NET_PREF}:0000:0000:0000:00b2
ipc1=${NET_PREF}:0000:0000:0000:00c1
ipc2=${NET_PREF}:0000:0000:0000:00c2

function prepare(){
	mk_testnet net
	mk_phys net a ip ${ipa}/64
	mk_phys net b ip ${ipb}/64
	mk_phys net c ip ${ipc}/64

	mk_virt a 1 ip ${ipa1}/64 mac 00:00:00:00:00:a1
	mk_virt a 2 ip ${ipa2}/64 mac 00:00:00:00:00:a2
	mk_virt a 3 ip ${ipa3}/64 mac 00:00:00:00:00:a3
	mk_virt b 1 ip ${ipb1}/64 mac 00:00:00:00:00:b1
	mk_virt b 2 ip ${ipb2}/64 mac 00:00:00:00:00:b2
	mk_virt c 1 ip ${ipc1}/64 mac 00:00:00:00:00:c1
	mk_virt c 2 ip ${ipc2}/64 mac 00:00:00:00:00:c2
	mk_bridge net switch a b c
}

function test_ping() {
	pass in_virt a 1 $qping ${ipa2}
	pass in_virt a 1 $qping ${ipb1}
	fail in_virt a 1 $qping ${ipa3}
	fail in_virt a 1 $qping ${ipb2}
}
