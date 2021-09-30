# FILE: bsdi.tcl
#
# Configuration feature definitions for the configuration 'bsdi',
# *BSD on an Intel x86 platform.
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
# The Portland Group 'pgCC'
# <URL:http://www.pgroup.com/>
#$config SetValue program_compiler_c++ {pgCC -c}


########################################################################
# SUPPORT PROCEDURES
#
# Routine to guess the CPU using /var/run/dmesg.boot
proc GuessCPU {} {
   set guess unknown
   catch {
      set cpuinfo {}
      # First try sysctl
      if {![catch {exec sysctl hw.model} cpustr] \
             && ![regexp {unknown} [string tolower $cpustr]]} {
         regexp {^hw.model:(.*)} [string tolower $cpustr] dum0 cpuinfo
      } else {
         # Fallback: look at boot info
         set fptr [open /var/run/dmesg.boot r]
         set bootstr [read $fptr]
         close $fptr
         # Use first processor info only
         set bootlst [split $bootstr "\n"]
         set cpurec [lsearch -regexp $bootlst {^(cpu|CPU).*:}]
         if {$cpurec>=0} {
            set cpustr [lindex $bootlst $cpurec]
         }
         regexp {^cpu.*:(.*)} [string tolower $cpustr] dum0 cpuinfo
      }
      set cpuinfo [string trim $cpuinfo]
      if {![string match {} $cpuinfo]} {
         # Try to match against "model name"
         switch -regexp -- $cpuinfo {
            {386}                    { set guess 386 }
            {486}                    { set guess 486 }
            {pentium(\(r\)|) (iv|4)}  { set guess pentium4 }
            {pentium(\(r\)|) (iii|3)} { set guess pentium3 }
            {pentium(\(r\)|) (ii|2)}  { set guess pentium2 }
            {pentium(\(r\)|) pro}     { set guess pentiumpro }
            {pentium}                 { set guess pentium }
            {overdrive podp5v83}      { set guess pentium }
            {opteron}                 { set guess opteron }
            {athlon(\(tm\)|) 64}      { set guess athlon64 }
            {athlon(\(tm\)|) fx}      { set guess athlon-fx }
            {athlon(\(tm\)|) mp}      { set guess athlon-mp }
            {athlon(\(tm\)|) xp}      { set guess athlon-xp }
            {athlon(\(tm\)|) (iv|4)}  { set guess athlon-4 }
            {athlon(\(tm\)|) (tbird)} { set guess athlon-tbird }
            {athlon}                  { set guess athlon }
            {^k8}                     { set guess k8 }
            {^k6-3}                   { set guess k6-3 }
            {^k6-2}                   { set guess k6-2 }
            {^k6}                     { set guess k6 }
            {^k5}                     { set guess k5 }
         }
      }
   }
   return $guess
}

# Routine to guess the gcc version
proc GuessGccVersion { gcc } {
    set guess {}
    catch {
	set fptr [open "|$gcc --version" r]
	set verstr [read $fptr]
	close $fptr
	regexp -- {[0-9][0-9.]+} $verstr guess
    }
    return [split $guess "."]
}

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
########################################################################
# BUILD CONFIGURATION

