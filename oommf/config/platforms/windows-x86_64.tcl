# FILE: windows-x86_64.tcl
#
# Configuration feature definitions for the configuration 'windows-x86_64'
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

# NOTE: The rest of the REQUIRED CONFIGURATION is required only
# for building OOMMF software from source code.  If you downloaded
# a distribution with pre-compiled executables, no more configuration
# is required.
#
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
# Microsoft Visual C++
# <URL:http://msdn.microsoft.com/visualc/>
$config SetValue program_compiler_c++ {cl /c}
#
# MINGW + gcc
#$config SetValue program_compiler_c++ {g++ -c}
#

########################################################################
# SUPPORT PROCEDURES
#
# Load routines to guess the CPU, determine compiler version, and
# provide appropriate cpu-specific and compiler version-specific
# optimization flags.
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         cpuguess-windows-x86_64.tcl]

# Miscellaneous processing routines
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         misc-support.tcl]

# On Windows, child threads get the system default x87 control word,
# which in particular means the floating point precision is set to use a
# 53 bit mantissa ( = 8-byte float)), rather than the 64 bit mantissa (
# = 10-byte float) used by default by the gcc and bcc (and possibly
# other) compilers for the main thread.  In principle (if not in
# practice), the SSE control word could be similarly affected.  The
# following option activates code to copy fpu control registers from the
# parent thread to its children.
$config SetValue \
   program_compiler_c++_property_init_thread_fpu_control_word 1

# Miscellaneous processing routines
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         misc-support.tcl]

########################################################################
# LOCAL CONFIGURATION
#
# The following options may be defined in the
# platforms/local/windows-x86_64.tcl file:
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
## Override default C++ compiler.  Note the "_override" suffix
## on the value name.
# $config SetValue program_compiler_c++_override {g++ -c}
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
# $config SetValue program_compiler_c++_async_exception_handling SEH
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
# $config SetValue program_compiler_c++_oc_index_type {
#    __int64 {unsigned __int64} 8
# }
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
## Extra library flags to throw onto link command.  Use sparingly ---
## for most needs program_linker_extra_lib_dirs and
## program_linker_extra_lib_scripts should suffice.
# $config SetValue program_linker_extra_args
#    {-L/opt/local/lib -lfftw3 -lsundials_cvode -lsundials_nvecserial}
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
   # Value not set in platforms/local/windows-x86_64.tcl,
   # so use Tcl setting.
   global tcl_platform
   if {[info exists tcl_platform(threaded)] \
          && $tcl_platform(threaded)} {
      $config SetValue oommf_threads 1  ;# Yes threads
   } else {
      $config SetValue oommf_threads 0  ;# No threads
   }
}
$config SetValue thread_count_auto_max 8 ;# Arbitrarily limit
## maximum number of "auto" threads to 8.
if {[catch {$config GetValue thread_count}]} {
   # Value not set in platforms/local/windows-x86_64.tcl, so
   # query the system:
   set processor_count 0
   if {![catch {exec wmic cpu get NumberOfCores /all} data]
       && [llength $data]>1} {
      foreach cpucores [lrange $data 1 end] {
         catch {incr processor_count $cpucores}
      }
   }
   if {$processor_count<1 && [info exists env(NUMBER_OF_PROCESSORS)]} {
      set processor_count $env(NUMBER_OF_PROCESSORS)
      # Note: NUMBER_OF_PROCESSORS is logical processor count, including
      # those resulting from hyperthreadeding (if any).
   }
   if {$processor_count<1} {
      set processor_count 1
   }
   set auto_max [$config GetValue thread_count_auto_max]
   if {$processor_count>$auto_max} {
      # Limit automatically set thread count to auto_max
      set processor_count $auto_max
   }
   $config SetValue thread_count $processor_count
}

if {[catch {$config GetValue program_compiler_c++_override} compiler] == 0} {
    $config SetValue program_compiler_c++ $compiler
}

# The absolute, native filename of the null device
$config SetValue path_device_null {nul:}

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
if {[catch {$config GetValue program_compiler_c++} ccbasename]} {
   set ccbasename {}  ;# C++ compiler not selected
} else {
   set ccbasename [file tail [lindex $ccbasename 0]]
}

