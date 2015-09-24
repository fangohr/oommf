# lintel.tcl
#
# Defines the Oc_Config name 'lintel' to indicate the Linux operating
# system running on an Intel architecture.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
    global tcl_platform env
    if {![regexp -nocase -- linux $tcl_platform(os)]} {
        return 0
    }
    if {[info exists tcl_platform(wordSize)]} {
	if {$tcl_platform(wordSize) != 4} {
	    return 0
	}
	if {![string match i?86 $tcl_platform(machine)] \
		&& ![string match x86* $tcl_platform(machine)] } {
	    return 0
	}
    } else {
	# Old Tcl
	if {![string match i?86 $tcl_platform(machine)]} {
	    return 0
	}
    }
    if {[info exists env(OOMMF_MPI_PROC_COUNT)] \
	    && $env(OOMMF_MPI_PROC_COUNT)>0} {
	return 0
    }
    return 1
}


