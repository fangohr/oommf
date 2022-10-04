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

set objects {
	demag
	demag2
	fft
	grid
	maganis
	magelt
	maginit
	mifio
	mmsinit
	mmsolve
	vecio
	threevec
	zeeman
}

MakeRule Define {
    -targets            all
    -dependencies       [concat configure tclIndex \
			        [Platform Specific appindex.tcl]]
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
    -dependencies       [Platform Executables mmsolve]
    -script             {puts "Updating [Platform Specific appindex.tcl]"
                        set f [open [Platform Specific appindex.tcl] w]

if {[catch {[Oc_Config RunPlatform] GetValue platform_use_mpi} _] || !$_} {
                        puts $f [format {
Oc_Application Define {
    -name               mmSolve
    -version            2.0b0
    -machine            %s
    -file               "%s"
}
                        } [Platform Name] \
                                [file tail [Platform Executables mmsolve]]]
} else {
                        puts $f [format {
Oc_Application Define {
    -name               mpirun
    -version            2.0b0
    -machine            %s
    -directory          {}
    -file               {%s}
    -options            {%s}
}
                        } \
	[Platform Name] \
	[[Oc_Config RunPlatform] GetValue platform_mpirun_command] \
	[[Oc_Config RunPlatform] GetValue platform_mpirun_options]]
                        puts $f [format {
Oc_Application Define {
    -name               mmSolve
    -version            2.0b0
    -machine            mpirun
    -file               "%s"
}
                        } [file tail [Platform Executables mmsolve]]]


}
                        close $f

                        }
}

MakeRule Define {
    -targets            [Platform Executables mmsolve]
    -dependencies       [concat [Platform Objects $objects] \
			        [Platform StaticLibraries {vf nb xp oc}] \
			        [list [file join .. .. pkg ow tclIndex] \
			        [file join .. .. pkg net tclIndex] \
			        tclIndex]]
    -script		[format {
			    Platform Link -obj {%s} \
			            -lib {vf nb xp oc tk tcl} \
			            -sub CONSOLE -out mmsolve
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
    -targets		upgrade
    -script		{DeleteFiles gif2ppm.tcl
			 DeleteFiles [file join scripts hystctrl.tcl]
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
			        [Platform Executables mmsolve]]}
}
 
MakeRule Define {
    -targets            objclean
    -dependencies       {}
    -script             [format {
	eval DeleteFiles [Platform Objects {%s}]
	eval DeleteFiles [Platform Intermediate {%s mmsolve}]
    } $objects $objects]
}

unset objects
