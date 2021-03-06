NETCONF="migrate"
PHYS_LIST="a b c"
PHASES=3

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

	run_daemons
}

LSPATH="/tmp/lsctld-tests"
function run_daemons() {
	rm -rf "$LSPATH" 2> /dev/null
	mkdir -p "$LSPATH"
	for p in $PHYS_LIST; do
		LSCTL_PHYS=$p in_phys $p ../daemon/lsctld -s "$LSPATH/$p"
	done
	sleep 1
}

function connect() {
	config=parts/migrate-daemon${1:-}.lsctl
	for p in $PHYS_LIST; do
		pass ../lsctlc/lsctlc "$LSPATH/$p" < $config
	done
}

function test_ping() {
	case ${1:-} in
		1)
			fail in_virt a 1 $qping 192.168.99.2
			;;
		2)
			fail in_virt a 1 $qping 192.168.99.2
			;;
		3)
			pass in_virt a 1 $qping 192.168.99.2
			pkill lsctld
			;;
	esac
}
