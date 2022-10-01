# FILE: makerules.tcl
#
# This file controls the application 'pimake' by defining rules which
# describe how to build the applications and/or extensions produced by
# the source code in this directory.
#
# Verify that this script is being sourced by pimake
if {[llength [info commands MakeRule]] == 0} {
    error "" "'[info script]' must be evaluated by pimake"
}
#
########################################################################

MakeRule Define {
    -targets            doc
    -dependencies       {}
    -script             {Recursive doc}
}

MakeRule Define {
    -targets		upgrade
    -script		{eval DeleteFiles \
			    [glob -nocomplain *.obj *.gif *.tex oommf* local*]
			 DeleteFiles pngfiles psfiles giffiles .latex2html-init
			 DeleteFiles userguide.pdf userguide.ps
			 Recursive upgrade
			}
}
