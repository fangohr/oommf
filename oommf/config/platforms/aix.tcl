# FILE: aix.tcl
#
# Configuration feature definitions for the configuration 'aix'
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
# AIX SPECIFIC NOTE: We have a user report that on at least some
#    machines AIX restricts the amount of stack and data space to
#    1 segment of 256 MB.  One workaround is to set the environment
#    variable LDR_CNTRL, e.g.,
#
#       setenv LDR_CNTRL MAXDATA=0x<n>0000000@DSA
#
#    where the <n> is the number of segments to request.
#
########################################################################
# REQUIRED CONFIGURATION

########################################################################
# OPTIONAL CONFIGURATION
####################################
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
#
# VisualAge C++ compiler for AIX, xlC
# The -qrtti=all flag allows the generation of both typeinfo
#  and dynamic cast RTTI information.
# The -qlanglvl=ansifor:newexcp flag enables ANSI for-init loop
#  scoping rules and std::bad_alloc exception throwing by new.
# See xlC --help for details.
$config SetValue program_compiler_c++ \
	{xlC -qtempinc=./aix -qrtti=all -qlanglvl=ansifor:newexcp -c}

####################################
# Set the feature 'path_directory_temporary' to the name of an existing 
# directory on your computer in which OOMMF software should write 
# temporary files.  All OOMMF users must have write access to this 
# directory.
#
# $config SetValue path_directory_temporary {/tmp}

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
set ccbasename [file tail [lindex [$config GetValue program_compiler_c++] 0]]
if {[string match g++ $ccbasename]} {
    # ...for GNU g++ C++ compiler

    set opts [list -O%s]
    # Default warnings disable
    set nowarn [list -Wno-non-template-friend]
    if {[info exists nowarn] && [llength $nowarn]>0} {
       set opts [concat $opts $nowarn]
    }
    catch {unset nowarn}
    $config SetValue program_compiler_c++_option_opt "format \"$opts\""

    $config SetValue program_compiler_c++_option_out {format "-o \"%s\""}
    $config SetValue program_compiler_c++_option_src {format \"%s\"}
    $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
 
    # NOTE: If you want good performance, be sure to edit ../options.tcl
    #  or ../local/options.tcl to include the line
    #    Oc_Option Add * Platform cflags {-def NDEBUG}
    #  so that the NDEBUG symbol is defined during compile.

    $config SetValue program_compiler_c++_option_debug {format "-g"}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}

    # Compiler warnings:
    # Omitted: -Wshadow -Wredundant-decls
    # I would also like to use -Wcast-qual, but casting away const is
    # needed on some occasions to provide "conceptual const" functions in
    # place of "bitwise const"; cf. p76-78 of Meyer's book, "Effective C++."
    #
    # NOTE: -Wno-uninitialized is required after -Wall by gcc 2.8+ because
    # of an apparent bug
    $config SetValue program_compiler_c++_option_warn {format "-Wall \
        -W -Wpointer-arith -Wbad-function-cast -Wwrite-strings \
        -Wstrict-prototypes -Wmissing-declarations \
        -Wnested-externs -Winline -Woverloaded-virtual -Wsynth -Werror \
        -Wcast-align -Wno-uninitialized -Wno-unused-function"}

    # Widest natively support floating point type
    if {![catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }

    # Directories to exclude from explicit include search path, i.e.,
    # the -I list.  Some versions of gcc complain if "system" directories
    # appear in the -I list.
    $config SetValue \
    	program_compiler_c++_system_include_path [list /usr/include]

} elseif {[string match xlC $ccbasename]} {
    # ...for xlC C++ compiler

    set opts [list -O]
    ## xlC interprets -O as identical to -O2, and also accepts -O3,
    ## -O4 and -O5.  It does not recognize -O1, however.

    # Disable some default warnings in the opts switch, as opposed
    # to the warnings switch below, so that these warnings are always
    # muted, even if '-warn' option in file options.tcl is disabled.
    # The -qsuppress option takes a comma separated list of warning
    # message numbers:
    #    1540-0095 : non-templated friend of template
    set nowarn [list -qsuppress=1540-0095]

    set opts [concat $opts $nowarn]
    unset nowarn

    $config SetValue program_compiler_c++_option_opt "format \"$opts\""

    $config SetValue program_compiler_c++_option_out {format "-o \"%s\""}
    $config SetValue program_compiler_c++_option_src {format \"%s\"}
    $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
 
    # NOTE: If you want good performance, be sure to edit ../options.tcl
    #  or ../local/options.tcl to include the line
    #    Oc_Option Add * Platform cflags {-def NDEBUG}
    #  so that the NDEBUG symbol is defined during compile.

    $config SetValue program_compiler_c++_option_debug {format "-g"}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}

    # Compiler warnings:
    # $config SetValue program_compiler_c++_option_warn {format "-qflag=w"}

    # Widest natively support floating point type.  By default,
    # long double is 8 bytes, same as double.  But the '-qlongdouble'
    # compiler switch can be used to promote long double to 16 bytes,
    # which provides about 31 decimal digits of precision.  (This is
    # actually implemented as two 8-byte doubles; a surprising artifact
    # of this representation is that the smallest number eps such that
    # 1 + eps != 1 is about 5e-324.)  See the 'long double' entry in
    # docsearch for details.
    #  If you want to try this, uncomment the next line and include
    # the -qlongdouble switch in the "opts" list above.
    # $config SetValue program_compiler_c++_typedef_realwide "long double"

    # The STL map<> on this platform doesn't like 'const string' as
    # a key.
    $config SetValue \
            program_compiler_c++_property_stl_map_broken_const_key 1

}
unset ccbasename

# The program to run on this platform to link together object files and
# library files to create an executable binary.
#
# Use the selected compiler to control the linking.
$config SetValue program_linker [lindex \
        [$config GetValue program_compiler_c++] 0]

# Linker option processing...
set lbasename [file tail [lindex [$config GetValue program_linker] 0]]
if {[string match g++ $lbasename]} {
    # ...for GNU g++ as linker
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_uses_-L-l {1}
} elseif {[string match xlC $lbasename]} {
    # ...for xlC as linker
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_uses_-L-l {1}
}
unset lbasename

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
