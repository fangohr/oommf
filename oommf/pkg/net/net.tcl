# FILE: net.tcl
#
#	The Net extension.
#
#	This extension provides a set of Oc_Classes which support networking
# operations.
#
# Last modified on: $Date: 2015/03/25 16:45:08 $
# Last modified by: $Author: dgp $
#
# When this version of the Net extension is selected by the 'package require'
# command, this file is sourced.

# Other package requirements
package require Tcl 8.5-
package require Oc 2
package require Nb 2  ;# Needed by socket connection access checks

Oc_CheckTclIndex Net

# CVS 
package provide Net 2.0b0

# Set up for autoloading of Net extension commands
set _net(library) [file dirname [info script]]
if { [lsearch -exact $auto_path $_net(library)] == -1 } {
    lappend auto_path $_net(library)
}

# Load in any local modifications
set local [file join [file dirname [info script]] local net.tcl]
if {[file isfile $local] && [file readable $local]} {
    uplevel #0 [list source $local]
}

Oc_EventHandler New _ Oc_Main LibShutdown Net_StartCleanShutdown
