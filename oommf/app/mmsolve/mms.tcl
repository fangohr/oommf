# FILE: mms.tcl
#
#	Master Tcl file for the mmsolve shell
#
# Last modified on: $Date: 2015/03/25 16:43:48 $
# Last modified by: $Author: dgp $

Oc_CheckTclIndex mmSolve

# NOTE: version number below must match that in ./mmsolve.h
package require -exact Mms 2.0b0

# Set up for autoloading of Oc extension commands
set mms(library) [file dirname [info script]]
if { [lsearch -exact $auto_path $mms(library)] == -1 } {
    lappend auto_path $mms(library)
}

# Load in any local modifications
set local [file join [file dirname [info script]] local mms.tcl]
if {[file isfile $local] && [file readable $local]} {
    uplevel #0 [list source $local]
}

