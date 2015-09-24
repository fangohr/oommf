# linalp.tcl
#
# Defines the Oc_Config name 'duxalp' to indicate the Digital Unix operating
# system (OSF) running on a DEC Alpha architecture.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
    global tcl_platform
    if {![regexp -nocase -- osf1 $tcl_platform(os)]} {
        return 0
    }
    return [string match alpha $tcl_platform(machine)]
}

