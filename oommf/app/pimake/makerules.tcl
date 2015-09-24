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
    -dependencies	appindex.tcl
}

# Still need rule for making appindex.tcl files
MakeRule Define {
    -targets		appindex.tcl
    -dependencies	{}
    -script		{puts "Updating appindex.tcl"}
}

MakeRule Define {
    -targets		upgrade
    -script		{DeleteFiles makerules.def}
}
