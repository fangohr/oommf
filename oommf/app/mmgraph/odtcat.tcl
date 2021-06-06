#!/bin/sh
# FILE: odtcat.tcl
#
# This Tcl script takes an .odt (OOMMF Data Table) file on stdin
# containing one or more tables, and prints to stdout an ODT file
# consisting of a single table.  When successive tables are joined,
# the tail of the first is truncated as necessary so that the
# specified control column is monotonic across the seam.
#   Each table in the input sequence is assumed to have the same
# layout as the first; header information between tables is
# summarily eliminated.  As each table is encountered, a check
# is made that the new table has the same number of columns as
# the first.  If not, an error is reported and processing is halted.
#
# Example Usage: tclsh odtcat.tcl -c 3 <infile.odt >outfile.odt
#
#    v--Edit here if necessary \
exec tclsh "$0" ${1+"$@"}

set default_control_glob [list {Oxs_TimeDriver:*:Simulation time} \
                               {Oxs_MinDriver:*:Iteration} ]

proc Usage {} {
   global default_control_glob
   puts stderr [subst -nocommands {Usage: tclsh odtcat.tcl [-h]\
          [-b overlap_lines] [-c control_column] [-o order] [-q] \\\n\
          \            <infile >outfile}]
   puts stderr "Defaults: control_column = $default_control_glob"
   puts stderr "           overlap_lines = 100"
   puts stderr "                   order = auto"
   exit
}

# Check for usage request
if {[lsearch -regexp $argv {-+h.*}]>=0} {
   Usage
}

# Quiet flag?  This suppresses informational messages written
# to stderr.
set quiet_flag 0
set cmdindex [lsearch -regexp $argv {-+q.*}]
if {$cmdindex>=0} {
    set quiet_flag 1
    set argv [lreplace $argv $cmdindex $cmdindex]
}

set control_column {}
while {[set cmdindex [lsearch -regexp $argv {-+c.*}]]>=0} {
   set control_column [lindex $argv [expr $cmdindex+1]]
   if {[regexp {^[0-9]+$} $control_column] && \
          $control_column<0} { Usage }
   set argv [lreplace $argv $cmdindex [expr $cmdindex+1]]
}

set overlap_lines 100
while {[set cmdindex [lsearch -regexp $argv {-+b.*}]]>=0} {
   set overlap_lines [lindex $argv [expr $cmdindex+1]]
   if {$overlap_lines<0} { Usage }
   set argv [lreplace $argv $cmdindex [expr $cmdindex+1]]
}

set orderproc OrderAuto
while {[set cmdindex [lsearch -regexp $argv {-+o.*}]]>=0} {
   set orderstr [string tolower [lindex $argv [expr $cmdindex+1]]]
   if {[string match "inc*" $orderstr]} {
      set orderproc OrderIncrease
   } elseif {[string match "dec*" $orderstr]} {
      set orderproc OrderDecrease
   } elseif {[string match "aut*" $orderstr]} {
      set orderproc OrderAuto
   } elseif {[string match "non*" $orderstr]} {
      set orderproc OrderNone
   } else {
      puts stderr "Invalid order selection; should be one of\
                   increase, decrease, auto, or none."
      exit
   }
   set argv [lreplace $argv $cmdindex [expr $cmdindex+1]]
}

if {[llength $argv]>0} { Usage }

# Tweak stdio streams
fconfigure stdin -buffering full -buffersize 30000
fconfigure stdout -buffering full  -buffersize 30000

proc ReadLine { bufname } {
   upvar $bufname buf
   set line {}
   while {[set readcode [gets stdin newline]]>=0} {
      append line $newline
      # Is new line continued on next read?
      if {[string match {*\\} $newline]} {
         # Yes: append trailing newline and read next line
         append line "\n"
         continue
      }
      break
   }
   set buf $line
   return $readcode
}

proc OrderNone { bufname line table_count } {
   upvar $bufname buffer
   lappend buffer $line
   return
}

