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

set allObjects {
    mmdispcmds
    mmdispsh
    bitmap
    colormap
    display
    psdraw
}

MakeRule Define {
    -targets            all
    -dependencies       [concat configure [Platform Specific appindex.tcl]]
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
    -targets            tclIndex
    -dependencies       [glob -nocomplain *.tcl]
    -script             {
                         MakeTclIndex .
                        }
}

# Could use better support for this:
MakeRule Define {
    -targets            [Platform Specific appindex.tcl]
    -dependencies       [Platform Executables {condispsh mmdispsh}]
    -script             {puts "Updating [Platform Specific appindex.tcl]"
                        set f [open [Platform Specific appindex.tcl] w]
                        puts $f [format {
Oc_Application Define {
    -name               mmDispSh
    -version            2.0b0
    -machine            %s
    -file               "%s"
}
Oc_Application Define {
    -name               conDispSh
    -version            2.0b0
    -machine            %s
    -file               "%s"
}
                        } [Platform Name] \
				[file tail [Platform Executables mmdispsh]] \
                          [Platform Name] \
				[file tail [Platform Executables condispsh]]]
                        close $f
                        }
}

MakeRule Define {
    -targets            [Platform Executables mmdispsh]
    -dependencies       [concat [Platform Objects $allObjects] \
			        [Platform StaticLibraries {vf nb xp oc}] \
			        [list [file join .. .. pkg ow tclIndex] \
			        [file join .. .. pkg net tclIndex] \
			        tclIndex]]
    -script		[format {
			    Platform Link -obj {%s} -lib {vf nb xp oc tk tcl} \
			            -sub WINDOWS -out mmdispsh
			} $allObjects]
}

MakeRule Define {
    -targets            [Platform Executables condispsh]
    -dependencies       [concat [Platform Objects $allObjects] \
			        [Platform StaticLibraries {vf nb xp oc}] \
			        [list [file join .. .. pkg oc tclIndex] \
			        [file join .. .. pkg ow tclIndex] \
			        [file join .. .. pkg net tclIndex] \
			        [file join .. .. pkg vf tclIndex] \
			        tclIndex]]
    -script		[format {
			    Platform Link -obj {%s} -lib {vf nb xp oc tk tcl} \
			            -sub CONSOLE -out condispsh
			} $allObjects]
}

foreach o $allObjects { 
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
    -targets		upgrade
    -script		{DeleteFiles avftoave.cc avftoovf.cc avftovio.cc
			 DeleteFiles mmdinit.cc mmdinit.h mmdisp.cc
			 DeleteFiles mmdisp.h mmdisp.tcl
			 DeleteFiles avf2ave.cc avf2ovf.cc avf2vio.cc
			 DeleteFiles avf2ppm.cc avf2ppm.tcl avf2ppm.def
                         DeleteFiles [file join scripts avf2ave.tcl]
                         DeleteFiles [file join scripts avf2ppm.def]
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
    -script             {eval DeleteFiles [concat tclIndex \
	        		[Platform Specific appindex.tcl] \
				[Platform Executables condispsh] \
				[Platform Executables mmdispsh] ]}
}
 
MakeRule Define {
    -targets            objclean
    -dependencies       {}
    -script             [format {
        eval DeleteFiles [Platform Objects {%s}]
	eval DeleteFiles [Platform Intermediate {%s condispsh mmdispsh}]
    } $allObjects $allObjects]
}

unset allObjects
