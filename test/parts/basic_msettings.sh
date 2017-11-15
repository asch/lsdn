NETCONF="msettings"

function connect(){
	lsctl_in_all_phys parts/msettings.lsctl
}

source "parts/basic_common.sh"
