namespace eval common {
	proc opt_env {var} {
		if [ info exists ::env($var) ] {return $::env($var) } {return ""}
	}

	proc settings {} {
		::settings $::env(LSCTL_NETTYPE) {*}[opt_env LSCTL_NETTYPE_SETTINGS]
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
