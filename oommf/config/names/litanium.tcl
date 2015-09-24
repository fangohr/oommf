# lintel.tcl
#
# Defines the Oc_Config name 'lintel' to indicate the Linux operating
# system running on an Intel architecture.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
    global tcl_platform env
    if {![regexp -nocase -- linux $tcl_platform(os)]} {
        return 0
    }
    if {![string match ia64 $tcl_platform(machine)]} {
	return 0
    }
    return 1
}


