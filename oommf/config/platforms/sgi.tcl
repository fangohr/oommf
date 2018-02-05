# FILE: sgi.tcl
#
# Configuration feature definitions for the configuration 'sgi'
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

# Environment variable override for C++ compiler.  Use OOMMF_CPP rather
# than OOMMF_C++ because the latter is an invalid name in Unix shells.
if {[info exists env(OOMMF_CPP)]} {
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
# SGI's MIPSpro C++ compiler
# <URL:http://www.sgi.com/developers/devtools/languages/mipspro.html>
# Specify -32, -o32, -n32 or -64 as necessary to match your Tcl/Tk
#   libraries.  Likely matches are -n32 for Tcl/Tk 8.0.3 and newer,
#   and -o32 for older Tcl/Tk.  If you are using the -64 switch,
#   make sure too that the system 64-bit library directories (e.g.,
#   /usr/lib64) are on LD_LIBRARY_PATH.  Also, you will have to
#   have a 64-bit build of Tcl/Tk.
if {[catch {$config GetValue program_compiler_c++_override} _] || \
       [string match CC* $_]} {
   # Select appropriate CC switches
   if {![catch {set tcl_platform(wordSize)} size] && $size != 8} {
      foreach {pla plb plc} [split [info patchlevel] .] { break }
      if {$pla>8 || ($pla == 8 && $plb>0) || \
             ($pla == 8 && $plb == 0 && $plc >=3)} {
         set ccstr {CC -n32 -LANG:std -c}
      } else {
         set ccstr {CC -o32 -LANG:std:libc_in_namespace_std=OFF -c}
      }
   } else {
      # set ccstr {CC -64 -LANG:std:libc_in_namespace_std=OFF -c}
      set ccstr {CC -64 -LANG:std -c}
   }
   $config SetValue program_compiler_c++ $ccstr
   if {![catch {$config GetValue program_compiler_c++_override}]} {
      # Expand override to correct value
      $config SetValue program_compiler_c++_override $ccstr
   }
   unset ccstr
}
# The "libc_in_namespace_std=OFF" option is protection against broken
# header files in some versions of the compiler and/or broken -Ofast
# switches. YMMV.
#
# The GNU C++ compiler 'g++'
# <URL:http://www.gnu.org/software/gcc/gcc.html>
# <URL:http://egcs.cygnus.com/>
# It appears that gcc will not build -o32 code.  So must link
#   against -n32 Tcl/Tk libraries, which is the default for
#   Tcl/Tk 8.0.3 and later.
set gnucc "g++" ;# OOMMF 1.1 can use either gcc or g++; 1.2+ needs g++
#$config SetValue program_compiler_c++ "$gnucc -c"

########################################################################
# LOCAL CONFIGURATION
#
# The following options may be defined in the
# platforms/local/sgi.tcl file:
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
   # Value not set in platforms/local/litanium.tcl,
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
   # Value not set in platforms/local/sgi.tcl, so use
   # getconf to report the number of "online" processors
   if {(![catch {set count [exec sysconf NPROC_ONLN]}] &&
        [regexp {[0-9]+} $count]) ||
       (![catch {set count [exec sysconf _NPROCESSORS_ONLN]}] &&
        [regexp {[0-9]+} $count]) ||
       (![catch {set count [exec getconf NPROC_ONLN]}] &&
        [regexp {[0-9]+} $count]) ||
       (![catch {set count [exec getconf _NPROCESSORS_ONLN]}] &&
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
   if {![string match {} $compiler]} {
      $config SetValue program_compiler_c++ $compiler
   }
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
set id32 ""
set id64 ""
set langopt ""
foreach _ [lrange [$config GetValue program_compiler_c++] 1 end] {
    if {[string match -*32 $_]} {
        set id32 $_
    }
    if {[string match -*64 $_]} {
        set id64 $_
    }
    if {[string match -LANG* $_]} {
	set langopt $_
    }
}
unset _
if {![info exists gnucc]} {
	set gnucc {}  ;# Safety.
}
if {[string match CC $ccbasename]} {
    # ...for SGI's compiler CC
    set opts {}
    #lappend opts -O-OPT:Olimit=25000
    #lappend opts -Ofast
    lappend opts -O2
    ## If you specify -Ofast here, then you must also specify -IPA
    ## in the program_linker line below.  Also, at least some versions
    ## of CC don't like the combination '-32 -Ofast', so you may have
    ## to avoid 'CC -32 -c' above in favor of 'CC -n32 -c'.
    ## Oxsii/boxsi built using version 7.4.4m of CC with -64 and IPA
    ## segfault.  As always, YMMV.

    # Disable certain spurious warnings.
    # Warning 3896: non-template friend to templated entity
    # Warning 3970: conversion from pointer to same-sized integral type
    # lappend opts -woff 3896,3970

    $config SetValue program_compiler_c++_option_opt "format \"$opts\""
    # NOTE: If you want good performance, be sure to edit ../options.tcl
    #  or ../local/options.tcl to include the line
    #    Oc_Option Add * Platform cflags {-def NDEBUG}
    #  so that the NDEBUG symbol is defined during compile.

    $config SetValue program_compiler_c++_option_out {format "-o \"%s\""}
    $config SetValue program_compiler_c++_option_src {format \"%s\"}
    $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
    $config SetValue program_compiler_c++_option_warn {format "-fullwarn"}
    $config SetValue program_compiler_c++_option_debug {format "-g"}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}

    # Wide floating point type.  Defaults to double, but you can
    # change this to "long double" for extra precision at greatly
    # reduced speed.
    # $config SetValue program_compiler_c++_typedef_realwide "long double"

    # Experimental: The OC_REAL8m type is intended to be at least
    # 8 bytes wide.  Generally OC_REAL8m is typedef'ed to double,
    # but you can try setting this to "long double" for extra
    # precision (and extra slowness).
    # $config SetValue program_compiler_c++_typedef_real8m "long double"

    # Broken cmath header?
    $config SetValue program_compiler_c++_property_bad_cmath_header 1

    # No strerror_r() routine.
    $config SetValue program_compiler_c++_property_no_strerror_r 1

} elseif {[string match $gnucc $ccbasename]} {
    # ...for GNU g++ C++ compiler

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

    # set opts "-O%s"  ;# Default optimization

    # Disable some default warnings in the opts switch, as opposed
    # to the warnings switch below, so that these warnings are always
    # muted, even if '-warn' option in file options.tcl is disabled.
    if {[lindex $gcc_version 0]>=3} {
       lappend nowarn [list -Wno-non-template-friend]
       # OOMMF code conforms to the new standard.  Silence this
       # "helpful" but inaccurate warning.
    }
    if {[info exists nowarn] && [llength $nowarn]>0} {
       set opts [concat $opts $nowarn]
    }
    catch {unset nowarn}

    $config SetValue program_compiler_c++_option_opt "format \"$opts\""
    # NOTE: If you want good performance, be sure to edit ../options.tcl
    #  or ../local/options.tcl to include the line
    #    Oc_Option Add * Platform cflags {-def NDEBUG}
    #  so that the NDEBUG symbol is defined during compile.

    $config SetValue program_compiler_c++_option_out {format "-o \"%s\""}
    $config SetValue program_compiler_c++_option_src {format \"%s\"}
    $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
    $config SetValue program_compiler_c++_option_debug {format "-g"}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}
 
    # Compiler warnings:
    # Omitted: -Wredundant-decls -W -shadow
    # I would also like to use -Wcast-qual, but casting away const is
    # needed on some occasions to provide "conceptual const" functions in
    # place of "bitwise const"; cf. p76-78 of Meyer's book, "Effective C++."
    #
    # NOTE: -Wno-uninitialized is required after -Wall by gcc 2.8+ because
    # of an apparent bug
    set warnings [list -Wall \
                     -Wpointer-arith -Wbad-function-cast -Wwrite-strings \
                     -Wstrict-prototypes -Wmissing-declarations \
                     -Wnested-externs -Winline -Woverloaded-virtual \
                     -Wsynth -Werror -Wcast-align]
    set nowarn {}
    if {[info exists nowarn] && [llength $nowarn]>0} {
       set warnings [concat $warnings $nowarn]
    }
    catch {unset nowarn}

    $config SetValue program_compiler_c++_option_warn "format [list $warnings]"

    # Wide floating point type.  Defaults to double, but you can
    # change this to "long double" for extra precision at greatly
    # reduced speed.
    # $config SetValue program_compiler_c++_typedef_realwide "long double"

    # Experimental: The OC_REAL8m type is intended to be at least
    # 8 bytes wide.  Generally OC_REAL8m is typedef'ed to double,
    # but you can try setting this to "long double" for extra
    # precision (and extra slowness).
    # $config SetValue program_compiler_c++_typedef_real8m "long double"

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
if {[string match CC [file tail [lindex \
        [$config GetValue program_compiler_c++] 0]]]} {
    set linkcmdline [lindex [$config GetValue program_compiler_c++] 0]
    if {![string match {} $id32]} {lappend linkcmdline $id32}
    if {![string match {} $id64]} {lappend linkcmdline $id64}
    if {![string match {} $langopt]} {lappend linkcmdline $langopt}

    # Disable "Multiply defined" warnings from linker
    lappend linkcmdline "-Wl,-woff15"

    # Include -IPA if you specified -Ofast in the compile line above.
    # There is an optimization bug affecting routines taking a variable
    # number of args (C++ "..." specification; cf. stdargs.h); one fix
    # is to turn off "aggressive interprocedural constant propagation"
    # with the linker option '-IPA:aggr_cprop=OFF'.
    # lappend linkcmdline "-IPA:aggr_cprop=OFF"

    $config SetValue program_linker $linkcmdline
    unset linkcmdline
} else {
    $config SetValue program_linker [lindex \
            [$config GetValue program_compiler_c++] 0]
}

# Linker option processing...
set lbasename [file tail [lindex [$config GetValue program_linker] 0]]
if {[string match CC $lbasename]} {
    # ...for SGI's CC as linker
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_uses_-L-l {1}
} elseif {[string match $gnucc $lbasename]} {
    # ...for GNU g++ as linker
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_uses_-L-l {1}
}
unset lbasename

# The program to run on this platform to create a single library file out
# of many object files.
if {[string match CC $ccbasename]} {
    # Use CC to get pre-linker run.
    set libmaker $ccbasename
    if {![string match {} $id32]} {lappend libmaker $id32}
    if {![string match {} $id64]} {lappend libmaker $id64}
    if {![string match {} $langopt]} {lappend libmaker $langopt}
    lappend libmaker -ar -o
    $config SetValue program_libmaker $libmaker
    unset libmaker
    # Option processing for ar
    $config SetValue program_libmaker_option_obj {format \"%s\"}
    $config SetValue program_libmaker_option_out {format \"%s\"}
} elseif {[string match $gnucc $ccbasename]} {
    $config SetValue program_libmaker {ar cr}
    # Option processing for ar
    $config SetValue program_libmaker_option_obj {format \"%s\"}
    $config SetValue program_libmaker_option_out {format \"%s\"}
}
unset ccbasename id32 id64 langopt gnucc


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

# A list of partial Tcl commands (or scripts) which when completed by
# lappending a file name stem and evaluated returns the corresponding
# file name for an intermediate file produced by the linker on this platform
$config SetValue script_filename_intermediate [list {format ii_files/%s.ii}]

########################################################################
unset config

