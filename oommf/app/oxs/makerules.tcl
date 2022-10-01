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
# Allow these Makerules to be ignored
global env
if {[info exists env(OOMMF_NO_BUILD_OXS)]} {
    return -code continue
}

set executables [list oxs demagtensor]
# We do not yet distribute any Preisach models
#if {![info exists env(OOMMF_NO_BUILD_PREISACH)]} {
#    lappend executables opmsh
#}

# Base source modules
set objects {
    arrayscalarfield
    atlas
    chunkenergy
    director
    driver
    energy
    evolver
    ext
    exterror
    labelvalue
    lock
    mesh
    meshvalue
    output
    outputderiv
    oxs
    oxscmds
    oxsexcept
    oxsthread
    oxswarn
    scalarfield
    simstate
    threevector
    uniformscalarfield
    uniformvectorfield
    util
    vectorfield
}

# Extension modules.  Note that extsrcs includes relative path and .cc
# extension, whereas extobjs holds just bare base names.  Sort these
# so that any order-dependent bugs will at least be reproducible.
set extsrcs [lsort [glob -nocomplain -- [file join ext *.cc]]]
set extobjs {}
foreach src $extsrcs {
    lappend extobjs [file rootname [file tail $src]]
}

set lclsrcs [list]
foreach subdir [Oc_FindSubdirectories local] {
   lappend lclsrcs {*}[glob -nocomplain -directory $subdir -- *.cc *.cpp *.C *.cxx]
}
set lclsrcs [lsort $lclsrcs]
set lclobjs {}
foreach src $lclsrcs {
   lappend lclobjs [join [file split [file rootname $src]] _]
}
# Note: The lclsrcs compilation command further down assumes that the
# number and order of lclsrcs and lclobjs match.

# Library dependencies extension modules
set RULESGLOB [list *.rules]
set rules [list]
foreach subdir [Oc_FindSubdirectories local] {
   lappend rules {*}[glob -nocomplain -directory $subdir -types f {*}$RULESGLOB]
}
set lcllibs [list]
foreach fn $rules {
   # Each rules file is a Tcl script that potentially sets the variable
   # Oxs_external_libs to a list of external libraries (stems only)
   # needed by an extension. (Maintenance note: This code is also used
   # by the oxspkg "requires" command. Changes here should be reflected
   # there.)
   if {[file readable $fn]} {
      set chan [open $fn r]
      set contents [read $chan]
      close $chan
      set slave [interp create -safe]
      $slave eval $contents
      if {![catch {$slave eval set Oxs_external_libs} libs]} {
	 lappend lcllibs {*}$libs
      }
      interp delete $slave
   }
}
set lcllibs [lsort -unique $lcllibs]

set psrcs [lsort [glob -nocomplain -- [file join preisach *.cc]]]
set pobjs {}
foreach src $psrcs {
    lappend pobjs [file rootname [file tail $src]]
}

MakeRule Define {
    -targets		all
    -dependencies       [concat configure [Platform Specific appindex.tcl]]
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
    -dependencies	[Platform Executables $executables]
    -script		[format {
        puts "Updating [Platform Specific appindex.tcl]"
        set f [open [Platform Specific appindex.tcl] w]
	foreach e {%s} {
            puts $f "
                Oc_Application Define {
                    -name		[list $e]
                    -version		2.0b0
                    -machine		[list [Platform Name]]
                    -file		[list [file tail \
					[Platform Executables [list $e]]]]
                }"
	}
        close $f
    } $executables]
}

set oxslibs {vf xp nb oc}
set fn [file join local makeextras.tcl]
if {[file readable $fn]} { source $fn }
MakeRule Define {
    -targets		[Platform Executables oxs]
    -dependencies	[concat [Platform Objects $objects] \
				[Platform Objects extinit] \
				[Platform Objects $extobjs] \
				[Platform Objects $lclobjs] \
			        [Platform StaticLibraries $oxslibs] \
				[file join base tclIndex]]
    -script		[format {
			    Platform Link -obj {%s %s %s extinit} \
			            -lib {%s %s tk tcl} \
			            -sub CONSOLE -out oxs
			} $objects $extobjs $lclobjs $oxslibs $lcllibs]
}

set dtobjs [list demagtensor demagcoef oxswarn oxsthread oxsexcept]
MakeRule Define {
    -targets		[Platform Executables demagtensor]
    -dependencies	[concat [Platform Objects $dtobjs] \
			        [Platform StaticLibraries $oxslibs] \
				[file join base tclIndex]]
    -script		[format {
			    Platform Link -obj {%s} \
			            -lib {%s tk tcl} \
			            -sub CONSOLE -out demagtensor
			} $dtobjs $oxslibs]
}
unset dtobjs
MakeRule Define {
   -targets	     [Platform Objects demagtensor]
   -dependencies     [concat [list [Platform Name]] \
           [[CSourceFile New _ demagtensor.cc -inc base] Dependencies]]
   -script           [format {Platform Compile C++ -opt 1 \
                         -inc [[CSourceFile New _ %s -inc base] DepPath] \
                         -out %s -src %s
                      } demagtensor.cc demagtensor demagtensor.cc]
}

MakeRule Define {
    -targets		[Platform Executables opmsh]
    -dependencies	[concat [Platform Objects $pobjs] \
			        [Platform StaticLibraries {nb oc}]]
    -script		[format {
			    Platform Link -obj {%s} \
			            -lib {nb oc tk tcl} \
			            -sub WINDOWS -out opmsh
			} $pobjs ]
}

