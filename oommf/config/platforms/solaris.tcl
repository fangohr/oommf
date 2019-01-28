# FILE: solaris.tcl
#
# Configuration feature definitions for the configuration 'solaris'
#
# Editing instructions begin at "START EDIT HERE" below.

set config [Oc_Config RunPlatform]

set scriptfn [Oc_DirectPathname [info script]]
if {![string match [string tolower [file rootname [file tail $scriptfn]]] \
        [$config GetValue platform_name]]} {
    error "Configuration file '$scriptfn'
sourced by '[$config GetValue platform_name]'"
}

set localfn [file join [file dirname $scriptfn] local \
                [file tail $scriptfn]]
if {[file readable $localfn]} {
    if {[catch {source $localfn} msg]} {
        global errorInfo errorCode
	set msg [join [split $msg \n] \n\t]
	error "Error sourcing local platform file:\n    $localfn:\n\t$msg" \
		$errorInfo $errorCode
    }
}

if {[catch {$config GetValue program_compiler_c++_override}] \
       && ![catch {$config GetValue program_compiler_c++} _]} {
   # If program_compiler_c++ is set, but program_compiler_c++_override
   # is not, then assume user set the former instead of the latter,
   # and so copy the former to the latter to preserve the setting
   # across the setting of program_compiler_c++ in the "REQUIRED
   # CONFIGURATION" section below.
   $config SetValue program_compiler_c++_override $_
}

# Environment variable override for C++ compiler.  The string OOMMF_C++
# is an invalid name in Unix shells, so also allow OOMMF_CPP
if {[info exists env(OOMMF_C++)]} {
   $config SetValue program_compiler_c++_override $env(OOMMF_C++)
} elseif {[info exists env(OOMMF_CPP)]} {
   $config SetValue program_compiler_c++_override $env(OOMMF_CPP)
}

# Support for the automated buildtest scripts
if {[info exists env(OOMMF_BUILDTEST)] && $env(OOMMF_BUILDTEST)} {
   source [file join [file dirname [info script]] buildtest.tcl]
}


########################################################################
# START EDIT HERE
# In order to properly build, install, and run on your computing 
# platform, the OOMMF software must know certain features of your
# computing environment.  In this file are lines which set the value of 
# certain features of your computing environment.  Each line looks like:
#
# $config SetValue <feature> {<value>}
#
# where each <feature> is the name of some feature of interest,
# and <value> is the value which is assigned to that feature in a
# description of your computing environment.  Your task is to edit
# the values as necessary to properly describe your computing 
# environment.
#
# The character '#' at the beginning of a line is a comment character.
# It causes the contents of that line to be ignored.  To select
# among lines providing alternative values for a feature, uncomment the
# line containing the proper value.
#
# The features in this file are divided into three sections.  The
# first section (REQUIRED CONFIGURATION) includes features which
# require you to provide a value.  The second section (LOCAL
# CONFIGURATION) includes features which have usable default values,
# but which you may wish to customize.  These can be edited here, but
# it is recommended instead that you create a subdirectory named
# "local", put a copy of the LOCAL CONFIGURATION section there in a
# file with the same name as this file, and then edit that file.  The
# third section (BUILD CONFIGURATION) contains features which you
# probably do not need or want to change without a good reason.
#
########################################################################
# REQUIRED CONFIGURATION

# Set the feature 'program_compiler_c++' to the program to run on this 
# platform to compile source code files written in the language C++ into 
# object files.  Select from the choices below.  If the compiler is not 
# in your path, be sure to use the whole pathname.  Also include any 
# options required to instruct your compiler to only compile, not link.  
#
# If your compiler is not listed below, additional features will have
# to be added in the BUILD CONFIGURATION section below to describe to
# the OOMMF software how to operate your compiler.  Send e-mail to the
# OOMMF developers for assistance.
#
# Sun Microsystems WorkShop C++ compiler
# <URL:http://www.sun.com/developer-products/>
$config SetValue program_compiler_c++ {CC -ptr./solaris -c}
#
# The GNU C++ compiler 'g++'
# <URL:http://www.gnu.org/software/gcc/gcc.html>
# <URL:http://egcs.cygnus.com/>
# $config SetValue program_compiler_c++ {g++ -c}
#
# NOTE: If the g++ compiler reports lots of errors like:
#
#	ANSI C++ forbids declaration `XSetTransientForHint' with no type
#
# then it is likely that your system has X header files which are not
# ANSI compliant (perhaps part of OpenWindows).  You may be able to work
# around these errors with the following definition of the compiler:
# $config SetValue program_compiler_c++ {g++ -fpermissive -c}

