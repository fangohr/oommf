# FILE: makerules.tcl
#
# This file controls the application 'pimake' by defining rules which
# describe how to build the applications and/or extensions produced by
# the source code in this directory.
#
# NOTICE: Please see the file ../../LICENSE
#
# Last modified on: $Date: 2000/08/25 17:01:25 $
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
    -targets		mostlyclean
    -dependencies	objclean
    -script		{DeleteFiles tclIndex}
}

MakeRule Define {
    -targets		tclIndex
    -dependencies	[concat [file join .. oc tclIndex] \
			        [glob -nocomplain *.tcl]]
    -script		{
			 MakeTclIndex .
			}
}

