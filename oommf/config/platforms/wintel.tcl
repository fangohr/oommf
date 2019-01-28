# FILE: wintel.tcl
#
# Configuration feature definitions for the configuration 'wintel'
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
# If your compiler is not listed below, additional features will
# have to be added in the BUILD CONFIGURATION section below to
# describe to the OOMMF software how to operate your compiler.  Send
# e-mail to the OOMMF developers for assistance.
#
# Microsoft Visual C++
# <URL:http://msdn.microsoft.com/visualc/>
$config SetValue program_compiler_c++ {cl /c}
#
# Intel C++
# <URL:http://www.intel.com>
# $config SetValue program_compiler_c++ {icl /nologo /c /GX /GR}
#
# Borland C++ 5.5 Win32 command line tools.
# <URL:http://www.inprise.com/>
#$config SetValue program_compiler_c++ {bcc32 -c}
#
#   It may also be necessary to add something like the following to
# the C++ code:
#
#      #ifndef STATIC_BUILD
#      #if defined(_MSC_VER)
#      #   define EXPORT(a,b) __declspec(dllexport) a b
#      #   define DllEntryPoint DllMain
#      #else
#      #   if defined(__BORLANDC__)
#      #       define EXPORT(a,b) a _export b
#      #   else
#      #       define EXPORT(a,b) a b
#      #   endif
#      #endif
#      #endif
#
# MINGW32 + gcc
#$config SetValue program_compiler_c++ {g++ -c}
#
# Digital Mars compiler.  Note: Setup below assumes that
# the STLport "C++ Standard Library" is installed.
#$config SetValue program_compiler_c++ {dmc -c -Aa -Ae -Ar}
#
# Open Watcom C++
#  Configuration notes:
#    This build tested with the Open Watcom 1.3 release, with the
#  4.6.2 release of STLport.  A mostly functional STL is required
#  to build and use OOMMF.  Unfortunately, Open Watcom 1.3 does not
#  ship with an STL, and the 4.6.2 release of STLport does not
#  support Open Watcom "out-of-the-box."  However, the OS2 port of
#  STLport for Watcom,
#          STLport-4.6.2_os2watcom-01.zip
#  is pretty close to working for OOMMF.  Be sure to get the directory
#  <STLport-4.6.2>/stlport in front of the other Watcom directories in
#  the include path.  The most convenient way to do this is to place
#  it at the front of the INCLUDE environment variable.  Then, you
#  also need to enable ARROW_OPERATOR by commenting out
#          #define _STLP_NO_ARROW_OPERATOR 1
#  in file stlport/config/stl_watcom.h, line 57.
#     If you plan to use Tcl/Tk libraries built with the Microsoft VC++
#  compiler, such as those distributed by ActiveState, you will need to
#  patch the tcl.h file, to inform the Watcom linker about the parameter
#  passing and symbol naming conventions in those libraries:
#
#      A) Around line 200, add:
#       #   elif defined(__WATCOMC__)
#       #       define DLLIMPORT __declspec(__cdecl)
#       #       define DLLEXPORT __declspec(__cdecl)
#
#      B) Around line 702, change
#       typedef TCL_STORAGE_CLASS int (Tcl_PackageInitProc) \
#                                _ANSI_ARGS_((Tcl_Interp *interp));
#
#  Diff output for tcl.h, version 8.4.11:
#
#   --- tcl.h~      2005-06-18 15:24:16.000000000 -0400
#   +++ tcl.h       2005-09-01 17:37:15.404608000 -0400
#   @@ -196,6 +196,9 @@
#    #   if (defined(__WIN32__) && (defined(_MSC_VER) || (__BORLANDC__ >= 0x0550) || (defined(__GNUC__) && defined(__declspec)))) || (defined(MAC_TCL) && FUNCTION_DECLSPEC)
#    #      define DLLIMPORT __declspec(dllimport)
#    #      define DLLEXPORT __declspec(dllexport)
#   +#   elif defined(__WATCOMC__)
#   +#       define DLLIMPORT __declspec(__cdecl)
#   +#       define DLLEXPORT __declspec(__cdecl)
#    #   else
#    #      define DLLIMPORT
#    #      define DLLEXPORT
#   @@ -697,7 +700,8 @@
#    typedef void (Tcl_NamespaceDeleteProc) _ANSI_ARGS_((ClientData clientData));
#    typedef int (Tcl_ObjCmdProc) _ANSI_ARGS_((ClientData clientData,
#           Tcl_Interp *interp, int objc, struct Tcl_Obj * CONST * objv));
#   -typedef int (Tcl_PackageInitProc) _ANSI_ARGS_((Tcl_Interp *interp));
#   +typedef TCL_STORAGE_CLASS int (Tcl_PackageInitProc)
#   +         _ANSI_ARGS_((Tcl_Interp *interp));
#    typedef void (Tcl_PanicProc) _ANSI_ARGS_(TCL_VARARGS(CONST char *, format));
#    typedef void (Tcl_TcpAcceptProc) _ANSI_ARGS_((ClientData callbackData,
#            Tcl_Channel chan, char *address, int port));
#
#
#  Another (untested) possibility is to use "#pragma aux <function> frame;"
#  to get parameters passed via the stack.
#
#  IMPORTANT NOTE: Open Watcom 1.3 has issues with spaces in
#  pathnames.  Build oommf in a directory without spaces anywhere in
#  its path, e.g., C:\oommf or C:\users\bjones\oommf are okay, but
#  "C:\Program Files\oommf" is not.
#
#  Uncomment the following SetValue line to select the Open Watcom
#  compiler.  The -xr switch enables RTTI, -xs enables exceptions.
#  The -6s option specifies Pentium Pro architecture with stack
#  calling convention.
#$config SetValue program_compiler_c++ {wcl386 -c -xr -xs -6s}