proc OrderIncrease { bufname line table_count } {
   global control_column line_cut_count
   upvar $bufname buffer

   set lineval [lindex $line $control_column]

   set breakline [llength $buffer]
   if {$breakline==0} {
      # The buffer is empty.
      lappend buffer $line
      return
   }
   incr breakline -1
   while {$breakline>=0} {
      # Pull data line out of buffer, and replace any embedded
      # backslach-newline sequences with spaces
      regsub -all {\\\n} [lindex $buffer $breakline] { } checkline
      set checkval [lindex $checkline $control_column]
      if {[info exists oldcheckval] && $oldcheckval<$checkval} {
         puts stderr "Merge failure between tables $table_count and \
                   [expr {$table_count+1}]: Seam data not increasing"
         exit
      }
      set oldcheckval $checkval
      if {$checkval<=$lineval} { break }
      incr breakline -1

   }
   if {$breakline<0} {
      puts stderr "Merge failure between tables $table_count\
                   and [expr {$table_count+1}]: New data precedes\
                   *all* old data."
      exit
   }
   if {$checkval<$lineval} { incr breakline }
   set cut [expr {[llength $buffer]-$breakline}]
   incr line_cut_count $cut
   if {$cut == 0} {
      lappend buffer $line
   } else {
      set buffer [lreplace $buffer $breakline end $line]
   }
}


proc OrderDecrease { bufname line table_count } {
   global control_column line_cut_count
   upvar $bufname buffer

   set lineval [lindex $line $control_column]

   set breakline [llength $buffer]
   if {$breakline==0} {
      # The buffer is empty.
      lappend buffer $line
      return
   }
   incr breakline -1
   while {$breakline>=0} {
      # Pull data line out of buffer, and replace any embedded
      # backslach-newline sequences with spaces
      regsub -all {\\\n} [lindex $buffer $breakline] { } checkline
      set checkval [lindex $checkline $control_column]
      if {[info exists oldcheckval] && $oldcheckval>$checkval} {
         puts stderr "Merge failure between tables $table_count and \
                   [expr {$table_count+1}]: Seam data not decreasing"
         exit
      }
      set oldcheckval $checkval
      if {$checkval>=$lineval} { break }
      incr breakline -1

   }
   if {$breakline<0} {
      puts stderr "Merge failure between tables $table_count\
                   and [expr {$table_count+1}]: New data precedes\
                   *all* old data."
      exit
   }
   if {$checkval>$lineval} { incr breakline }
   set cut [expr {[llength $buffer]-$breakline}]
   incr line_cut_count $cut
   if {$cut == 0} {
      lappend buffer $line
   } else {
      set buffer [lreplace $buffer $breakline end $line]
   }
}


proc OrderAuto { bufname line table_count } {
   global control_column
   upvar $bufname buffer

   set lineval [lindex $line $control_column]

   set breakline [llength $buffer]
   if {$breakline==0} {
      # The buffer is empty.
      lappend buffer $line
      return
   }
   if {$breakline<2} {
      puts stderr "Merge failure: line buffer holds only one line of\
                   data, which is insufficient for \"auto\" ordering."
   }
   incr breakline -1

   # Look at back end of buffer, and see whether the data in
   # control_column are increasing or decreasing.
   regsub -all {\\\n} [lindex $buffer $breakline] { } checkline
   set oldcheckval [lindex $checkline $control_column]
   incr breakline -1
   while {$breakline>=0} {
      # Pull data line out of buffer, and replace any embedded
      # backslach-newline sequences with spaces
      regsub -all {\\\n} [lindex $buffer $breakline] { } checkline
      set checkval [lindex $checkline $control_column]
      if {$checkval<$oldcheckval} {
         set orderproc OrderIncrease
         break
      }
      if {$checkval>$oldcheckval} {
         set orderproc OrderDecrease
         break
      }
      set oldcheckval $checkval
      incr breakline -1
   }
   if {![info exists orderproc]} {
      puts stderr "Merge failure between tables $table_count and\
                   [expr {$table_count+1}]: Unable to determine sort\
                   direction because all data in line buffer has the\
                   same value."
      exit
   }

   # Merge using order determined above.
   $orderproc buffer $line $table_count
}


