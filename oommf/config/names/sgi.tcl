# sgi.tcl
#
# Defines the Oc_Config name 'sgi' to indicate the IRIX operating
# system running on an Sgi.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
    global tcl_platform env
    if {![regexp -nocase -- ^irix $tcl_platform(os)]} {
        return 0
    }
    if {[info exists env(OOMMF_MPI_PROC_COUNT)] \
	    && $env(OOMMF_MPI_PROC_COUNT)>0} {
	return 0
    }
    return 1
}
