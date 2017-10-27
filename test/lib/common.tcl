namespace eval common {
	proc settings {nets} {
		::settings {*}$::env(LSCTL_NETTYPE) -nets $nets
	}
	proc claimLocal {} {
		set phys [lindex $::argv 0]
		::claimLocal $phys
	}
}