########################################################################
# SUPPORT PROCEDURES
#
# Load routines to guess the CPU, determine compiler version, and
# provide appropriate cpu-specific and compiler version-specific
# optimization flags.
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         cpuguess-wintel.tcl]

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
# The following options may be defined in the platforms/local/wintel.tcl
# file:
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
## depend on the selected compiler).  Note: The cpu_arch selection
## below must support the desired sse_level.  In particular, on 32-bit
## systems cpu_arch == generic does not support SSE.
# $config SetValue sse_level 2  ;# Replace '2' with desired level
#
## Override default C++ compiler.  Note the "_override" suffix
## on the value name.
# $config SetValue program_compiler_c++_override {icl /nologo /c /GX /GR}
#
## Processor architecture for compiling.  The default is "generic"
## which should produce an executable that runs on any cpu model for
## the given platform.  Optionally, one may specify "host", in which
## case the build scripts will try to automatically detect the
## processor type on the current system, and select compiler options
## specific to that processor model.  The resulting binary will
## generally not run on other architectures.
# $config SetValue program_compiler_c++_cpu_arch generic
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
   # Value not set in platforms/local/wintel.tcl, so
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
#  BUILD CONFIGURATION

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
   if {[lindex $cl_version 0]<12} {
      puts stderr "WARNING: This version of OOMMF requires\
                   Visual C++ 12.0 or later (C++ 11 support)"
   }
   $config SetValue program_compiler_c++_banner_cmd \
      [list GetClBannerVersion  [lindex $compilestr 0]]
   lappend compilestr /nologo /GR ;# /GR turns on RTTI
   set cl_major_version [lindex $cl_version 0]

   # Exception handling: /EHa enables C++ exceptions with SEH
   # exceptions, /EHs enables C++ exceptions without SEH exceptions,
   # and /EHc sets extern "C" to default to nothrow.
   lappend compilestr /EHac

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
   #                   Require SSE2 support: /arch:SSE2
   #                   Require AVX  support: /arch:AVX
   #                   Require AVX2 support: /arch:AVX2
   # Fast (less predictable) floating point: /fp:fast
   #     Use portable but insecure lib fcns: /D_CRT_SECURE_NO_DEPRECATE
   #
   # Default optimization
   #   set opts {}
   # Max optimization
   set opts [GetClGeneralOptFlags $cl_version x86]
   # Aggressive optimization flags, some of which are specific to
   # particular cl versions, but are all processor agnostic.  CPU
   # specific opts are introduced in farther below.  See
   # cpuguess-wintel.tcl and x86-support.tcl for details.

   # CPU model architecture specific options.  To override, set value
   # program_compiler_c++_cpu_arch in
   # oommf/config/platform/local/wintel.tcl.
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
      set cpuopts [GetClCpuOptFlags $cl_version $cpu_arch x86]
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
   if {[info exists cpuopts] && [llength $cpuopts]>0} {
      set opts [concat $opts $cpuopts]
   }

   # Use/don't use SSE source-code intrinsics (as opposed to compiler
   # generated SSE instructions, which are controlled by the /arch:
   # command line option.  The default is '0', because x86 (32-bit)
   # does not guarantee any SSE.  You can override the value by
   # setting the $config sse_level value in the local platform file
   # (see LOCAL CONFIGURATION above).
   if {[catch {$config GetValue sse_level}]} {
      $config SetValue sse_level 0
   }

   # Default stack alignment with this platform/compiler is 8 bytes,
   # which in particular means that SSE variables on the stack won't
   # necessarily be 16-byte aligned.
   $config SetValue program_compiler_c++_stack_alignment 8

   # Silence security warnings
   if {$cl_major_version>7} {
      lappend opts /D_CRT_SECURE_NO_DEPRECATE
   }

   # Make user requested tweaks to compile line options
   set opts [LocalTweakOptFlags $config $opts]

   # NOTE: If you want good performance, be sure to edit ../options.tcl
   #  or ../local/options.tcl to include the line
   #    Oc_Option Add * Platform cflags {-def NDEBUG}
   #  so that the NDEBUG symbol is defined during compile.
   $config SetValue program_compiler_c++_option_opt "format \"$opts\""

   $config SetValue program_compiler_c++_option_out {format "\"/Fo%s\""}
   $config SetValue program_compiler_c++_option_src {format "\"/Tp%s\""}
   $config SetValue program_compiler_c++_option_inc {format "\"/I%s\""}
   $config SetValue program_compiler_c++_option_warn {
      format "/W4 /wd4505 /wd4702 /wd4127"
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
   # $config SetValue program_compiler_c++_option_debug {format "/MLd"}
   $config SetValue program_compiler_c++_option_debug {
      format "/Zi /Fdwintel/"
   }
   $config SetValue program_compiler_c++_option_def {format "\"/D%s\""}

   # Use OOMMF supplied erf() error function
   $config SetValue program_compiler_c++_property_no_erf 1

   # Use _hypot() in place of hypot()
   $config SetValue program_compiler_c++_property_use_underscore_hypot 1

   # Use _getpid() in place of getpid()
   $config SetValue program_compiler_c++_property_use_underscore_getpid 1

   # Widest natively support floating point type
   if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
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
   # if {$cl_major_version >= 10} {
   #    $config SetValue program_compiler_c++_set_taskbar_id 1
   #    lappend cl_libs shell32.lib
   # }

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
   # $config SetValue program_linker {link /DEBUG} ;# For debugging
   $config SetValue program_linker_option_obj {format \"%s\"}
   $config SetValue program_linker_option_out {format "\"/OUT:%s\""}
   $config SetValue program_linker_option_lib {format \"%s\"}
   $config SetValue program_linker_option_sub {format "\"/SUBSYSTEM:%s\""}

   $config SetValue TCL_LIB_SPEC [$config GetValue TCL_VC_LIB_SPEC]
   $config SetValue TK_LIB_SPEC [$config GetValue TK_VC_LIB_SPEC]
   # Note 1: advapi32 is needed for GetUserName function in Nb package.
   # Note 2: We could add here support for the Windows 7 API call
   #   SetCurrentProcessExplicitAppUserModelID(), which can be used to
   #   tie all OOMMF apps to the same taskbar group in Windows 7.  This
   #   involves adding shell32.lib to the TCL/TK_LIBS lists, and setting
   #   the config value program_compiler_c++_set_taskbar_id.  See the
   #   windows-x86_64.tcl platform file for details.  We choose not to
   #   do this here because we expect 32-bit Windows builds to be aimed
   #   primarily at Windows XP and possibly Windows Vista, neither of
   #   which support that API call.
   lappend cl_libs user32.lib advapi32.lib
   $config SetValue TK_LIBS $cl_libs
   $config SetValue TCL_LIBS $cl_libs
   $config SetValue program_linker_uses_-L-l {0}
   $config SetValue program_linker_uses_-I-L-l {0}

   unset cl_libs
   unset cl_version
   unset cl_major_version
} elseif {[string match icl $ccbasename]} {
    # ... for Intel C++
    # Default optimization
    # $config SetValue program_compiler_c++_option_opt {}
    # Options:
    #             Maximum optimization: /O3
    #          multi-file optimization: /Qipo
    #   80 bit register floating point: /Qpc80
    #      Enable runtime debug checks: /GZ
    #   Optimize for Pentium processor: /G5
    #   Optimize for Pentium Pro, Pentium II and Pentium III: /G6
    #           Optimize for Pentium 4: /G7
    # Improved fp precision (some speed penalty): /Qprec
    #
    set opts [list /Qpc80 /O3]
    #
    # Note: Unfortunately, Intel C++ 5 fails with an "internal compiler
    #  error" when building Oxs if /Qipo is enabled. -mjd, 2002-10-30
    #
    # NOTE 2: If you want good performance, be sure to edit ../options.tcl
    # or ../local/options.tcl to include the line
    #  Oc_Option Add * Platform cflags {-def NDEBUG}
    # so that the NDEBUG symbol is defined during compile.
    #

    # Make user requested tweaks to compile line options
    set opts [LocalTweakOptFlags $config $opts]

   # NOTE: If you want good performance, be sure to edit ../options.tcl
   #  or ../local/options.tcl to include the line
   #    Oc_Option Add * Platform cflags {-def NDEBUG}
   #  so that the NDEBUG symbol is defined during compile.
    $config SetValue program_compiler_c++_option_opt "format \"$opts\""

    $config SetValue program_compiler_c++_option_out {format "\"/Fo%s\""}
    $config SetValue program_compiler_c++_option_src {format "\"/Tp%s\""}
    $config SetValue program_compiler_c++_option_inc {format "\"/I%s\""}
    $config SetValue program_compiler_c++_option_warn {format "/W4"}
    # $config SetValue program_compiler_c++_option_debug {format "/MLd"}
    $config SetValue program_compiler_c++_option_def {format "\"/D%s\""}

    # Note: To enable debugging output, put "-debug 1" into the
    # options file, and include the "/DEBUG" switch in the
    # program_linker definition below.
    $config SetValue program_compiler_c++_option_debug {format "/Zi"}

    # Use OOMMF supplied erf() error function
    $config SetValue program_compiler_c++_property_no_erf 1

    # Widest natively support floating point type
    if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }

    # The program to run on this platform to create a single library file out
    # of many object files.
    # Intel library maker
    $config SetValue program_libmaker {xilib}
    $config SetValue program_libmaker_option_obj {format \"%s\"}
    $config SetValue program_libmaker_option_out {format "\"/OUT:%s\""}

    # The program to run on this platform to link together object files and
    # library files to create an executable binary.
    # Intel linker
    $config SetValue program_linker {xilink}
    # $config SetValue program_linker {xilink /DEBUG}
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "\"/OUT:%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_option_sub {format "\"/SUBSYSTEM:%s\""}
    $config SetValue TCL_LIB_SPEC [$config GetValue TCL_VC_LIB_SPEC]
    $config SetValue TK_LIB_SPEC [$config GetValue TK_VC_LIB_SPEC]
    # Note: advapi32 is needed for GetUserName function in Nb package.
    $config SetValue TK_LIBS {user32.lib advapi32.lib}
    $config SetValue TCL_LIBS {user32.lib advapi32.lib}
    $config SetValue program_linker_uses_-L-l {0}
    $config SetValue program_linker_uses_-I-L-l {0}

} elseif {[string match bcc32 $ccbasename]} {
    # ... for Borland C++
    $config SetValue program_compiler_c++_banner_cmd \
       [list GetBcc32BannerVersion  \
           [lindex [$config GetValue program_compiler_c++] 0]]


    ### Compile line should look like
    ###    bcc32 -c -oOUT.obj -P infile.cc
    ### Multi-threaded builds require -WM switch
    ### NOTE: Options must come first.
    $config SetValue program_compiler_c++_option_out {format "\"-o%s\""}
    $config SetValue program_compiler_c++_option_src {format "-P \"%s\""}
    $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
    $config SetValue program_compiler_c++_option_warn \
            {format "-w"}
    $config SetValue program_compiler_c++_option_debug {format "-v"}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}

    # Use OOMMF supplied erf() error function
    $config SetValue program_compiler_c++_property_no_erf 1

    # Optimization
    # No optimizations:
    #set opts [list -Od]
    #set opts [list -Od -xs]
    # Standard optimization:
    #set opts [list -O%s]
    # High optimizations:
    set opts [list -6 -O2 -ff]
    ## Where -6: pentiumpro instructions, -O2: fastest code,
    ## -ff fast (non-comforming) floating point
    # NOTE: If you want good performance, be sure to edit ../options.tcl
    # or ../local/options.tcl to include the line
    #  Oc_Option Add * Platform cflags {-def NDEBUG}
    # so that the NDEBUG symbol is defined during compile.

    # Threads?
    if {![catch {$config GetValue oommf_threads} _] && $_} {
       lappend opts -WM    ;# Multi-threaded build
    }

    # Disable some default warnings in the opts switch, as opposed
    # to the warnings switch, so that these warnings are always
    # muted, even if '-warn' option in file options.tcl is disabled.
    # -w-8027 turns off warning messages about for and while loops not
    # being inlineable.  8004 is warning message about assigned value
    # being unused.  8026 warns about non-inlining of function because
    # of class pass by value.  8008 is conditionals that are always
    # true or false.  8066 notifies about unreachable code.
    set nowarn [list -w-8027 -w-8004 -w-8026]

    if {[info exists nowarn] && [llength $nowarn]>0} {
        set opts [concat $opts $nowarn]
    }
    catch {unset nowarn}

    # Wide floating point type.
    # NOTE: "long double" provides somewhat better precision than
    # "double", but at a cost of increased memory usage and a decrease
    # in speed.  (At this writing, long double takes 10 bytes of
    # storage as opposed to 8 for double, but provides the x86 native
    # floating point format having approx.  19 decimal digits
    # precision as opposed to 16 for double.)  Default is "double".
    # NOTE BCC: Some of the demag code in oxs (perhaps elsewhere too?)
    # requires structs of OC_REALWIDE and OC_REAL8m to be tight packed.  The
    # default alignment with the BCC compiler is 4 bytes, which results
    # in a 2 byte hole between long double members.  To remove this
    # hole, you must include the "-a2" option in the C++ compiler
    # options.
    if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }

    # Experimental: The OC_REAL8m type is intended to be at least
    # 8 bytes wide.  Generally OC_REAL8m is typedef'ed to double,
    # but you can try setting this to "long double" for extra
    # precision (and extra slowness).  If this is set to "long double",
    # then so should realwide in the preceding stanza.
    # NOTE: If you use long double, then include the -a2 switch
    # in the options, as discussed in the "realwide" stanza above.
    if {![catch {$config GetValue program_compiler_c++_typedef_real8m}]} {
       $config SetValue program_compiler_c++_typedef_real8m "double"
    }

    if {![catch {join [$config GetValue program_compiler_c++_typedef_real8m]} \
              _] && [string match {long double} $_]} {
       ## join command above drops spurious whitespace
       lappend opts -a2
    }

    # Make user requested tweaks to compile line options
    set opts [LocalTweakOptFlags $config $opts]

    # NOTE: If you want good performance, be sure to edit ../options.tcl
    #  or ../local/options.tcl to include the line
    #    Oc_Option Add * Platform cflags {-def NDEBUG}
    #  so that the NDEBUG symbol is defined during compile.
    $config SetValue program_compiler_c++_option_opt "format \"$opts\""

    # Work arounds
    # $config SetValue program_compiler_c++_property_strict_atan2 1
    ## The above should be set automatically based on the results
    ## from oommf/ext/oc/varinfo.cc.

    # Borland C++ 5.5.1 does not have placement new[].  Workaround
    # by using the single item placement new in an explicit loop.
    # If you are using a more recent verion of Borland C++ that
    # supports placement new[], then you may comment this out.
    $config SetValue program_compiler_c++_no_placment_new_array 1

    # The program to run on this platform to create a single library file out
    # of many object files.
    # Borland librarian
    $config SetValue program_libmaker {tlib}
    proc fibar { name } { return [list +[file nativename $name]] }
    $config SetValue program_libmaker_option_obj {fibar}
    proc fifibar { name } { return [list [file nativename $name]] }
    $config SetValue program_libmaker_option_out {fifibar}

    # The program to run on this platform to link together object files and
    # library files to create an executable binary.
    # Borland linker
    # $config SetValue program_linker {ilink32 -Gn -aa -x c0x32}
    $config SetValue program_linker {ilink32 -Gn -x c0x32}

    # ...for Borland linker
    ### Seems link line should look like
    ###     ilink32 -Gn -aa -x c0x32.obj OBJS, outfile.exe,,LIBS import32 cw32
    ### For multi-threaded builds, use cw32mt instead of cw32.
    ### Order is important.  The Tcl 7.5 build has the options
    ### -Tpe -ap -c pasted on in front (before c0x32.obj).  I think
    ### -c means case sensitive, but not sure about the rest.
    ### -a? is application type, where -aa=Windows GUI, -ad=Native,
    ### and -ap=Windows character.  -T?? is "output file type".
    ### Here -Gn=no state files, -x=no map. (Another option is
    ### -C=clear state before linking.)
    proc fubar { name } { return [list [file nativename $name]] }
    $config SetValue program_linker_option_obj {fubar}
    if {![catch {$config GetValue oommf_threads} _] && $_} {
       # Multi-threaded
       proc fufubar { name } { return " , [fubar $name] , , import32 cw32mt" }
    } else {
       # Single-threaded
       proc fufubar { name } { return " , [fubar $name] , , import32 cw32" }
    }
    $config SetValue program_linker_option_out {fufubar}
    $config SetValue program_linker_option_lib {fubar}
    proc fufufubar { subsystem } {
        if {[string match CONSOLE $subsystem]} {
            return "-ap"  ;# Windows character
        }
        return "-aa"      ;# Windows GUI
    }
    $config SetValue program_linker_option_sub {fufufubar}
    set dummy [$config GetValue TCL_VC_LIB_SPEC]
    regsub {\.lib$} $dummy bc.lib dummy
    $config SetValue TCL_LIB_SPEC $dummy
    set dummy [$config GetValue TK_VC_LIB_SPEC]
    regsub {\.lib$} $dummy bc.lib dummy
    $config SetValue TK_LIB_SPEC $dummy
    unset dummy
    $config SetValue TK_LIBS {}
    $config SetValue TCL_LIBS {}
    $config SetValue program_linker_uses_-L-l {0}
    $config SetValue program_linker_uses_-I-L-l {0}
} elseif {[string match g++* $ccbasename]} {
   # ... for MINGW32 + GNU g++ C++ compiler
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
   # set opts [list -O0 -fno-inline -ffloat-store]  ;# No optimization
   # set opts [list -O%s]                      ;# Minimal optimization
   set opts [GetGccGeneralOptFlags $gcc_version]
   # Aggressive optimization flags, some of which are specific to
   # particular gcc versions, but are all processor agnostic.  CPU
   # specific opts are introduced in farther below.  See
   # x86-support.tcl for details.

   # Some versions of MinGW g++ have broken headers.  A workaround
   # is to replace -std=c++11 with -std=gnu++11
   regsub -all -- {-std=c\+\+11} $opts {-std=gnu++11} opts

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
      # the GuessCpu return.  If cpu_arch==generic, then the default
      # is no.  You can always override the default behavior setting
      # the $config sse_level value in the local platform file (see
      # LOCAL CONFIGURATION above).
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
   if {[info exists cpuopts] && [llength $cpuopts]>0} {
      set opts [concat $opts $cpuopts]
   }

   # Include flag for support of requested sse_level, if necessary
   if {![catch {$config GetValue sse_level} sse_level] && $sse_level>=2} {
      if {![regexp -- {-m(sse|avx)} $opts]} {
         append opts " -msse${sse_level}"
      }
   }

   # Use ANSI conformant printf routines, which in particular
   # support the L format modifier for long double types.  The
   # __USE_MINGW_ANSI_STDIO macro is an internal compiler macro,
   # and arguably one should use -ansi or -posix (or ...?) instead,
   # but those switches introduces additional changes to compiler
   # behavior that we might not want.  BTW, this macro needs to
   # be set before any header file inclusion.  For more on this
   # macro see the mingw header file _mingw.h
   lappend opts "-D__USE_MINGW_ANSI_STDIO=1"

   # SSE support
   if {[catch {$config GetValue sse_level} sse_level]} {
      set sse_level 0  ;# Default setting for 32-bit x86
   }
   if {$sse_level} {
      if {[lindex $gcc_version 0]<4 ||
          ([lindex $gcc_version 0]==4 && [lindex $gcc_version 1]<2)} {
         # See notes below about -mstackrealign, which first appears in
         # gcc 4.2.x.
         set sse_level 0
      }
   }
   if {$sse_level} {
      # Note: -mstackrealign adds an alternative prologue and epilogue
      # to function calls that realigns the stack.  This supports mixing
      # 4-byte aligned stacks (specified by the 32-bit x86 ABI) with
      # 16-byte aligned stacks for SSE (and used by gcc).  The stack
      # realignment somewhat increases function call overhead, but is
      # needed if linking a gcc build of OOMMF against a Visual C++
      # build of Tcl/Tk.  An option here may be to use a gcc build of
      # Tcl/Tk.
      lappend opts -mstackrealign
   } else {
      # Otherwise, strip out all SSE options
      regsub -all -- {-mfpmath=[^ ]*} $opts {} opts
      regsub -all -- {-msse[^ ]*} $opts {} opts
   }
   $config SetValue sse_level $sse_level
   unset sse_level

   # Default warnings disable
   set nowarn [list -Wno-non-template-friend]
   if {[lindex $gcc_version 0]>=6} {
      lappend nowarn {-Wno-misleading-indentation}
   }
   if {[info exists nowarn] && [llength $nowarn]>0} {
      set opts [concat $opts $nowarn]
   }
   catch {unset nowarn}

   # Make user requested tweaks to compile line
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
   # Update: gcc 3.4.5 issues an uninitialized warning in the STL,
   #  so -Wno-uninitialized is necessary.
   # Update: gcc 4.7.2 format checks don't recognize the long double
   #  "L" modifier, so -Wno-format is necessary.  Apparently there
   #  is a workaround using the __MINGW_PRINTF_FORMAT macro, if you
   #  don't mind the GCC + MinGW specific nature of it.
   $config SetValue program_compiler_c++_option_warn {format "-Wall \
        -W -Wpointer-arith -Wwrite-strings \
        -Woverloaded-virtual -Wsynth -Werror -Wno-uninitialized \
        -Wno-unused-function -Wno-format"}

   # Wide floating point type.
   # NOTE: On the x86+gcc platform, "long double" provides
   # somewhat better precision than "double", but at a cost of
   # increased memory usage and a decrease in speed.  (At this writing,
   # long double takes 12 bytes of storage as opposed to 8 for double,
   # but provides the x86 native floating point format having approx.
   # 19 decimal digits precision as opposed to 16 for double.)
   # Default is "double".
   if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
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
   set linkername [list $ccbasename]

   # NOTE: g++-built executables link in libgcc and libstdc++
   #       libraries from the g++ distribution.  By default, g++ will
   #       link in libgcc statically (for proper exception handling)
   #       but the libstdc++ library will be linked shared.  In this
   #       case, the directory containing the libstdc++ library
   #       (typically C:\MinGW\bin) needs to be included in the PATH
   #       environment variable.  Alternatively, with g++ 4.5.x and
   #       later, one can add the -static-libstdc++ option to the link
   #       line, in which case the PATH doesn't need to be set,
   #       although executables will be somewhat larger.
   #       
   #       For shared linking of libstdc++ comment out this block:
   if {[lindex $gcc_version 0]>4 ||
          ([lindex $gcc_version 0]==4 && [lindex $gcc_version 1]>=5)} {
      lappend linkername -static-libgcc -static-libstdc++
   }
   $config SetValue program_linker $linkername
   unset linkername


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
   # First try "Visual C" names for Tcl/Tk libraries.  If that
   # doesn't work, then try unix naming convention, which should
   # pick up MinGW builds of Tcl/Tk.
   set tcllib [$config GetValue TCL_VC_LIB_SPEC]
   set tklib  [$config GetValue TK_VC_LIB_SPEC]
   if {![file exists $tcllib]} {
      set path [file dirname $tcllib]
      set fname [file tail $tcllib]
      regsub {\..*$} $fname {} fname
      set tname [file join $path lib${fname}.a]
      if {[file exists $tname]} {
         set tcllib $tname
      }
   }
   if {![file exists $tklib]} {
      set path [file dirname $tklib]
      set fname [file tail $tklib]
      regsub {\..*$} $fname {} fname
      set tname [file join $path lib${fname}.a]
      if {[file exists $tname]} {
         set tklib $tname
      }
   }
   $config SetValue TCL_LIB_SPEC $tcllib
   $config SetValue TK_LIB_SPEC $tklib
   $config SetValue TK_LIBS {}
   $config SetValue TCL_LIBS {}

   unset gcc_version
} elseif {[string match dmc $ccbasename]} {
    # ... for Digital Mars C++
    $config SetValue program_compiler_c++_option_out {format "\"-o%s\""}
    $config SetValue program_compiler_c++_option_src {format "-cpp \"%s\""}
    $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
    $config SetValue program_compiler_c++_option_warn \
            {format ""}
    $config SetValue program_compiler_c++_option_debug {format "-g"}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}

    # Use OOMMF supplied erf() error function
    $config SetValue program_compiler_c++_property_no_erf 1

    # Digital Marc C++ version 8.57 doesn't support partial
    # template instantiation.  Implement workarounds.
    $config SetValue program_compiler_c++_no_partial_instantiation 1

    # Optimization
    # Standard optimization:
    #set opts [list -6 -ff]
    # High optimizations:
    set opts [list -6 -o -ff]
    # NOTE: If you want good performance, be sure to edit ../options.tcl
    # or ../local/options.tcl to include the line
    #  Oc_Option Add * Platform cflags {-def NDEBUG}
    # so that the NDEBUG symbol is defined during compile.

    # Default maximum memory limit for dmc is apparently 30 MB (8.57,
    # Aug-2013).  This is too small for some of the files in Oxs.  The
    # -HPnn option allows the limit to be increased to nn MB.  (Some
    # online discussion indicates that as of DMC 8.50, the max value
    # for "nn" is 299.)
    lappend opts "-HP99"

    # Make user requested tweaks to compile line options
    set opts [LocalTweakOptFlags $config $opts]

    # NOTE: If you want good performance, be sure to edit ../options.tcl
    #  or ../local/options.tcl to include the line
    #     Oc_Option Add * Platform cflags {-def NDEBUG}
    #  so that the NDEBUG symbol is defined during compile.
    $config SetValue program_compiler_c++_option_opt "format \"$opts\""

    # Wide floating point type.  Defaults to double, but you can
    # change this to "long double" for extra precision and somewhat
    # reduced speed.
    if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }

    # Experimental: The OC_REAL8m type is intended to be at least 8 bytes
    # wide.  Generally OC_REAL8m is typedef'ed to double, but you can try
    # setting this to "long double" for extra precision (and extra
    # slowness).  If this is set to "long double", then so should
    # realwide in the preceding stanza.
    # $config SetValue program_compiler_c++_typedef_real8m "long double"

    # Digital Mars C++ 8.57 has a placement new[], but for some reason
    # sometimes it increments the requested offset by 4 bytes?!  Work
    # around this bug by using the single item placement new in an
    # explicit loop.  If you are using a verion of Digital Mars C++
    # with a working placement new[], then you may comment this out.
    # (BTW, there is an assert in oommf/app/oxs/base/oxsthread.h to
    # catch this error.)
    $config SetValue program_compiler_c++_no_placment_new_array 1

    # The program to run on this platform to create a single library file out
    # of many object files.
    # ... Digital Mars lib ...
    # NOTE: The -p32 switch sets the pagesize.  The default is
    # apparently -p16, which is apparently too small for some of the
    # OOMMF libraries.
    $config SetValue program_libmaker {lib -p32 -c}
    proc fibar { name } { return [list [file nativename $name]] }
    $config SetValue program_libmaker_option_obj {fibar}
    proc fifibar { name } { return [list [file nativename $name]] }
    $config SetValue program_libmaker_option_out {fifibar}

    # The program to run on this platform to link together object files and
    # library files to create an executable binary.
    # ... Digital Mars link ...
    # Seems link line should look like
    #     link OBJS, outfile.exe,,LIBS
    # Order is important.
    # Note: This linker uses Borland-style libraries
    $config SetValue program_linker {link}
    proc fubar { name } { return [list [file nativename $name]] }
    proc fubarmap { name } { return [list [file nativename "[file rootname $name].map"]] }
    $config SetValue program_linker_option_obj {fubar}
    proc fufubar { name } { return " , [fubar $name] , [fubarmap $name] , " }
    $config SetValue program_linker_option_out {fufubar}
    $config SetValue program_linker_option_lib {fubar}
    proc fufufubar { subsystem } {
        if {[string match CONSOLE $subsystem]} {
            return "-DELEXECUTABLE -EXETYPE:NT -SUBSYSTEM:CONSOLE -ENTRY:mainCRTStartup"
	    ;# Console application
        }
        return "-DELEXECUTABLE -EXETYPE:NT -SUBSYSTEM:WINDOWS:4.0 -ENTRY:WinMainCRTStartup"
	;# Windows GUI
    }
    $config SetValue program_linker_option_sub {fufufubar}
    set dummy [$config GetValue TCL_VC_LIB_SPEC]
    regsub {\.lib$} $dummy bc.lib dummy
    $config SetValue TCL_LIB_SPEC $dummy
    set dummy [$config GetValue TK_VC_LIB_SPEC]
    regsub {\.lib$} $dummy bc.lib dummy
    $config SetValue TK_LIB_SPEC $dummy
    unset dummy
    # Note: advapi32 is needed for GetUserName function in Nb package.
    $config SetValue TK_LIBS {advapi32.lib}
    $config SetValue TCL_LIBS {advapi32.lib}
    #$config SetValue TK_LIBS {}
    #$config SetValue TCL_LIBS {}
    $config SetValue program_linker_uses_-L-l {0}
    $config SetValue program_linker_uses_-I-L-l {0}
} elseif {[string match wcl386 $ccbasename]} {
    # ... for Open Watcom compiler
    proc fibar { prefix name } {
        return [list ${prefix}[file nativename $name]]
    }
    $config SetValue program_compiler_c++_option_out {fibar "-fo="}
    $config SetValue program_compiler_c++_option_src {fibar ""}
    $config SetValue program_compiler_c++_option_inc {fibar "-i="}
    $config SetValue program_compiler_c++_option_warn {format "-wx"}
    # Debugging option -d2 provides full debugging symbols, but
    # disables most optimizations.  Use -d1 for line-number info
    # but allowing full optimization.
    $config SetValue program_compiler_c++_option_debug {format "-d1"}
    $config SetValue program_compiler_c++_option_def {format "\"-d%s\""}

    # Use OOMMF supplied erf() error function
    $config SetValue program_compiler_c++_property_no_erf 1

    # Broken exception handling: uncaught_exception() undefined.
    $config SetValue program_compiler_c++_property_missing_uncaught_exception 1

    # Open Watcom or maybe STLport specific bug, involving
    # vector<> and class member function returns with reference
    # parameters.  This bug bites in the
    #    void Oxs_Ext::GetGroupedUIntListInitValue(const String&,
    #                                           vector<OC_UINT4m>&)
    # function in oommf/app/oxs/base/ext.cc.
    $config SetValue \
       program_compiler_c++_property_watcom_broken_vector_return 1

    # Optimization
    # No optimizations:
    #set opts [list -od]
    # Standard optimization:
    #set opts [list]
    # High optimizations:
    set opts [list -onatx -oh -oi+ -ei -zp8 -fp6]
    # NOTE: If you want good performance, be sure to edit ../options.tcl
    # or ../local/options.tcl to include the line
    #  Oc_Option Add * Platform cflags {-def NDEBUG}
    # so that the NDEBUG symbol is defined during compile.

    # Make user requested tweaks to compile line options
    set opts [LocalTweakOptFlags $config $opts]

    # NOTE: If you want good performance, be sure to edit ../options.tcl
    #  or ../local/options.tcl to include the line
    #    Oc_Option Add * Platform cflags {-def NDEBUG}
    #  so that the NDEBUG symbol is defined during compile.
    $config SetValue program_compiler_c++_option_opt "format \"$opts\""

    # Widest natively support floating point type
    if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }

    # The program to run on this platform to create a single library file out
    # of many object files.
    # Watcom librarian
    $config SetValue program_libmaker {wlib -n -b}
    $config SetValue program_libmaker_option_obj {fibar "+"}
    $config SetValue program_libmaker_option_out {fibar ""}

    # The program to run on this platform to link together object files and
    # library files to create an executable binary.
    # Watcom linker
    $config SetValue program_linker {wlink}
    proc fifibar { directive name } {
       return [list $directive [file nativename $name]]
    }
    $config SetValue program_linker_option_obj {fifibar "FILE"}
    $config SetValue program_linker_option_out {fifibar "NAME"}
    $config SetValue program_linker_option_lib {fifibar "LIBRARY"}
    # OPTION START=entrypoint
    proc fififibar { subsystem } {
        if {[string match CONSOLE $subsystem]} {
            return "SYSTEM nt OPTION START=mainCRTStartup" ;# Windows character
        }
        return "SYSTEM nt_win OPTION START=WinMainCRTStartup" ;# Windows GUI
    }
    $config SetValue program_linker_option_sub {fififibar}
    $config SetValue TCL_LIB_SPEC [$config GetValue TCL_VC_LIB_SPEC]
    $config SetValue TK_LIB_SPEC [$config GetValue TK_VC_LIB_SPEC]
    $config SetValue TK_LIBS {}
    $config SetValue TCL_LIBS {}
    $config SetValue program_linker_uses_-L-l {0}
    $config SetValue program_linker_uses_-I-L-l {0}
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
if {[catch {$config GetValue program_linker_extra_lib_scripts} extra_lib_scripts]} {
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
# If not specified otherwise, assume that double precision arithmetic
# may be performed with extra precision intermediate values:
if {[catch {$config GetValue \
               program_compiler_c++_property_fp_double_extra_precision}]} {
   $config SetValue program_compiler_c++_property_fp_double_extra_precision 1
}

########################################################################
unset config
