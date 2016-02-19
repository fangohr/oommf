# FILE: makerules.tcl
#
# This file controls the application 'pimake' by defining rules which
# describe how to build the applications and/or extensions produced by
# the source code in this directory.
#
# Last modified on: $Date: 2007/03/21 01:17:01 $
# Last modified by: $Author: donahue $
#
# Verify that this script is being sourced by pimake
if {[llength [info commands MakeRule]] == 0} {
    error "" "'[info script]' must be evaluated by pimake"
}
#
########################################################################

# Default rules cover everything

