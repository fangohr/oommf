# FILE: makerules.tcl
#
# This file controls the application 'pimake' by defining rules which
# describe how to build the applications and/or extensions produced by
# the source code in this directory.
#
# Last modified on: $Date: 2012/09/19 21:34:21 $
# Last modified by: $Author: donahue $
#
# Verify that this script is being sourced by pimake
if {[llength [info commands MakeRule]] == 0} {
    error "'[info script]' must be evaluated by pimake"
}
#
########################################################################

set ocObjects {
	autobuf
	byte
	imports
	messages
	nullchannel
	oc
        ocexcept
        ocfpu
        ocnuma
        octhread
}

MakeRule Define {
    -targets            all
    -dependencies       [concat configure [Platform StaticLibrary oc]]
}

MakeRule Define {
    -targets            configure
    -dependencies       [Platform Specific ocport.h]
}

MakeRule Define {
    -targets		[Platform Specific ocport.h]
    -dependencies	[concat procs.tcl config.tcl [Platform Executables varinfo]]
    -script		{Oc_MakePortHeader [Platform Executables varinfo] \
			        [Platform Specific ocport.h]
			}
}

set acceptslm 0
if {![catch {[Oc_Config RunPlatform] GetValue TCL_LIBS} _]} {
    foreach _ [split $_] {
	if {[string match -lm $_]} {
	    set acceptslm 1
	    break
	}
    }
}

if {$acceptslm} {
MakeRule Define {
    -targets		[Platform Executables varinfo]
    -dependencies	[Platform Objects varinfo]
    -script		{Platform Link -obj varinfo -sub CONSOLE \
			-[[Oc_Config RunPlatform] GetValue platform_name] \
			{-lm} -out varinfo
			}
}
} else {
MakeRule Define {
    -targets		[Platform Executables varinfo]
    -dependencies	[Platform Objects varinfo]
    -script		{Platform Link -obj varinfo -sub CONSOLE -out varinfo}
}
}
unset acceptslm

