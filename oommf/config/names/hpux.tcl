# hpux.tcl
#
# Defines the Oc_Config name 'hpux' to indicate the HP-UX operating system.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
    global tcl_platform
    return [regexp -nocase -- hp-ux $tcl_platform(os)]
}

