# FILE: lintel.tcl
#
# Configuration feature definitions for the configuration 'lintel'
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
#
# The Portland Group 'pgCC'
# <URL:http://www.pgroup.com/>
#$config SetValue program_compiler_c++ {pgCC -c}
#
# Temporary kludge to get pgCC to play with pimake.  First, create
# a shell script with the contents:
#
#      #!/bin/sh
#      # Arguments: CFLAGS library objs
#      flags="$1"
#      shift
#      library="$1"
#      shift
#      objs="$*"
#      pgCC --prelink_objects $flags $objs
#      ar cr $library lintel/*.o
#
# Call this pgcclib.sh and place it in the PATH.  Then, edit this
# file to include
#
#         --one_instantiation_per_object
#
# in the "opts" variable of the pgCC compiler section below, and
# exclude
#
#         -Minline
#
# (all flavors).  Then cd into the oommf/pkg directory, and run
# pimake from there, to build all the libraries.  Then re-edit
# this file to remove --one_instantiation_per_object from opts,
# and re-include -Minline (if desired).  Then cd back to the
# oommf top level directory, and re-run pimake to complete
# the build process.
#
#
# The Intel C++ compiler 'icpc'
# <URL:http://www.intel.com>
#$config SetValue program_compiler_c++ {icpc -c}
#
# The Open64 C++ compiler 'openCC'
# <URL:http://www.open64.net/>
# $config SetValue program_compiler_c++ {openCC -c}
#

########################################################################
# SUPPORT PROCEDURES
#
# Load routines to guess the CPU using /procs/cpuinfo, determine
# compiler version, and provide appropriate cpu-specific and compiler
# version-specific optimization flags.
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         cpuguess-lintel.tcl]

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
# platforms/local/lintel.tcl file:
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
## Use SSE intrinsics?  If so, specify level here.  Set to 0 to not use
## SSE intrinsics.  Leave unset to get the default.
# $config SetValue sse_level 2  ;# Replace '2' with desired level
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
   # Value not set in platforms/local/lintel.tcl,
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
   # Value not set in platforms/local/lintel.tcl, so use
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

########################################################################
# ADVANCED CONFIGURATION

