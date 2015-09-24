# bsd-alpha.tcl
#
# Defines the Oc_Config name 'bsd-alpha' to indicate the *BSD operating
# system running on the DEC/Compaq/HP "alpha" architecture.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
    global tcl_platform env
    if {![regexp -nocase -- {bsd$} $tcl_platform(os)]} {
        return 0
    }
    if {![string match alpha $tcl_platform(machine)]} {
	return 0
    }
    return 1
}


