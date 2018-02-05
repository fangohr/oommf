# darwin.tcl
#
# Configuration feature definitions for the configuration 'darwin'
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

# Enable experimental code to set application name in Mac OS X menubar
$config SetValue program_compiler_c++_set_macosx_appname 1


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
#$config SetValue program_compiler_c++ {g++ -c}
#
# The Clang C++ compiler 'clang++'
$config SetValue program_compiler_c++ {clang++ -c}

########################################################################
# SUPPORT PROCEDURES
#
# Load routines to guess the CPU using /procs/cpuinfo, determine
# compiler version, and provide appropriate cpu-specific and compiler
# version-specific optimization flags.
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         cpuguess-darwin.tcl]

# Miscellaneous processing routines
source [file join [file dirname [Oc_DirectPathname [info script]]]  \
         misc-support.tcl]

########################################################################
# LOCAL CONFIGURATION
#
# The following options may be defined in the
# platforms/local/darwin.tcl file:
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
set auto_max 8 ;# Arbitrarily limit maximum number of "auto" threads.
if {[catch {$config GetValue thread_count}]} {
   # Value not set in platforms/local/darwin.tcl, so use sysctl or
   # system_profiler to get count of physical cores.  If those fail,
   # then cut auto_max in half and fall back to getconf or sysctl to
   # get count of logical cores (which may include cores introduced by
   # hyperthreading).
   if {[catch {lindex [exec sysctl hw.physicalcpu] end} processor_count] &&
       ([catch {exec system_profiler SPHardwareDataType} result] ||
	![regexp {Total Number of Cores: *([0-9]+)} \
	      $result dummy processor_count])} {
      # Can't determine physical core count
      set auto_max [expr {(1+$auto_max)/2}]
      if {([catch {exec getconf _NPROCESSORS_ONLN} processor_count] ||
	   ![regexp {[0-9]+} $processor_count]) &&
	  ([catch {lindex [exec sysctl hw.availcpu] end} processor_count] ||
	   ![regexp {[0-9]+} $processor_count])} {
	 # Sorry, no clue on core count
	 set processor_count 0
      }
   }
   if {$processor_count>0} {
      if {$processor_count>$auto_max} {
         # Limit automatically set thread count to auto_max
         set processor_count $auto_max
      }
      $config SetValue thread_count $processor_count
   }
}
$config SetValue thread_count_auto_max $auto_max

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
if {[string match g++ [file tail [lindex \
        [$config GetValue program_compiler_c++] 0]]]} {
   # ...for GNU g++ C++ compiler

   if {![info exists gcc_is_clang]} {
      set gcc_is_clang 0
   }
   if {![info exists gcc_version]} {
      set gcc_version [GuessGccVersion \
                          [lindex [$config GetValue program_compiler_c++] 0]]
      if {[string match {} $gcc_version]} {
	 set gcc_version [GuessClangVersion \
                          [lindex [$config GetValue program_compiler_c++] 0]]
	 set gcc_is_clang 1  ;# gcc is actually clang
      }
   }
   if {!$gcc_is_clang} {
      if {[lindex $gcc_version 0]<4 ||
          ([lindex $gcc_version 0]==4 && [lindex $gcc_version 1]<5)} {
         puts stderr "WARNING: This version of OOMMF requires g++ 4.5\
                   or later (C++ 11 support)"
      }
      $config SetValue program_compiler_c++_banner_cmd \
	 [list GetGccBannerVersion  \
	     [lindex [$config GetValue program_compiler_c++] 0]]
   } else {
      $config SetValue program_compiler_c++_banner_cmd \
	 [list GetClangBannerVersion  \
	     [lindex [$config GetValue program_compiler_c++] 0]]
   }

   # Optimization options
   # set opts [list -O0 -fno-inline -ffloat-store]  ;# No optimization
   # set opts [list -O%s]                      ;# Minimal optimization
   if {!$gcc_is_clang} {
      set opts [GetGccGeneralOptFlags $gcc_version]
   } else {
      set opts [GetClangGeneralOptFlags $gcc_version]
   }
   # Aggressive optimization flags, some of which are specific to
   # particular gcc versions, but are all processor agnostic.  CPU
   # specific opts are introduced below.

   # CPU model architecture specific options.  To override, set value
   # program_compiler_c++_cpu_arch in
   # oommf/config/platform/local/darwin.tcl.  See note about SSE
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
      #   Note: cpuopts setting is complicated a bit by the
      # Apple-specific "-fast" flag; this flag specifies a variety of
      # general optimization flags, but is also arch specific on the
      # PowerPC arch, but not on Intel targets.
      if {[string match host $cpu_arch]} {
         set cpu_arch [GuessCpu]
         if {[catch {$config GetValue sse_level}]} {
            # In principle, the result from Find_SSE_Level may be more
            # accurate than what comes from GuessCpu.
            if {![catch {Find_SSE_Level} sse]} {
               $config SetValue sse_level $sse
            }
         }
         if {[catch {$config GetValue fma_type}]} {
            if {![catch {Find_FMA_Type} fma]} {
               $config SetValue fma_type $fma
            }
         }
      }
      if {!$gcc_is_clang} {
	 set cpuopts [GetGccCpuOptFlags $gcc_version $cpu_arch]
      } else {
	 set cpuopts [GetClangCpuOptFlags $gcc_version $cpu_arch]
      }
   }
   unset cpu_arch

   if {[catch {$config GetValue sse_level}] \
          && [string compare x86_64 $tcl_platform(machine)]==0} {
      # SSE level 2 guaranteed on x86_64
      $config SetValue sse_level 2
   }

   # You can override the above results by directly setting or
   # unsetting the cpuopts variable, e.g.,
   #
   #    set cpuopts [list -march=core2]
   # or
   #    unset cpuopts
   #
   if {[info exists cpuopts] && [llength $cpuopts]>0} {
      set opts [concat $opts $cpuopts]
   }

   # Uncomment the following to remove loop array prefetch flag
   # regsub -all -- \
   #   {^-fprefetch-loop-arrays\s+|\s+-fprefetch-loop-arrays(?=\s|$)} $opts {} opts

   # 32 or 64 bit?
   if {[info exists tcl_platform(pointerSize)]} {
      switch -exact $tcl_platform(pointerSize) {
         4 { lappend opts -m32 }
         8 { lappend opts -m64 }
      }
   } else {
      switch -exact $tcl_platform(wordSize) {
         4 { lappend opts -m32 }
         8 { lappend opts -m64 }
      }
   }

   # Default warnings disable
   set nowarn {}
   if {!$gcc_is_clang} {
       # clang doesn't support the -Wno-non-template-friend option.
       lappend nowarn -Wno-non-template-friend
   }
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

   # Widest natively support floating point type
   if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
      $config SetValue program_compiler_c++_typedef_realwide "double"
   }

   # Mac OS X 10.9 uses libc++ rather than libstdc++ by default, and
   # its libc++ doesn't like const keys in maps.  The workaround
   # shouldn't hurt, so apply it in all cases:
   $config SetValue program_compiler_c++_property_stl_map_broken_const_key 1

   # Directories to exclude from explicit include search path, i.e.,
   # the -I list.  Some versions of gcc complain if "system" directories
   # appear in the -I list.
   $config SetValue \
      program_compiler_c++_system_include_path [list /usr/include]

} elseif {[string match clang++ [file tail [lindex \
        [$config GetValue program_compiler_c++] 0]]]} {
   # ...for Clang C++ compiler

   if {![info exists clang_version]} {
      set clang_version [GuessClangVersion \
                          [lindex [$config GetValue program_compiler_c++] 0]]
   }
   $config SetValue program_compiler_c++_banner_cmd \
      [list GetClangBannerVersion  \
          [lindex [$config GetValue program_compiler_c++] 0]]

   # Optimization options
   # set opts [list -O0]  ;# No optimization
   # set opts [list -O%s]                      ;# Minimal optimization
   set opts [GetClangGeneralOptFlags $clang_version]
   # Aggressive optimization flags, some of which are specific to
   # particular clang versions, but are all processor agnostic.  CPU
   # specific opts are introduced below.

   # CPU model architecture specific options.  To override, set value
   # program_compiler_c++_cpu_arch in
   # oommf/config/platform/local/darwin.tcl.  See note about SSE
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
         if {[catch {$config GetValue sse_level}]} {
            # In principle, the result from Find_SSE_Level may be more
            # accurate than what comes from GuessCpu.
            if {![catch {Find_SSE_Level} sse]} {
               $config SetValue sse_level $sse
            }
         }
         if {[catch {$config GetValue fma_type}]} {
            if {![catch {Find_FMA_Type} fma]} {
               $config SetValue fma_type $fma
            }
         }
      }
      set cpuopts [GetClangCpuOptFlags $clang_version $cpu_arch]
   }
   unset cpu_arch

   if {[catch {$config GetValue sse_level}] \
          && [string compare x86_64 $tcl_platform(machine)]==0} {
      # SSE level 2 guaranteed on x86_64
      $config SetValue sse_level 2
   }


   # You can override the above results by directly setting or
   # unsetting the cpuopts variable, e.g.,
   #
   #    set cpuopts [list -march=core2]
   # or
   #    unset cpuopts
   #
   if {[info exists cpuopts] && [llength $cpuopts]>0} {
      set opts [concat $opts $cpuopts]
   }

   # 32 or 64 bit?
   if {[info exists tcl_platform(pointerSize)]} {
      switch -exact $tcl_platform(pointerSize) {
         4 { lappend opts -m32 }
         8 { lappend opts -m64 }
      }
   } else {
      switch -exact $tcl_platform(wordSize) {
         4 { lappend opts -m32 }
         8 { lappend opts -m64 }
      }
   }

   # Make user requested tweaks to compile line options
   set opts [LocalTweakOptFlags $config $opts]

   $config SetValue program_compiler_c++_option_opt "format \"$opts\""

   $config SetValue program_compiler_c++_option_out {format "-o \"%s\""}
   $config SetValue program_compiler_c++_option_src {format \"%s\"}
   $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
   $config SetValue program_compiler_c++_option_debug {format "-g"}
   $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}

   # Widest natively support floating point type
    if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }

   # Mac OS X 10.9 uses libc++ rather than libstdc++ by default, and
   # its libc++ doesn't like const keys in maps.  The workaround
   # shouldn't hurt, so apply it in all cases:
   $config SetValue program_compiler_c++_property_stl_map_broken_const_key 1
}

