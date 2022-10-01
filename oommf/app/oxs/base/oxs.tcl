# FILE: oxs.tcl
#
#	Master Tcl file for the oxs shell
#
# Last modified on: $Date: 2015/02/18 02:39:47 $
# Last modified by: $Author: donahue $

Oc_CheckTclIndex Oxs

# NOTE: version number below must match that in ./oxs.h
package require -exact Oxs 2.0b0

# Bind any events from "source" Oxs to the tag "Oxs"
Oc_EventHandler Bindtags Oxs [list Oxs]

# Set up for autoloading of Oc extension commands
set oxs(library) [file dirname [info script]]
if { [lsearch -exact $auto_path $oxs(library)] == -1 } {
    lappend auto_path $oxs(library)
}

# Load in any local modifications
set local [file join [file dirname [info script]] local oxs.tcl]
if {[file isfile $local] && [file readable $local]} {
    uplevel #0 [list source $local]
}
