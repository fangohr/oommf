# FILE: cygtel.tcl
#
# Configuration feature definitions for the configuration 'cygtel',
# which is 32-bit Cygwin on Windows.
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

# Environment variable override for C++ compiler
if {[info exists env(OOMMF_C++)]} {
   $config SetValue program_compiler_c++_override $env(OOMMF_C++)
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
# The features in this file are divided into three sections.  The first
# section (REQUIRED CONFIGURATION) includes features which require you 
# to provide a value.  The second section (OPTIONAL CONFIGURATION)
# includes features which have usable default values, but which you
# may wish to customize.  The third section (ADVANCED CONFIGURATION)
# contains features which you probably do not need or want to change
# without a good reason.
########################################################################
# REQUIRED CONFIGURATION

# NOTE: The rest of the REQUIRED CONFIGURATION is required only
# for building OOMMF software from source code.  If you downloaded
# a distribution with pre-compiled executables, no more configuration
# is required.

# Set the feature 'program_compiler_c++' to the program to run on this 
# platform to compile source code files written in the language C++ into 
# object files.  Select from the choices below.  If the compiler is not 
# in your path, be sure to use the whole pathname.  Also include any 
# options required to instruct your compiler to only compile, not link.  
#
# If your compiler is not listed below, additional features will
# have to be added in the ADVANCED CONFIGURATION section below to 
# describe to the OOMMF software how to operate your compiler.  Send
# e-mail to the OOMMF developers for assistance.
#
# The GNU C++ compiler 'g++'
# <URL:http://www.gnu.org/software/gcc/gcc.html>
# <URL:http://egcs.cygnus.com/>
$config SetValue program_compiler_c++ {g++ -c}
# NOTE: On some versions of Cygwin, you will need to append a
# version number onto 'g++' in the above line.  For example,
#   $config SetValue program_compiler_c++ {g++-3 -c}


########################################################################
# SUPPORT PROCEDURES
#
# Load routines to guess the CPU, determine compiler version, and
# provide appropriate cpu-specific and compiler version-specific
# optimization flags.
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         cpuguess-cygtel.tcl]

# Miscellaneous processing routines
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         misc-support.tcl]

