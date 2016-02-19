#!/bin/sh
# FILE: restart_lastjob.tcl
#
# Small Tcl script that reads oxsii.errors or boxsi.errors to find last
# job, and restarts it with the -restart 1 option.
#
#    v--Edit here if necessary \
exec tclsh "$0" ${1+"$@"}
########################################################################

proc Usage {} {
   puts stderr \
       {Usage: lastjob [-logfile logname] [-unfinished] [-v] <show|restart>\
           <oxsii|boxsi> [hostname] [username]}
   exit 99
}

###################################################################
###      Several helper functions for parsing log lines         ###
proc Extract_MIF_filename { logfile start_logline } {
   # Extract MIF file from log line
   if {![regexp {^Start: (.+)$} $start_logline dummy miffile]} {
      puts stderr "Invalid Start line in logfile \"$logfile\":"
      puts stderr " $start_logline"
      exit 60
   }
   set miffile [string trim $miffile \"]
   return $miffile
}
proc Extract_Options { logfile option_logline } {
   # Extract options from log line
   if {![regexp {^Options: (.*)$} $option_logline dummy runopts]} {
      puts stderr "Invalid Options line in logfile \"$logfile\":"
      puts stderr " $option_logline"
      exit 65
   }
   set runopts [string trim $runopts]
   return $runopts
}
proc Extract_Parameters { logfile option_logline } {
   # Extract parameters from log line
   set params {}
   set index [lsearch -regexp $option_logline {^-+parameters$}]
   if {$index >= 0} {
      set params [lindex $option_logline [expr {$index+1}]]
   }
   return $params
}
###################################################################

set logfile {}
if {[set index [lsearch -regexp $argv {^-+logfile$}]]>=0 && \
	$index+1<[llength $argv]} {
   set nindex [expr {$index + 1}]
   set logfile [lindex $argv $nindex]
   set argv [lreplace $argv $index $nindex]
}

set verbose 0
if {[set index [lsearch -regexp $argv {^-+v(|erbose)$}]]>=0} {
   set verbose 1
   set argv [lreplace $argv $index $index]
}

set find_unfinished 0
if {[set index [lsearch -regexp $argv {^-+u(|nfinished)$}]]>=0} {
   set find_unfinished 1
   set argv [lreplace $argv $index $index]
}

if {[llength $argv]<2 || [llength $argv]>4 \
	|| [lsearch -regexp $argv {^-+h(|elp)$}]>=0} {
   Usage
}

foreach {action progname hostname username} $argv break
set action [string tolower $action]
if {[string compare show $action]!=0 && \
        [string compare restart $action]!=0} {
   puts stderr "Invalid action: $action"
   exit 10
}
if {[string match {} $hostname]} {
   regsub {[.].*$} [info hostname] {} hostname
}
if {[string match {} $username] && \
       [catch {set tcl_platform(user)} username]} {
   set username {}
}
if {![string match {} $username]} {
   set username "-[string trim $username]"
}
set progname [string tolower $progname]
if {[string compare oxsii $progname]==0} {
   set progname Oxsii
} elseif {[string compare boxsi $progname]==0} {
   set progname Boxsi
} else {
   puts stderr "Invalid program name: $progname"
   exit 20
}

# Use above info to construct regexps used to scan log file
set scanexp [format {^\[%s<[0-9]+-%s%s>} $progname $hostname $username]
set pidexp [format {^\[%s<([0-9]+)-%s%s>} $progname $hostname $username]
set timeexp [format {^\[%s<[0-9]+-%s%s> *([^]]+)\]} $progname $hostname $username]

# Subsequently, use lowercase form of program name
set progname [string tolower $progname]

# Figure out OOMMF root directory
if {[catch {file normalize [info script]} _]} {
   # 'file normalize' new in Tcl 8.4.  The following
   # approximation can break in some cases.
   set oxsdir [file dirname [file join [pwd] [info script]]]
} else {
   set oxsdir [file dirname $_]
}
set oommfdir [file dirname [file dirname $oxsdir]]

# Open and read logfile
if {[string match {} $logfile]} {
   # logfile not set by user; use default based on progname
   set logfile [file join $oommfdir ${progname}.errors]
}
if {![file exists $logfile]} {
   puts stderr "Logfile \"$logfile\" not found."
   exit 30
}
if {![file isfile $logfile] || ![file readable $logfile] \
       || [catch {open $logfile r} chan]} {
   puts stderr "Logfile \"$logfile\" not readable."
   exit 40
}
fconfigure $chan -eofchar {}   ;# Ignore EOF characters
set data [read $chan]
close $chan

# Search through logfile data for entries from this host/username
set lines [split $data "\n"]
if {[catch {lsearch -all -regexp $lines $scanexp} checklines]} {
   # Apparently old version of lsearch that doesn't support -all.
   # Do it by hand.
   set checklines {}
   set worklines $lines
   set offset 0
   while {[set match [lsearch -regexp $worklines $scanexp]]>=0} {
      lappend checklines [incr offset $match]
      set worklines [lrange $worklines [incr match] end]
      incr offset
   }
}

# Starting at the back of the match list, look for a "Start"
# entry immediately following.
set startline {}
set donepids {}
set donejobs {}
catch {unset donepid_info}
catch {unset donejob_info}
for {set i [llength $checklines]} {[incr i -1] >= 0} {} {
   set index [lindex $checklines $i]
   set infoline [lindex $lines $index]
   if {![regexp -- $pidexp $infoline dummy pid]} {
      puts stderr "Invalid log record: $infoline"
      exit 45
   }
   set teststart [lindex $lines [incr index]]
   set testargs  [lindex $lines [incr index]]
   if {[string match "End *" $teststart]} {
      # Run completed; record pid
      lappend donepids $pid
      set donepid_info($pid) $infoline
   }
   if {[string match {Start: *} $teststart] \
       && [string match {Options: *} $testargs]} {
      # Check to see if job is finished.
      set pidmatch [lsearch -exact $donepids $pid]
      set runspec [list [Extract_MIF_filename $logfile $teststart] \
                      [Extract_Parameters $logfile $testargs]]
      if {$pidmatch >= 0} {
         set donepids [lreplace $donepids $pidmatch $pidmatch]
         lappend donejobs $runspec
         set doneline [set donejob_info($runspec) $donepid_info($pid)]
         set jobdone 1
      } elseif {[lsearch -exact $donejobs $runspec]>=0} {
         set doneline $donejob_info($runspec)
         set jobdone 1
      } else {
         set jobdone 0
      }
      if {!$find_unfinished || !$jobdone} {
         set startline $teststart
         set optsline $testargs
         break
      }
   }
}
if {[string match {} $startline]} {
   if {$find_unfinished} {
      puts stderr "No unfinished problem Start lines found\
                   in logfile \"$logfile\""
   } else {
      puts stderr "No problem Start lines found in logfile \"$logfile\""
      puts stderr "Line count: [llength $lines]"
      puts stderr "Check count: [llength $checklines]"
      puts stderr "Scan exp: $scanexp"
   }
   exit 50
}

if {$verbose} {
   puts "------------ Log entry ------------"
   puts "$infoline"
   puts "$startline"
   puts "$optsline"
   if {$jobdone} {
      regexp -- $timeexp $doneline dummy timespec
      puts "Job completed at ${timespec}."
   } else {
      puts "Job not completed."
   }
   puts "-----------------------------------"
}

set miffile [Extract_MIF_filename $logfile $startline]
set runopts [Extract_Options $logfile $optsline]

if {[string compare restart $action]==0} {
   # Append restart flag to options
   lappend runopts -restart 1
}

if {[string match "-*" $miffile]} {
   # Name of MIF file starts with a hyphen.
   lappend runopts "--"   ;# Mark end of options.
}

set TCLSH [info nameofexecutable]
set OOMMF [file join $oommfdir oommf.tcl]
set cmd [list $TCLSH $OOMMF $progname]
set cmd [concat $cmd $runopts]
lappend cmd $miffile

switch -exact $action {
   show {
      puts "COMMAND: $cmd"
   }
   restart {
      puts "RESTART: $cmd"
      if {$jobdone} {
         regexp -- $timeexp $doneline dummy timespec
         puts "Job previously completed at ${timespec}; no restart."
      } else {
         # If Tcl is version 8.5 or later, then run cmd inside
         # current shell, with stdio and stderr redirected.  Otherwise
         # the -ignorestderr option isn't present, so rather than mess
         # around for Tcl 8.4 and older just launch cmd into background
         if {[regexp {^([0-9]+)\.([0-9]+)\.} [info patchlevel] \
                 dummy vmaj vmin] \
                && ($vmaj>8 || ($vmaj==8 && $vmin>=5))} {
            eval exec $cmd >@ stdout 2>@ stderr
         } else {
            eval exec $cmd &
         }
      }
   }
   default {
      puts stderr "ERROR: Invalid action: $action"
      exit 90
   }
}
