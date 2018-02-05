# FILE: winalp.tcl
#
# Configuration feature definitions for the configuration 'winalp'
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
} elseif {[info exists env(OOMMF_CPP)]} {
   # Note: "OOMMF_C++" is an invalid name in Unix shells.
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
# <URL:http://www.microsoft.com/visualc/>
$config SetValue program_compiler_c++ {cl /nologo /GX /GR /c}
# /GX enables exception handling.  On x86, /GR enables RTTI.
# For MVC++ 6.0 on the Alpha, /GR is not a documented option,
# but the compiler does not complain if it is included.

########################################################################
# LOCAL CONFIGURATION
#
# The following options may be defined in the platforms/local/winalp.tcl
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
###################
# Default handling of local defaults:
#
if {[catch {$config GetValue oommf_threads}]} {
   # Value not set in platforms/local/winalp.tcl,
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
   # Value not set in platforms/local/winalp.tcl, so try
   # to get value from environment:
   if {[info exists env(NUMBER_OF_PROCESSORS)]} {
      set processor_count $env(NUMBER_OF_PROCESSORS)
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
if {[string match cl [file tail [lindex \
        [$config GetValue program_compiler_c++] 0]]]} {
    # ...for Microsoft Visual C++
    $config SetValue program_compiler_c++_option_opt {}
    $config SetValue program_compiler_c++_option_out {format "\"/Fo%s\""}
    $config SetValue program_compiler_c++_option_src {format "\"/Tp%s\""}
    $config SetValue program_compiler_c++_option_inc {format "\"/I%s\""}
    $config SetValue program_compiler_c++_option_warn {format "/W2"}
    $config SetValue program_compiler_c++_option_debug {format "/MLd"}
    $config SetValue program_compiler_c++_option_def {format "\"/D%s\""}

    # NOTE: If you want good performance, be sure to edit ../options.tcl
    #  or ../local/options.tcl to include the line
    #    Oc_Option Add * Platform cflags {-def NDEBUG}
    #  so that the NDEBUG symbol is defined during compile.

    # Widest natively support floating point type
    if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }

    # Deletion of an empty STL map<> is broken.
    $config SetValue \
            program_compiler_c++_property_stl_map_broken_empty_delete 1
}

# The program to run on this platform to link together object files and
# library files to create an executable binary.
#
if {[string match cl [file tail [lindex \
        [$config GetValue program_compiler_c++] 0]]]} {
    # Microsoft Visual C++'s linker
    $config SetValue program_linker {link}
}

# Linker option processing...
if {[string match link [file tail [lindex \
        [$config GetValue program_linker] 0]]]} {
    # ...for Microsoft VC++ linker
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "\"/OUT:%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_option_sub {format "\"/SUBSYSTEM:%s\""}

    $config SetValue TCL_LIB_SPEC [$config GetValue TCL_VC_LIB_SPEC]
    $config SetValue TK_LIB_SPEC [$config GetValue TK_VC_LIB_SPEC]
    $config SetValue TK_LIBS {user32.lib}
    $config SetValue TCL_LIBS {user32.lib}
    $config SetValue program_linker_uses_-L-l {0}
    $config SetValue program_linker_uses_-I-L-l {0}
}

# The program to run on this platform to create a single library file out
# of many object files.
if {[string match cl [file tail [lindex \
        [$config GetValue program_compiler_c++] 0]]]} {
    # Microsoft Visual C++'s library maker
    $config SetValue program_libmaker {lib}
}

# Library making option processing...
if {[string match lib [file tail [lindex \
        [$config GetValue program_libmaker] 0]]]} {
    # ...for Microsoft VC++ lib
    $config SetValue program_libmaker_option_obj {format \"%s\"}
    $config SetValue program_libmaker_option_out {format "\"/OUT:%s\""}
}

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

########################################################################
unset config
