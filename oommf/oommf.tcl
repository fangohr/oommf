#!/bin/sh
# FILE: oommf.tcl
#
# This Tcl script is the master application of the OOMMF project.
# It must be evaluted by a Tcl interpreter such as tclsh or wish
# (or whatever other name they are known by on your platform).
#
# On Windows platforms, when Tcl/Tk was installed, an association was
# placed in the registry to recognize the file extension .tcl of this
# file and use the installed wish program to evaluate it.
#
# On Unix platforms, it may be necessary to edit the "exec tclsh" line
# below to replace "tclsh" with the name of a Tcl interpreter shell
# installed on your computer.  Make the edit in place without changing
# the lines just before or after, and without changing the number of
# lines in this file.  See the documentation for a full explanation.
#
#    v--Edit here if necessary \
exec tclsh "$0" ${1+"$@"}
########################################################################

########################################################################
# This script is meant to be portable to many platforms and many
# versions of Tcl/Tk.  Reporting of errors in startup scripts wasn't
# reliable on all platforms until Tcl version 8.0, so this script
# catches it's own errors and reports them to support older Tcl
# releases.
#
# The contents of the following "script" variable are run inside a
# [catch] command that catches any errors arising in the main body
# of the script.
#
set script {

if {[catch {package require Tcl 8.5-}]} {
	return -code error "OOMMF software requires Tcl 8.5 or\
		higher.\n\tYour version: Tcl [info patchlevel]\nPlease\
		upgrade."
}

# When this script is evaluated by a shell which doesn't include the
# Oc extension, we must find and load the Oc extension on our own.
if {[string match "" [package provide Oc]]} {

# Robust determination of the absolute, direct pathname of this script:
set cwd [pwd]
set myAbsoluteName [file join $cwd [info script]]
if {[catch {cd [file dirname $myAbsoluteName]} msg]} {
    # This is probably impossible.  Can we read a script if we can't
    # cd to the directory which contains it?
    return -code error "Can't determine direct pathname for\
	    $myAbsoluteName:\n\t$msg"
}
set myAbsoluteName [file join [pwd] [file tail $myAbsoluteName]]
cd $cwd

# Create search path for OOMMF runtime library
set oommf(libPath) {}
if {[info exists env(OOMMF_ROOT)] \
        && [string match absolute [file pathtype $env(OOMMF_ROOT)]] \
        && [file isdirectory [file join $env(OOMMF_ROOT) app]]} {
    lappend oommf(libPath) $env(OOMMF_ROOT)
}
set _ [file dirname $myAbsoluteName]
if {[file isdirectory [file join $_ app]]} {
    lappend oommf(libPath) $_
}
if {![llength $oommf(libPath)]} {
    return -code error "Unable to find your OOMMF installation.\n\nSet the\
            environment variable OOMMF_ROOT to its root directory."
}

# Construct search path for extensions
foreach lib $oommf(libPath) {
    set dir [file join $lib pkg]
    if {[file isdirectory $dir] && [file readable $dir] \
            && [string match absolute [file pathtype $dir]]} {
        if {[lsearch -exact $auto_path $dir] < 0} {
            lappend auto_path $dir
        }
    }
}

# Locate a version of the Oc (OOMMF core) extension and set it up for loading.
if {[catch {package require Oc 2} msg]} {
    # When can 'package require Oc' raise an error?
    #	1. There's no Oc extension installed.
    #	2. An error occurred in the initialization of the Oc extension.
    #	3. Some other extension has a broken pkgIndex.tcl file, and
    #	   we are running a pre-8.0 version of Tcl, which didn't
    #	   control such problems.
    #
    # In case 3 only, it makes sense to try again.  Why?  Because the
    # pkgIndex.tcl file of the Oc extension may have been sourced before
    # the error.  Then on the second try, the [package unknown] step of
    # [package require] will be bypassed, so the error won't occur again.
    #
    # Otherwise, we should just report the error.
    if {[llength [package versions Oc]]
	    && [string match "" [package provide Oc]]} {
	package require Oc 2
    } else {
	error $msg $errorInfo $errorCode
    }
}
# Cleanup globals created by boilerplate code
foreach varName {cwd myAbsoluteName msg _ lib dir} {
    catch {unset $varName}
}
unset varName

# End of code for finding/loading the Oc extension
}

########################################################################
########################################################################
#
# Here's where the non-boilerplate code starts
#
########################################################################
########################################################################

if {[Oc_Main HasTk]} {
    wm withdraw .
}
Oc_Main SetAppName OOMMF
Oc_Main SetVersion 2.0b0

Oc_CommandLine Switch +
# Disable the default command line options that don't make sense for
# the bootstrap program
Oc_CommandLine Option console {} {}
Oc_CommandLine Option cwd {} {}

Oc_CommandLine Option fg {} {global bg; set bg 0} \
	"Run app in foreground; block until it completes"
Oc_CommandLine Option bg {} {global bg; set bg 1} \
	"Run app in background; exit after starting it"
Oc_CommandLine Option command {} {global commandPrint; set commandPrint 1} \
	"Print app's command line to stdout and exit"
set commandPrint 0
Oc_CommandLine Option out {outFile} {global out; set out $outFile} \
	"Redirect app's stdout to outFile"
set out ""
Oc_CommandLine Option err {errFile} {global err; set err $errFile} \
	"Redirect app's stderr to errFile"
set err ""

Oc_CommandLine Option platform {} {
   global dump_platform ; set dump_platform 1
} "Print platform summary and exit"
set dump_platform 0

Oc_CommandLine Option v {} { global verbose ; set verbose 1 } \
   "Verbose output (with platform summary)"
set verbose 0

Oc_CommandLine Option [Oc_CommandLine Switch] {
	{{appSpec optional} {expr {![string length $appSpec]
		|| [string match {[a-zA-Z]*} [lindex $appSpec 0]]}}
		"Application to launch:\
		\"appName \[-exact\] \[version-requirement\]\""}
	{{arg list} {} "Arguments to pass to the application"}
	} {
    if {[string length $appSpec] == 0} {
	set appSpec mmLaunch
    }
    global argv; set argv [concat [list $appSpec] $arg]
} "End of options; next argument is appSpec"

Oc_CommandLine Parse $argv

if {$dump_platform} {
   # Run this after Oc_CommandLine Parse so we can check
   # for verbose request (Oc_CommandLine Option v).
   # A not uncommon source of problems is erroneously set environment
   # variables.  Grab these up front so we can report them if
   # Oc_Config fails to initialize.  (NB: Oc_Config initialization
   # fills missing values in the local copy of env, so to report the
   # original configuration we need to cache the existing names before
   # calling Oc_Config.)
   global env verbose
   set predefinedEnvVars [concat [array names env OOMMF*] \
                             [array names env TCL*] [array names env TK*]]
   if {[catch {Oc_Config RunPlatform} runplatform]} {
      puts stderr "Error initializing Oc_Config:"
      puts stderr "--------------------------------------"
      puts stderr $runplatform
      puts stderr "--------------------------------------\n"
      if {[llength $predefinedEnvVars]>0} {
         puts stderr "Please check following environment variables:\n"
         foreach elt $predefinedEnvVars {
            puts [format "%20s = %s" $elt $env($elt)]
         }
         puts stderr "\nIn particular, try unsetting some or all of these."
      }
      exit 1
   }
   set snapshot [$runplatform SnapshotDate]
   if {![string match {} $snapshot]} {
      set snapshot ", snapshot $snapshot"
   }
   puts "OOMMF release\
         [package provide Oc]$snapshot\n[$runplatform \
	 Summary]"

   if {$verbose} {
      set rootdir [Oc_Main GetOOMMFRootDir]

      # Dump active lines from local options.tcl
      set loptionsfile [file join $rootdir config local options.tcl]
      if {![file exists $loptionsfile]} {
         puts "\nNo local options.tcl file"
      } else {
         if {[catch {open $loptionsfile} chan]} {
            puts stderr "ERROR: Can't open local options file\
                         \"$loptionsfile\": $chan"
         } else {
            set lopts [split [read -nonewline $chan] "\n"]
            close $chan
            # Dump active lines from local options file
            set lopts [lsearch -inline -all -not -regexp $lopts "^\s*#"]
            set lopts [lsearch -inline -all -not -regexp $lopts "^\s*$"]
            if {[llength $lopts]==0} {
               puts "\nNo local options set"
            } else {
               puts "\n--- Local config options ---"
               foreach line $lopts { puts "   $line" }
            }
         }
      }

      # Dump active lines from local platform file
      set platform [$runplatform GetValue platform_name]
      set lplatformfile \
         [file join $rootdir config platforms local ${platform}.tcl]
      if {![file exists $lplatformfile]} {
         puts "\nNo local platform file"
      } else {
         if {[catch {open $lplatformfile} chan]} {
            puts stderr "ERROR: Can't open local platform file\
                         \"$lplatformfile\": $chan"
         } else {
            set lopts [split [read -nonewline $chan] "\n"]
            close $chan
            # Dump active lines from local platform file
            set lopts [lsearch -inline -all -not -regexp $lopts "^\s*#"]
            set lopts [lsearch -inline -all -not -regexp $lopts "^\s*$"]
            if {[llength $lopts]==0} {
               puts "\nNo local platform options set"
            } else {
               puts "\n--- Local platform options ---"
               foreach line $lopts { puts "   $line" }
            }
         }
      }

      # Dump compile options
      source [file join $rootdir app pimake platform.tcl]
      if {![catch {$runplatform GetValue \
                     program_compiler_c++_option_optlevel} copts]} {
         set copts [eval $copts]
      } elseif {![catch {$runplatform GetValue \
                     program_compiler_c++_option_opt} copts]} {
         set copts [eval $copts]
      } else {
         set copts "ERROR: $copts"
      }
      if {[catch {$runplatform GetValue \
                     program_compiler_c++_option_valuesafeopt} cvsopts]} {
         set cvsopts "ERROR: $cvsopts"
      } else {
         set cvsopts [eval $cvsopts]
      }
      puts "\n--- Compiler options ---"
      puts "     Standard options: $copts"
      puts "   Value-safe options: $cvsopts"

      puts "\nOOMMF root directory: $rootdir"
   }
   exit
}

if {[catch {eval Oc_Application CommandLine $argv} cmd]} {
    return -code error $cmd
}

if {![info exists bg]} {
    # Set background according to default for the app
    set bg [string match & [lindex $cmd end]]
} elseif {$bg} {
    if {![string match & [lindex $cmd end]]} {
	lappend cmd &
    }
} else {
    while {[string match & [lindex $cmd end]]} {
	set cmd [lreplace $cmd end end]
    }
}

# Stdout redirection.
if {[string length $out]} {
    if {$bg} {
	set cmd [lreplace $cmd end end > $out &]
    } else {
	lappend cmd > $out
    }
}

# Stderr redirection.
if {[string length $err]} {
    if {$bg} {
	set cmd [lreplace $cmd end end 2> $err &]
    } else {
	lappend cmd > $err
    }
}

if {$commandPrint} {
    puts stdout $cmd
    exit
}

if {$bg} {
    if {[catch {eval exec $cmd} msg]} {
        # Couldn't launch background app; report the problem
        return -code error $msg
    }
} else {
    if {[catch {eval Oc_Exec Foreground $cmd} msg]} {
	if {[string match "child process exited abnormally*" $msg]} {
	    # Expected error message - don't bother displaying
            if {[string match CHILDSTATUS [lindex $errorCode 0]] \
                   && [lindex $errorCode 2]>0 } {
               exit [lindex $errorCode 2]
            }
	    exit 1
	}
	return -code error $msg
    }
}


########################################################################
########################################################################
#
# End of the non-boilerplate code.
#
########################################################################
########################################################################

# The following line is the end of the [set script] command which
# encloses the entire main body of this script.
}

