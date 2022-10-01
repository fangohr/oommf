# FILE: mmarchive.tcl
#
# Accept data from solvers and save to file.
# Accepts vectorfield, scalarfield, and datatable formats
# Provide no GUI, so this app survives death of X

# Support libraries
package require Oc 2      ;# [Oc_TempName]
package require Net 2
if {[Oc_Main HasTk]} {
   wm withdraw .
}

Oc_Main SetAppName mmArchive
Oc_Main SetVersion 2.0b0
regexp \\\044Date:(.*)\\\044 {$Date: 2015/11/24 21:17:20 $} _ date
Oc_Main SetDate [string trim $date]
Oc_Main SetAuthor [Oc_Person Lookup dgp]
Oc_Main SetDataRole consumer

Oc_Main Preload Oc_TempName Oc_TempFile

Oc_CommandLine ActivateOptionSet Net
Oc_CommandLine Parse $argv

Oc_EventHandler Bindtags all all

##------ Message log ---------#
#
# Each log entry is a triplet: timestamp message-type message,
# where message-type is error, warning, status.
# When a GUI is initialized, the log is copied over into the
# GUI widget log display.  After that, each write to the shared
# "status" variable is appended to the log display.  The "log"
# variable itself is not shared because then each append to log
# would trigger a copy of the complete log to each GUI.
set log {}
set logshrink 500 ;# Number of messages to remove when shrinking log
set logmax [expr {5000 + $logshrink}] ;# Maximum number of messages to retain
proc LogMessage { type msg } {
   global log logmax logshrink status
   set report [list [clock seconds] $type $msg]
   if {[llength $log]>=$logmax} {
      set log [lrange $log $logshrink end]
   }
   lappend log $report
   set status $report ;# "share" report with Ow_Logs, if any.
}
LogMessage status "mmArchive started"
#
##------ Message log ---------#

##------ GUI ---------#
#
set gui {
   package require Oc 2
   package require Tk
   package require Ow
   wm withdraw .
}
append gui "[list Oc_Main SetAppName [Oc_Main GetAppName]]\n"
append gui "[list Oc_Main SetVersion [Oc_Main GetVersion]]\n"
append gui "[list Oc_Main SetPid [pid]]\n"
append gui {

   regexp \\\044Date:(.*)\\\044 {$Date: 2015/11/24 21:17:20 $} _ date
   Oc_Main SetDate [string trim $date]

   # This won't cross different OOMMF installations nicely
   Oc_Main SetAuthor [Oc_Person Lookup dgp]

   catch {
      set mmArchive_doc_section [list [file join \
                  [Oc_Main GetOOMMFRootDir] doc userguide userguide \
                                        Data_Archive_mmArchive.html]]
      Oc_Main SetHelpURL [Oc_Url FromFilename $mmArchive_doc_section]
   }

   # May want to add error sharing code here.
   set menubar .mb
   foreach {fmenu omenu hmenu} [Ow_MakeMenubar . $menubar File Options Help] {}
   if {![Oc_Option Get Menu show_console_option _] && $_} {
      $fmenu add command -label "Show console" \
          -command { console show } -underline 0
   }
   $fmenu add command -label "Close interface" \
       -command { closeGui } -underline 0
   $fmenu add command -label "Exit [Oc_Main GetAppName]" \
       -command exit -underline 1
   $omenu add checkbutton -label "Wrap lines" -underline 0 \
       -offvalue 0 -onvalue 1 -variable WrapState \
       -command {$logwidget Wrap $WrapState}
   $omenu add command -label "Clear buffer" \
       -command {$logwidget Clear} -underline 1
   $omenu add separator
   $omenu add command -label "Enlarge font" \
       -command {$logwidget AdjustFontSize 1} -underline 0 \
       -accelerator {Ctrl++}
   bind . <Control-KeyPress-plus> { $omenu invoke "Enlarge font" }
   $omenu add command -label "Reduce font" \
       -command {$logwidget AdjustFontSize -1} -underline 0 \
       -accelerator {Ctrl+-}
   bind . <Control-KeyPress-minus> { $omenu invoke "Reduce font" }
   $omenu add command -label "Copy" -underline 0 \
      -command {$logwidget CopySelected} -accelerator "Ctrl+C"
   bind . <Control-c>  { $omenu invoke "Copy" }

   Ow_StdHelpMenu $hmenu
   set menuwidth [Ow_GuessMenuWidth $menubar]
   set brace [canvas .brace -width $menuwidth -height 0 -borderwidth 0 \
                  -highlightthickness 0]
   pack $brace -side top

   # Resize root window brace when OID is assigned:
   Oc_EventHandler New _ Net_Account NewTitle [subst -nocommands {
      $brace configure -width \
         [expr {%winwidth<$menuwidth ? $menuwidth : %winwidth}]
   }]

   share status
}
append gui "[list Ow_Log New logwidget \
                -memory $logmax -memory_shrink $logshrink]\n"
