# linalp.tcl
#
# Defines the Oc_Config name 'linalp' to indicate the Linux operating
# system running on a DEC Alpha architecture.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
    global tcl_platform env
    if {![regexp -nocase -- linux $tcl_platform(os)]} {
        return 0
    }
    if {![string match alpha $tcl_platform(machine)]} {
	return 0
    }
    if {[info exists env(OOMMF_MPI_PROC_COUNT)] \
	    && $env(OOMMF_MPI_PROC_COUNT)>0} {
	return 0
    }
    return 1
}

