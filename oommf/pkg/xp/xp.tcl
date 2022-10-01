# FILE: xp.tcl
#
#	Floating point extra precision support
#
# When this version of the xp extension is selected by the 'package require'
# command, this file is sourced.

# Verify that C++ portion of this version of the Xp extension 
# has been initialized
#
# NOTE: version number below must match that in ./xp.h
package require -exact Xp 2.0b0

Oc_CheckTclIndex Xp

# Set up for autoloading of xp extension commands
set xp(library) [file dirname [info script]]
if { [lsearch -exact $auto_path $xp(library)] == -1 } {
    lappend auto_path $xp(library)
}

# Load in any local modifications
set local [file join [file dirname [info script]] local xp.tcl]
if {[file isfile $local] && [file readable $local]} {
    uplevel #0 [list source $local]
}