append gui {
   proc LogInit { code result args } {
      global logwidget status
      foreach elt $result {
         $logwidget Append $elt
      }
      trace variable status w [subst {$logwidget Append \$status ;#}]
   }
   remote -callback LogInit set log

   if {[Ow_IsAqua]} {
      # Add some padding to allow space for Aqua window resize
      # control in lower righthand corner
      . configure -bd 4 -relief ridge -padx 10
   }
}
#
##------ GUI ---------#

##------ Support procs ---------#
#
proc CheckFileMove { f1 f2 errmsg_var } {
   upvar $errmsg_var errmsg
   # Returns 1 if it looks okay to move file f1 to f2
   if {[string compare [file extension $f1] [file extension $f2]]!=0} {
      set errmsg "File extensions don't match"
      return 0  ;# Don't allow extension change
   }
   if {![file isfile $f1]} {
      set errmsg "File \"$f1\" is not a regular file"
      return 0; ;# Only move regular files
   }
   if {[file executable $f1]} {
      set errmsg "File \"$f1\" is executable"
      return 0  ;# Only move non-executable files
   }
   if {![file owned $f1]} {
      set errmsg "File \"$f1\" not owned by current user"
      set sidfile [Oc_WinGetFileSID $f1]
      set siduser [Oc_WinGetCurrentProcessSID]
      set fileowner [Oc_WinGetSIDAccountName $sidfile]
      set user  [Oc_WinGetSIDAccountName $siduser]
      if {[string compare $fileowner $user]!=0} {
         append errmsg " ($fileowner vs. $user)"
      } elseif {[string compare $sidfile $siduser]!=0} {
         append errmsg " ($sidfile vs. $siduser)"
      }
      if {[Oc_AmRoot]>0} {
         global tcl_platform
         if {[string compare windows $tcl_platform(platform)]==0} {
            append errmsg \
               "\nOOMMF applications should not be run as administrator"
         } else {
            append errmsg \
               "\nOOMMF applications should not be run as root"
         }
      }
      return 0  ;# Require ownership
   }
   return 1
}
proc CheckFileAppend { filename } {
   # Returns 1 if it looks okay to append to filename
   if {[string compare .odt [file extension $filename]]!=0} {
      return 0  ;# Only append to .odt files
   }
   if {[file exists $filename]} {
      # Additional checks for pre-existing files:
      if {![file isfile $filename] || [file executable $filename]} {
         return 0  ;# Only append to non-executable, normal files
      }
   }
   return 1
}
#
##------ Support procs ---------#

##------ Control server ---------#
#
[Net_Server New _ -alias [Oc_Main GetAppName] \
     -protocol [Net_GeneralInterfaceProtocol $gui {}]] Start 0
#
##------ Control server ---------#

##------ DataTable server ---------#
#
Net_Protocol New protocol(dt) -name "OOMMF DataTable protocol 0.1"
$protocol(dt) AddMessage start DataTable { triples } {
   if {[catch {WriteTriples $connection $triples} errmsg]} {
      LogMessage error $errmsg  ;# Log
      error $errmsg             ;# rethrow
   }
#after 200
   return [list start [list 0 0]]
}
[Net_Server New server(dt) -protocol $protocol(dt) \
     -alias [Oc_Main GetAppName]] Start 0
#
##------ DataTable server ---------#

##------ Vectorfield server ---------#
#
Net_Protocol New protocol(vf) -name "OOMMF vectorField protocol 0.1"
$protocol(vf) AddMessage start datafile { fnlist } {
   set tmpfile [lindex $fnlist 0]
   set permfile [lindex $fnlist 1]
   if {![CheckFileMove $tmpfile $permfile errmsg]} {
      set errmsg "Can't move file $tmpfile to $permfile: $errmsg"
      LogMessage error $errmsg ;# Log
      error $errmsg            ;# Throw
   }
   # Report writing of file
   if {[catch {file rename -force $tmpfile $permfile} errmsg]} {
      LogMessage error "$errmsg (error writing vector field)" ;# Log
      error $errmsg                                       ;# rethrow
   }
   LogMessage status "Wrote vector field $permfile"
#after 2000
   return [list start [list 0 0]]
}
[Net_Server New server(vf) -protocol $protocol(vf) \
     -alias [Oc_Main GetAppName]] Start 0
#
##------ Vectorfield server ---------#

##------ Scalarfield server ---------#
#
Net_Protocol New protocol(sf) -name "OOMMF scalarField protocol 0.1"
$protocol(sf) AddMessage start datafile { fnlist } {
   set tmpfile [lindex $fnlist 0]
   set permfile [lindex $fnlist 1]
   if {![CheckFileMove $tmpfile $permfile errmsg]} {
      set errmsg "Can't move file $tmpfile to $permfile: $errmsg"
      LogMessage error $errmsg ;# Log
      error $errmsg            ;# Throw
   }
   # Report writing of file
   if {[catch {file rename -force $tmpfile $permfile} errmsg]} {
      LogMessage error "$errmsg (error writing scalar field)" ;# Log
      error $errmsg                                       ;# rethrow
   }
   LogMessage status "Wrote scalar field $permfile"
   return [list start [list 0 0]]
}
[Net_Server New server(sf) -protocol $protocol(sf) \
     -alias [Oc_Main GetAppName]] Start 0
#
##------ Scalarfield server ---------#

##------ DataTable helper procs ---------#
proc WriteTriples {conn triples} {
   global conn_file_req   ;# Maps connection to requested filename
   global conn_file       ;# Maps connection to actual filename
   global conn_channel    ;# Maps connection to open channel
   global conn_col_list   ;# Maps connection to table column labels
   global conn_head_width ;# Column header widths
   global conn_data_fmt    ;# Format list for data

   set basewidth 20        ;# Default column width
   set basefmt "%- 20.17g"  ;# Base format for table body

   # Create "tripdata" array that maps labels to values
   foreach trip $triples {
      foreach {label {} value} $trip {
         set tripdata($label) $value
      }
   }

   # Close current file?
   set newfile [lindex [array names tripdata "@Filename:*"] end]
   if {![string match {} $newfile]} {
      set newfile [string trim [string range $newfile 10 end]]
      if {[string match {} $newfile]} {
         # Explicit close-file request
         CloseTriplesFile $conn
         return
      }
      if {[info exists conn_file_req($conn)] && \
              ![string match $conn_file_req($conn) $newfile]} {
         CloseTriplesFile $conn
      }
   }


   # Have we already opened a file for this connection?
   if {[catch {set chan $conn_channel($conn)}]} {
      # New connection!
      if {![CheckFileAppend $newfile]} {
         error "Invalid DataTable output file request: $newfile"
         # Note: Error will get logged by catch around WriteTriples
      }

      # Build column list
      set conn_file($conn) {}
      set conn_file_req($conn) $newfile
      set conn_channel($conn) {}
      set conn_col_list($conn) {}
      set conn_head_width($conn) {}
      set conn_data_fmt($conn) {}
      set unit_list {}
      foreach trip $triples {
         foreach {label unit {}} $trip {
            if {![string match "@Filename:*" $label]} {
               lappend conn_col_list($conn) $label  ;# New data column
               lappend unit_list $unit
            }
         }
      }

      # Determine column formats
      foreach label $conn_col_list($conn) unit $unit_list {
         set width [string length [list $label]]
         set unit_width [string length [list $unit]]
         if {$width<$unit_width} { set width $unit_width }
         lappend conn_head_width($conn) $width
         set fmt " $basefmt"
         if {$width>$basewidth} {
            # Extra-wide column.  Pad numeric format with
            # spaces on both sides.
            set left [expr {($width-$basewidth)/2}]
            set right [expr {$width-$basewidth-$left}]
            set fmt [format " %*s%s%*s" $left {} $basefmt $right {}]
         }
         lappend conn_data_fmt($conn) $fmt
      }

      # Open output file for append
      set chan {}
      set conn_file($conn) $conn_file_req($conn)
      if {[string match {} $conn_file($conn)] || \
              [catch {open $conn_file($conn) "a+"} chan]} {
         # Unable to open output file.  Try using temporary filename
         set conn_file($conn) [Oc_TempName "mmArchive" ".odt"]
         if {[catch {open $conn_file($conn) "a+"} chan]} {
            # Abandon ship!
            error "Unable to open DataTable output file: $chan"
            # Note: Error will get logged by catch around WriteTriples
         }
      }
      LogMessage status "Opened $conn_file($conn)"
      set conn_channel($conn) $chan

      # Setup exit handler
      global server
      Oc_EventHandler New _ $conn Delete \
          [list CloseTriplesFile $conn] -oneshot 1
      Oc_EventHandler New _ Oc_Main Exit     \
          [list CloseTriplesFile $conn]  \
          -oneshot 1 -groups [list $conn]

      # Write out table header
      if {[tell $chan]<1} {
         puts $chan "# ODT 1.0" ;# File magic
      } else {
         puts $chan "#"         ;# Separate blocks with an empty line
      }
      puts $chan "# Table Start"
      puts $chan "# Title: mmArchive Data Table,\
                [clock format [clock seconds]]"
      puts -nonewline $chan "# Columns:"
      # Drop labels and units on left of each column, mutually centered.
      foreach label $conn_col_list($conn) \
          headwidth $conn_head_width($conn) {
             set key [list $label]
             set keylength [string length $key]
             if {$keylength<$headwidth} {
                set left [expr {($headwidth-$keylength)/2}]
                set right [expr {$headwidth-$keylength-$left}]
             } else {
                set left 0
                set right 0
             }
             if {$headwidth<$basewidth} {
                incr right [expr {$basewidth-$headwidth}]
             }
             puts -nonewline $chan \
                 [format " %*s%s%*s" $left {} $key $right {}]
          }
      puts -nonewline $chan "\n# Units:  "
      foreach unit $unit_list \
          headwidth $conn_head_width($conn) {
             set key [list $unit]
             set keylength [string length $key]
             if {$keylength<$headwidth} {
                set left [expr {($headwidth-$keylength)/2}]
                set right [expr {$headwidth-$keylength-$left}]
             } else {
                set left 0
                set right 0
             }
             if {$headwidth<$basewidth} {
                incr right [expr {$basewidth-$headwidth}]
             }
             puts -nonewline $chan \
                 [format " %*s%s%*s" $left {} $key $right {}]
          }
      puts $chan {}  ;# newline
      flush $chan
   }

   # Index into tripdata using the column list.  Note that
   # there is no check for extra (new) data columns; any
   # such will just be ignored.
   puts -nonewline $chan "          "  ;# Indent past "# Columns:" heading
   foreach label $conn_col_list($conn)  \
       width $conn_head_width($conn) \
       fmt $conn_data_fmt($conn) {
          if {[catch {set tripdata($label)} value]} {
             # Missing value.  This shouldn't happen, but the format could
             # be pre-computed and saved in a list if needed (for speed).
             set left [expr {($width-2)/2}]
             if {$width<$basewidth} { set width $basewidth }
             set right [expr {$width-2-$left}]
             set value [format " %*s{}%*s" $left {} $right {}]
          } else {
             if {![catch {format $fmt $value} test]} {
                set value $test
             } else {
                # Format failed; value is probably a NaN
                set value [format " %*s" $width $value]
             }
          }
          puts -nonewline $chan $value
       }
   puts $chan {}
   flush $chan
}

proc CloseTriplesFile {conn} {
   global conn_file_req conn_file conn_channel conn_col_list
   global conn_head_width conn_data_fmt
   if {![catch {set conn_channel($conn)} chan]} {
      puts $chan "# Table End"
      close $chan
      LogMessage status "Closed $conn_file($conn)"
   }
   catch {unset conn_file_req($conn)}
   catch {unset conn_file($conn)}
   catch {unset conn_channel($conn)}
   catch {unset conn_col_list($conn)}
   catch {unset conn_head_width($conn)}
   catch {unset conn_data_fmt($conn)}
}

##^^---- DataTable helper procs -------^^#

vwait forever
