# winalp.tcl
#
# Defines the Oc_Config name 'winalp' to indicate the Windows 95/NT operating
# system running on a DEC Alpha architecture.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
    global tcl_platform
    if {![regexp -nocase -- windows $tcl_platform(platform)]} {
        return 0
    }
    return [string match alpha $tcl_platform(machine)]
}

