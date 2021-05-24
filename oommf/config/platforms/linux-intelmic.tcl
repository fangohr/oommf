# FILE: linux-intelmic.tcl
#
# Configuration feature definitions for the configuration
# 'linux-intelmic', for the Intel Phi architecture on Linux.'
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
# OOMMF software cannot classify your computing platform type.  See
# the Installation section of the OOMMF User Manual for instructions on
# how to add a new platform type to the collection of types recognized
# by OOMMF software.
#
# Say you add the new platform type 'foo' to describe your computing
# platform.  Then copy this file to ./foo.tcl , and edit it to
# describe your computing platform.  In its initial state, this file
# describes a rather generic Linux system, so there shouldn't be much
# editing required for any Unix-based platform.  Other platforms may
# be more difficult or impossible.  In any event, please e-mail the
# OOMMF developers for assistance setting up OOMMF for your particular
# circumstances.
########################################################################
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
# The Intel C++ compiler 'icpc'
# <URL:http://www.intel.com>
$config SetValue program_compiler_c++ {icpc -mmic -c}
#

########################################################################
# SUPPORT PROCEDURES
#
# Load routines to guess the CPU using /procs/cpuinfo, determine
# compiler version, and provide appropriate cpu-specific and compiler
# version-specific optimization flags.
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         cpuguess-linux-intelmic.tcl]

# Load routine to determine glibc version
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         glibc-support.tcl]

# Miscellaneous processing routines
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         misc-support.tcl]

########################################################################
# LOCAL CONFIGURATION
#
# The following options may be defined in the a "local" file, which
# has the same name as this file but is stored in the
# platforms/local/ subdirectory.
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
# $config SetValue thread_count 244  ;# Replace '244' with desired thread count.
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
## Use NUMA (non-uniform memory access) libraries?  This is only
## supported on Linux systems that have both NUMA runtime (numactl) and
## NUMA development (numactl-devel) packages installed.
# $config SetValue use_numa 1  ;# 1 to enable, 0 (default) to disable.
#
## Override default C++ compiler.  Note the "_override" suffix
## on the value name.
# $config SetValue program_compiler_c++_override {icpc -mmic -c}
#
## Processor architecture for compiling.  The default is "generic"
## which should produce an executable that runs on any cpu model for
## the given platform.  In principle, one may specify "host", in which
## case the build scripts will try to automatically detect the
## processor type on the current system, and select compiler options
## specific to that processor model (in which case he resulting binary
## will generally not run on other architectures) --- HOWEVER, this
## is not supported in the generic "unknown" platform script; support
## would need to be added by the author of a refined platform-specific
## script.
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
## Cross-compiling support:
##
## Part of the OOMMF build process involves building a test program
## (e.g., oommf/pkg/oc/varinfo.cc), and then running it to determine
## properties of the target platform.  This is complicated, however, when
## cross-compiling because the executable doesn't run on the host doing
## the build.  To work around this, the cross_compile_exec property
## specifies a command (typically ssh) that can be invoked in place of a
## direct exec call.  If using ssh, then you probably want a key agent
## running so that the exec call runs without prompting for passwords.
# $config SetValue cross_compile_exec {ssh target}
#
## If the target machine sees the OOMMF executables at some point in its
## filesystem, then cross_compile_path_to_oommf can be used to direct
## executable file access.
# $config SetValue cross_compile_path_to_oommf {/home/jones/oommf}
#
# System type (typically $tcl_platform(platform) on target machine)
# $config SetValue cross_compile_systemtype unix
#
## tclConfig.sh, tkConfig.sh and include files for the target are needed
## for the build.  These files must exist on the host; the following
## variables specify the paths *on the build host* to these files.
# $config SetValue cross_compile_host_tcl_config {/home/jones/tcl/lib/tclConfig.sh}
# $config SetValue cross_compile_host_tcl_include_dir {/home/jones/tcl/include}
# $config SetValue cross_compile_host_tk_config {/home/jones/tcl/lib/tkConfig.sh}
# $config SetValue cross_compile_host_tk_include_dir {/home/jones/tcl/include}
#
## If the Tcl install on the target does not place the Tcl library in
## one of the standard search places for the run-time linker, then specify
## the Tcl library directory (on the target) here.
# $config SetValue cross_compile_target_tcl_rpath {/home/smith/tcl/lib}
#
## If the tclConfig.sh doesn't have the proper target-side path for
## TCL_PREFIX, then set cross_compiler_target_tcl_library to the path
## *on the target system* to the directory holding the init.tcl file
## (typically ${TCL_PREFIX}/lib/tcl${TCL_VERSION}).
# $config SetValue cross_compile_target_tcl_library /home/smith/tcl/lib/tcl8.6

###################
# Default handling of local defaults:
#
if {[catch {$config GetValue oommf_threads}]} {
   # Value not set in platforms/local/ script,
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
   # Value not set in platforms/local/ script, so use
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

# Disable Tk by default (can be overridden in local/intelmic.tcl file).
if {[catch {$config GetValue use_tk}]} {
   $config SetValue use_tk 0
}

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
if {[string match icpc $ccbasename]} {
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

   # set opts [list -O0]
   # set opts [list -O%s]
   # set opts [list -fast -ansi_alias -wd1572]
   # set opts [list -O3 -ipo -no-prec-div -ansi_alias \
   #             -fp-model fast=2 -fp-speculation fast]
   set opts [GetIcpcGeneralOptFlags $icpc_version]

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
      } else {
         set cpuopts [GetIcpcCpuOptFlags $icpc_version $cpu_arch]
      }
   }
   unset cpu_arch
   # You can override the above results by directly setting or
   # unsetting the cpuopts variable.
   if {[info exists cpuopts] && [llength $cpuopts]>0} {
      set opts [concat $opts $cpuopts]
   }

   # Default warnings disable.
   # Warning 1572 is floating point comparisons being unreliable.
   # Warning 1624 is non-template friends of templated classes.
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
}

# The program to run on this platform to link together object files and
# library files to create an executable binary.
# The program to run on this platform to link together object files and
# library files to create an executable binary.
set linkername [$config GetValue program_compiler_c++]
if {[set compileonly [lsearch -exact $linkername "-c"]]>=0} {
    # Remove "-c" compile only flag from compiler string, so that the
    # string may be used to run the linker.
    set linkername [lreplace $linkername $compileonly $compileonly]
}
unset compileonly
if {[string match icpc $ccbasename]} {
   # ...for Intel's icpc as linker
   $config SetValue program_linker $ccbasename
   set linkcmdline $linkername
   # If -fast or other flags that enable interprocedural optimizations
   # (IPO) appear in the program_compiler_c++_option_opt value above,
   # then append those flags into program_linker too.
   if {[lsearch -exact $opts -fast]>=0} {
      lappend linkcmdline -fast
   }
   if {[set index_ipo [lsearch -glob $opts -ipo*]]>=0} {
      lappend linkcmdline [lindex $opts $index_ipo] -wd11021
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
   $config SetValue program_linker_rpathlink \
      {format "-Qoption,ld,-rpath-link=%s"}
   $config SetValue program_linker_uses_-L-l {1}
}

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
unset config