if {[catch {package require Tcl 8.5-}]} {
    set code [catch $script msg]
} else {
    # Really old releases of Tcl were really dumb in their failed
    # efforts to compile [catch], so we do a dumb thing to work around it.
    set catch catch
    set code [$catch $script msg o]
    if {$code == 1 || $code == 2} {
        set errorCode [dict get $o -errorcode]
    }
    unset o
}

#
########################################################################

########################################################################
# Check the result of the enclosing [catch] and construct any error
# message to be reported.
#
if {$code == 0} {
    # No error; we're done
    exit 0
} elseif {$code == 1} {
    # Unexpected error; Report whole stack trace plus errorCode
    set rei $errorInfo
    set rec $errorCode
    set logtype error
} elseif {$code != 2} {
    # Weirdness -- just pass it on
    return -code $code $msg
} else {
    # Expected error; just report the message
    set rei $msg
    set rec $errorCode
    set logtype panic
}
#
########################################################################

########################################################################
# Try various error reporting methods
#
# First, use Oc_Log if it is available.
set src ""
if {[string match OC [lindex $rec 0]]} {
    set src [lindex $rec 1]
}
set errorInfo $rei
set errorCode $rec
if {![catch {Oc_Log Log $msg $logtype $src}]} {
    # Oc_Log reported the error; now die
    if {[string match CHILDSTATUS [lindex $rec 0]] \
           && [lindex $rec 2]>0 } {
       exit [lindex $rec 2]
    }
    exit 1
}

# If we're on Windows using pre-8.0 Tk, we have to report the message
# ourselves.  Use [tk_dialog].
if {[array exists tcl_platform]
        && [string match windows $tcl_platform(platform)]
        && [string length [package provide Tk]]
        && [package vsatisfies [package provide Tk] 4]} {
    wm withdraw .
    option add *Dialog.msg.wrapLength 0
    tk_dialog .error "<[pid]> [info script] error:" $rei error 0 Die
    exit 1
}

# Fall back on Tcl/Tk's own reporting of startup script errors
error $msg "\n<[pid]> [info script] error:\n$rei" $rec
#
########################################################################