MakeRule Define {
    -targets		[Platform Objects varinfo]
    -dependencies	[concat [list [Platform Name]] \
			        [[CSourceFile New _ varinfo.cc] Dependencies]]
   -script		[subst -nocommands {Platform Compile C++ -opt 1 \
			        -inc [[CSourceFile New _ varinfo.cc] DepPath] \
			        -out varinfo -src varinfo.cc
                         }]
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


MakeRule Define {
    -targets            [Platform StaticLibrary oc]
    -dependencies	[concat tclIndex [Platform Objects $ocObjects]]
    -script		[format {
			    eval file delete [Platform StaticLibrary oc]
			    Platform MakeLibrary -out oc -obj {%s}
			    Platform IndexLibrary oc
			} $ocObjects]
}

MakeRule Define {
    -targets		[Platform Objects autobuf]
    -dependencies	[concat [list [Platform Name]] \
			        [[CSourceFile New _ autobuf.cc] Dependencies]]
    -script		{Platform Compile C++ -opt 1 \
			        -inc [[CSourceFile New _ autobuf.cc] DepPath] \
			        -out autobuf -src autobuf.cc
			}
}

# The object-building rules are uglier than I would like because of a
# "chicken and egg" problem with ocport.h.
# When ocport.h doesn't exist, the $include dependency search checks
# to see if there is a MakeRule for it.  To do that it must source
# this file.  That causes the MakeRule immediately above to be
# evaluated, and dependencies of autobuf.cc are sought.  They include
# oc.h, which is what leads us to ocport.h in the first place.
# The Dependencies check regards this as a circular dependency and fails.
# The workaround is to avoid the CSourceFile Dependencies call where a
# circular dependency occurs, and to instead directly insert all the
# dependencies into the MakeRule -dependencies.  (Of course, this is
# a maintenance headache.)

MakeRule Define {
    -targets		[Platform Objects oc]
    -dependencies	[concat [list [Platform Name]] oc.cc \
			        oc.h [Platform Specific ocport.h] \
				messages.h version.h autobuf.h \
			        [[CSourceFile New _ imports.h] Dependencies]]
    -script		{Platform Compile C++ -opt 1 \
			        -inc [[CSourceFile New _ oc.cc] DepPath] \
			        -out oc -src oc.cc
			}
}

MakeRule Define {
    -targets		[Platform Objects byte]
    -dependencies	[concat [list [Platform Name]] byte.cc \
			        [Platform Specific ocport.h] byte.h]
    -script		{Platform Compile C++ -opt 1 \
			        -inc [[CSourceFile New _ byte.cc] DepPath] \
			        -out byte -src byte.cc
			}
}

MakeRule Define {
    -targets		[Platform Objects imports]
    -dependencies	[concat [list [Platform Name]] imports.cc \
			        oc.h version.h [Platform Specific ocport.h] \
			        [[CSourceFile New _ imports.h] Dependencies] \
                                autobuf.h]
    -script		{Platform Compile C++ -opt 1 \
			        -inc [[CSourceFile New _ imports.cc] DepPath] \
			        -out imports -src imports.cc
			}
}

# Doubly ugly.  ocport.h problem plus some compilers have broken
# optimizations we have to work around
if {[catch {
    [Oc_Config RunPlatform] GetValue \
            program_compiler_c++_property_optimization_breaks_varargs
} _] || !$_} {
MakeRule Define {
    -targets		[Platform Objects messages]
    -dependencies	[concat [list [Platform Name]] messages.cc \
			        oc.h [Platform Specific ocport.h] \
				messages.h version.h autobuf.h \
			        [[CSourceFile New _ imports.h] Dependencies]]
    -script		{Platform Compile C++ -opt 1 \
			        -inc [[CSourceFile New _ messages.cc] DepPath] \
			        -out messages -src messages.cc
			}
}
} else {
MakeRule Define {
    -targets		[Platform Objects messages]
    -dependencies	[concat [list [Platform Name]] messages.cc \
			        oc.h [Platform Specific ocport.h] \
				messages.h version.h  autobuf.h  \
			        [[CSourceFile New _ imports.h] Dependencies]]
    -script		{Platform Compile C++ -opt 0 \
			        -inc [[CSourceFile New _ messages.cc] DepPath] \
			        -out messages -src messages.cc
			}
}
}

MakeRule Define {
    -targets		[Platform Objects nullchannel]
    -dependencies	[concat [list [Platform Name]] \
			        [[CSourceFile New _ nullchannel.cc] Dependencies]]
    -script		{Platform Compile C++ -opt 1 \
			        -inc [[CSourceFile New _ nullchannel.cc] DepPath] \
			        -out nullchannel -src nullchannel.cc
			}
}

MakeRule Define {
    -targets		[Platform Objects ocexcept]
    -dependencies	[concat [list [Platform Name]] \
			        [[CSourceFile New _ ocexcept.cc] Dependencies]]
    -script		{Platform Compile C++ -opt 1 \
			        -inc [[CSourceFile New _ ocexcept.cc] DepPath] \
			        -out ocexcept -src ocexcept.cc
			}
}

MakeRule Define {
    -targets		[Platform Objects ocfpu]
    -dependencies	[concat [list [Platform Name]] \
			        [[CSourceFile New _ ocfpu.cc] Dependencies]]
    -script		{Platform Compile C++ -opt 1 \
			        -inc [[CSourceFile New _ ocfpu.cc] DepPath] \
			        -out ocfpu -src ocfpu.cc
			}
}

MakeRule Define {
    -targets		[Platform Objects ocnuma]
    -dependencies	[concat [list [Platform Name]] \
			        [[CSourceFile New _ ocnuma.cc] Dependencies]]
    -script		{Platform Compile C++ -opt 1 \
			        -inc [[CSourceFile New _ ocnuma.cc] DepPath] \
			        -out ocnuma -src ocnuma.cc
			}
}

MakeRule Define {
    -targets		[Platform Objects octhread]
    -dependencies	[concat [list [Platform Name]] \
			        [[CSourceFile New _ octhread.cc] Dependencies]]
    -script		{Platform Compile C++ -opt 1 \
			        -inc [[CSourceFile New _ octhread.cc] DepPath] \
			        -out octhread -src octhread.cc
			}
}

MakeRule Define {
    -targets		upgrade
    -script		{DeleteFiles functions.h em.tcl}
}

MakeRule Define {
    -targets            distclean
    -dependencies       clean
    -script             {DeleteFiles [Platform Specific ocport.h]
			 DeleteFiles [Platform Executables varinfo]
			 DeleteFiles [Platform Name]}
}

MakeRule Define {
    -targets            clean
    -dependencies       mostlyclean
}

MakeRule Define {
    -targets            mostlyclean
    -dependencies       objclean
    -script             {eval DeleteFiles [Platform StaticLibrary oc]
			 DeleteFiles tclIndex
			}
}

MakeRule Define {
    -targets            objclean
    -dependencies       {}
    -script             [format {
			    eval DeleteFiles [Platform Objects {varinfo %s}]
			    eval DeleteFiles [Platform Intermediate varinfo]
			} $ocObjects]
}

unset ocObjects
