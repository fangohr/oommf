# bsd-x86_64.tcl
#
# Defines the Oc_Config name 'bsd-x86_64' to indicate the *BSD operating
# system running on an x86-64 architecture.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
    global tcl_platform env
    if {![regexp -nocase -- {bsd$} $tcl_platform(os)]} {
        return 0
    }
    if {![string match amd64 $tcl_platform(machine)]} {
	return 0
    }
    return 1
}