# Compiler option processing...
set ccbasename [file tail [lindex [$config GetValue program_compiler_c++] 0]]
if {[string match g++* $ccbasename]} {
    # ...for GNU g++ C++ compiler

   if {![info exists gcc_version]} {
      set gcc_version [GuessGccVersion \
                          [lindex [$config GetValue program_compiler_c++] 0]]
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
         if {[catch {$config GetValue sse_level} sse_level]} {
            # sse_level not set in local/lintel.tcl; use SSE
            # level returned by GuessCpu
            $config SetValue sse_level [lindex $cpu_arch 2]
         }
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

   # You may want to try appending to opts
   #    -parallel -par-threshold49 -par-schedule-runtime

   # Uncomment the following two lines to remove SSE enabling flags.
   # regsub -all -- {-mfpmath=[^ ]*} $opts {} opts
   # regsub -all -- {-msse[^ ]*} $opts {} opts

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
   $config SetValue program_compiler_c++_option_warn {format "-Wall \
        -W -Wpointer-arith -Wwrite-strings \
        -Woverloaded-virtual -Wsynth -Werror \
        -Wno-unused-function"}

   # Wide floating point type.
   # NOTE: On the Linux/x86+gcc platform, "long double" provides
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

} elseif {[string match pgCC $ccbasename]} {
    # ...for Portland Group C++ compiler

   if {![info exists pgcc_version]} {
      set pgcc_version [GuessPgccVersion \
           [lindex [$config GetValue program_compiler_c++] 0]]
   }
   $config SetValue program_compiler_c++_banner_cmd \
      [list GetPgccBannerVersion  \
          [lindex [$config GetValue program_compiler_c++] 0]]


    # Optimization options
    # set opts [list -O0]  ;# No optimization
    # set opts [list -O%s] ;# Minimal
    set opts [list -fast -Mlre=assoc \
	    -Minline=levels:10 \
            -Mvect=noaltcode,assoc,idiom,recog,notransform \
	    --instantiation_dir lintel]
    #set opts [list -fast -Minline=levels:10,lib:lintel \
    #    --instantiation_dir lintel -Mvect -Mcache_align]
    # Some suggested options: -fast, -Minline=levels:10,
    # -O3, -Mipa, -Mcache_align, -Mvect, -Mvect=sse, -Mconcur.
    #    WRT -Mvect, the transform suboption produces segfault upon
    # execution.  Use notransform to explicitly disable.
    #    If you use -Mconcur, be sure to include -Mconcur on the
    # link line as well, and set the environment variable NCPUS
    # to the number of CPU's to use at execution time.
    #    If you use -Mipa, include it also on the link
    # line.  Using IPA info requires two build passes.  After
    # building once, wipe away the object modules with
    # 'tclsh oommf.tcl pimake objclean', and then rebuild.
    # In theory, anyway.  I haven't been able to get this
    # switch to work with pgCC 5.0-2.
    #
    # The -fast option sets processor specific options.  You can
    # override these by directly setting the cpuopts variable, e.g.,
    #
    # set cpuopts [list -tp k8-32]
    #
    # See the compiler documentation for options to the -tp
    # flag, but possibilities include p5, p6, p7, k8-32,
    # k8-64, and px.  The last is generic x86.
    if {[info exists cpuopts] && [llength $cpuopts]>0} {
	set opts [concat $opts $cpuopts]
    }
    #
    # Template handling for libraries
    # lappend opts --one_instantiation_per_object

    # Make user requested tweaks to compile line options
    set opts [LocalTweakOptFlags $config $opts]

    $config SetValue program_compiler_c++_option_opt "format \"$opts\""

    # Use/don't use SSE intrinsics, and at what level.  The default,
    # "0" indicates no SSE.  Level "1" (indicating SSE level 1, i.e.,
    # the first SSE specification) is in effect the same as 0, because
    # OOMMF calculations overwhelmingly use double precision floating
    # point operations; these aren't available as SSE instructions
    # earlier than level 2.
    #    You can override the value by setting the $config sse_level
    # value in the local platform file (see LOCAL CONFIGURATION above).
    if {[catch {$config GetValue sse_level}]} {
       $config SetValue sse_level 2
    }

    # pgCC version 7.2-5 doesn't include the _mm_cvtsd_f64 intrinsic.
    # Use the oc_sse_cvtsd_f64 wrapper workaround by default; we
    # can change this if we learn of a later version of pgCC that
    # supports _mm_cvtsd_f64.
    if {[catch {$config GetValue program_compiler_c++_missing_cvtsd_f64}]} {
       $config SetValue program_compiler_c++_missing_cvtsd_f64 1
    }

    # pgCC version 7.2-5 has a broken implementation of the SSE2
    # intrinsic _mm_storel_pd.  Invoke a workaround
    if {[catch {$config GetValue program_compiler_c++_broken_storel_pd}]} {
       $config SetValue program_compiler_c++_broken_storel_pd 1
    }

    # NOTE: If you want good performance, be sure to edit ../options.tcl
    #  or ../local/options.tcl to include the line
    #    Oc_Option Add * Platform cflags {-def NDEBUG}
    #  so that the NDEBUG symbol is defined during compile.
    $config SetValue program_compiler_c++_option_out {format "-o \"%s\""}
    $config SetValue program_compiler_c++_option_src {format \"%s\"}
    $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}

    # Compiler warnings:
    $config SetValue program_compiler_c++_option_warn {format \
	    "-Minformlevel=warn"}
    $config SetValue program_compiler_c++_option_debug {format "-g"}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}

    # Wide floating point type.  Default is double.
    # $config SetValue program_compiler_c++_typedef_realwide "long double"

    # Directories to exclude from explicit include search path, i.e.,
    # the -I list.  Some of the gcc versions don't play well with
    # the Portland Group compilers, so keep them off the compile line.
    $config SetValue \
    	program_compiler_c++_system_include_path [list /usr/include]

    # Other compiler properties
    $config SetValue \
            program_compiler_c++_property_optimization_breaks_varargs 0
} elseif {[string match icpc $ccbasename]} {
   # ...for Intel's icpc C++ compiler

   if {![info exists icpc_version]} {
      set icpc_version [GuessIcpcVersion \
           [lindex [$config GetValue program_compiler_c++] 0]]
   }
   $config SetValue program_compiler_c++_banner_cmd \
      [list GetIcpcBannerVersion  \
          [lindex [$config GetValue program_compiler_c++] 0]]

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

   # set opts [list -O0]
   # set opts [list -O%s]
   # set opts [list -fast -ansi_alias -wd1572]
   # set opts [list -O3 -ipo -no-prec-div -ansi_alias \
   #             -fp-model fast=2 -fp-speculation fast]
   set opts [GetIcpcGeneralOptFlags $icpc_version]

   # CPU model architecture specific options.  To override, set value
   # program_compiler_c++_cpu_arch in
   # oommf/config/platform/local/lintel.tcl.
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
      set cpuopts [GetIcpcCpuOptFlags $icpc_version $cpu_arch]
   }
   unset cpu_arch
   # You can override the above results by directly setting or
   # unsetting the cpuopts variable.
   if {[info exists cpuopts] && [llength $cpuopts]>0} {
      set opts [concat $opts $cpuopts]
   }

   # Default warnings disable
   set nowarn [list -wd1572,1624]
   if {[info exists nowarn] && [llength $nowarn]>0} {
      set opts [concat $opts $nowarn]
   }
   catch {unset nowarn}

   # Make user requested tweaks to compile line options
   set opts [LocalTweakOptFlags $config $opts]

   $config SetValue program_compiler_c++_option_opt "format \"$opts\""

   $config SetValue program_compiler_c++_option_out {format "-o \"%s\""}
   $config SetValue program_compiler_c++_option_src {format \"%s\"}
   $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
   $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}
   $config SetValue program_compiler_c++_option_debug {format "-g"}

   $config SetValue program_compiler_c++_option_warn \
      { format "-Wall -Werror -wd1418,1419,279,810,981,383,1572" }
   # { format "-w0 -verbose \
      #   -msg_disable undpreid,novtbtritmp,boolexprconst,badmulchrcom" }

   # Wide floating point type.  Defaults to double, but you can
   # change this to "long double" for extra precision and somewhat
   # reduced speed.
   # $config SetValue program_compiler_c++_typedef_realwide "long double"

   # Experimental: The OC_REAL8m type is intended to be at least
   # 8 bytes wide.  Generally OC_REAL8m is typedef'ed to double,
   # but you can try setting this to "long double" for extra
   # precision (and extra slowness).  If this is set to "long double",
   # then so should realwide in the preceding stanza.
   # $config SetValue program_compiler_c++_typedef_real8m "long double"
} elseif {[string match openCC* $ccbasename]} {
   # Optimization options
   set opts -Ofast
   # Additional options:
   lappend opts -ffast-math
   lappend opts -fp-accuracy=aggressive ;# strict, strict-contract,
                                       ;## relaxed, or aggressive
   lappend opts -funsafe-math-optimizations
   lappend opts -mso ;# Optimize for multicore scalability

   # CPU model architecture specific options.  To override, set value
   # program_compiler_c++_cpu_arch in
   # oommf/config/platform/local/lintel.tcl.
   if {[catch {$config GetValue program_compiler_c++_cpu_arch} cpu_arch]} {
      set cpu_arch generic
   }
   set cpuopts {}
   if {![string match generic [string tolower $cpu_arch]]} {
      # Arch specific build.  If cpu_arch is "host", then use "auto".
      # Otherwise, assume user knows what he is doing and has inserted
      # an appropriate cpu_arch string, e.g., barcelona, core, opteron,
      # ...  You can replace -mtune with -march if you don't care if
      # the executable doesn't run on other x86 architectures.
      if {[string match host $cpu_arch]} {
         set cpuopts -mtune=auto
      } else {
         set cpuopts -mtune=$cpu_arch
      }
   }
   # You can override the above results by directly setting or
   # unsetting the cpuopts variable, e.g.,
   #
   #    set cpuopts [list -mtune=athlon]
   # or
   #    unset cpuopts
   #
   if {[info exists cpuopts] && [llength $cpuopts]>0} {
      set opts [concat $opts $cpuopts]
   }

   # Disable some default warnings in the opts switch, as opposed
   # to the warnings switch below, so that these warnings are always
   # muted, even if '-warn' option in file options.tcl is disabled.
   lappend nowarn -Wno-non-template-friend ;# OOMMF code conforms to
   ## the new standard.  Silence this "helpful" but inaccurate warning.
   lappend nowarn -Wno-uninitialized ;# This warning appears to be
   ## badly broken in Open64 v4.2.4 + IPA.
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
   $config SetValue program_compiler_c++_option_warn {format "-Wall \
        -W -Wpointer-arith -Wwrite-strings \
        -Woverloaded-virtual -Wsynth -Werror \
        -Wno-unused-function"}

   # Widest natively support floating point type.
   # The x86_64 architecture provides 8-byte floating point registers
   # in addition to the 8 12-byte floating point registers of the x86
   # architecture.  If you specify "long double" as the realwide type,
   # then in some parts of the program OOMMF will be restricted to
   # using the higher precision 12-byte registers.  (The 12-byte format
   # provides about 19 decimal digits of precision, as opposed to 16
   # decimal digits for the 8-byte format.)  However, in this case the
   # extra 8-byte x86_64 registers will be under utilized.  If you
   # specify "double" as the realwide type, then the 8-byte registers
   # can be used in all instances.
   #   Use of realwide is restricted in the code so that the speed
   # advantage of using "double" over "long double" should be pretty
   # minimal on this platform, but YMMV.
   # Default is "double" (because on some platforms "long double"
   # is a wide type simulated via software and is very slow).
    if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }

   # Experimental: The OC_REAL8m type is intended to be at least
   # 8 bytes wide.  Generally OC_REAL8m is typedef'ed to double,
   # but you can try setting this to "long double" for extra
   # precision (and extra slowness).  If this is set to "long double",
   # then so should realwide in the preceding stanza.
   #$config SetValue program_compiler_c++_typedef_real8m "long double"

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
   # $config SetValue program_compiler_c++_property_bad_wide2int 1

   # Other compiler properties
   $config SetValue \
      program_compiler_c++_property_optimization_breaks_varargs 0
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
if {[string match g++* $ccbasename]} {
    # ...for GNU g++ as linker
    $config SetValue program_linker $linkername
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_rpath {format "-Wl,-rpath=%s"}
    $config SetValue program_linker_uses_-L-l {1}
} elseif {[string match pgCC $ccbasename]} {
    # ...for Portland Group pgCC as linker
    $config SetValue program_linker $linkername
    if {[info exists opts]} {
	$config SetValue program_linker [concat $linkername $opts]
    }
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_uses_-L-l {1}
    $config SetValue program_linker_uses_-I-L-l {0}
} elseif {[string match icpc $ccbasename]} {
    # ...for Intel's icpc as linker
    set linkcmdline $linkername
    # If -fast or other flags that enable interprocedural optimizations
    # (IPO) appear in the program_compiler_c++_option_opt value above,
    # then append those flags into program_linker too.
    if {[lsearch -exact $opts -fast]>=0 || [lsearch -glob $opts -ipo*]>=0} {
       lappend linkcmdline -ipo -lsvml
       # The svml library is needed for the dvl/spectrum executable
       # and the oommf/app/oxs/ext/fft3v.cc object module when compiled
       # with some releases of the icpc v9 and v10 compiler.
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
 } elseif {[string match openCC* $ccbasename]} {
    # ...for openCC as linker
    set linkcmdline $linkername
    # If -Ofast or other flags that enable interprocedural optimizations
    # (IPO) appear in the program_compiler_c++_option_opt value above,
    # then append those flags into program_linker too.
    if {[lsearch -exact $opts -Ofast]>=0 || \
           [lsearch -glob [string tolower $opts] -ipa*]>=0} {
       lappend linkcmdline -ipa
    }
    $config SetValue program_linker $linkcmdline
    unset linkcmdline
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_rpath {format "-Wl,-rpath=%s"}
    $config SetValue program_linker_uses_-L-l {1}
}
unset linkername

# The program to run on this platform to create a single library file out
# of many object files.
if {[string match icpc $ccbasename]} {
    $config SetValue program_libmaker {xiar csr}
    $config SetValue program_libmaker_option_obj {format \"%s\"}
    $config SetValue program_libmaker_option_out {format \"%s\"}
} elseif {[string match pgCC $ccbasename]} {
    $config SetValue program_libmaker [subst {pgcclib.sh "$opts"}]
    $config SetValue program_libmaker_option_obj {format \"%s\"}
    $config SetValue program_libmaker_option_out {format \"%s\"}
} else {
    $config SetValue program_libmaker {ar cr}
    $config SetValue program_libmaker_option_obj {format \"%s\"}
    $config SetValue program_libmaker_option_out {format \"%s\"}
}
unset ccbasename

# The absolute, native filename of the null device
$config SetValue path_device_null {/dev/null}

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