# Copy header
if {![regexp {^[0-9]+$} $control_column]} {
   # control_column does not specify a column number
   set control_glob [list $control_column]
   if {[string match {} $control_column]} {
      set control_glob $default_control_glob
   }
}
while {[ReadLine line]>=0} {
   if {[string length $line]>0 && \
          ![string match "#" [string index $line 0]]} {
      break
   } 
   puts stdout $line
   # If this is column listing and control_column is not yet
   # set, then process line to try to extract control_column
   if {[info exists control_glob]} {
      # Replace any embedded backslash-newlines with spaces
      regsub -all {\\\n} $line { } dataline
      if {[regexp -nocase -- {^# *columns[ :]*(.*)} $dataline dum ent]} {
         # Column header line
         set indices {}
         for {set i 0} {$i<[llength $ent]} {incr i} {
            foreach globpat $control_glob {
               if {[string match $globpat [lindex $ent $i]]} {
                  lappend indices $i
               }
            }
         }
         if {[llength $indices]==0} {
            puts stderr "Requested control_column glob-string\
                        \"$control_glob\" does not match any\
                        column header."
            exit
         }
         if {[llength $indices]>1} {
            puts stderr "Requested control_column glob-string\
                        \"$control_glob\" matches more\
                        than one column header:"
            foreach i $indices {
               puts stderr " \"[lindex $ent $i]\""
            }
            exit
         }
         set control_column [lindex $indices 0]
      }
   }
}

if {![regexp {^[0-9]+$} $control_column]} {
   # control_column does not specify a column number
   puts stderr "Unable to assign control_column glob request\
                \"$control_column\" to any column because\
                table header is missing a Column record."
   exit
}

set line_cut_count 0
set linebuf {}
lappend linebuf $line
regsub -all {\\\n} $line { } dataline   ;# Replace any embedded
                              ;# backslash-newlines with spaces
set column_count [llength $dataline]
if {$control_column >= $column_count} {
   puts stderr "Number of columns in table ($column_count) too small\
                to meet control column request ($control_column)."
   if {$control_column == $column_count} {
      puts stderr "(Note: control column count is zero-based.)"
   }
   exit
}

# Read remainder of file
set fillsize [llength $linebuf]
set table_count 0
while {[ReadLine line]>=0} {
   if {[string match {} $line]} { continue } ;# Blank line; for now
   # just throw away any blank lines appearing in the data area.  If
   # it is decided in the future to embed this in the output stream,
   # then one can either append \n's to the last line in the buffer,
   # or prefix \n's to the current line.  Note that this choice has
   # a potential effect at table seams.
   if {![string match "#" [string index $line 0]]} {
      # Data row
      lappend linebuf $line
      if {$fillsize<$overlap_lines} {
         incr fillsize
      } else {
         puts stdout [lindex $linebuf 0]
         set linebuf [lreplace $linebuf 0 0]
      }
      continue
   }

   # For comparison purposes, replace spaces with tabs
   regsub -all -- "\t" $line " " checkline
   if {![regexp -nocase -- {^# *table *start} $checkline]} {
      # Comment line other than "Table Start".  Pitch
      continue
   }

   # Otherwise, new table detected.  Skip over any immediately
   # following comments.
   while {[ReadLine line]>=0 && \
             ([string length $line]==0 || \
                 [string match "#" [string index $line 0]])} {}

   # Process next data line
   if {[string length $line]<1} { break }  ;# EOF
   regsub -all {\\\n} $line { } dataline   ;# Replace any embedded
                                 ;# backslash-newlines with spaces
   if {[llength $dataline] != $column_count} {
      puts stderr "Table column mismatch.  Initial table has\
                  $column_count columns, table [expr {$table_count+1}]\
                  has [llength $dataline] columns."
      exit
   }

   $orderproc linebuf $dataline $table_count
   incr table_count

   set fillsize [llength $linebuf]
   set bufdiff [expr {$fillsize - $overlap_lines}]
   if {$bufdiff>0} {
      for {set i 0} {$i<$bufdiff} {incr i} {
         puts stdout [lindex $linebuf $i]
      }
      set linebuf [lreplace $linebuf 0 [expr {$bufdiff-1}]]
      set fillsize [llength $linebuf]
   }
}

foreach line $linebuf { puts stdout $line }
puts stdout "# Table End"

if {!$quiet_flag} {
   if {$table_count==0} {
      puts stderr "WARNING: Only one table found in input stream."
   } else {
      puts stderr "Successfully merged [expr {$table_count+1}] tables."
   }
   puts stderr "$line_cut_count lines cut."
}
