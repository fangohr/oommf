#!/bin/sh
# FILE: loadtest.tcl
#
# This script runs one or more boxsi jobs in parallel and reports the
# total run time.  The first parameter is the number of jobs to run in
# parallel.  The remaining parameters are MIF 2.x files to run
#
#    Bootstrap trick \
exec tclsh "$0" ${1+"$@"}

# Set these variables appropriately
set OommfRootDir ..
set TclCmd tclsh
#set TclCmd tclsh8.4

set OommfCmd [file join $OommfRootDir oommf.tcl]
set JobTemplate [list $TclCmd $OommfCmd boxsi %s -parameters {id %s}]

proc Usage {} {
    puts stderr {Usage: loadtest <parallel count> job1.mif [job2.mif ...]}
    exit 1
}

if {[llength $argv]<2 \
	|| [string match [lindex $argv 0] "-h"] \
	|| [string match [lindex $argv 0] "--help"] } {
    Usage
}

set parallel_count [lindex $argv 0]
set joblist [lrange $argv 1 end]

if {[package vsatisfies [package provide Tcl] 8.3]} {
    set time_milliseconds 1
} else {
    set time_milliseconds 0
}

proc ReadTest { id chan } {
   global start_time time_milliseconds
   if {[eof $chan]} {
      if {[catch {close $chan} errcode]} {
         puts [format "ERROR %2d: $errcode" $id]
      } else {
         if {$time_milliseconds} {
            set timestop [clock clicks -milliseconds]
            set timediff [expr {($timestop - $start_time($id))*0.001}]
            set timediff [format "%.3f" $timediff]
         } else {
            set timestop [clock seconds]
            set timediff [expr {$timestop - $start_time($id)}]
            set timediff [format "%.0f" $timediff]
         }
         puts [format "%3d END; job elapsed time %s seconds" $id $timediff]
      }
      global donecount
      incr donecount
   } else {
      while {[gets $chan text]>=0} {
         puts [format "%3d-> $text" $id]
      }
   }
}

set timefmt "%a %b %d %H:%M:%S %Z %Y"
puts "JOB COUNT: [llength $joblist]"
puts "START TIME: [clock format [clock seconds] -format $timefmt]"
if {$time_milliseconds} {
 set jobstart [clock clicks -milliseconds]
} else {
 set jobstart [clock seconds]
}

set launchcount 0
set donecount 0
while {$launchcount<[llength $joblist]} {
   if {$launchcount-$donecount>=$parallel_count} {
      # All compute threads busy
      vwait donecount
   }
   set job [format $JobTemplate [lindex $joblist $launchcount] $launchcount]
   incr launchcount
   puts [format "CMD%3d: $job" $launchcount]
   if {$time_milliseconds} {
      set timestart [clock clicks -milliseconds]
   } else {
      set timestart [clock seconds]
   }
   global start_time
   set start_time($launchcount) $timestart
   set chan [open "| $job 2>@ stdout"]
   fconfigure $chan -blocking 0
   fileevent $chan readable [list ReadTest $launchcount $chan]
}

while {$donecount<$launchcount} {
   vwait donecount ;# Wait for all jobs to finish
}

if {$time_milliseconds} {
   set jobstop [clock clicks -milliseconds]
} else {
   set jobstop [clock seconds]
}
puts "STOP TIME: [clock format [clock seconds] -format $timefmt]"
if {$time_milliseconds} {
   set timediff [expr {($jobstop-$jobstart)*0.001}]
   puts [format "ELAPSED TIME: %.3f s" $timediff]
} else {
   set timediff [expr {($jobstop-$jobstart)}]
   puts [format "ELAPSED TIME: %.0f s" $timediff]
}