########################################################################
# LOCAL CONFIGURATION
#
# The following options may be defined in the
# platforms/local/solaris.tcl file:
#
## Set the feature 'path_directory_temporary' to the name of an existing 
## directory on your computer in which OOMMF software should write 
## temporary files.  All OOMMF users must have write access to this 
## directory.
# $config SetValue path_directory_temporary {/tmp}
#
## Specify whether or not to build in thread support.
## Thread support is included automatically if the tclsh interpreter used
## during the build process is threaded.  If you have a thread enabled
## tclsh, but don't want oommf_threads, override here.
# $config SetValue oommf_threads 0  ;# 1 to force threaded build,
#                                   ## 0 to force non-threaded build.
#
## Specify the number of default threads.  This is only meaningful
## for builds with thread support.
# $config SetValue thread_count 4  ;# Replace '4' with desired thread count.
#
## Specify hard limit on the max number of threads per process.  This is
## only meaningful for builds with thread support.  If not set, then there
## is no limit.
# $config SetValue thread_limit 8
#
## Override default C++ compiler.  Note the "_override" suffix
## on the value name.
# $config SetValue program_compiler_c++_override {g++ -c}
#
## Variable type used for array indices, OC_INDEX.  This is a signed
## type which by default is sized to match the pointer width.  You can
## force the type by setting the following option.  The value should
## be a three item list, where the first item is the name of the
## desired (signed) type, the second item is the name of the
## corresponding unsigned type, and the third is the width of these
## types, in bytes.  It is assumed that both the signed and unsigned
## types are the same width, as otherwise significant code breakage is
## expected.  Example:
# $config SetValue program_compiler_c++_oc_index_type {int {unsigned int} 4}
#
## For OC_INDEX type checks.  If set to 1, then various segments in
## the code are activated which will detect some array index type
## mismatches at link time.  These tests are not comprehensive, and
## will probably break most third party code, but may be useful during
## development testing.
# $config SetValue program_compiler_c++_oc_index_checks 1
#
###################
# Default handling of local defaults:
#
if {[catch {$config GetValue oommf_threads}]} {
   # Value not set in platforms/local/solaris.tcl,
   # so use Tcl setting.
   global tcl_platform
   if {[info exists tcl_platform(threaded)] \
          && $tcl_platform(threaded)} {
      $config SetValue oommf_threads 1  ;# Yes threads
   } else {
      $config SetValue oommf_threads 0  ;# No threads
   }
}
$config SetValue thread_count_auto_max 4 ;# Arbitrarily limit
## maximum number of "auto" threads to 4.
if {[catch {$config GetValue thread_count}]} {
   # Value not set in platforms/local/solaris.tcl, so use
   # psrinfo to report the number of processors
   if {(![catch {set count [exec psrinfo -p]}] &&
       [regexp {[0-9]+} $count])} {
      set auto_max [$config GetValue thread_count_auto_max]
      if {$count>$auto_max} {
         # Limit automatically set thread count to auto_max
         set count $auto_max
      }
      $config SetValue thread_count $count
   }
}
if {[catch {$config GetValue program_compiler_c++_override} compiler] == 0} {
    $config SetValue program_compiler_c++ $compiler
}

# The absolute, native filename of the null device
$config SetValue path_device_null {/dev/null}

