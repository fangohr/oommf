# FILE: makerules.tcl
#
# This file controls the application 'pimake' by defining rules which
# describe how to build the applications and/or extensions produced by
# the source code in this directory.
#
# NOTICE: Please see the file ../../LICENSE
#
# Last modified on: $Date: 2011/11/09 00:50:02 $
# Last modified by: $Author: donahue $
#
# Verify that this script is being sourced by pimake
if {[llength [info commands MakeRule]] == 0} {
    error "'[info script]' must be evaluated by pimake"
}
#
########################################################################

set objects {
    chanwrap
    crc
    dstring
    evoc
    floatvec
    functions
    imgobj
    messagelocker
    nb
    stopwatch
    tclobjarray
    tclcommand
    xpfloat
}

set valuesafe {
   xpfloat
}

MakeRule Define {
    -targets            all
    -dependencies       [concat configure [Platform StaticLibrary nb]]
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
    -dependencies       [concat [file join .. oc tclIndex] \
    			        [glob -nocomplain *.tcl]]
    -script             {
                         MakeTclIndex .
                        }
}

MakeRule Define {
    -targets            [Platform StaticLibrary nb]
    -dependencies	[concat tclIndex \
	                   [Platform Objects [concat errhandlers $objects]]]
    -script		[format {
			    eval file delete [Platform StaticLibrary nb]
			    Platform MakeLibrary -out nb -obj {errhandlers %s}
			    Platform IndexLibrary nb
			} $objects]
}

foreach o $objects {
   if {[lsearch -exact $valuesafe $o] >= 0} {
      # File requires value-safe optimizations only
      MakeRule Define {
        -targets	[Platform Objects $o]
        -dependencies	[concat [list [Platform Name]] \
			        [[CSourceFile New _ $o.cc] Dependencies]]
        -script		[format {Platform Compile C++ -valuesafeopt 1 \
			        -inc [[CSourceFile New _ %s.cc] DepPath] \
			        -out %s -src %s.cc
			} $o $o $o]
      }
   } else {
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
}

# Compilation of errhandlers.cc is handled specially because optimization level
# 1 causes pointer trouble on some platforms.
if {[catch {
    [Oc_Config RunPlatform] GetValue \
            program_compiler_c++_property_optimization_breaks_varargs
} _] || !$_} {
MakeRule Define {
    -targets		[Platform Objects errhandlers]
    -dependencies	[concat [list [Platform Name]] [[CSourceFile New _ \
			        errhandlers.cc] Dependencies]]
    -script		{Platform Compile C++ -opt 1 -inc \
			        [[CSourceFile New _ errhandlers.cc] DepPath] \
			        -out errhandlers -src errhandlers.cc
			}
}
} else {
MakeRule Define {
    -targets		[Platform Objects errhandlers]
    -dependencies	[concat [list [Platform Name]] [[CSourceFile New _ \
			        errhandlers.cc] Dependencies]]
    -script		{Platform Compile C++ -opt 0 -inc \
			        [[CSourceFile New _ errhandlers.cc] DepPath] \
			        -out errhandlers -src errhandlers.cc
			}
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
			        [Platform StaticLibrary nb]]}
}

MakeRule Define {
    -targets            objclean
    -dependencies       {}
    -script             [format {
	eval DeleteFiles [Platform Objects {errhandlers %s}]
	eval DeleteFiles [Platform Intermediate {errhandlers %s}]
    } $objects $objects]
}

unset objects
