# darwin.tcl
#
# Defines the Oc_Config name 'darwin' to indicate the Darwin (MacOSX)
#  operating system.

Oc_Config New _ [string tolower [file rootname [file tail [info script]]]] {
    global tcl_platform
    return [regexp -nocase -- darwin $tcl_platform(os)]
}

