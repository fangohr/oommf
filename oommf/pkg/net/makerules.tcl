# FILE: makerules.tcl
#
# This file controls the application 'pimake' by defining rules which
# describe how to build the applications and/or extensions produced by
# the source code in this directory.
#
# Last modified on: $Date: 2007/03/21 01:17:04 $
# Last modified by: $Author: donahue $
#
# Verify that this script is being sourced by pimake
if {[llength [info commands MakeRule]] == 0} {
    error "'[info script]' must be evaluated by pimake"
}
#
########################################################################

MakeRule Define {
    -targets		all
    -dependencies	tclIndex
}

MakeRule Define {
    -targets		upgrade
    -script		{DeleteFiles [file join threads mmarchive.tcl]}
}

MakeRule Define {
    -targets		mostlyclean
    -dependencies	objclean
    -script		{DeleteFiles tclIndex}
}

MakeRule Define {
    -targets		tclIndex
    -dependencies	[concat [list [file join .. oc tclIndex] \
			        [file join .. ow tclIndex]] \
    			        [glob -nocomplain *.tcl]]
    -script		{
			 MakeTclIndex .
			}
}

