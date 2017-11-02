namespace eval common {
	proc settings {nets} {
		::settings {*}$::env(LSCTL_NETTYPE) -nets $nets
	}
	proc claimLocal {} {
		set phys [lindex $::argv 0]
		::claimLocal $phys
	}
	proc free {} {
		if [ info exists ::env(LSCTL_CLEANUP) ] {
			::cleanup
		} else {
			::free
		}
	}
}
