# FILE: litanium.tcl
#
# Configuration feature definitions for the configuration 'litanium'
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

########################################################################
# OPTIONAL CONFIGURATION

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
# The GNU C++ compiler 'g++'
# <URL:http://www.gnu.org/software/gcc/gcc.html>
# <URL:http://egcs.cygnus.com/>
# $config SetValue program_compiler_c++ {g++ -c}

# The Intel C++ compiler 'icpc'
# <URL:http://www.intel.com>
$config SetValue program_compiler_c++ {icpc -c}

########################################################################
# SUPPORT PROCEDURES
#
# Routine to guess the Intel C++ version.  The import, icpc, is used
# via "exec $icpc --version" (or, rather, the "open" analogue) to
# determine the icpc version (since the flags accepted by icpc vary by
# version).  Return value is the icpc version string as a list of
# numbers, for example, {10 1} for version 10.1
proc GuessIcpcVersion { icpc } {
    set guess {}
    catch {
	set fptr [open "|$icpc --version" r]
	set verstr [read $fptr]
	close $fptr
        set digstr {[0-9]+\.[0-9.]+}
        set ws "\[ \t\n\r\]"
	regexp -- "(^|$ws)($digstr)($ws|$)" $verstr dummy dum0 guess dum1
    }
    return [split $guess "."]
}

########################################################################
# LOCAL CONFIGURATION
#
# The following options may be defined in the
# platforms/local/litanium.tcl file:
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
## Use NUMA (non-uniform memory access) libraries?  This is only
## supported on Linux systems that have both NUMA runtime (numactl) and
## NUMA development (numactl-devel) packages installed.
# $config SetValue use_numa 1  ;# 1 to enable, 0 (default) to disable.
#
## Override default C++ compiler.  Note the "_override" suffix
## on the value name.
# $config SetValue program_compiler_c++_override {icpc -c}
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
   # Value not set in platforms/local/litanium.tcl, so use
   # getconf to report the number of "online" processors
   if {[catch {exec getconf _NPROCESSORS_ONLN} processor_count]} {
      # getconf call failed.  Try using /proc/cpuinfo
      unset processor_count
      catch {
         set threadchan [open "/proc/cpuinfo"]
         set cpuinfo [split [read $threadchan] "\n"]
         close $threadchan
         set proclist [lsearch -all -regexp $cpuinfo \
                          "^processor\[ \t\]*:\[ \t\]*\[0-9\]+$"]
         if {[llength $proclist]>0} {
            set processor_count [llength $proclist]
         }
      }
   }
   if {[info exists processor_count]} {
      set auto_max [$config GetValue thread_count_auto_max]
      if {$processor_count>$auto_max} {
         # Limit automatically set thread count to auto_max
         set processor_count $auto_max
      }
      $config SetValue thread_count $processor_count
   }
}
if {[catch {$config GetValue use_numa}]} {
   # Default is a non-NUMA aware build, because NUMA builds
   # require install of system NUMA development package.
   $config SetValue use_numa 0
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

# If compiler is specified in the oommf/config/local/options.tcl file,
# then that value overrides selection above.  The override setting is
# set by an entry like this, in the oommf/config/local/options.tcl file:
#   Oc_Option Add * Oc_Config compiler_override {icpc -c}
if {[catch {$config GetValue program_compiler_c++_override} compiler] == 0} {
    $config SetValue program_compiler_c++ $compiler
}

# Compiler option processing...
set ccbasename [file tail [lindex [$config GetValue program_compiler_c++] 0]]
if {[string match g++ $ccbasename]} {
    # ...for GNU g++ C++ compiler
    # Optimization options
    # set opts [list -O0 -ffloat-store]  ;# No optimization
    # set opts [list -O%s]               ;# Minimal
    set opts [list -O3 -fomit-frame-pointer -fstrict-aliasing]
    # You can also try adding -malign-double, -funroll-loops, -ffast-math

    # Default warnings disable
    set nowarn [list -Wno-non-template-friend]
    if {[info exists nowarn] && [llength $nowarn]>0} {
       set opts [concat $opts $nowarn]
    }
    catch {unset nowarn}

    $config SetValue program_compiler_c++_option_opt "format \"$opts\""

    # NOTE: If you want good performance, be sure to edit ../options.tcl
    #  or ../local/options.tcl to include the line
    #	Oc_Option Add * Platform cflags {-def NDEBUG}
    #  so that the NDEBUG symbol is defined during compile.
    $config SetValue program_compiler_c++_option_out {format "-o \"%s\""}
    $config SetValue program_compiler_c++_option_src {format \"%s\"}
    $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}
    $config SetValue program_compiler_c++_option_debug {format "-g"}
 
    # Compiler warnings:
    # Omitted: -Wredundant-decls -Wshadow -Wcast-align
    # I would also like to use -Wcast-qual, but casting away const is
    # needed on some occasions to provide "conceptual const" functions in
    # place of "bitwise const"; cf. p76-78 of Meyer's book, "Effective C++."
    #
    # NOTE: -Wno-uninitialized is required after -Wall by gcc 2.8+ because
    # of an apparent bug.  The STL breaks -Winline and -Wuninitialized
    # Also, parts of the STL are not clean wrt sign comparisons.  You
    # may want to add -Wno-sign-compare to disable those warnings as
    # well.
    $config SetValue program_compiler_c++_option_warn {format "-Wall \
        -W -Wpointer-arith -Wwrite-strings \
        -Woverloaded-virtual -Wsynth -Werror \
	-Wno-uninitialized -Wno-unused-function"}

    # Widest natively support floating point type
    if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }

    # Directories to exclude from explicit include search path, i.e.,
    # the -I list.  Some versions of gcc complain if "system" directories
    # appear in the -I list.
    $config SetValue \
    	program_compiler_c++_system_include_path [list /usr/include]

    # Other compiler properties.  Don't know if this is still necessary
    # with recent compilers.
    $config SetValue \
            program_compiler_c++_property_optimization_breaks_varargs 1
} elseif {[string match icpc $ccbasename]} {
    # ...for Intel's icpc C++ compiler

    if {![info exists icpc_version]} {
        set icpc_version [GuessIcpcVersion $ccbasename]
    }
    if {[lindex $icpc_version 0]<11} {
	set fma -IPF-fma
    } else {
	set fma -fma
    }

    # set opts "-O%s"
    # set opts "-O3 -ansi_alias -IPF_fp_speculationsafe $fma -wd1572"
    # set opts "-O3 -mcpu=itanium2 -ansi_alias \
    #             -fp-speculation=safe $fma -wd1572 -wd1624"
    set opts "-O3 -mcpu=itanium2 -ansi_alias -inline-level=2 \
                 -opt-mem-bandwidth2 \
                 -scalar-rep -funroll-loops \
                 -fp-speculation=fast $fma -wd1572 -wd1624"
    # set opts "-O0 -g -wd1572 -wd1624"
    # set opts "-fast"

    $config SetValue program_compiler_c++_option_opt "format \"$opts\""
    # NOTES on program_compiler_c++_option_opt:
    #   If you use -ipo, or any other flag that enables interprocedural
    #     optimizations (IPO) such as -fast, then the program_linker
    #     value (see below) needs to have -ipo added.  This option
    #     causes some difficulties with the Tcl/Tk and the oommf/pkg
    #     libraries.  Our tests have only shown a gain of about 5%.
    #   If you add -parallel to program_compiler_c++_option_opt, then
    #     add -parallel to the program_linker value too.  -parallel
    #     doesn't seem to produce any noticeable runtime speedup.
    #   For good performance, be sure that ../options.tcl
    #     or ../local/options.tcl includes the line
    #         Oc_Option Add * Platform cflags {-def NDEBUG}
    #     so that the NDEBUG symbol is defined during compile.

    $config SetValue program_compiler_c++_option_out {format "-o \"%s\""}
    $config SetValue program_compiler_c++_option_src {format \"%s\"}
    $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}
    $config SetValue program_compiler_c++_option_debug {format "-g"}

    $config SetValue program_compiler_c++_option_warn \
      { format "-Wall -Werror -wd279 -wd383 -wd810 -wd981 \
                -wd1418 -wd1419 -wd1572 -wd1624 -wd2259" }

    # Widest natively support floating point type.
    if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }

    unset opts fma icpc_version
}

