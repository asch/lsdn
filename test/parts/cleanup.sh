export LSCTL_CLEANUP=1

function test() {
	for p in $PHYS_LIST; do
		local interfaces="$(in_phys $p ls /sys/class/net)"
		echo "> Left-over interfaces: " $interfaces
		if echo $interfaces | grep -q lsdn- ; then
			test_error
		fi
	done
	# TODO: also check that there are not qdiscs, filters and other cruft on virt interfaces
}
