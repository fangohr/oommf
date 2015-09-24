# FILE: hpux.tcl
#
# Configuration feature definitions for the configuration 'hpux'
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
# HP-UX's cfront-based C++ compiler: CC
# NB: This compiler does not support the C++ Standard Template library,
#     and in particular will not build the files in the OOMMF
#     subdirectory oommf/app/oxs (the 3D micromagnetic solver).
# <URL:http://www.hp.com/esy/lang/cpp/cxx.html>
# $config SetValue program_compiler_c++ {CC -c}
#
# HP-UX's ISO/IEC 14882 C++ compiler, aC++
# <URL:http://www.hp.com/esy/lang/cpp/>
# The +DAarchitecture switch specifies the particular PA-RISC
#  architecture to generate code for.  This should be either
#  absent, or else a string like +DA2.0W or +DAportable.  See
#  the aCC documentation for details, but in particular, the
#  chosen value must be compatible with the Tcl/Tk libraries
#  that you are linking against.
# The -AA switch activates some more recent ANSI C++ features.
# The '+W495' switch disables some linkage directive warnings.
#$config SetValue program_compiler_c++ "aCC +DA2.0W -AA +W495 -c"
# Try this for HP-UX ia64 systems:
$config SetValue program_compiler_c++ "aCC -AA +W495 -c"
#
# The GNU C++ compiler 'g++'
# <URL:http://www.gnu.org/software/gcc/gcc.html>
# <URL:http://egcs.cygnus.com/>
# $config SetValue program_compiler_c++ {g++ -c}
#

########################################################################
# OPTIONAL CONFIGURATION

# Set the feature 'path_directory_temporary' to the name of an existing 
# directory on your computer in which OOMMF software should write 
# temporary files.  All OOMMF users must have write access to this 
# directory.
#
# $config SetValue path_directory_temporary {/tmp}

########################################################################
# ADVANCED CONFIGURATION

# Compiler option processing...
set ccbasename [file tail [lindex [$config GetValue program_compiler_c++] 0]]
if {[string match CC $ccbasename]} {
    # ...for HP-UX's compiler CC.
    # NOTE: Debugging (-g) is incompatible with optimization.
    $config SetValue program_compiler_c++_option_opt {format "\"+O%s\""}
    $config SetValue program_compiler_c++_option_out \
            {format "-ptr hpux/tr -o \"%s\""}
    $config SetValue program_compiler_c++_option_src {format \"%s\"}
    $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
    $config SetValue program_compiler_c++_option_warn {format "+w1"}
    $config SetValue program_compiler_c++_option_debug {format "-g"}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}
    $config SetValue program_compiler_c++_has_signed_char 0
} elseif {[string match aCC $ccbasename]} {
    # ...for HP-UX's compiler aCC.
    # NOTE: Debugging (-g) is incompatible with optimization.
    $config SetValue program_compiler_c++_option_opt {format "\"+O%s\""}
    $config SetValue program_compiler_c++_option_out \
            {format "-o \"%s\""}
    $config SetValue program_compiler_c++_option_src {format \"%s\"}
    $config SetValue program_compiler_c++_option_inc {format "\"-I%s\""}
    $config SetValue program_compiler_c++_option_warn {format "+w"}
    $config SetValue program_compiler_c++_option_debug {format "-g"}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}

    # Some older versions of the HP aCC compiler don't support the
    # 'using namespace std' directive.
    # Uncomment the following if needed.
    # $config SetValue \
    #     program_compiler_c++_property_no_std_namespace 1

    # Some older versions of the HP aCC compiler don't have <iostream>
    # and <iomanip> header files, but rather <iostream.h> and <iomanip.h>.
    # Uncomment the following if needed.
    #  $config SetValue program_compiler_c++_property_oldstyle_headers \
    #       [list iostream iomanip]

} elseif {[string match g++ $ccbasename]} {
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
    $config SetValue program_compiler_c++_option_debug {format "-g"}
    $config SetValue program_compiler_c++_option_def {format "\"-D%s\""}
 
    # Compiler warnings:
    # Omitted: -Wredundant-decls -W -shadow
    # I would also like to use -Wcast-qual, but casting away const is
    # needed on some occasions to provide "conceptual const" functions in
    # place of "bitwise const"; cf. p76-78 of Meyer's book, "Effective C++."
    #
    # NOTE: -Wno-uninitialized is required after -Wall by gcc 2.8+ because
    # of an apparent bug
    $config SetValue program_compiler_c++_option_warn {format "-Wall \
	-Wpointer-arith -Wbad-function-cast -Wwrite-strings \
	-Wstrict-prototypes -Wmissing-declarations \
	-Wnested-externs -Winline -Woverloaded-virtual -Wsynth -Werror \
	-Wcast-align -Wno-unused-function"}

    # Widest natively support floating point type
    $config SetValue program_compiler_c++_typedef_realwide "double"

    # Directories to exclude from explicit include search path, i.e.,
    # the -I list.  Some versions of gcc complain if "system" directories
    # appear in the -I list.
    $config SetValue \
    	program_compiler_c++_system_include_path [list /usr/include]
}

# The program to run on this platform to link together object files and
# library files to create an executable binary.
#
# Use the selected compiler to control the linking.
if {[string match CC [file tail [lindex \
        [$config GetValue program_compiler_c++] 0]]]} {
    $config SetValue program_linker [concat [lreplace \
            [$config GetValue program_compiler_c++] end end] \
            [list -ptr hpux/tr]]
} else {
    # aCC compiler.  Use the aCC compile command minus the "-c" switch.
    set linkcmd [$config GetValue program_compiler_c++]
    set cindex [lsearch $linkcmd "-c"]
    if {$cindex>=0} {
	set linkcmd [lreplace $linkcmd $cindex $cindex]
    }
    $config SetValue program_linker $linkcmd
}

# Linker option processing...
set lbasename [file tail [lindex [$config GetValue program_linker] 0]]
if {[string match CC $lbasename]} {
    # ...for HP-UX's CC as linker
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_uses_-L-l {0}
    $config SetValue program_linker_uses_-I-L-l {1}
} elseif {[string match aCC $lbasename]} {
    # ...for HP-UX's aCC as linker
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_uses_-L-l {1}
} elseif {[string match g++ $lbasename]} {
    # ...for GNU g++ as linker
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_uses_-L-l {1}
}
unset lbasename

# The program to run on this platform to create a single library file out
# of many object files.
$config SetValue program_libmaker {ar cr}
# Option processing for ar
$config SetValue program_libmaker_option_obj {format \"%s\"}
$config SetValue program_libmaker_option_out {format \"%s\"}

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
unset config