# The program to run on this platform to link together object files and
# library files to create an executable binary.
set lbasename $ccbasename
if {[string match g++ $lbasename]} {
    # ...for GNU g++ as linker
    $config SetValue program_linker [list $lbasename]
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_uses_-L-l {1}
} elseif {[string match icpc $lbasename]} {
    # ...for Intel's icpc as linker
    set linkcmdline [list $lbasename]
    # If -fast or other flags that enable interprocedural optimizations
    # (IPO) appear in the program_compiler_c++_option_opt value above,
    # then append those flags into program_linker too.
    # lappend linkcmdline "-ipo"
    # lappend linkcmdline "-parallel"
    $config SetValue program_linker $linkcmdline
    unset linkcmdline

    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_uses_-L-l {1}
}
unset lbasename

# The program to run on this platform to create a single library file out
# of many object files.
if {[string match icpc $ccbasename]} {
    $config SetValue program_libmaker {xiar csr}
    $config SetValue program_libmaker_option_obj {format \"%s\"}
    $config SetValue program_libmaker_option_out {format \"%s\"}
} else {
    $config SetValue program_libmaker {ar cr}
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
#$config SetValue TCL_LIBS [concat -lfftw3 [$config GetValue TCL_LIBS]]
#$config SetValue TK_LIBS [concat -lfftw3 [$config GetValue TK_LIBS]]

if {![catch {$config GetValue use_numa} _] && $_} {
   # Include NUMA (non-uniform memory access) library
   $config SetValue TCL_LIBS [concat [$config GetValue TCL_LIBS] -lnuma]
   $config SetValue TK_LIBS [concat [$config GetValue TK_LIBS] -lnuma]
}

########################################################################
unset config
