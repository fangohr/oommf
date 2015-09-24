# FILE: makerules.tcl
#
# This file controls the application 'pimake' by defining rules which
# describe how to build the applications and/or extensions produced by
# the source code in this directory.
#
# Verify that this script is being sourced by pimake
if {[llength [info commands MakeRule]] == 0} {
    error "'[info script]' must be evaluated by pimake"
}
#
########################################################################

# The following is a hack that runs the all target from the makerules.tcl
# file in the parent directory.  Eventually the appropriate rules there
# should be moved down into this directory.
MakeRule Define {
    -targets		all
    -dependencies       [list [file join [file dirname [pwd]] all]]
}

