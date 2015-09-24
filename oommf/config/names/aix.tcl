# aix.tcl
#
# Defines the Oc_Config name 'aix' to indicate the AIX operating system.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
    global tcl_platform
    return [regexp -nocase -- aix $tcl_platform(os)]
}

