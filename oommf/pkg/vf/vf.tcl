# FILE: vf.tcl
#
#	Vector Field support
#
# Last modified on: $Date: 2015/03/25 16:45:23 $
# Last modified by: $Author: dgp $
#
# When this version of the vf extension is selected by the 'package require'
# command, this file is sourced.

# Verify that C++ portion of this version of the Vf extension 
# has been initialized
#
# NOTE: version number below must match that in ./vf.h
package require -exact Vf 2.0b0

Oc_CheckTclIndex Vf

# Set up for autoloading of vf extension commands
set vf(library) [file dirname [info script]]
if { [lsearch -exact $auto_path $vf(library)] == -1 } {
    lappend auto_path $vf(library)
}

# Load in any local modifications
set local [file join [file dirname [info script]] local vf.tcl]
if {[file isfile $local] && [file readable $local]} {
    uplevel #0 [list source $local]
}

