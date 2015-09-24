# sunos.tcl
#
# Defines the Oc_Config name 'sunos' to indicate the SunOS operating
# system (pre-Solaris - version 4.x or less) running on a Sun (Sparc) 
# architecture.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
    global tcl_platform
    if {![regexp -nocase -- sun.* $tcl_platform(machine)]} {
        return 0
    }
    regexp {([^.]*).*} $tcl_platform(osVersion) _ OSMAJOR
    return [expr {$OSMAJOR < 5}]
}