# Compiler option processing...
set ccbasename [file tail [lindex [$config GetValue program_compiler_c++] 0]]
if {[string match g++ $ccbasename]} {
    # ...for GNU g++ C++ compiler
    # Optimization options
    # set opts [list -O0 -ffloat-store]  ;# No optimization
    # set opts [list -O%s]               ;# Minimal
    set opts [list -O3 -fomit-frame-pointer -fstrict-aliasing]
    # You can also try adding -malign-double -ffast-math 
    #
    # The next block sets cpu-specific options, based on a guess (see
    # GuessCPU proc defined at the top of this file) of the current
    # CPU and gcc version.  If this guess is wrong, or you want to
    # build for a generic CPU, unset cpuopts below.  NOTE: This is a
    # first try, so it is most certainly broken!
    set gccversion [GuessGccVersion \
			[lindex [$config GetValue program_compiler_c++] 0]]
    if {[llength $gccversion]>1} {
       set verA [lindex $gccversion 0]
       set verB [lindex $gccversion 1]
       if {$verA==2 && $verB>=95} {
          switch -glob -- [GuessCPU] {
             k6*      { set cpuopts [list -march=k6] }
             pentium  { set cpuopts [list -march=pentium] }
             athlon*  -
             opteron  -
             pentium* { set cpuopts [list -march=pentiumpro] }
          }
       } elseif {$verA==3 && $verB<1} {
          switch -glob -- [GuessCPU] {
             pentium  { set cpuopts [list -march=pentium] }
             pentium* { set cpuopts [list -march=pentiumpro] }
             k6*      { set cpuopts [list -march=k6] }
             opteron  -
             athlon*  { set cpuopts [list -march=athlon] }
          }
       } elseif {$verA>=3} {
          switch -exact -- [GuessCPU] {
             pentium    { set cpuopts [list -march=pentium] }
             pentiumpro { set cpuopts [list -march=pentiumpro] }
             pentium2   { set cpuopts [list -march=pentium2] }
             pentium3   { set cpuopts [list -march=pentium3] }
             pentium4   { set cpuopts \
                             [list -march=pentium4 -msse2 -mfpmath=sse]
                ## Another valid -march option for later pentium 4's
                ## is -march=nocona (cpu family 15, model 4).  For
                ## this core one can try
                ##    [list -march=nocona -msse3 -mfpmath=sse]
             }
             k6         { set cpuopts [list -march=k6] }
             k6-2       { set cpuopts [list -march=k6-2] }
             k6-3       { set cpuopts [list -march=k6-3] }
             athlon     { set cpuopts [list -march=athlon] }
             athlon-tbird { set cpuopts [list -march=athlon-tbird] }
             athlon-4   { set cpuopts [list -march=athlon-4] }
             athlon-xp  { set cpuopts [list -march=athlon-xp] }
             athlon-mp  { set cpuopts [list -march=athlon-mp] }
             athlon-fx  -
             athlon64   -
             opteron    -
             k8         { set cpuopts \
                             [list -march=k8 -m32 -msse2 -mfpmath=sse] }
          }
          # Note: -march=k8 is okay for gcc 3.2.3.  Others?
       }
    }
    # You can override the GuessCPU results by directly setting or
    # unsetting the cpuopts variable, e.g.,
    #
    #    unset cpuopts
    #    set cpuopts [list -march=athlon]
    #
    if {[info exists cpuopts] && [llength $cpuopts]>0} {
	set opts [concat $opts $cpuopts]
    }

    # Default warnings disable
    set nowarn [list -Wno-non-template-friend]
    if {[lindex $gcc_version 0]>=6} {
       lappend nowarn {-Wno-misleading-indentation}
    }
    if {[info exists nowarn] && [llength $nowarn]>0} {
       set opts [concat $opts $nowarn]
    }
    catch {unset nowarn}

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
    # NOTE: On the x86+gcc platform, "long double" provides
    # somewhat better precision than "double", but at a cost of
    # increased memory usage and a decrease in speed.  (At this writing,
    # long double takes 12 bytes of storage as opposed to 8 for double,
    # but provides the x86 native floating point format having approx.
    # 19 decimal digits precision as opposed to 16 for double.)
    # Default is "double".
    if {[catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }

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
    # Optimization options
    # set opts [list -O0]  ;# No optimization
    # set opts [list -O%s] ;# Minimal
    set opts [list -fast -Minline=levels:10 -Mvect -Mcache_align]
    # set opts [list -fast -Minline=levels:10]
    #set opts [list -fast -Minline=levels:10,lib:bsdi/Extract.dir]
    # Some suggested options: -fast, -Minline=levels:10,
    # -O3, -Mipa, -Mcache_align, -Mvect, -Mvect=sse, -Mconcur.
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
    # Template handling
    # lappend opts --one_instantiation_per_object \
    #  --instantiation_dir bsdi/Template.dir

    $config SetValue program_compiler_c++_option_opt "format \"$opts\""

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

    # Wide floating point type.
    if {[catch {$config GetValue program_compiler_c++_typedef_realwide}]} {
       $config SetValue program_compiler_c++_typedef_realwide "double"
    }

    # Directories to exclude from explicit include search path, i.e.,
    # the -I list.  Some of the gcc versions don't play well with
    # the Portland Group compilers, so keep them off the compile line.
    $config SetValue \
    	program_compiler_c++_system_include_path [list /usr/include]

    # Other compiler properties
    $config SetValue \
            program_compiler_c++_property_optimization_breaks_varargs 0
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
} elseif {[string match pgCC $lbasename]} {
    # ...for Portland Group pgCC as linker
    if {[info exists opts]} {
	$config SetValue program_linker [concat [list $lbasename] $opts]
    }
    $config SetValue program_linker_option_obj {format \"%s\"}
    $config SetValue program_linker_option_out {format "-o \"%s\""}
    $config SetValue program_linker_option_lib {format \"%s\"}
    $config SetValue program_linker_uses_-L-l {1}
    $config SetValue program_linker_uses_-I-L-l {0}
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
# If not specified otherwise, assume that double precision arithmetic
# may be performed with extra precision intermediate values:
if {[catch {$config GetValue \
               program_compiler_c++_property_fp_double_extra_precision}]} {
   $config SetValue program_compiler_c++_property_fp_double_extra_precision 1
}

########################################################################
unset config
