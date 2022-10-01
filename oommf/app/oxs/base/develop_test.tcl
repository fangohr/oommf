# FILE: develop_test.tcl
#
# Support script for development testing of C++ code.  To use this code
# the DEVELOP_TEST macro in director.cc needs to be defined.  Run like
#
#   ../linux-x86_64/oxs develop_test.tcl meshvalue 10000 100
#

# Support libraries
package require Oc 2
package require Oxs 2
package require Net 2

# Make background errors fatal.
rename bgerror orig_boxsi_bgerror
proc bgerror { msg } {
   Oc_Log Log "FATAL BACKGROUND ERROR: $msg" error
   orig_boxsi_bgerror $msg
   puts stderr "\nFATAL BACKGROUND ERROR: $msg"
   catch {exit 13}
}

Oc_IgnoreTermLoss  ;# Try to keep going, even if controlling terminal
## goes down.

# Application description boilerplate
Oc_Main SetAppName develop_test.tcl
Oc_Main SetVersion 2.0b0
regexp \\\044Date:(.*)\\\044 {$Date: 2016/01/31 02:14:37 $} _ date
Oc_Main SetDate [string trim $date]
Oc_Main SetAuthor [Oc_Person Lookup mjd]
Oc_Main SetHelpURL [Oc_Url FromFilename [file join [file dirname \
        [file dirname [file dirname [Oc_DirectPathname [info \
        script]]]]] doc userguide userguide\
        OOMMF_eXtensible_Solver_Int.html]]
Oc_Main SetDataRole producer

# Command line options
Oc_CommandLine ActivateOptionSet Net

# Multi-thread support
if {[Oc_HaveThreads]} {
   set threadcount [Oc_GetMaxThreadCount]
   Oc_CommandLine Option threads {
      {count {expr {[regexp {^[0-9]+$} $count] && $count>0}}}
   } {
      global threadcount;  set threadcount $count
   } [subst {Number of concurrent threads (default is $threadcount)}]
} else {
   set threadcount 1  ;# Safety
}

# NUMA (non-uniform memory access) support
set numanodes none
if {[Oc_NumaAvailable]} {
   set numanodes [Oc_GetDefaultNumaNodes]
   Oc_CommandLine Option numanodes {
      {nodes {regexp {^([0-9 ,]*|auto|none)$} $nodes}}
   } {
      global numanodes
      set numanodes $nodes
   } [subst {NUMA memory and run nodes (or "auto" or "none")\
                (default is "$numanodes")}]
}

Oc_CommandLine Option [Oc_CommandLine Switch] {
	{subroutine {} {subroutine to test}}
	{{argv list} {} {Optional argument list}}
    } {
       global test_routine test_argv
       set test_routine $subroutine
       set test_argv $argv
} {End of options; next argument is test subroutine}

##########################################################################
# Parse commandline and initialize threading
##########################################################################
Oc_CommandLine Parse $argv

if {[Oc_HaveThreads]} {
   Oc_SetMaxThreadCount $threadcount
   set aboutinfo "Number of threads: $threadcount"
} else {
   set aboutinfo "Single threaded build"
}

if {[Oc_NumaAvailable]} {
   if {[string match auto $numanodes]} {
      set nodes {}
   } elseif {[string match none $numanodes]} {
      set nodes none
   } else {
      set nodes [split $numanodes " ,"]
   }
   if {![string match "none" $nodes]} {
      Oc_NumaInit $threadcount $nodes
   }
   append aboutinfo "\nNUMA: $numanodes"
}
Oc_Main SetExtraInfo $aboutinfo
set update_extra_info $aboutinfo
unset aboutinfo

##########################################################################
# Handle Tk
##########################################################################
if {[Oc_Main HasTk]} {
    wm withdraw .        ;# "." is just an empty window.
    package require Ow
    Ow_SetIcon .

    # Evaluate $gui?  (in a slave interp?) so that when this app
    # is run with Tk it presents locally the same interface it
    # otherwise exports to be displayed remotely?

}

puts "Test routine: $test_routine"

if {[catch {eval Oxs_DirectorDevelopTest {$test_routine} $test_argv} result]} {
   puts $result
} else {
   puts "Test  result: $result"
}
