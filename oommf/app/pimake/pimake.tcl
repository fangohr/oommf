#!/bin/sh
# FILE: pimake.tcl
#
# A platform-independent make -- written entirely in Tcl, so it runs on all
# platforms to which Tcl 7.5 or later has been ported.
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
#
# Usage:
# -----
# pimake.tcl ?options? ?target?
#
# Recognized options:
# ------------------
# -i	Ignore errors
# -k	Continue after errors; don't make targets dependent on error
# -d	Verbose dependency information
# --	End of options; next argument is the target
#
########################################################################

# Announce to those concerned that a build is in progress
global env
set env(OOMMF_BUILD_ENVIRONMENT_NEEDED) 1

########################################################################
# This script is meant to be portable to many platforms and many
# versions of Tcl/Tk.  Reporting of errors in startup scripts wasn't
# reliable on all platforms until Tcl version 8.0, so this script
# catches it's own errors and reports them to support older Tcl
# releases.
#
# The following [set script] command stores a script which is run below
# inside a [catch] that catches any errors arising in the main body of
# the script.
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
set _ [file join [file dirname [file dirname $myAbsoluteName]] lib oommf]
if {[file isdirectory [file join $_ app]]} {
    lappend oommf(libPath) $_
}
set _ [file dirname [file dirname [file dirname $myAbsoluteName]]]
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
if {[catch {package require Oc} msg]} {
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
	package require Oc
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
Oc_Main SetAppName pimake
Oc_Main SetVersion 2.0b0

# Disable the -console option; we don't enter an event loop
Oc_CommandLine Option console {} {}
Oc_CommandLine Option out {file} {
    global out; set out $file
} "Redirect output to file"
set out ""
Oc_CommandLine Option i {} {
    global options; set options(-i) 1
} "Ignore errors"
Oc_CommandLine Option k {} {
    global options; set options(-k) 1
} "Continue after errors"
Oc_CommandLine Option d {} {
    global options; set options(-d) 1
} "Verbose dependency information"
Oc_CommandLine Option root {} {
    cd [Oc_Main GetOOMMFRootDir]
} "Change directory to oommf root"
Oc_CommandLine Option [Oc_CommandLine Switch] {
	{{target optional} {} "Target to build (default=\"all\";\
		 \"help\" prints choices)"}} {
    upvar #0 target globalTarget; set globalTarget $target
} "End of options; next argument is target"
Oc_CommandLine Parse $argv

if {[string length $out]} {
    close stdout
    open $out w		;# Becomes new stdout

    # Redirect panic messages to $out as well
    proc toOut {m args} { puts $m }
    Oc_Log SetLogHandler toOut panic 
    Oc_Exec SetErrChannel stdout
}

# Define the MakeRule class and other supports
source [file join [file dirname [info script]] makerule.tcl]
source [file join [file dirname [info script]] csourcefile.tcl]
source [file join [file dirname [info script]] platform.tcl]
source [file join [file dirname [info script]] procs.tcl]

# Pass options to MakeRule class
MakeRule SetOptions [array get options]

# Evaluate the makerules.tcl file in the current directory
if {![MakeRule SourceRulesFile]} {
    return -code error "Can't find or read makerules file 'makerules.tcl'"
}

# Write command lines to stdout before executing them
Oc_Exec SetOutChannel stdout

if {[string length $target]} {
    set target [Oc_DirectPathname $target]
}
if {[catch {MakeRule Build $target} errmsg]} {
#    set index [string last "\n    ('MakeRule' proc 'Build'" $errorInfo]
#    set ei [string trimright [string range $errorInfo 0 $index]]
    flush stdout
    if {[string match CHILD* [lindex $errorCode 0]]} {
        return -code error "$errmsg\n[MakeRule NumErrors] Error(s)\ detected"
    } else {
        error "$errmsg\n[MakeRule NumErrors] Error(s)\ detected" \
		$errorInfo $errorCode
    }
} elseif {[MakeRule NumErrors]} {
    flush stdout
    Oc_Log Log "[MakeRule NumErrors] Error(s) detected" info
} else {
    flush stdout
    Oc_Log Log "Target '$target' up to date.\nBuilt [clock format [expr \
    {$errmsg - [MakeRule Offset]}]]\nCurrent time: [clock format \
    [clock seconds]]" info
}

########################################################################
########################################################################
#
# End of the non-boilerplate code.
#
########################################################################
########################################################################

# The following line is the end of the [set script] command enclosing the
# entire main body of this script.
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