# Are we building OOMMF, or running it?
if {![info exists env(OOMMF_BUILD_ENVIRONMENT_NEEDED)] \
       || !$env(OOMMF_BUILD_ENVIRONMENT_NEEDED)} {
   # Remainder of script concerns the build environment only,
   # none of which is not relevant at run time.
   unset config
   return
}

########################################################################
# BUILD CONFIGURATION

# Compiler option processing...
set ccbasename [file tail [lindex [$config GetValue program_compiler_c++] 0]]
if {[string match CC $ccbasename]} {
    # ...for Sun WorkShop C++ compiler
    # set opts [list -O%s] ;# Minimal optimization
    set opts -fast         ;# Maximal optimization
    # NOTE: If you want good performance, be sure to edit ../options.tcl
    #  or ../local/options.tcl to include the line

    # 64-bit?
    if {[info exists tcl_platform(pointerSize)]
        && $tcl_platform(pointerSize)>4} {
       # NOTE: The -xarch option has to come AFTER -xtarget, -fast, or
       # any other option that sets -xarch.
       lappend opts "-xarch=v9"
       lappend linkopts "-xarch=v9"
    }

    # Finish compiler options setting
    $config SetValue program_compiler_c++_option_opt "format \"$opts\""

    #    Oc_Option Add * Platform cflags {-def NDEBUG}
    #  so that the NDEBUG symbol is defined during compile.
    $config SetValue program_compiler_c++_option_out {format "-o \"%s\""}
    $config SetValue program_compiler_c++_option_src {format \"%s\"}
    $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
    $config SetValue program_compiler_c++_option_debug {format "-g"}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}

    # Compiler warnings:
    #$config SetValue program_compiler_c++_option_warn {format "-xwe +w"}
    # ...The +w warnings appear to be largely broken
    $config SetValue program_compiler_c++_option_warn {format "-xwe"}

    # Widest natively support floating point type
    if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }

    # Prototype for math function hypot(x,y) may be missing from
    # system <cmath> include file.  Any one of the following three
    # options should work around this problem:
    # $config SetValue program_compiler_c++_property_bad_cmath_header 1
    # $config SetValue program_compiler_c++_property_no_hypot 1
    $config SetValue program_compiler_c++_prototype_supply_hypot \
      {extern "C" double hypot(double,double);}

    # The STL map<> on this platform doesn't like 'const string' as
    # a key.
    $config SetValue \
            program_compiler_c++_property_stl_map_broken_const_key 1

} elseif {[string match g++ $ccbasename]} {
    # ...for GNU g++ C++ compiler
    # NOTE: If you want good performance, be sure to edit ../options.tcl
    #  or ../local/options.tcl to include the line
    #    Oc_Option Add * Platform cflags {-def NDEBUG}
    #  so that the NDEBUG symbol is defined during compile.

    # Load and run gcc support-routines to generate generic optimization
    # flags based on gcc version.
    source [file join [file dirname [Oc_DirectPathname [info script]]] \
	    gcc-support.tcl]
    if {![info exists gcc_version]} {
	set gcc_version [GuessGccVersion \
		[lindex [$config GetValue program_compiler_c++] 0]]
    }
    if {[lindex $gcc_version 0]<4 ||
       ([lindex $gcc_version 0]==4 && [lindex $gcc_version 1]<5)} {
        puts stderr "WARNING: This version of OOMMF requires g++ 4.5\
                   or later (C++ 11 support)"
    }
    set opts [GetGccGeneralOptFlags $gcc_version]

    # Otherwise, override opts here:c
    # set opts [list -O%s]
    # set opts [list -O3 -ffast-math -mcpu=ultrasparc]

    # Default warnings disable
    set nowarn [list -Wno-non-template-friend -Wno-unknown-pragmas]
    if {[lindex $gcc_version 0]>=6} {
       lappend nowarn {-Wno-misleading-indentation}
    }
    if {[info exists nowarn] && [llength $nowarn]>0} {
       set opts [concat $opts $nowarn]
    }
    catch {unset nowarn}

    $config SetValue program_compiler_c++_option_opt "format \"$opts\""

    $config SetValue program_compiler_c++_option_out {format "-o \"%s\""}
    $config SetValue program_compiler_c++_option_src {format \"%s\"}
    $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}

    # Compiler warnings:
    # I would also like to use -Wredundant-decls -Wshadow
    # but unclean system header files prevent this.
    # I would also like to use -Wcast-qual, but casting away const is 
    # needed on some occasions to provide "conceptual const" functions in 
    # place of "bitwise const"; cf. p76-78 of Meyer's book, "Effective C++."
    #
    # NOTE: -Wno-uninitialized is required after -Wall by gcc 2.8+ because
    # of an apparent bug
    set warnflags [list -Wall \
	-W -Wpointer-arith -Wwrite-strings \
	-Wstrict-prototypes \
	-Winline -Woverloaded-virtual -Werror \
	-Wno-unused-function]
    # Default warnings disable
    set nowarn {}
    if {[info exists nowarn] && [llength $nowarn]>0} {
	lappend warnflags $nowarn
    }
    catch {unset nowarn}
    $config SetValue program_compiler_c++_option_warn \
	    [list format $warnflags]
    unset warnflags

    $config SetValue program_compiler_c++_option_debug {format "-g"}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}

    # Widest natively support floating point type
    if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }

    # Directories to exclude from explicit include search path, i.e.,
    # the -I list.  Some versions of gcc complain if "system" directories
    # appear in the -I list.
    $config SetValue \
    	program_compiler_c++_system_include_path [list /usr/include]
}

