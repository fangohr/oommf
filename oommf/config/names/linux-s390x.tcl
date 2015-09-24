# linux-s390x.tcl
#
# Defines the Oc_Config name 'linux-s390x' to indicate the Linux operating
# system running on an IBM s390x zSeries architecture.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
    global tcl_platform env
    if {![regexp -nocase -- linux $tcl_platform(os)]} {
        return 0
    }
    if {![string match s390x $tcl_platform(machine)]} {
	return 0
    }
    return 1
}