# The program to run on this platform to link together object files and
# library files to create an executable binary.
# (Use the selected compiler to control the linking.)
#
# Linker and linker option processing...
if {[string match g++ [file tail [lindex \
        [$config GetValue program_compiler_c++] 0]]]} {
   # ...for GNU g++ as linker
   set linkcmdline [lindex \
                       [$config GetValue program_compiler_c++] 0]
   # 32 or 64 bit?
   switch -exact $tcl_platform(wordSize) {
      4 { lappend linkcmdline -m32 }
      8 { lappend linkcmdline -m64 }
   }
   # Extra flags
   if {![catch {$config GetValue program_linker_add_flags} extraflags]} {
       foreach elt $extraflags {
	   if {[lsearch -exact $linkcmdline $elt]<0} {
	       lappend linkcmdline $elt
	   }
       }
   }
   $config SetValue program_linker $linkcmdline
   unset linkcmdline
   $config SetValue program_linker_option_obj {format \"%s\"}
   $config SetValue program_linker_option_out {format "-o \"%s\""}
   $config SetValue program_linker_option_lib {format \"%s\"}
   $config SetValue program_linker_uses_-L-l {1}
} elseif {[string match clang++ [file tail [lindex \
        [$config GetValue program_compiler_c++] 0]]]} {
   # ...for Clang clang++ as linker
   set linkcmdline [lindex \
                       [$config GetValue program_compiler_c++] 0]
   # 32 or 64 bit?
   switch -exact $tcl_platform(wordSize) {
      4 { lappend linkcmdline -m32 }
      8 { lappend linkcmdline -m64 }
   }
   # Extra flags
   if {![catch {$config GetValue program_linker_add_flags} extraflags]} {
       foreach elt $extraflags {
	   if {[lsearch -exact $linkcmdline $elt]<0} {
	       lappend linkcmdline $elt
	   }
       }
   }
   $config SetValue program_linker $linkcmdline
   unset linkcmdline
   $config SetValue program_linker_option_obj {format \"%s\"}
   $config SetValue program_linker_option_out {format "-o \"%s\""}
   $config SetValue program_linker_option_lib {format \"%s\"}
   $config SetValue program_linker_uses_-L-l {1}
}


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

unset config