# Microsoft Visual C++ compiler
if {[string match cl $ccbasename]} {
   catch {unset cl_libs}
   set compilestr [$config GetValue program_compiler_c++]
   if {![info exists cl_version]} {
      set cl_version [GuessClVersion [lindex $compilestr 0]]
   }
   if {![string match {} $cl_version] && [lindex $cl_version 0]<12} {
      puts stderr "WARNING: This version of OOMMF requires\
                   Visual C++ 12.0 (aka Visual Studio 2013)\
                   or later (C++ 11 support)"
   }
   $config SetValue program_compiler_c++_banner_cmd \
      [list GetClBannerVersion  [lindex $compilestr 0]]
   lappend compilestr /nologo /GR ;# /GR turns on RTTI
   set cl_major_version [lindex $cl_version 0]

   if {[lindex $cl_major_version 0]>7} {
      # The exception handling specification switch "/GX" is deprecated
      # in version 8.  /EHa enables C++ exceptions with SEH exceptions,
      # /EHs enables C++ exceptions without SEH exceptions, and /EHc
      # sets extern "C" to default to nothrow.
      if {[catch {$config \
           GetValue program_compiler_c++_async_exception_handling} _eh]} {
         set _eh POSIX ;# Default setting
         $config SetValue program_compiler_c++_async_exception_handling $_eh
      }
      if {[string compare -nocase SEH $_eh]==0} {
         lappend compilestr /EHac
      } else {
         lappend compilestr /EHsc
      }
      unset _eh
   } else {
      lappend compilestr /GX
   }

   $config SetValue program_compiler_c++ $compilestr
   unset compilestr

   # Optimization options for Microsoft Visual C++
   #
   # VC++ earlier than 12.0 (2013) are not supported.
   #
   # Options for VC++:
   #                  Disable optimizations: /Od
   #                   Maximum optimization: /Ox
   #                    Enable stack checks: /GZ
   #                Buffers security checks: /GS[-] (default is enabled)
   #                   Require AVX  support: /arch:AVX
   #                   Require AVX2 support: /arch:AVX2
   # Fast (less predictable) floating point: /fp:fast
   #         Value safe floating point opts: /fp:precise
   #     Use portable but insecure lib fcns: /D_CRT_SECURE_NO_DEPRECATE
   # Note: x86_64 guarantees SSE2 support.
   #

   # CPU model architecture specific options.  To override, set value
   # program_compiler_c++_cpu_arch in
   # oommf/config/platform/local/windows-x86_64.tcl.
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
      # Use/don't use SSE intrinsics.  In the cpu_arch!=generic case,
      # the default behavior is to set this from the third element of
      # the GuessCpu return.  If cpu_arch==generic, then the default
      # is no.  You can always override the default behavior setting
      # the $config sse_level value in the local platform file (see
      # LOCAL CONFIGURATION above).
      if {[catch {$config GetValue sse_level}]} {
         # sse_level not set in LOCAL CONFIGURATION block
         $config SetValue sse_level [lindex $cpu_arch 2]
      }
      set cpuopts [GetClCpuOptFlags $cl_version $cpu_arch x86_64]
      # Note: In the cpu_arch != generic case, GetClCpuOptFlags will
      # include the appropriate SSE flags.  If possible, use this
      # rather than trying to manually set the SSE level because
      # GetClCpuOptFlags knows something about what options are
      # available in which versions of cl
   }
   unset cpu_arch
   # You can override the above results by directly setting or
   # unsetting the cpuopts variable, e.g.,
   #
   #    set cpuopts [list /arch:SSE2]
   # or
   #    unset cpuopts
   #

   # Use/don't use SSE source-code intrinsics (as opposed to compiler
   # generated SSE instructions, which are controlled by the /arch:
   # command line option.  The default is '2', because x86_64
   # guarantees at least SSE2.  You can override the value by setting
   # the $config sse_level value in the local platform file (see LOCAL
   # CONFIGURATION above).
   if {[catch {$config GetValue sse_level}]} {
      $config SetValue sse_level 2
   }

   # User-requested optimization level
   if {[catch {Oc_Option GetValue Platform optlevel} optlevel]} {
      set optlevel 2 ;# Default
   }

   # Aggressive optimization flags, some of which are specific to
   # particular cl versions, but are all processor agnostic.
   set opts [GetClGeneralOptFlags $cl_version x86_64 $optlevel]
   set valuesafeopts [GetClValueSafeOptFlags $cl_version x86_64 $optlevel]

   if {[info exists cpuopts] && [llength $cpuopts]>0} {
      set opts [concat $opts $cpuopts]
      set valuesafeopts [concat $valuesafeopts $cpuopts]
   }

   # Silence security warnings
   if {$cl_major_version>7} {
      lappend opts /D_CRT_SECURE_NO_DEPRECATE
      lappend valuesafeopts /D_CRT_SECURE_NO_DEPRECATE
   }

   # Make user requested tweaks to compile line options
   set opts [LocalTweakOptFlags $config $opts]
   set valuesafeopts [LocalTweakValueSafeOptFlags $config $valuesafeopts]

   # NOTE: If you want good performance, be sure to edit ../options.tcl
   #  or ../local/options.tcl to include the line
   #    Oc_Option Add * Platform cflags {-def NDEBUG}
   #  so that the NDEBUG symbol is defined during compile.
   $config SetValue program_compiler_c++_option_opt \
      "format \"$opts\""
   $config SetValue program_compiler_c++_option_valuesafeopt \
      "format \"$valuesafeopts\""
   $config SetValue program_compiler_c++_option_out {format "\"/Fo%s\""}
   $config SetValue program_compiler_c++_option_src {format "\"/Tp%s\""}
   $config SetValue program_compiler_c++_option_inc {format "\"/I%s\""}
   $config SetValue program_compiler_c++_option_warn {
      Platform Index [list {} {/W4 /wd4505 /wd4702 /wd4127}]
   }

   #   Warning C4505 is about removal of unreferenced local functions.
   # This seems to be a common occurrence when using templates with the
   # so-called "Borland" model.
   #   Warning C4702 is about unreachable code.  A lot of warnings of
   # this type are generated in the STL; I'm not even sure they are all
   # true.
   #   Warning C4127 is "conditional expression is constant".  This
   # occurs intentionally many places in the code, either for
   # readability or as a consequence of supporting multiple platforms.
   #
   $config SetValue program_compiler_c++_option_debug {
      Platform Index [list {} {/Zi /Fdwindows-x86_64/}]
   }
   $config SetValue program_compiler_c++_option_def {format "\"/D%s\""}

   # Use OOMMF supplied erf() error function
   $config SetValue program_compiler_c++_property_no_erf 1

   # Use _hypot() in place of hypot()
   $config SetValue program_compiler_c++_property_use_underscore_hypot 1

   # Use _getpid() in place of getpid()
   $config SetValue program_compiler_c++_property_use_underscore_getpid 1

   # Widest natively support floating point type
   if {[catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
      $config SetValue program_compiler_c++_typedef_realwide "double"
   }

   # Under the Microsoft Visual C++ compiler, 80-bit floating point
   # is not supported; both double and long double are 64-bits. and
   # there are no extra precision intermediate values.
   $config SetValue program_compiler_c++_property_fp_double_extra_precision 0

   # Visual C++ 6.0 does not support direct conversion from
   # unsigned __int64 to double.  If automatic detection doesn't
   # work, set cl_version directly to 5, 6, or 7, as appropriate.
   $config SetValue program_compiler_c++_uint64_to_double 0
   if {$cl_major_version>=7} {
      $config SetValue program_compiler_c++_uint64_to_double 1
   }

   # Visual C++ 8.0 does not provide the _mm_cvtsd_f64 intrinsic.
   # Visual C++ 9.0 and later do.
   if {$cl_major_version < 9} {
      $config SetValue program_compiler_c++_missing_cvtsd_f64 1
   } else {
      $config SetValue program_compiler_c++_missing_cvtsd_f64 0
   }

   # Visual C++ support for SetCurrentProcessExplicitAppUserModelID
   # began with version 10 (2010).  Note: Enabling this block makes
   # the executables require Windows 7 or later.
   if {$cl_major_version >= 10} {
      $config SetValue program_compiler_c++_set_taskbar_id 1
      lappend cl_libs shell32.lib
   }

   # The program to run on this platform to create a single library file out
   # of many object files.
   # Microsoft Visual C++'s library maker
   $config SetValue program_libmaker {link /lib}
   # If your link doesn't accept the /lib option, try this instead:
   # $config SetValue program_libmaker {lib}
   $config SetValue program_libmaker_option_obj {format \"%s\"}
   $config SetValue program_libmaker_option_out {format "\"/OUT:%s\""}

   # The program to run on this platform to link together object files and
   # library files to create an executable binary.
   # Microsoft Visual C++'s linker
   $config SetValue program_linker {link}
   $config SetValue program_linker_option_debug {format "/DEBUG"}
   $config SetValue program_linker_option_obj {format \"%s\"}
   $config SetValue program_linker_option_out {format "\"/OUT:%s\""}
   $config SetValue program_linker_option_lib {format \"%s\"}
   $config SetValue program_linker_option_sub {format "\"/SUBSYSTEM:%s\""}
   # Note 1: advapi32 is needed for GetUserName function in Nb package.
   # Note 2: shell32.lib is needed for SetCurrentProcessExplicitAppUserModelID
   #   function called inside WinMain in pkg/oc/oc.cc to tie all OOMMF
   #   apps to the same taskbar group in Windows 7.
   # Note 3: ntdll.lib is used in processing structured exception codes.
   lappend cl_libs user32.lib advapi32.lib
   if {![catch {$config GetValue program_compiler_c++_async_exception_handling} _]
       && [string compare -nocase SEH $_]==0} {
      lappend cl_libs ntdll.lib
   }
   $config SetValue TK_LIBS $cl_libs
   $config SetValue TCL_LIBS $cl_libs
   $config SetValue program_linker_uses_-L-l {0}
   $config SetValue program_linker_uses_-I-L-l {0}

   unset cl_libs
   unset cl_version
   unset cl_major_version
} elseif {[string match g++* $ccbasename]} {
   # ... for MINGW + GNU g++ C++ compiler
   if {![info exists gcc_version]} {
      set gcc_version [GuessGccVersion \
                          [lindex [$config GetValue program_compiler_c++] 0]]
   }
   if {[lindex $gcc_version 0]<4 ||
       ([lindex $gcc_version 0]==4 && [lindex $gcc_version 1]<7)} {
      puts stderr "WARNING: This version of OOMMF requires g++ 4.7\
                   or later (C++ 11 support)"
   }
   $config SetValue program_compiler_c++_banner_cmd \
      [list GetGccBannerVersion  \
          [lindex [$config GetValue program_compiler_c++] 0]]

   # Optimization options
   # CPU model architecture specific options.  To override, set Option
   # program_compiler_c++_cpu_arch in oommf/config/options.tcl (or,
   # preferably, in oommf/config/local/options.tcl).  See note about SSE
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
      # Use/don't use SSE intrinsics.  In the cpu_arch!=generic case,
      # the default behavior is to set this from the third element of
      # the GuessCpu return.  If cpu_arch==generic, then the default is
      # 2 (minimum level guaranteed on x86_64).  You can always override
      # the default behavior setting the $config sse_level value in the
      # local platform file (see LOCAL CONFIGURATION above).
      if {[catch {$config GetValue sse_level}]} {
         # sse_level not set in LOCAL CONFIGURATION block
         $config SetValue sse_level [lindex $cpu_arch 2]
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

   # C++11 support
   lappend opts -std=c++11

   # The gcc x86 toolchain supports 80-bit long doubles, and the "L"
   # length modifier (e.g., %Le, %Lf, %Lg, %La) to the printf family for
   # formatting long double values.  However, the long double type in
   # Windows is the same width as double, namely 64-bits.  The result is
   # that gcc 80-bit long doubles can't be printed through the Windows
   # I/O libraries.  This can be checked looking at the long double
   # output from oommf/pkg/oc/<platform>/varinfo.
   #
   #  MinGW provides an alternative, POSIX/ISOC99 compliant, I/O
   # library.  This is selected (1) or deselected (0) by the macro
   # __USE_MINGW_ANSI_STDIO.  Older references recommendation setting
   # this macro to 1 before #including any system header files.
   # However, beginning with a 23-Dec-2018 commit of _mingw.h, a
   # deprecation warning is triggered if __USE_MINGW_ANSI_STDIO is
   # user-defined.  Instead, one is suppose to #define one of the
   # following feature macros:
   #
   #    _POSIX, _POSIX_SOURCE, _POSIX_C_SOURCE, _SVID_SOURCE,
   #    _ISOC99_SOURCE,_XOPEN_SOURCE, _XOPEN_SOURCE_EXTENDED,
   #
   # (or _GNU_SOURCE with __cplusplus).  If any of these is #defined
   # and __USE_MINGW_ANSI_STDIO is not defined, then the _mingw.h
   # header will define __USE_MINGW_ANSI_STDIO to 1 and thus enable
   # the MinGW stdio routines.
   #
   #  If __USE_MINGW_ANSI_STDIO is not defined, then C++ may or may not
   # enable the MinGW I/O, depending upon the MinGW/gcc release and
   # "bitness" (32- or 64-).  The os_defines.h file buried deep in the
   # mingw{32,64}/include/c++/ tree implies that the MinGW I/O routines
   # are required for libstdc++, but my Sep 2019 gcc 9.2.0-2 install
   # enables them for 64-bit builds but not for 32-bit builds.  YMMV.
   # See the _mingw.h header in
   #
   #    mingw32/i686-w64-mingw32/include/
   # or
   #    mingw64/x86_64-w64-mingw32/include/
   #
   # for additional details.  My best guess at the most robust solution
   # at this time is to define _POSIX_SOURCE on the compile line.  (mjd,
   # 2019/09)
   lappend opts "-D_POSIX_SOURCE"

   # SSE support
   if {[catch {$config GetValue sse_level} sse_level]} {
      set sse_level 2  ;# Default setting for x86_64
      $config SetValue sse_level $sse_level
   }
   if {!$sse_level} {
      # Strip out all SSE options
      regsub -all -- {-mfpmath=[^ ]*} $opts {} opts
      regsub -all -- {-msse[^ ]*} $opts {} opts
      lappend opts -mfpmath=387
   }

   # Default warnings disable
   set nowarn [list -Wno-non-template-friend]
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
   set valuesafeopts [concat $opts [GetGccValueSafeOptFlags $gcc_version $optlevel]]
   set opts [concat $opts [GetGccGeneralOptFlags $gcc_version $optlevel]]

   if {[info exists cpuopts] && [llength $cpuopts]>0} {
      set opts [concat $opts $cpuopts]
      set valuesafeopts [concat $valuesafeopts $cpuopts]
   }

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
   # Update: gcc 3.4.5 issues an uninitialized warning in the STL,
   #  so -Wno-uninitialized is necessary.
   # Update: gcc 4.7.3 format checks don't recognize the long double
   #  "L" modifier, so -Wno-format is necessary.  Apparently there
   #  is a workaround using the __MINGW_PRINTF_FORMAT macro, if you
   #  don't mind the GCC + MinGW specific nature of it.
   $config SetValue program_compiler_c++_option_warn {
      Platform Index [list {} { \
         -Wall -W -Wpointer-arith -Wwrite-strings \
         -Woverloaded-virtual -Wsynth -Werror -Wno-uninitialized \
         -Wno-unused-function -Wno-format}]
   }

   # Wide floating point type.
   # NOTE: On the x86_64+gcc platform, "long double" provides better
   # precision than "double", but at a cost of increased memory usage
   # and a decrease in speed.  (The long double takes 16 bytes of
   # storage as opposed to 8 for double, but provides the x86 80-bit
   # native floating point format having approx. 19 decimal digits
   # precision as opposed to 16 for double.)
   # Default is "double".
   if {[catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
      $config SetValue program_compiler_c++_typedef_realwide "double"
   }

   # Experimental: The OC_REAL8m type is intended to be at least
   # 8 bytes wide.  Generally OC_REAL8m is typedef'ed to double,
   # but you can try setting this to "long double" for extra
   # precision (and extra slowness).  If this is set to "long double",
   # then so should realwide in the preceding stanza.
   # $config SetValue program_compiler_c++_typedef_real8m "long double"

   # Directories to exclude from explicit include search path, i.e.,
   # the -I list.  Some versions of gcc complain if "system" directories
   # appear in the -I list.
   $config SetValue \
      program_compiler_c++_system_include_path [list /usr/include]

   # Other compiler properties
   $config SetValue \
      program_compiler_c++_property_optimization_breaks_varargs 0

   # The program to run on this platform to create a single library file out
   # of many object files.
   # ... GNU ar ...
   $config SetValue program_libmaker {ar cr}
   $config SetValue program_libmaker_option_obj {format \"%s\"}
   $config SetValue program_libmaker_option_out {format \"%s\"}

   # The program to run on this platform to link together object files and
   # library files to create an executable binary.

   # NOTE: g++-built executables link in libgcc and libstdc++ libraries
   #       from the g++ distribution.  The default it to link these as
   #       shared libraries, in which case the directory containing
   #       these libraries (typically C:\MinGW\bin) needs to be included
   #       in the PATH environment variable.  Alternatively, one can add
   #       the -static-libgcc -static-libstdc++ options to the link
   #       line, in which case the PATH doesn't need to be set, but the
   #       executables will be larger.
   #       Pick one:
   # $config SetValue program_linker [list $ccbasename]
   $config SetValue program_linker \
       [list $ccbasename -static-libgcc -static-libstdc++]

   $config SetValue program_linker_option_obj {format \"%s\"}
   $config SetValue program_linker_option_out {format "-o \"%s\""}
   $config SetValue program_linker_option_lib {format \"%s\"}
   proc fufufubar { subsystem } {
      if {[string match CONSOLE $subsystem]} {
         return "-Wl,--subsystem,console"
      }
      return "-Wl,--subsystem,windows"
   }
   $config SetValue program_linker_option_sub {fufufubar}
   $config SetValue program_linker_uses_-L-l {1}
   $config SetValue TK_LIBS {}
   $config SetValue TCL_LIBS {}

   unset gcc_version
} else {
   puts stderr "Warning: Requested compiler \"$ccbasename\" is not supported."
}
catch {unset ccbasename}

# A partial Tcl command (or script) which when completed by lappending
# a file name stem and evaluated returns the corresponding file name for
# an executable on this platform
$config SetValue script_filename_executable {format %s.exe}

# A partial Tcl command (or script) which when completed by lappending
# a file name stem and evaluated returns the corresponding file name for
# an object file on this platform
$config SetValue script_filename_object {format %s.obj}

# A partial Tcl command (or script) which when completed by lappending
# a file name stem and evaluated returns the corresponding file name for
# a static library on this platform
$config SetValue script_filename_static_library {format %s.lib}

# Partial Tcl commands (or scripts) for creating library file names from
# stems for third party libraries that don't follow the
# script_filename_static_library rule.
if {[catch {$config GetValue program_linker_extra_lib_scripts} \
         extra_lib_scripts]} {
   set extra_lib_scripts {}
}
lappend extra_lib_scripts {format "lib%s.lib"}
lappend extra_lib_scripts {format "lib%s.a"}
$config SetValue program_linker_extra_lib_scripts $extra_lib_scripts
unset extra_lib_scripts

# A list of partial Tcl commands (or scripts) which when completed by
# lappending a file name stem and evaluated returns the corresponding
# file name for an intermediate file produced by the linker on this platform
$config SetValue script_filename_intermediate [list \
   {format %s.ilk} {format %s.pdb} {format %s.map}]

########################################################################
unset config
