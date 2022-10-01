# FILE: nb.tcl
#
#	Nuts & Bolts support
#
# Last modified on: $Date: 2015/03/25 16:45:03 $
# Last modified by: $Author: dgp $
#
# When this version of the nb extension is selected by the 'package require'
# command, this file is sourced.

# Verify that C++ portion of this version of the Nb extension 
# has been initialized
#
# NOTE: version number below must match that in ./nb.h

package require -exact Nb 2.0b0

Oc_CheckTclIndex Nb

# Set up for autoloading of nb extension commands
set nb(library) [file dirname [info script]]
if { [lsearch -exact $auto_path $nb(library)] == -1 } {
    lappend auto_path $nb(library)
}

# Load in any local modifications
set local [file join [file dirname [info script]] local nb.tcl]
if {[file isfile $local] && [file readable $local]} {
    uplevel #0 [list source $local]
}