# The program to run on this platform to link together object files and
# library files to create an executable binary.
#
# Use the selected compiler to control the linking.
$config SetValue program_linker [lreplace \
        [$config GetValue program_compiler_c++] end end]

# Linker option processing...
set lbasename [file tail [lindex [$config GetValue program_linker] 0]]
if {[string match CC $lbasename]} {
    # ...for Sun CC as linker
    #
    set linkprog [$config GetValue program_linker]
    if {[info exists linkopts]} { lappend linkprog $linkopts}

    # NOTE: We had to add the following option to get OOMMF to build
    # on Solaris 2.7 (SunOS 5.7) with the compiler Sun WorkShop 6,
    # 2000/04/07 C++ 5.1.  If you have trouble building OOMMF, try
    # commenting out the following line:
    lappend linkprog "-staticlib=Crun"

    $config SetValue program_linker $linkprog
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_rpath {format "-R %s"}
    $config SetValue program_linker_uses_-L-l {1}
} elseif {[string match g++ $lbasename]} {
    # ...for GNU g++ as linker
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_rpath {format "-Wl,-rpath=%s"}
    $config SetValue program_linker_uses_-L-l {1}
}
unset lbasename

# The program to run on this platform to create a single library file out
# of many object files.
if {[string match CC $ccbasename]} {
    $config SetValue program_libmaker {CC -xar -ptr./solaris -o} ;# Do templates
    # Option processing for ar
    $config SetValue program_libmaker_option_obj {format \"%s\"}
    $config SetValue program_libmaker_option_out {format \"%s\"}
} elseif {[string match g++ $ccbasename]} {
    $config SetValue program_libmaker {ar cr}
    # Option processing for ar
    $config SetValue program_libmaker_option_obj {format \"%s\"}
    $config SetValue program_libmaker_option_out {format \"%s\"}
}
unset ccbasename

# A partial Tcl command (or script) which when completed by lappending
# a file name stem and evaluated returns the corresponding file name for
# an executable on this platform
$config SetValue script_filename_executable {format %s}

# A partial Tcl command (or script) which when completed by lappending
# a file name stem and evaluated returns the corresponding file name for
# an object file on this platform
$config SetValue script_filename_object {format %s.o}

# A partial Tcl command (or script) which when completed by lappending
# a file name stem and evaluated returns the corresponding file name for
# a static library on this platform
$config SetValue script_filename_static_library {format lib%s.a}

########################################################################
unset config

