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
    -dependencies	[list configure [Platform Specific appindex.tcl]]
}

MakeRule Define {
    -targets		configure
    -dependencies	[Platform Name]
}

MakeRule Define {
    -targets		[Platform Name]
    -dependencies	{}
    -script		{MakeDirectory [Platform Name]}
}

# Could use better support for this:
MakeRule Define {
    -targets		[Platform Specific appindex.tcl]
    -dependencies	[Platform Executables {omfsh filtersh} ]
    -script		{puts "Updating [Platform Specific appindex.tcl]"
			set f [open [Platform Specific appindex.tcl] w]
			puts $f [format {
Oc_Application Define {
    -name		omfsh
    -version		2.0b0
    -machine		%s
    -file		"%s"
}
Oc_Application Define {
    -name		filtersh
    -version		2.0b0
    -machine		%s
    -file		"%s"
}
			} [Platform Name] \
                                [file tail [Platform Executables omfsh]] \
			        [Platform Name] \
                                [file tail [Platform Executables filtersh]]]
			close $f
			}
}

MakeRule Define {
    -targets		[Platform Executables omfsh]
    -dependencies	[concat [Platform Objects omfsh] \
			        [Platform StaticLibraries oc] \
			        [Platform StaticLibraries xp] \
			        [Platform StaticLibraries nb] \
			        [Platform StaticLibraries if]]
    -script		{Platform Link -obj omfsh -lib {if nb xp oc tk tcl} \
                                 -sub WINDOWS -out omfsh
			} 
}

MakeRule Define {
    -targets		[Platform Executables filtersh]
    -dependencies	[concat [Platform Objects omfsh] \
			        [Platform StaticLibraries oc] \
			        [Platform StaticLibraries xp] \
			        [Platform StaticLibraries nb] \
			        [Platform StaticLibraries if]]
    -script		{Platform Link -obj omfsh -lib {if nb xp oc tk tcl} \
                                 -sub CONSOLE -out filtersh
			} 
}

MakeRule Define {
    -targets		[Platform Objects omfsh]
    -dependencies	[concat [list [Platform Name]] \
			        [[CSourceFile New _ omfsh.cc] Dependencies]]
    -script		{Platform Compile C++ -opt 1 \
                                 -inc [[CSourceFile New _ omfsh.cc] DepPath] \
                                 -out omfsh -src omfsh.cc
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
    -script             {eval DeleteFiles [Platform Executables omfsh] \
                            [Platform Executables filtersh] \
                            [Platform Specific appindex.tcl]}
}

MakeRule Define {
    -targets            objclean
    -dependencies       {}
    -script             {
			 DeleteFiles [Platform Objects omfsh]
			 eval DeleteFiles \
				[Platform Intermediate {omfsh filtersh}]
			}
}

