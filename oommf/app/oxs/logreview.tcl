# FILE: logreview.tcl
#
# PURPOSE: Extract records from oxsii and boxsi log files.

set oommf_rootdir [file dirname [file dirname [file dirname \
                                       [file normalize [info script]]]]]

########################################################################

proc Usage { {errcode 0} } {
   if {$errcode == 0} {
      set chan stdout
   } else {
      set chan stderr
   }
   puts $chan {Usage: oommf logreview <file> [-info] [-collate] [search_opts]}
   puts $chan "where"
   puts $chan "  <file> is either oxsii, boxsi, or -file filename"
   puts $chan "  -info prints general information about selected log records"
   puts $chan "  -collate reorders records by job run"
   puts $chan " search_opts are one or more of the following:"
   puts $chan {  -after <timestamp>}
   puts $chan {  -before <timestamp>}
   puts $chan {  -firstrun}
   puts $chan {  -glob <globstring>}
   puts $chan {  -grep <regexpr>}
   puts $chan {  -lastrun}
   puts $chan {  -machine <machinename>}
   puts $chan {  -pid <pid#>}
   puts $chan {  -runselect first last}
   puts $chan {  -user <username>}
   puts $chan "The output consists of all log entries matching\
               all specified search criteria."
   exit $errcode
}

if {[lsearch -regexp $argv {-+h(|elp)$}]>=0} {
   Usage
}

set info_flag 0
set collate_flag 0
for {set i 0} {$i<[llength $argv]} {incr i} {
   set elt [lindex $argv $i]
   switch -regexp [lindex $argv $i] {
      {^oxsii$} {
         set infile [file join $oommf_rootdir oxsii.errors]
      }
      {^boxsi$} {
         set infile [file join $oommf_rootdir boxsi.errors]
      }
      {^-+file$} {
         if {$i+1<[llength $argv]} {
            incr i
            set infile [lindex $argv $i]
         } else {
            Usage 10
         }
      }
      {^-+pid$} {
         if {$i+1<[llength $argv]} {
            incr i
            set pidchk [lindex $argv $i]
            set pidchk [regsub -all "\[, \t\n\]+" $pidchk " "]
            if {![regexp {^[0-9 ]+$} $pidchk]} {
               Usage 11
            }
            lappend pid {*}$pidchk
         } else {
            Usage 11
         }
      }
      {^-+after$} {
         if {$i+1<[llength $argv]} {
            incr i
            set after_timestamp [lindex $argv $i]
         } else {
            Usage 12
         }
      }
      {^-+before$} {
         if {$i+1<[llength $argv]} {
            incr i
            set before_timestamp [lindex $argv $i]
         } else {
            Usage 13
         }
      }
      {^-+runselect$} {
         if {$i+2<[llength $argv]} {
            set runselect_first [lindex $argv $i+1]
            set runselect_last  [lindex $argv $i+2]
            set collate_flag 1 ;# Run selection implies collate
            incr i 2
         } else {
            Usage 13
         }
      }
      {^-+firstrun$} {
         set runselect_first 0
         set runselect_last  0
         set collate_flag 1 ;# Run selection implies collate
      }
      {^-+lastrun$} {
         set runselect_first end
         set runselect_last  end
         set collate_flag 1 ;# Run selection implies collate
      }
      {^-+machine$} {
         if {$i+1<[llength $argv]} {
            incr i
            set machine [lindex $argv $i]
         } else {
            Usage 14
         }
      }
      {^-+user$} {
         if {$i+1<[llength $argv]} {
            incr i
            set user [lindex $argv $i]
         } else {
            Usage 15
         }
      }
      {^-+glob$} {
         if {$i+1<[llength $argv]} {
            incr i
            set globpat [lindex $argv $i]
         } else {
            Usage 16
         }
      }
      {^-+grep$} {
         if {$i+1<[llength $argv]} {
            incr i
            set greppat [lindex $argv $i]
         } else {
            Usage 17
         }
      }
      {^-+info$} {
         set info_flag 1
      }
      {^-+collate$} {
         set collate_flag 1
      }
      default {
         Usage 19
      }
   }
}

if {![info exists infile]} {
   Usage 20
}

if {[string compare "-" $infile]!=0} {
   # Set infile to "-" to read from stdin.
   if {![file exists $infile]} {
      puts stderr "Requested log file $infile does not exist"
      exit 21
   }
   if {![file readable $infile]} {
      puts stderr "No read access to requested log file $infile"
      exit 22
   }
}

proc ScanTimestamp { timestamp } {
   # Returns the number of seconds since the start of the epoch.
   # If the import is in the log file time format, then the result
   # will be a real number with millisecond resolution.
   set subsec {}
   if {[regexp -- {^([^.]+)(\.[0-9]*)([^.]*)$} $timestamp _ a subsec b]} {
      set timestamp "$a$b"
   }
   if {[catch {clock scan $timestamp} secs]} {
      error "Unrecognized timestamp: \"$timestamp\""
   }
   # For some reason the format-free clock scan is ~5 times faster than
   #    clock scan $timestamp -format "%H:%M:%S %Y-%m-%d"

   return "$secs$subsec"
}

if {[info exists after_timestamp]} {
   if {[catch {ScanTimestamp $after_timestamp} after_seconds]} {
      puts stderr "Error scanning -after time: $after_seconds"
      Usage 20
   }
}

if {[info exists before_timestamp]} {
   if {[catch {ScanTimestamp $before_timestamp} before_seconds]} {
      puts stderr "Error scanning -before time: $before_seconds"
      Usage 20
   }
}

proc SplitByRecord { data } {
   set recrexp {^\[[^<>]+<[^>]*> *[0-9:. -]+\]} ;# Record header regexp
   set brkpts [lsearch -regexp -all $data $recrexp]
   if {[llength $brkpts]==0} { return {} }
   set newdata {}
   set start_index [lindex $brkpts 0]
   foreach next_index [lrange $brkpts 1 end] {
      set record [lrange $data $start_index $next_index-1]
      set record [string trimright [join $record "\n"]]
      lappend newdata $record
      set start_index $next_index
   }
   set record [lrange $data $start_index end]
   set record [string trimright [join $record "\n"]]
   # Note: The trimright commands will drop trailing whitespace
   #       from the original record. If you want to keep the
   #       original, set the trim characters to just "\n".
   lappend newdata $record
   return $newdata
}

################################################################
# Keys for record dictionary is a five element list of the form
#
#   src pid machine user {timestamp}
#
# where src is "oxsii" or "boxsi", and the timestamp
# format is 
#
#   hh:mm:ss.xxx yyyy-mm-dd
#
# Here .xxx is milliseconds. The millisecond portion
# of this format is not supported by the Tcl 'clock scan'
# command, so use proc ScanTimestamp from above.
################################################################

# The log record header looks like
#
#   [Oxsii<15647-betelgeuse-barney> 00:17:18.748 2023-06-15]
#
# where Oxsii is the program name, 15647 is the pid, betelgeuse is the
# machine name, barney is the username, and the remainder is the
# timestamp (with millisecond resolution). This can be parsed with
# the following regexp:
#
#  set headerexp {^\[([^<]+)<([0-9]+)-(.*)-([^>-]*)> *([0-9:. -]+)\]}
#                     prog    pid   machine  user      timestamp
#
# but is seems faster to break this up into two regexps. (See proc
# ParseHeader)

proc ParseHeader { record } {
   # It seems faster to break regexp into two pieces than one,
   # perhaps because of the .* pattern needed to handle machine
   # names such as "mr-french".
   if {![regexp -- {^\[([^<]+)<([^>]*)> *([0-9:. -]+)\]} \
            $record _ src base timestamp] \
          || ![regexp -- {([0-9]+)-(.*)-([^-]*)$} $base _ pid machine user]} {
      error "Unrecognized header in record -->$elt<--"
   }
   return [list $src $pid $machine $user $timestamp]
}

proc CreateRecordDict { records } {
   # Create dictionary for records, indexed by src (Oxsii or Boxsi),
   # pid, machine, user, and timestamp. Note: The regexp used allows
   # hyphens in the machine name (e.g., "mr-french") but assumes no
   # hyphens in the user name.
   #
   # Note: 'dict lappend' is used to add records to a key; this
   #       supports multiple records having the same key. (This
   #       should be unlikely because the record timestamps have
   #       millisecond resolution.)
   set record_dict [dict create]
   foreach elt $records {
      lassign [ParseHeader $elt] src pid machine user timestamp
      set seconds [ScanTimestamp $timestamp]
      dict lappend record_dict [list $src $pid $machine $user $seconds] $elt
   }
   return $record_dict
}

proc CollateDict { indict } {
   # Given a dict value import, returns a list consisting of a dicts,
   # with each dict containing one job run. (The length of the returned
   # list is the number of jobs.) A job run is defined by the dict key
   # excluding the time stamp, terminating either by a "\nStart:" record
   # with the same dict key (indicating reuse of a pid on a new run) or
   # else end-of-file.

   # Sort keys by pid/machine/user/timestamp
   set keylist [lsort [dict keys $indict]]

   # Break consecutive runs of keys into separate dictionaries.
   # Each run ends when either the keybase (i.e., the key
   # excluding the timestamp) changes or else the associated value
   # us a Start record
   set biglst [list]
   set tmplst [list]
   set runkey [set keyref {}]
   foreach key $keylist {
      set val [dict get $indict $key]
      if {[string match $keyref $key] && \
             ![string match "*\nStart: *" $val]} {
         # Run sequence continues
         lappend tmplst $key $val
      } else {
         # New run sequence
         if {[string length $runkey]>0} {
            lappend biglst [list $runkey [dict create {*}$tmplst]]
         }
         set tmplst [list $key $val] ;# Start new lst
         set runkey $key ;# Save first dicttmp record
         set keyref "[lrange $key 0 3]*" ;# Glob match string
      }
   }
   if {[string length $runkey]>0} { ;# Empty buffer
      lappend biglst [list $runkey $tmplst]
   }
   unset tmplst runkey

   # biglst is a list of lists, with each sublist a two element
   # list of runkey + run sequence dict associated with runkey.
   # The runkey has the form
   #
   #   <program> <pid> <machine> <user> <timestamp>
   #
   # for example,
   #
   #   Boxsi 14820 pg903091 donahue 1695177421.409
   #
   # The timestamp is the recorded time for the first run in the
   # associated run sequence. In most cases the runkeys in biglst will
   # be sorted by increasing timestamps, with two exceptions: 1) If the
   # records in the log file are stored out-of-order, or 2) If distinct
   # runs with the same program/machine/user combo have the same
   # pid. (This happens with regularity on Windows.) In this case a
   # "Start:" record in the preceding processing step will split the run
   # sequence and enter an out-of-order record into biglst. Bottom line
   # is that we need to sort biglst on the runsequence timestamp, and
   # that sort had best perform well on a sorted or nearly sorted
   # lists. The Tcl documentation states that lsort uses a merge-sort
   # algoritm, which should be OK.
   set biglst [lsort -real -index {0 4} $biglst]

   # Create and return list w/o runkeys
   set runlst [list]
   foreach elt $biglst {
      lappend runlst [lindex $elt 1]
   }
   return $runlst
}

# Read log file and split by record
if {[string compare "-" $infile]==0} {
   set chan stdin
} else {
   set chan [open $infile]
}
set data [read -nonewline $chan]
close $chan
set data [split $data "\n"]
set records [SplitByRecord $data]

# Create indexed dictionary of records
set record_dict [CreateRecordDict $records]

# Filter keys by search criteria
set keys [dict keys $record_dict]
if {[info exists pid]} {
   set pidkeys {}
   foreach p $pid {
      lappend pidkeys {*}[lsearch -all -inline -index 1 -exact $keys $p]
   }
   set keys $pidkeys
}
if {[info exists machine]} {
   set keys [lsearch -all -inline -index 2 -exact $keys $machine]
}
if {[info exists user]} {
   set keys [lsearch -all -inline -index 3 -exact $keys $user]
}
if {[info exists after_seconds] || [info exists before_seconds]} {
   if {![info exists after_seconds]} {
      set after_seconds -1.0
   }
   if {![info exists before_seconds]} {
      set before_seconds Inf
   }
   set newkeys {}
   foreach key $keys {
      set seconds [lindex $key 4]
      if {$after_seconds <= $seconds && $seconds <= $before_seconds} {
         lappend newkeys $key
      }
   }
   set keys $newkeys
}
if {[info exists user]} {
   set keys [lsearch -all -inline -index 3 -exact $keys $user]
}

# Create reduced dict
set thinned_dict [dict create]
foreach key $keys {
   dict set thinned_dict $key [dict get $record_dict $key]
}

# Collate reduced dict, if requested. Do this before value filtering
# while we still have all the job start records
if {$collate_flag} {
   set runlst [CollateDict $thinned_dict]
   set jobcount [llength $runlst]
   if {[info exist runselect_first] && [info exist runselect_last]} {
      set runlst [lrange $runlst $runselect_first $runselect_last]
   }
   set thinned_dict [dict merge {*}$runlst]
}

# Filter by value (glob, grep)
if {[info exists globpat]} {
   set thinned_dict [dict filter $thinned_dict value $globpat]
}
if {[info exists greppat]} {
   set thinned_dict [dict filter $thinned_dict script {key val} {
      regexp -- $greppat $val
   }]
}

# If -info requested, print information about the filtered dictionary
if {$info_flag} {
   if {[dict size $thinned_dict]==0} {
      puts "No records."
      exit
   }
   set thinned_keys [dict keys $thinned_dict]
   set reccnt [llength $thinned_keys]
   set recA \
      [lindex [dict get $thinned_dict [lindex $thinned_keys 0]] 0]
   lassign [ParseHeader $recA] prog pid machine user first_timestamp
   set recB \
      [lindex [dict get $thinned_dict [lindex $thinned_keys end]] end]
   lassign [ParseHeader $recB] prog pid machine user final_timestamp
   set recstr record
   if {$reccnt!=1} { append recstr "s" }
   if {[info exists jobcount]} {
      append recstr " and $jobcount job"
      if {$jobcount!=1} { append recstr "s" }
   }
   
   puts "File $infile contains $reccnt ${recstr},"
   puts "with timestamps between"
   puts "  $first_timestamp"
   puts "and"
   puts "  $final_timestamp"
   exit
}

# Otherwise print out the filtered records
if {[dict size $thinned_dict]==0} {
   puts stderr "No records"
} else {
   dict for {key val} $thinned_dict {
      foreach elt $val {
         puts "\n$elt"
      }
   }
}
