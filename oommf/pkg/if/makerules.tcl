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

set objects {
    if
}

MakeRule Define {
    -targets            all
    -dependencies       [concat configure [Platform StaticLibrary if]]
}

MakeRule Define {
    -targets            configure
    -dependencies       [Platform Name]
}

MakeRule Define {
    -targets            [Platform Name]
    -dependencies       {}
    -script             {MakeDirectory [Platform Name]}
}

MakeRule Define {
    -targets            [Platform StaticLibrary if]
    -dependencies	[concat [Platform Objects $objects]]
    -script		[format {
			    eval DeleteFiles [Platform StaticLibrary if]
			    Platform MakeLibrary -out if -obj {%s}
			    Platform IndexLibrary if
			} $objects]
}

foreach o $objects { 
    MakeRule Define {
        -targets	[Platform Objects $o]
        -dependencies	[concat [list [Platform Name]] \
			        [[CSourceFile New _ $o.cc] Dependencies]]
        -script		[format {Platform Compile C++ -opt 1 \
			        -inc [[CSourceFile New _ %s.cc] DepPath] \
			         -out %s -src %s.cc
			} $o $o $o]
    }
}

MakeRule Define {
    -targets            distclean
    -dependencies       clean
    -script             {DeleteFiles [Platform Name]}
}
 
MakeRule Define {
    -targets            clean
    -dependencies       mostlyclean
}
 
MakeRule Define {
    -targets            mostlyclean
    -dependencies       objclean
    -script             {eval DeleteFiles [Platform StaticLibrary if]}
}
 
MakeRule Define {
    -targets            objclean
    -dependencies       {}
    -script             [format {
			    eval DeleteFiles [Platform Objects {%s}]
			} $objects]
}

unset objects
