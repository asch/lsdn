namespace eval common {
	proc opt_env {var} {
		if [ info exists ::env($var) ] {return $::env($var) } {return ""}
	}
	proc settings {{name ""}} {
		if {${name} eq ""} {
			::settings $::env(LSCTL_NETTYPE) {*}[opt_env LSCTL_NETTYPE_SETTINGS]
		} else {
			set u LSCTL_NETTYPE_${name}
			set v LSCTL_NETTYPE_SETTINGS_${name}
			::settings $::env($u) {*}[opt_env $v]
		}
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
