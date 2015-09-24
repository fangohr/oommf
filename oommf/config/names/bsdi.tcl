# bsdi.tcl
#
# Defines the Oc_Config name 'bsdi' to indicate the *BSD operating
# system running on an Intel architecture.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
    global tcl_platform env
    if {![regexp -nocase -- {bsd$} $tcl_platform(os)]} {
        return 0
    }
    if {![string match i?86 $tcl_platform(machine)]} {
	return 0
    }
    return 1
}


