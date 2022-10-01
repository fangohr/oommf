# FILE: linux-x86_64.tcl
#
# Configuration feature definitions for the configuration
# 'linux-x86_64'
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
# The GNU C++ compiler 'g++'
# <URL:http://www.gnu.org/software/gcc/gcc.html>
# <URL:http://egcs.cygnus.com/>
$config SetValue program_compiler_c++ {g++ -c}
#
# The Portland Group C++ compiler, 'pgc++'
# <URL:http://www.pgroup.com/>
# $config SetValue program_compiler_c++ {pgc++ -c}
#
# The Intel C++ compiler 'icpc'
# <URL:http://www.intel.com>
#$config SetValue program_compiler_c++ {icpc -c}
#

########################################################################
# SUPPORT PROCEDURES
#
# Load routines to guess the CPU using /procs/cpuinfo, determine
# compiler version, and provide appropriate cpu-specific and compiler
# version-specific optimization flags.
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         cpuguess-linux-x86_64.tcl]

# Load routine to determine glibc version
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         glibc-support.tcl]

# Miscellaneous processing routines
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         misc-support.tcl]

########################################################################
# LOCAL CONFIGURATION
#
# The following options may be defined in the
# platforms/local/linux-x86_64.tcl file:
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
## If problems occur involving the host server, account server, or other
## interprocess communications, try disabling async socket connections:
# $config SetValue socket_noasync 1
#
## If windows don't auto resize properly, set this value to 1.
# $config SetValue bad_geom_propagate 1
#
## Use SSE intrinsics?  If so, specify level here.  Set to 0 to not
## use SSE intrinsics.  Leave unset to get the default (which may
## depend on the compiler).  Note: The cpu_arch selection below must
## support the desired sse_level.  In particular, on 64-bit systems
## cpu_arch == generic supports only SSE 2 or lower.
# $config SetValue sse_level 2  ;# Replace '2' with desired level
#
## Use FMA (fused multiply-add) intrinsics?  If so, specify either "3"
## for three argument intrinsics or "4" for four argument intrinsics.
## Set to 0 to not use FMA intrinsics.  Leave unset to get the default
## (which may depend on the selected compiler).
# $config SetValue fma_type 0 ;# Replace '0' with desired type
#
## Use NUMA (non-uniform memory access) libraries?  This is only
## supported on Linux systems that have both NUMA runtime (numactl) and
## NUMA development (numactl-devel) packages installed.
# $config SetValue use_numa 1  ;# 1 to enable, 0 (default) to disable.
#
## Default nodes if NUMA is enables.  This is either a numeric list or
## else one of the keywords "auto" or "none".
# $config SetValue numanodes auto
#
## Override default C++ compiler.  Note the "_override" suffix
## on the value name.
# $config SetValue program_compiler_c++_override {icpc -c}
#
## Max asynchronous exception (A.E.) handling level: one of none, POSIX,
## or SEH. This setting works in conjuction with the Oc_Option
## AsyncExceptionHandling selection (see file oommf/appconfig/options.tcl)
## according to the following table:
##
##    Oc_Option          \ program_compiler_c++_async_exception_handling
## AsyncExceptionHandling \      none         POSIX          SEH
## ------------------------+-------------------------------------------
##       none              |     abort         abort         abort*
##      POSIX              |     abort        sig+abort    sig+abort
##       SEH               |     abort        sig+abort    seh+abort*
##
## In the abort cases, the program immediately terminates with no error
## message. In the sig+abort or seh+abort cases a error message is
## logged followed by program termination. In the sig+abort setting the
## error message reports the corresponding POSIX signal raised, while
## seh+abort prints the corresponding Microsoft Windows Structured
## Exception error message. In the SEH column all A.E. run through the
## the Windows SEH handling, so there is some additional overhead, even
## if Oc_Option AsyncExceptionHandling is none. The error message is
## generally logged to stderr, and may additionally be written to
## a program log file.
##
## The SEH option is only available on Windows with the Visual C++
## compiler, and may have a (minor) negative performance impact. The
## default setting is POSIX.
# $config SetValue program_compiler_c++_async_exception_handling POSIX
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
## Flags to remove from compiler "opts" string (regexp match):
# $config SetValue program_compiler_c++_remove_flags \
#                          {-fomit-frame-pointer -fprefetch-loop-arrays}
#
## Flags to add to compiler "opts" string:
# $config SetValue program_compiler_c++_add_flags \
#                          {-funroll-loops}
#
## Flags to add (resp. remove (regexp)) from "valuesafeopts" string:
# $config SetValue program_compiler_c++_remove_valuesafeflags \
#                          {-fomit-frame-pointer -fprefetch-loop-arrays}
# $config SetValue program_compiler_c++_add_valuesafeflags \
#                          {-funroll-loops}
#
## For debugging builds try
# $config SetValue program_compiler_c++_remove_flags {.*}
# $config SetValue program_compiler_c++_add_flags {--std=c++11 -pthread -O0}
# $config SetValue program_compiler_c++_remove_valuesafeflags {.*}
# $config SetValue program_compiler_c++_add_valuesafeflags {--std=c++11 -pthread -O0}
#
## and set
##  Oc_Option Add * Platform cflags {-debug 1}
## in oommf/config/local/options.tcl
#
#
### Options for Xp_DoubleDouble high precision package
## Select base variable type.  One of auto (default), double, long double
# $config SetValue program_compiler_xp_doubledouble_basetype {long double}
#
### Perform range checks?  Enable to pass vcv tests. Default follows NDEBUG.
# $config SetValue program_compiler_xp_doubledouble_rangecheck 1
#
## Use alternative single variable option, with variable one of double,
## long double, or MPFR.  The last requires installation of the Boost
## multiprecision C++ libraries.
# $config SetValue program_compiler_xp_doubledouble_altsingle {long double}
#
## Disable (0) or enable (1) use of std::fma (fused-multiply-add) in the
## Xp_DoubleDouble package.  Only use this if your architecture supports
## a true fma instruction with a single rounding.  Default is to
## auto-detect at build time and use fma if it is single rounding.
# $config SetValue program_compiler_xp_use_fma 0
#
## Disable (1) or enable (0, default) testing of Xp_DoubleDouble package.
# $config SetValue program_pimake_xp_doubledouble_disable_test 1
###
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
## Extra library flags to throw onto link command.  Some g++ builds may
## require the "-pthread" switch, but otherwise use this control
## sparingly --- for most needs program_linker_extra_lib_dirs and
## program_linker_extra_lib_scripts should suffice.
# $config SetValue program_linker_extra_args -pthread
# $config SetValue program_linker_extra_args
#    {-L/opt/local/lib -lfftw3 -lsundials_cvode -lsundials_nvecserial}
#
## Uncomment the following "use_tk 0" line to build OOMMF without Tk.
## This is for special-purpose use only, typically for remote batch jobs
## on a headless machine with Tcl but without X11 (and hence Tk).  This
## significantly hobbles OOMMF --- not only are all GUI interfaces
## disabled, but so too the Tk image processing code.  Even on a
## headless machine, it is better if possible to run X11 with a virtual
## frame buffer (e.g. Xvfb).
# $config SetValue use_tk 0
#
#
## Debugging options. These control memory alignment. See the
## "Debugging OOMMF" section in the OOMMF Programming Manual for
## details.
# $config SetValue program_compiler_c++_property_cache_linesize 1
# $config SetValue program_compiler_c++_property_pagesize 1
# $config SetValue sse_no_aligned_access 1
#
# END LOCAL CONFIGURATION
########################################################################
#
# Default handling of local defaults:
#
if {[catch {$config GetValue oommf_threads}]} {
   # Value not set in platforms/local/linux-x86_64.tcl,
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
   # Value not set in platforms/local/linux-x86_64.tcl, so use getconf
   # to report the number of "online" processors.  NOTE: Neither
   # _NPROCESSORS_ONLN nor processor_count in /proc/cpuinfo
   # distinguish between physical cores and logical cores introduced
   # via hyperthreading.
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

# CPU model architecture specific options.  To override, set value
# program_compiler_c++_cpu_arch in
# oommf/config/platform/local/linux-x86_64.tcl.  See notes about SSE
# below.
if {[catch {$config GetValue program_compiler_c++_cpu_arch} cpu_arch]} {
   set cpu_arch generic
}

# Compiler name regularization
set ccbasename [file tail [lindex [$config GetValue program_compiler_c++] 0]]
if {[string match -nocase *g++* $ccbasename]} {
    set ccbasename g++
} elseif {[string match -nocase pgc* $ccbasename]} {
    # Matches both pgc++ and the older pgCC name variant
    set ccbasename pgc++
} elseif {[string match -nocase icpc* $ccbasename]} {
    set ccbasename icpc
}

# Compiler option processing...
if {[string match g++ $ccbasename]} {
   # ...for GNU g++ C++ compiler
   set gccexec [lindex [$config GetValue program_compiler_c++] 0]

   if {![info exists gcc_version]} {
      set gcc_version [GuessGccVersion $gccexec]
   }
   if {[lindex $gcc_version 0]<4 ||
       ([lindex $gcc_version 0]==4 && [lindex $gcc_version 1]<7)} {
      puts stderr "WARNING: This version of OOMMF requires g++ 4.7\
                   or later (C++11 support)"
   }
   $config SetValue program_compiler_c++_banner_cmd \
      [list GetGccBannerVersion  \
          [lindex [$config GetValue program_compiler_c++] 0]]

   # Optimization options
   set cpuopts {}
   if {![string match generic [string tolower $cpu_arch]]} {
      # Arch specific build.  If cpu_arch is "host", then try to
      # guess.  Otherwise, assume user knows what he is doing and has
      # inserted an appropriate cpu_arch string, i.e., one that
      # matches the format and known types as returned from GuessCpu.
      if {[string match host $cpu_arch]} {
         set cpu_arch [GuessCpu]
         if {[catch {$config GetValue sse_level}]} {
            # In principle, the result from Find_SSE_Level may be more
            # accurate than what comes from GuessCpu.
            $config SetValue sse_level [Find_SSE_Level]
         }
         if {[catch {$config GetValue fma_type}]} {
            $config SetValue fma_type [Find_FMA_Type]
         }
      } else {
         if {[catch {$config GetValue sse_level}]} {
            # sse_level not set in LOCAL CONFIGURATION block;
            # Take value from cpu_arch
            $config SetValue sse_level [lindex $cpu_arch 2]
         }
      }
      set cpuopts [GetGccCpuOptFlags $gcc_version $cpu_arch]
   }
   # You can override the above results by directly setting or
   # unsetting the cpuopts variable, e.g.,
   #
   #    set cpuopts [list -march=athlon]
   # or
   #    unset cpuopts
   #

   set opts {}
   if {[info exists cpuopts] && [llength $cpuopts]>0} {
      set opts [concat $opts $cpuopts]
   }

   # You may also want to try appending to opts
   #    -parallel -par-threshold49 -par-schedule-runtime

   # Use/don't use SSE intrinsics.  The default is yes, at level 2,
   # since x86_64 guarantees at least SSE2.  If cpu_arch is host, then
   # the default setting is pulled from the /proc/cpuinfo flags, via the
   # Find_SSE_Level proc (see above).
   #    You can override the value by setting the $config sse_level
   # value in the local platform file (see LOCAL CONFIGURATION above).
   if {[catch {$config GetValue sse_level}]} {
      $config SetValue sse_level 2
   }
   if {[$config GetValue sse_level]<2} {
      # Remove SSE enabling flags, if any
      regsub -all -- {-mfpmath=[^ ]*} $opts {} opts
      regsub -all -- {-msse[^ ]*} $opts {} opts
      # Since x64_64 guarantees SSE2, to disable requires explicit flags
      lappend opts -mfpmath=387
   }

   # Older versions of GCC don't include the _mm_cvtsd_f64 intrinsic.
   # If not set in the local platform file, then use the gcc version
   # date to guess the right behavior.
   if {[catch {$config GetValue program_compiler_c++_missing_cvtsd_f64}]} {
      # Not set yet.  Try auto-detect
      set gccyear 0
      if {[regexp {[0-9][0-9][0-9][0-9]} [lindex $gcc_version 3]]} {
         # $gcc_version is "major minor release year month day"
         set gccyear [lindex $gcc_version 3]
      }
      if {$gccyear>2008} {
          $config SetValue program_compiler_c++_missing_cvtsd_f64 0
       } else {
          $config SetValue program_compiler_c++_missing_cvtsd_f64 1
       }
   }

   # Disable some default warnings in the opts switch, as opposed
   # to the warnings switch below, so that these warnings are always
   # muted, even if '-warn' option in file options.tcl is disabled.
   if {[lindex $gcc_version 0]>=3} {
      lappend nowarn [list -Wno-non-template-friend]
      # OOMMF code conforms to the new standard.  Silence this
      # "helpful" but inaccurate warning.
   }
   if {[lindex $gcc_version 0]>=6} {
      lappend nowarn {-Wno-misleading-indentation}
   }
   if {[lindex $gcc_version 0]>=8} {
      # Allow strncpy to truncate strings
      lappend nowarn {-Wno-stringop-truncation}
   }
   if {[info exists nowarn] && [llength $nowarn]>0} {
      set opts [concat $opts $nowarn]
   }
   catch {unset nowarn}

   # Asychronous exceptions
   if {[catch {$config \
         GetValue program_compiler_c++_async_exception_handling} _eh]} {
      set _eh POSIX ;# Default setting
      $config SetValue program_compiler_c++_async_exception_handling $_eh
   }
   if {[string compare -nocase none $_eh]!=0} {
      lappend opts -fnon-call-exceptions
   }
   unset _eh

   # User-requested optimization level
   if {[catch {Oc_Option GetValue Platform optlevel} optlevel]} {
      set optlevel 2 ;# Default
   }

   # Aggressive optimization flags, some of which are specific to
   # particular gcc versions, but are all processor agnostic.
   set valuesafeopts [concat $opts \
                         [GetGccValueSafeOptFlags $gcc_version $optlevel]]
   set opts [concat $opts [GetGccGeneralOptFlags $gcc_version $optlevel]]

   # Make user requested tweaks to compile line options
   set opts [LocalTweakOptFlags $config $opts]
   set valuesafeopts [LocalTweakValueSafeOptFlags $config $valuesafeopts]

   # NOTE: If you want good performance, be sure to edit ../options.tcl
   #  or ../local/options.tcl to include the line
   #    Oc_Option Add * Platform cflags {-def NDEBUG}
   #  so that the NDEBUG symbol is defined during compile.
   $config SetValue program_compiler_c++_option_opt "format \"$opts\""
   $config SetValue program_compiler_c++_option_valuesafeopt \
      "format \"$valuesafeopts\""
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
   $config SetValue program_compiler_c++_option_warn {format "-Wall \
        -W -Wpointer-arith -Wwrite-strings \
        -Woverloaded-virtual -Wsynth -Werror \
        -Wno-unused-function"}

   # Widest natively support floating point type.
   # The x86_64 architecture provides 8-byte floating point registers in
   # addition to the 8 10-byte (80 bits) floating point registers of the
   # x86 architecture.  (On x86_64, "long double" values are padded out
   # in memory to 16-bytes, as compared to 12-bytes on x86_32.)  If you
   # specify "long double" as the realwide type, then in some parts of
   # the program OOMMF will be restricted to using the higher precision
   # 10-byte registers.  (The 10-byte format provides about 19 decimal
   # digits of precision, as opposed to 16 decimal digits for the 8-byte
   # format.)  However, in this case the extra 8-byte x86_64 registers
   # will be under utilized.  If you specify "double" as the realwide
   # type, then the 8-byte registers can be used in all instances.
   #   Use of realwide is restricted in the code so that the speed
   # advantage of using "double" over "long double" should be pretty
   # minimal on this platform, but YMMV.
   if {[catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
      # Not set
      $config SetValue program_compiler_c++_typedef_realwide "long double"
   }

   # Experimental: The OC_REAL8m type is intended to be at least
   # 8 bytes wide.  Generally OC_REAL8m is typedef'ed to double,
   # but you can try setting this to "long double" for extra
   # precision (and extra slowness).  If this is set to "long double",
   # then so should realwide in the preceding stanza.
   if {[catch {$config GetValue program_compiler_c++_typedef_real8m}]} {
      # Not set
      $config SetValue program_compiler_c++_typedef_real8m "double"
   }

   # Directories to exclude from explicit include search path, i.e.,
   # the -I list.  Some versions of gcc complain if "system" directories
   # appear in the -I list.
   $config SetValue \
      program_compiler_c++_system_include_path [list /usr/include]

   # Other compiler properties
   $config SetValue \
      program_compiler_c++_property_optimization_breaks_varargs 0

} elseif {[string match pgc++ $ccbasename]} {
   # ...for Portland Group C++ compiler
   if {![info exists pgcc_version]} {
      set pgcc_version [GuessPgccVersion \
           [lindex [$config GetValue program_compiler_c++] 0]]
   }
   if {[lindex $pgcc_version 0]<17} {
      puts stderr "WARNING: This version of OOMMF requires pgc++ 17\
                   or later (C++11 support)"
   }
   $config SetValue program_compiler_c++_banner_cmd \
      [list GetPgccBannerVersion  \
          [lindex [$config GetValue program_compiler_c++] 0]]

   # CPU model architecture specific options.  To override, set value
   # program_compiler_c++_cpu_arch in
   # oommf/config/platform/local/linux-x86_64.tcl.
   if {[catch {$config GetValue program_compiler_c++_cpu_arch} cpu_arch]} {
      set cpu_arch generic
   }

   # Optimization options
   set opts [list --c++11]
   if {[string match host [string tolower $cpu_arch]]} {
      # Arch specific build.
      lappend opts -fastsse -Mipa=fast,inline
   } else {
      lappend opts -fast -Mipa=fast,inline -tp=x64
      # -tp=x64 specifies a generic x86_64 platform
   }
   # Note 1: We do not need -Mconcur because OOMMF does its own
   #    threading.  Also, -Mconcur in pgc++ 18.4-0 causes segfaults in
   #    OOMMF.
   # Note 2: Option -mp is for OpenMP, which OOMMF doesn't use.
   # Note 3: If there is no IPA information for the Tcl libraries,
   #    does -Mipa do anything?

   if {[info exists cpuopts] && [llength $cpuopts]>0} {
      set opts [concat $opts $cpuopts]
   }

   # The --exceptions option is an old option not supported
   # in the 18.4 release of PG C++.
   #lappend opts "--exceptions" ;# This has to come after -mp

   # Use/don't use SSE intrinsics.  The default is yes, at level 2,
   # since x86_64 guarantees at least SSE2.
   #    You can override the value by setting the $config sse_level
   # value in the local platform file (see LOCAL CONFIGURATION above).
   if {[catch {$config GetValue sse_level}]} {
      $config SetValue sse_level 2
   }

   # Disable selected warnings
   # Warning 1301: non-template friend of a template class
   # Warning 111: unreachable code
   lappend opts --display_error_number
   set nowarn [list --diag_suppress1301 --diag_suppress111]
   if {[info exists nowarn] && [llength $nowarn]>0} {
      set opts [concat $opts $nowarn]
   }
   catch {unset nowarn}

   # Is there a pgc++ equivalent to the g++ -fnon-call-exceptions
   # option? If so, refer to g++ code block and put equivalent here.

   # Aggressive optimization flags, some of which are specific to
   # particular Pgcc versions, but are all processor agnostic.
   set valuesafeopts [GetPgccValueSafeOptFlags $pgcc_version $cpu_arch]
   set fastopts [GetPgccFastOptFlags $pgcc_version $cpu_arch]

   set valuesafeopts [concat $opts $valuesafeopts]
   set opts [concat $opts $fastopts]

   # Make user requested tweaks to compile line options
   set opts [LocalTweakOptFlags $config $opts]
   set valuesafeopts [LocalTweakValueSafeOptFlags $config $valuesafeopts]

   # NOTE: If you want good performance, be sure to edit ../options.tcl
   #  or ../local/options.tcl to include the line
   #    Oc_Option Add * Platform cflags {-def NDEBUG}
   #  so that the NDEBUG symbol is defined during compile.
   $config SetValue program_compiler_c++_option_opt "format \"$opts\""
   $config SetValue program_compiler_c++_option_valuesafeopt \
      "format \"$valuesafeopts\""
   $config SetValue program_compiler_c++_option_out {format "-o \"%s\""}
   $config SetValue program_compiler_c++_option_src {format \"%s\"}
   $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}

   # Compiler warnings:
   $config SetValue program_compiler_c++_option_warn {format \
                                                         "-Minform=warn"}
   $config SetValue program_compiler_c++_option_debug {format "-g"}
   $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}

   # Widest natively support floating point type.  See note about
   # "realwide" in the g++ block above.
   if {[catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
      # Not set
      $config SetValue program_compiler_c++_typedef_realwide "long double"
   }

   # Experimental: The OC_REAL8m type is intended to be at least
   # 8 bytes wide.  Generally OC_REAL8m is typedef'ed to double,
   # but you can try setting this to "long double" for extra
   # precision (and extra slowness).  If this is set to "long double",
   # then so should realwide in the preceding stanza.
   if {[catch {$config GetValue program_compiler_c++_typedef_real8m}]} {
      # Not set
      $config SetValue program_compiler_c++_typedef_real8m "double"
   }

   # The long double versions of floor() and ceil() are badly broken
   # on some machines.  If the following option is set to 1, then
   # then the standard double versions of floor() and ceil() will be
   # used instead.  The double versions don't provide the same range
   # as the long double versions, but the accuracy is not affected if
   # the operand is inside the double range.  This is not an issue
   # unless you have selected "long double" in one of the preceding
   # two stanzas.  If you have, though, then you should check this
   # sample test case: see what floorl(-0.253L) and x-floorl(x) with
   # x=-0.253L return.  If you get 0 for the first or NAN for the
   # second, then you should enable this option.
   $config SetValue program_compiler_c++_property_bad_wide2int 0

   # Directories to exclude from explicit include search path, i.e.,
   # the -I list.  Some of the gcc versions don't play well with
   # the Portland Group compilers, so keep them off the compile line.
   $config SetValue \
      program_compiler_c++_system_include_path [list /usr/include]

   # Other compiler properties
   $config SetValue \
      program_compiler_c++_property_optimization_breaks_varargs 0

   # pgc++ has _mm_cvtsd_f64, but for at least some releases (16.10-0)
   # it appears to return the upper instead of lower floating point
   # value.  Request workaround:
   $config SetValue program_compiler_c++_missing_cvtsd_f64 1

} elseif {[string match icpc $ccbasename]} {
   # ...for Intel's icpc C++ compiler

   if {![info exists icpc_version]} {
      set icpc_version [GuessIcpcVersion \
           [lindex [$config GetValue program_compiler_c++] 0]]
   }
   $config SetValue program_compiler_c++_banner_cmd \
      [list GetIcpcBannerVersion  \
          [lindex [$config GetValue program_compiler_c++] 0]]

   # Intel compiler on Linux relies on parts of gcc install.
   # Assume here that g++ or gcc is on PATH:
   if {![info exists gcc_version]} {
      set gcc_version [GuessGccVersion g++]
      if {[llength $gcc_version]==0} {
         set gcc_version [GuessGccVersion gcc]
      }
   }
   set gcc_bad 0
   if {[llength $gcc_version]>0} {
      if {[lindex $gcc_version 0]<4 ||
          ([lindex $gcc_version 0]==4 && [lindex $gcc_version 1]<8)} {
         set gcc_bad 1
      }
   }
   if {[lindex $icpc_version]<14 || $gcc_bad} {
      puts stderr "WARNING: This version of OOMMF requires\
                   Intel icpc version 14 or later and g++ 4.8\
                   or later (C++ 11 support)"
   }

   # NOTES on program_compiler_c++_option_opt:
   #   If you use -ipo, or any other flag that enables interprocedural
   #     optimizations (IPO) such as -fast, then the program_linker
   #     value (see below) needs to have -ipo added.
   #   In icpc 10.0, -fast throws in a non-overrideable -xT option, so
   #     don't use this unless you are using a Core 2 processor.  We
   #     prefer to manually set the equivalent options and add an
   #     appropriate -x option based on the cpu type.
   #   If you add -parallel to program_compiler_c++_option_opt, then
   #     add -parallel to the program_linker value too.
   #   You may also want to try
   #     -parallel -par-threshold49 -par-schedule-runtime
   #   For good performance, be sure that ../options.tcl
   #     or ../local/options.tcl includes the line
   #         Oc_Option Add * Platform cflags {-def NDEBUG}
   #     so that the NDEBUG symbol is defined during compile.
   #   The -wd1572 option disables warnings about floating
   #     point comparisons being unreliable.
   #   The -wd1624 option disables warnings about non-template
   #     friends of templated classes.

   # CPU model architecture specific options.  To override, set value
   # program_compiler_c++_cpu_arch in
   # oommf/config/platform/local/linux-x86_64.tcl.
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
         set cpuopts [list -xHost]
         set cpu_arch [GuessCpu]
         if {[catch {$config GetValue sse_level}]} {
            $config SetValue sse_level [Find_SSE_Level]
         }
         if {[catch {$config GetValue fma_type}]} {
            $config SetValue fma_type [Find_FMA_Type]
         }
      } else {
         set cpuopts [GetIcpcCpuOptFlags $icpc_version $cpu_arch]
      }
   }
   unset cpu_arch
   # You can override the above results by directly setting or
   # unsetting the cpuopts variable.
   set opts {}
   if {[info exists cpuopts] && [llength $cpuopts]>0} {
      set opts [concat $opts $cpuopts]
   }

   # Use/don't use SSE intrinsics.  The default is yes, at level 2,
   # since x86_64 guarantees at least SSE2.  If cpu_arch is host, then
   # the default setting is pulled from the /proc/cpuinfo flags, via the
   # Find_SSE_Level proc (see above).
   #    You can override the value by setting the $config sse_level
   # value in the local platform file (see LOCAL CONFIGURATION above).
   if {[catch {$config GetValue sse_level}]} {
      $config SetValue sse_level 2
   }

   # Default warnings disable.
   # Warning 1572 is floating point comparisons being unreliable.
   # Warning 1624 is non-template friends of templated classes.
   set nowarn [list -wd1572,1624]
   if {[info exists nowarn] && [llength $nowarn]>0} {
      set opts [concat $opts $nowarn]
   }
   catch {unset nowarn}

   # Is there a icpc equivalent to the g++ -fnon-call-exceptions option?
   # If so, refer to g++ code block and put equivalent here.

   # Aggressive optimization flags, some of which are specific to
   # particular gcc versions, but are all processor agnostic.
   set valuesafeopts [concat $opts [GetIcpcValueSafeOptFlags $icpc_version]]
   set opts [concat $opts [GetIcpcGeneralOptFlags $icpc_version]]

   # Make user requested tweaks to compile line options
   set opts [LocalTweakOptFlags $config $opts]
   set valuesafeopts [LocalTweakValueSafeOptFlags $config $valuesafeopts]

   # NOTE: If you want good performance, be sure to edit ../options.tcl
   #  or ../local/options.tcl to include the line
   #    Oc_Option Add * Platform cflags {-def NDEBUG}
   #  so that the NDEBUG symbol is defined during compile.
   $config SetValue program_compiler_c++_option_opt "format \"$opts\""
   $config SetValue program_compiler_c++_option_valuesafeopt \
      "format \"$valuesafeopts\""
   $config SetValue program_compiler_c++_option_out {format "-o \"%s\""}
   $config SetValue program_compiler_c++_option_src {format \"%s\"}
   $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
   $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}
   $config SetValue program_compiler_c++_option_debug {format "-g"}

   # Warning enabled by '-warn 1' setting to cflags in Oc_Option
   #   Platform setting.
   $config SetValue program_compiler_c++_option_warn \
      { format "-Wall -Werror -wd1418,1419,279,810,981,383,1572,1624" }
   # { format "-w0 -verbose \
   #   -msg_disable undpreid,novtbtritmp,boolexprconst,badmulchrcom" }

   # Wide floating point type.  Defaults to "long double" which provides
   # extra precision.  Change to "double" for somewhat faster runs with
   # reduced precision.
   if {[catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
      # Not set
      $config SetValue program_compiler_c++_typedef_realwide "long double"
   }

   # Experimental: The OC_REAL8m type is intended to be at least
   # 8 bytes wide.  Generally OC_REAL8m is typedef'ed to double,
   # but you can try setting this to "long double" for extra
   # precision (and extra slowness).  If this is set to "long double",
   # then so should realwide in the preceding stanza.
   if {[catch {$config GetValue program_compiler_c++_typedef_real8m}]} {
      # Not set
      $config SetValue program_compiler_c++_typedef_real8m "double"
   }

} else {
   puts stderr "Warning: Requested compiler \"$ccbasename\" is not supported."
   exit 1
}

# The program to run on this platform to link together object files and
# library files to create an executable binary.
set linkername [$config GetValue program_compiler_c++]
if {[set compileonly [lsearch -exact $linkername "-c"]]>=0} {
    # Remove "-c" compile only flag from compiler string, so that the
    # string may be used to run the linker.
    set linkername [lreplace $linkername $compileonly $compileonly]
}
unset compileonly
if {[string match g++ $ccbasename]} {
    # ...for GNU g++ as linker
    lappend linkername -pthread  ;# Apparently needed by some gcc installs
    $config SetValue program_linker $linkername
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_rpath {format "-Wl,-rpath=%s"}
    $config SetValue program_linker_uses_-L-l {1}
} elseif {[string match pgc++ $ccbasename]} {
    # ...for Portland Group pgc++ as linker
    $config SetValue program_linker $linkername
    set linkeropts [GetPgccLinkOptFlags $pgcc_version]
    ## Without -Mnoen_frame, threads in Oxs abort during
    ## Tcl_ExitThread() processing with the error "terminate called
    ## without an active exception" if the thread code contains a
    ## try/catch block.  (At least for pgc++ version 16.10-0.)
    if {[info exists opts]} {
       set linkeropts [concat $opts $linkeropts]
    }
    $config SetValue program_linker [concat $linkername $linkeropts]
    unset linkeropts
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_rpath {format "-Wl,-rpath=%s"}
    $config SetValue program_linker_uses_-L-l {1}
    $config SetValue program_linker_uses_-I-L-l {0}
 } elseif {[string match icpc $ccbasename]} {
    # ...for Intel's icpc as linker
    set linkcmdline $linkername
    # If -fast or other flags that enable interprocedural optimizations
    # (IPO) appear in the program_compiler_c++_option_opt value above,
    # then append those flags into program_linker too.
    if {[lsearch -exact $opts -fast]>=0 || [lsearch -glob $opts -ipo*]>=0} {
       lappend linkcmdline -ipo -lsvml -wd11021
       # The svml library is needed for the dvl/spectrum executable
       # and the oommf/app/oxs/ext/fft3v.cc object module when compiled
       # with some releases of the icpc v9 and v10 compiler.
       # -wd11021 disables ipo warning 11021 which complains about a
       # long list of unresolved symbols in libtk.
    }
    if {[lsearch -exact $opts -parallel]>=0} {
       lappend linkcmdline -parallel
    }
    $config SetValue program_linker $linkcmdline
    unset linkcmdline

    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_rpath {format "-Qoption,ld,-rpath=%s"}
    $config SetValue program_linker_uses_-L-l {1}
}
unset linkername

# The program to run on this platform to create a single library file out
# of many object files.
if {[string match icpc $ccbasename]} {
    $config SetValue program_libmaker {xiar csr}
    $config SetValue program_libmaker_option_obj {format \"%s\"}
    $config SetValue program_libmaker_option_out {format \"%s\"}
} elseif {[string match pgc++ $ccbasename]} {
    $config SetValue program_libmaker {ar cr}
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
# If we're linking to the Tcl and Tk shared libraries, we don't need to
# explicitly pull in the extra libraries set in the TCL_LIBS and TK_LIBS
# variables by tclConfig.sh and tkConfig.sh.  Moreover, the TK_LIBS list
# may contain libraries such as -lXft that aren't installed on the oommf
# target.  For Tcl/Tk 8.3 and latter, shared libraries are the default.
# Adjust as necessary.
set major [set minor [set serial 0]]
foreach {major minor serial} [split [info patchlevel] .] { break }
if {$major>8 || ($major==8 && $minor>=3)} {
   $config SetValue TCL_LIBS {}
   $config SetValue TK_LIBS {}
}
unset major ; unset minor ; unset serial

########################################################################
if {[catch {$config GetValue program_linker_extra_libs} extra_libs]} {
   set extra_libs {}
}
foreach {glibc_major glibc_minor} [GetGlibcVersion] break
if {$glibc_major<2 || ($glibc_major==2 && $glibc_minor<17)} {
   # The realtime extensions library is needed for glibc prior
   # to 2.17 for clock_gettime() support.
   lappend extra_libs -lrt
}
if {![catch {$config GetValue use_numa} _] && $_} {
   # Include NUMA (non-uniform memory access) library
   lappend extra_libs -lnuma
}
if {[llength $extra_libs]>0} {
   $config SetValue TCL_LIBS [concat [$config GetValue TCL_LIBS] $extra_libs]
   $config SetValue TK_LIBS [concat [$config GetValue TK_LIBS] $extra_libs]
}

########################################################################
# If not specified otherwise, assume that double precision arithmetic
# may be performed with extra precision intermediate values:
if {[catch {$config GetValue \
               program_compiler_c++_property_fp_double_extra_precision}]} {
   $config SetValue program_compiler_c++_property_fp_double_extra_precision 1
}

########################################################################

unset config
