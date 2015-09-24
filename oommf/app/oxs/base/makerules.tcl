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

MakeRule Define {
    -targets		all
    -dependencies       [list tclIndex [file join [file dirname [pwd]] all]]
}

MakeRule Define {
    -targets            tclIndex
    -dependencies       [glob -nocomplain *.tcl]
    -script             {
                         MakeTclIndex .
                        }
}

MakeRule Define {
    -targets            mostlyclean
    -dependencies       {}
    -script             {eval DeleteFiles tclIndex}
}
