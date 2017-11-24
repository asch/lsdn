NETCONF="basic_ip6"

function connect(){
	lsctl_in_all_phys parts/basic_ip6.lsctl
}

source "parts/basic_common_ip6.sh"
