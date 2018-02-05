# FILE: linalp.tcl
#
# Configuration feature definitions for the configuration 'linalp'
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
$config SetValue program_compiler_c++ {g++ -c}

# The COMPAQ C++ compiler 'cxx'
# <URL:http://www.compaq.com>
# $config SetValue program_compiler_c++ {cxx -c}

########################################################################
# LOCAL CONFIGURATION
#
# The following options may be defined in the
# platforms/local/linalp.tcl file:
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
# $config SetValue program_compiler_c++_override {cxx -c}
#
## Processor architecture for compiling.  The default is "generic"
## which should produce an executable that runs on any cpu model for
## the given platform.  Optionally, one may specify "host", in which
## case the build scripts will try to automatically detect the
## processor type on the current system, and select compiler options
## specific to that processor model.  The resulting binary will
## generally not run on other architectures.
# $config SetValue program_compiler_c++_cpu_arch host
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
   # Value not set in platforms/local/linalp.tcl,
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
   # Value not set in platforms/local/linalp.tcl, so use
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

if {[catch {$config GetValue program_compiler_c++_override} compiler] == 0} {
    $config SetValue program_compiler_c++ $compiler
}

# Compiler option processing...
set ccbasename [file tail [lindex [$config GetValue program_compiler_c++] 0]]
if {[string match g++ $ccbasename]} {
    # ...for GNU g++ C++ compiler
    # set opts [list -O%s]
    # set opts [list -O0]
    # set opts [list -O6]
    # set opts [list -O6 -ffast-math -funroll-loops -fomit-frame-pointer -mcpu=21264]
    set opts [list -O2 -funroll-loops -fomit-frame-pointer -fstrict-aliasing]
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
    $config SetValue program_compiler_c++_option_debug {format "-g"}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}
 
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
        -W -Wpointer-arith -Wbad-function-cast -Wwrite-strings \
        -Wstrict-prototypes -Wmissing-declarations \
        -Wnested-externs -Woverloaded-virtual -Wsynth -Werror \
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
} elseif {[string match cxx $ccbasename]} {
    # ...for Compaq's cxx C++ compiler
#    $config SetValue program_compiler_c++_option_opt {format "\"-O%s\""}
    $config SetValue program_compiler_c++_option_opt {format "-fast"}
# NOTE: If you want good performance, be sure to edit ../options.tcl
#  or ../local/options.tcl to include the line
#	Oc_Option Add * Platform cflags {-def NDEBUG}
#  so that the NDEBUG symbol is defined during compile.
    $config SetValue program_compiler_c++_option_out \
            {format "-ptr linalp/tr -o \"%s\""}
    $config SetValue program_compiler_c++_option_src {format \"%s\"}
    $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
    $config SetValue program_compiler_c++_option_warn \
      { format "-w0 -verbose \
        -msg_disable undpreid,novtbtritmp,boolexprconst,badmulchrcom" }
    $config SetValue program_compiler_c++_option_debug {format "-g"}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}

    # Widest natively support floating point type.  (On Tru64 unix,
    # cxx supports a long double type, but this isn't supported on
    # linux/axp at this time (9-May-2000).)
    if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }
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
    $config SetValue program_linker_rpath {format "-Wl,-rpath=%s"}
    $config SetValue program_linker_uses_-L-l {1}
} elseif {[string match cxx $lbasename]} {
    # ...for DU's cxx as linker
    $config SetValue program_linker \
	    [list $lbasename -ptr linalp/tr]
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_uses_-L-l {0}
    $config SetValue program_linker_uses_-I-L-l {1}
}
unset lbasename
unset ccbasename

# The program to run on this platform to create a single library file out
# of many object files.
$config SetValue program_libmaker {ar cr}
if {[string match ar [file tail [lindex \
        [$config GetValue program_libmaker] 0]]]} {
    # Option processing for ar
    $config SetValue program_libmaker_option_obj {format \"%s\"}
    $config SetValue program_libmaker_option_out {format \"%s\"}
}

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
#
# Hack edits: Linux/AXP/GCC specific fast math library, either ffm or cpml
# Comment this section out if you don't have either.  Compaq's cxx expands
# '-lm' to '-lcpml -lm' by default.
#set fast_math_lib cpml
#$config SetValue TCL_LIBS [concat -l$fast_math_lib [$config GetValue TCL_LIBS]]
#$config SetValue TK_LIBS [concat -l$fast_math_lib [$config GetValue TK_LIBS]]
#
########################################################################

unset config