foreach obj $objects {
    set src [file join base $obj.cc]
    MakeRule Define {
        -targets	[Platform Objects $obj]
        -dependencies	[concat [list [Platform Name]] \
		[[CSourceFile New _ $src] Dependencies]]
        -script		[format {Platform Compile C++ -opt 1 \
			        -inc [[CSourceFile New _ %s] DepPath] \
			        -out %s -src %s
			} $src $obj $src]
    }
}
unset obj src

set path base
foreach src $extsrcs {
    set obj [file rootname [file tail $src]]
    MakeRule Define {
        -targets      [Platform Objects $obj]
        -dependencies [concat [list [Platform Name]] \
		[[CSourceFile New _ $src -inc $path] Dependencies]]
        -script       [format {Platform Compile C++ -opt 1 \
		-inc [[CSourceFile New _ %s -inc {%s}] DepPath] \
		-out %s -src %s
			} $src $path $obj $src]
    }
    unset obj
}
catch {unset src}
unset path

set path [list base ext]
foreach src $lclsrcs obj $lclobjs {
    MakeRule Define {
        -targets      [Platform Objects $obj]
        -dependencies [concat [list [Platform Name]] \
		[[CSourceFile New _ $src -inc $path] Dependencies]]
        -script       [format {Platform Compile C++ -opt 1 \
		-inc [[CSourceFile New _ %s -inc {%s}] DepPath] \
		-out %s -src %s
			} $src $path $obj $src]
    }
    unset obj
}
catch {unset src}
unset path

set path [list]
foreach src $psrcs {
    set obj [file rootname [file tail $src]]
    MakeRule Define {
        -targets      [Platform Objects $obj]
        -dependencies [concat [list [Platform Name]] \
		[[CSourceFile New _ $src -inc $path] Dependencies]]
        -script       [format {Platform Compile C++ -opt 1 \
		-inc [[CSourceFile New _ %s -inc {%s}] DepPath] \
		-out %s -src %s
			} $src $path $obj $src]
    }
    unset obj
}
catch {unset src}
unset path

##########################################################################
# extinit.cc and extinit.o files.  These might disappear in the future.
set src [file join base extinit.cc]
set header [file join base ext.h]
MakeRule Define {
    -targets		[Platform Objects extinit]
    -dependencies	[concat [list [Platform Name]] $src \
		        	[[CSourceFile New _ $header] Dependencies]]
    -script		[format {Platform Compile C++ -opt 1 \
			        -inc [[CSourceFile New _ %s] DepPath] \
			        -out extinit -src %s
			} $src $src]
}
unset src header
set extsources {}
set src [file join base extinit.cc]
MakeRule Define {
    -targets            $src
    -dependencies       $extsources
    -script             [concat MakeOxsExtInit $src $extsources]
}
unset src extsources
##########################################################################

MakeRule Define {
    -targets		upgrade
    -script		{DeleteFiles \
             [file join ext atlasscalarfieldinit.cc] \
             [file join ext atlasscalarfieldinit.h]  \
             [file join ext atlasvectorfieldinit.cc] \
             [file join ext atlasvectorfieldinit.h]  \
             [file join ext filevectorfieldinit.cc] \
             [file join ext filevectorfieldinit.h]  \
             [file join ext planerandomvectorfieldinit.cc] \
             [file join ext planerandomvectorfieldinit.h]  \
             [file join ext randomvectorfieldinit.cc] \
             [file join ext randomvectorfieldinit.h]  \
             [file join ext rectangularregion.cc] \
             [file join ext rectangularregion.h]  \
             [file join ext rectangularsection.cc] \
             [file join ext rectangularsection.h]  \
             [file join ext scriptscalarfieldinit.cc] \
             [file join ext scriptscalarfieldinit.h]  \
             [file join ext scriptvectorfieldinit.cc] \
             [file join ext scriptvectorfieldinit.h]  \
             [file join ext sectionatlas.cc] \
             [file join ext sectionatlas.h]  \
             [file join ext standarddriver.cc] \
             [file join ext standarddriver.h]  \
             [file join ext uniformscalarfieldinit.cc] \
             [file join ext uniformscalarfieldinit.h]  \
             [file join ext uniformvectorfieldinit.cc] \
             [file join ext uniformvectorfieldinit.h]  \
             [file join base region.cc] \
             [file join base region.h]  \
             [file join base scalarfieldinit.cc] \
             [file join base scalarfieldinit.h]  \
             [file join base section.cc] \
             [file join base section.h]  \
             [file join base vectorfieldinit.cc] \
             [file join base vectorfieldinit.h]  \
    }
}

MakeRule Define {
    -targets            distclean
    -dependencies       clean
    -script             {DeleteFiles [Platform Name] base/extinit.cc}
}
 
MakeRule Define {
    -targets            clean
    -dependencies       mostlyclean
}
 
MakeRule Define {
    -targets            mostlyclean
    -dependencies       objclean
    -script             [format {eval DeleteFiles [concat \
			        [Platform Specific appindex.tcl] \
			        [Platform Executables {%s}]]
                         Recursive mostlyclean} $executables]
}

MakeRule Define {
    -targets            objclean
    -dependencies       {}
    -script             [format {
       eval DeleteFiles [Platform Objects {%s %s %s %s demagtensor extinit}]
       eval DeleteFiles [Platform Intermediate {%s %s %s %s %s extinit}]
    } $objects $extobjs $lclobjs $pobjs \
      $objects $extobjs $lclobjs $pobjs $executables]
}

unset objects extobjs lclobjs lcllibs pobjs executables oxslibs