########################################################################
# LOCAL CONFIGURATION
#
# The following options may be defined in the
# platforms/local/cygtel.tcl file:
#
## Set the feature 'path_directory_temporary' to the name of an existing 
## directory on your computer in which OOMMF software should write 
## temporary files.  All OOMMF users must have write access to this 
## directory.
# $config SetValue path_directory_temporary {C:\temp}
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
#
## Override default C++ compiler.  Note the "_override" suffix
## on the value name.
# $config SetValue program_compiler_c++_override {icpc -c}
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
## Flags to remove from compiler "opts" string:
# $config SetValue program_compiler_c++_remove_flags \
#                          {-fomit-frame-pointer -fprefetch-loop-arrays}
#
## Flags to add to compiler "opts" string:
# $config SetValue program_compiler_c++_add_flags \
#                          {-funroll-loops}
#
## EXTERNAL PACKAGE SUPPORT:
## Extra include directories for compiling:
# $config SetValue program_compiler_extra_include_dirs /opt/local/include
#
## Extra directories to search for libraries.
# $config SetValue program_linker_extra_lib_dirs [list "/opt/local/lib"]
#
## Script to form library full name from stem name, for external libraries.
## This is usually not needed, as default scripts suffice.
# $config SetValue program_linker_extra_lib_scripts [list {format "lib%s.lib"}]
#
## Extra library flags to throw onto link command.  Use sparingly ---
## for most needs program_linker_extra_lib_dirs and
## program_linker_extra_lib_scripts should suffice.
# $config SetValue program_linker_extra_args
#    {-L/opt/local/lib -lfftw3 -lsundials_cvode -lsundials_nvecserial}
# 
# END LOCAL CONFIGURATION
########################################################################
#
# Default handling of local defaults:
#
if {[catch {$config GetValue oommf_threads}]} {
   # Value not set in platforms/local/wintel.tcl,
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
   # Value not set in platforms/local/cygtel.tcl, so use
   # getconf to report the number of "online" processors
   if {[catch {exec getconf _NPROCESSORS_ONLN} processor_count]} {
      # getconf call failed.  Try using /proc/cpuinfo
      unset processor_count
      if {[catch {
         set threadchan [open "/proc/cpuinfo"]
         set cpuinfo [split [read $threadchan] "\n"]
         close $threadchan
         set proclist [lsearch -all -regexp $cpuinfo \
                          "^processor\[ \t\]*:\[ \t\]*\[0-9\]+$"]
         if {[llength $proclist]>0} {
            set processor_count [llength $proclist]
         }
      }] || ![info exists processor_count]} {
         # Both getconf and /proc/cpuinfo failed.  Try Windows
         # environment variable NUMBER_OF_PROCESSORS
         if {[info exists env(NUMBER_OF_PROCESSORS)]} {
            set processor_count $env(NUMBER_OF_PROCESSORS)
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
if {[catch {$config GetValue program_compiler_c++_override} compiler] == 0} {
    $config SetValue program_compiler_c++ $compiler
}


########################################################################
# ADVANCED CONFIGURATION

# The absolute, native filename of the null device
# If using a cygwin native build of tclsh, this should be set to /dev/null
# If using a Windows native build of tclsh, this should be set to nul:
$config SetValue path_device_null {/dev/null}

# Compiler option processing...
set ccbasename [file tail [lindex [$config GetValue program_compiler_c++] 0]]
if {[string match g++* $ccbasename]} {
   # ...for GNU g++ C++ compiler
   if {![info exists gcc_version]} {
      set gcc_version [GuessGccVersion \
                          [lindex [$config GetValue program_compiler_c++] 0]]
   }

   # Optimization options
   # set opts [list -O0 -fno-inline -ffloat-store]  ;# No optimization
   # set opts [list -O%s]                      ;# Minimal optimization
   set opts [GetGccGeneralOptFlags $gcc_version]
   # Aggressive optimization flags, some of which are specific to
   # particular gcc versions, but are all processor agnostic.  CPU
   # specific opts are introduced in farther below.  See
   # x86-support.tcl for details.

   # CPU model architecture specific options.  To override, set value
   # program_compiler_c++_cpu_arch in
   # oommf/config/platform/local/cygtel.tcl.  See note about SSE
   # below.
   if {[catch {$config GetValue program_compiler_c++_cpu_arch} cpu_arch]} {
      set cpu_arch generic
   }
   set cpuopts {}
   if {![string match generic [string tolower $cpu_arch]]} {
      # Arch specific build.  If cpu_arch is "host", then try to
      # guess.  Otherwise, assume user knows what he is doing and has
      # inserted an appropriate cpu_arch string, i.e., one that
      # matches the format and known types as returned from GuessCpu.
      if {[string match host $cpu_arch]} {
         set cpu_arch [GuessCpu]
      }
      set cpuopts [GetGccCpuOptFlags $gcc_version $cpu_arch]
   }
   unset cpu_arch
   # You can override the above results by directly setting or
   # unsetting the cpuopts variable, e.g.,
   #
   #    set cpuopts [list -march=athlon]
   # or
   #    unset cpuopts
   #
   if {[info exists cpuopts] && [llength $cpuopts]>0} {
      set opts [concat $opts $cpuopts]
   }
   # gcc 3.x has some problems with SSE instructions
   # (STATUS_ACCESS_VIOLATION), at least when built against the
   # ActiveTcl libraries.  This probably arises from data alignment
   # problems.  Reports are that these problems are fixed on gcc 4.x,
   # but YMMV. See also the gcc 4.2 -mstackrealign option.  Another
   # option appears to be to use a Tcl/Tk built using gcc, as opposed
   # to the ActiveTcl builds that reportedly use Microsoft Visual C++
   # 6 (from 1998).  The following lines remove SSE enabling flags
   # from the options list for gcc earlier than 4.0.
   if {[lindex $gcc_version 0]<4} {
      regsub -all -- {-mfpmath=[^ ]*} $opts {} opts
      regsub -all -- {-msse[^ ]*} $opts {} opts
   }

   # Default warnings disable
   set nowarn [list -Wno-non-template-friend]
   if {[info exists nowarn] && [llength $nowarn]>0} {
      set opts [concat $opts $nowarn]
   }
   catch {unset nowarn}

   # Make user requested tweaks to compile line options
   set opts [LocalTweakOptFlags $config $opts]

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
   # Omitted: -Wredundant-decls -Wshadow -Wcast-align
   # I would also like to use -Wcast-qual, but casting away const is
   # needed on some occasions to provide "conceptual const" functions in
   # place of "bitwise const"; cf. p76-78 of Meyer's book, "Effective C++."
   #
   # NOTE: -Wno-uninitialized is required after -Wall by gcc 2.8+ because
   # of an apparent bug.  -Winline is out because of failures in the STL.
   # Depending on the gcc version, the following options may also be
   # available:     -Wbad-function-cast     -Wstrict-prototypes
   #                -Wmissing-declarations  -Wnested-externs
   # gcc 3.4.4 also needs -Wno-uninitialized because of problems with
   # the STL.
   $config SetValue program_compiler_c++_option_warn {format "-Wall \
        -W -Wpointer-arith -Wwrite-strings \
        -Woverloaded-virtual -Wsynth -Werror -Wno-uninitialized \
        -Wno-unused-function"}

   # Wide floating point type.
   # At this time cygwin uses the Windows math libraries,
   # which don't provide long double support
    if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }
   $config SetValue program_compiler_c++_property_floorl_flag -DNO_FLOORL_CHECK

   # Directories to exclude from explicit include search path, i.e.,
   # the -I list.  Some versions of gcc complain if "system" directories
   # appear in the -I list.
   $config SetValue \
      program_compiler_c++_system_include_path [list /usr/include]

   # Other compiler properties
   $config SetValue \
      program_compiler_c++_property_optimization_breaks_varargs 0

   # Missing function prototypes
   $config SetValue program_compiler_c++_prototype_supply_nice \
      {extern "C" int nice(int);}
}

# The program to run on this platform to link together object files and
# library files to create an executable binary.
set lbasename $ccbasename
if {[string match g++* $ccbasename]} {
    # ...for GNU g++ as linker
    $config SetValue program_linker [list $lbasename]
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_uses_-L-l {1}
    proc fubar { subsystem } { return "-m[string tolower $subsystem]" }
    $config SetValue program_linker_option_sub {fubar}
}
unset lbasename

# The program to run on this platform to create a single library file out
# of many object files.
if {[string match g++* $ccbasename]} {
    $config SetValue program_libmaker {ar crs}
    $config SetValue program_libmaker_option_obj {format \"%s\"}
    $config SetValue program_libmaker_option_out {format \"%s\"}
}
unset ccbasename

# Name formats on generated files
$config SetValue script_filename_object {format %s.o}
$config SetValue script_filename_static_library {format lib%s.a}
$config SetValue script_filename_executable {format %s.exe}

# Some Cygwin distributions appear to have a bad TCL_RANLIB entry
# Check it.
if {[catch {$config GetValue TCL_RANLIB} ranlib]
    || ![llength [auto_execok $ranlib]]} {
    $config SetValue TCL_RANLIB ranlib
}
unset ranlib

# If we are building with a Windows build of Tcl/Tk, then tclConfig.sh
# and tkConfig.sh probably won't be available.  First try "Visual C"
# names for Tcl/Tk libraries.  If that doesn't work, then try unix
# naming convention, which should pick up MinGW builds of Tcl/Tk.
if {[regexp -nocase -- windows $tcl_platform(platform)]} {
   set tcllib {}
   if {![catch {$config GetValue TCL_VC_LIB_SPEC} tcllib]} {
      if {![file exists $tcllib]} {
         set path [file dirname $tcllib]
         set fname [file tail $tcllib]
         regsub {\..*$} $fname {} fname
         set tcllib [file join $path lib${fname}.a]
         if {![file exists $tcllib]} {
            set tcllib {}
         }
      }
   }
   if {[string match {} $tcllib]} {
      set tcllib [list -L[file join [$config GetValue TCL_EXEC_PREFIX] lib] \
                     -ltcl[$config GetValue TCL_MAJOR_VERSION][$config \
                     GetValue TCL_MINOR_VERSION]]
   }
   if {[file exists $tcllib]} {
      $config SetValue TCL_LIB_SPEC $tcllib
   }
   unset tcllib

   set tklib {}
   if {![catch {$config GetValue TK_VC_LIB_SPEC} tklib]} {
      if {![file exists $tklib]} {
         set path [file dirname $tklib]
         set fname [file tail $tklib]
         regsub {\..*$} $fname {} fname
         set tklib [file join $path lib${fname}.a]
         if {![file exists $tklib]} {
            set tklib {}
         }
      }
   }
   if {[string match {} $tklib]} {
      set tklib [list -L[file join [$config GetValue TK_EXEC_PREFIX] lib] \
                     -ltcl[$config GetValue TK_MAJOR_VERSION][$config \
                     GetValue TK_MINOR_VERSION]]
   }
   if {[file exists $tklib]} {
      $config SetValue TK_LIB_SPEC $tklib
   }
   unset tklib
} else {
   # Extra libraries needed on link line.  Why are these missing?
   if {[catch {$config GetValue TK_LIB_SPEC} tklib]} {
      set tklib {}
   }
   lappend tklib -lfontconfig -lXft
   $config SetValue TK_LIB_SPEC $tklib
}

# Workaround for bad lib path in patched cygwin-b20.1 (2000-03-20).  On the
# stock b20.1 release this should have no effect.
#set goodpath "/cygnus/cygwin-b20/H-i586-cygwin32/lib/gcc-lib/i586-cygwin32/2.95.2"
#$config SetValue TK_LIB_SPEC [concat -L$goodpath [$config GetValue TK_LIB_SPEC]]
#$config SetValue TCL_LIB_SPEC [concat -L$goodpath [$config GetValue TCL_LIB_SPEC]]
#unset goodpath

# Additional libraries to use when linking against Tcl, Tk.
# If building against a Windows-based version of Tcl/Tk, then
# the {tcl,tk}Config.sh scans will likely pick up a list of
# Windows libraries that aren't needed in Cygwin, and which
# Cygwin probably can't find or use anyway.  So best bet is
# to just make sure these are empty:
if {[regexp -nocase -- windows $tcl_platform(platform)]} {
   $config SetValue TCL_LIBS {}
   $config SetValue TK_LIBS {}
}

# Some ancient versions of Cygwin need -luser32 if SUBSYSTEM==WINDOWS
# in ext/messages.cc to provide MessageBeep (and maybe MessageBox?).
#$config SetValue TCL_LIBS {-luser32}
#$config SetValue TK_LIBS {-luser32}

# The tclConfig and tkConfig files on Cygwin can include libraries not
# available on a standard install, so just empty these.  Tweak as
# necessary.
$config SetValue TCL_LIBS {}
$config SetValue TK_LIBS {}

# Workaround for bad TK_BUILD_INCLUDES in tkConfig.sh, Cygwin Tcl/Tk
# package 20001125-1, concurrent with Cygwin DLL release 1.3.13-2.
# Update 16-Jan-2003: Build of win side of Tcl/Tk 8.4.1, under Cygwin,
#  does not set TK_XINCLUDES in tkConfig.sh, so added catch around
#  GetValue TK_XINCLUDES to handle this case.
set oldpath {}
#catch {set oldpath [$config GetValue TK_XINCLUDES]}
$config SetValue TK_XINCLUDES [concat $oldpath "-I/usr/X11R6/include"]
unset oldpath

# Directory holding init.tcl.  Use this if environment variable
# TCL_LIBRARY is not set.
catch {$config SetValue TCL_LIBRARY \
           [file join [$config GetValue TCL_PREFIX] \
                lib tcl[$config GetValue TCL_VERSION]]}


# Some old versions of cygwin are missing an WinMainCRTStartup()
# startup entry point.  Set this to provide a workaround, by
# creating a wrapper around mainCRTStartup().  See the file
# oommf/pkg/oc/oc.cc for details.
#$config SetValue program_compiler_c++_missing_startup_entry 1

########################################################################
unset config
