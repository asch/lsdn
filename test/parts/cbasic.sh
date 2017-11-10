NETCONF="cbasic"

function connect(){
	for p in $PHYS_LIST; do
		pass in_phys $p ${TEST_RUNNER:-} ./test_basic $p
	done
}

source "parts/basic_common.sh"
