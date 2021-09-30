#!/bin/sh
# FILE: odtcols.tcl
#
# This Tcl script takes an .odt (OOMMF Data Table) file on stdin, and
# prints to stdout the requested columns, which are specified numerically
# starting with 1.
#
# Example Usage: tclsh odtcols.tcl 6 1 3 10 <infile.odt >outfile.odt
#
#    v--Edit here if necessary \
exec tclsh "$0" ${1+"$@"}

proc Usage {} {
   puts stderr [subst -nocommands {Usage: odtcols [-h]\
          [-f format] [-m missing] [-q] [-s] [-S] [-t output_type]\\\n\
          \           [-table select] [-no-table deselect] [-w colwidth]\
          col [col ...] \
          <infile >outfile}]
   exit
}

# Check for usage request
if {[llength $argv]<1 || [lsearch -regexp $argv {^-+h.*}]>=0} {
   Usage
}

# Version of puts that exits quietly on "broken pipe" errors
proc quiet_puts { args } {
   if {[catch {eval puts $args} errmsg]} {
      if {[regexp {^error writing [^:]+: broken pipe} $errmsg]} {
         # Ignore broken pipe and exit
         exit
      } else {
         error $errmsg
      }
   }
}

# Quiet flag?  Suppresses most errors.
set quiet_flag 0
set cmdindex [lsearch -regexp $argv {^-+q.*}]
if {$cmdindex>=0} {
    set quiet_flag 1
    set argv [lreplace $argv $cmdindex $cmdindex]
}

# Check for file summary request
set show_summary 0
set cmdindex [lsearch -regexp $argv {^-+s.*}]
if {$cmdindex>=0} {
    set show_summary 1
    set argv [lreplace $argv $cmdindex $cmdindex]
}
set cmdindex [lsearch -regexp $argv {^-+S.*}]
if {$cmdindex>=0} {
    set show_summary 2
    set argv [lreplace $argv $cmdindex $cmdindex]
}

# Table selection
set table_select_ranges {}
while {[set table_index [lsearch -regexp $argv {^-+ta.*}]]>=0} {
   if {$table_index+1>=[llength $argv]} { Usage }
   set select_str [lindex $argv [expr {$table_index+1}]]
   set argv [lreplace $argv $table_index [expr {$table_index+1}]]
   foreach item [split $select_str ","] {
      set pair [split $item ":"]
      if {[llength $pair]==1} { lappend pair [lindex $pair 0] }
      if {[llength $pair]!= 2 \
             || ![regexp {^[0-9]+$} [lindex $pair 0]] \
             || ![regexp {^[0-9]+$} [lindex $pair 1]] } {
         puts stderr "ERROR Invalid table select spec: \"$select_str\""
         puts stderr "      should be like 0 or 3:6 or 2:4,7,9:12"
         exit 1
      }
      lappend table_select_ranges $pair
   }
}

set no_table_select_ranges {}
while {[set no_table_index [lsearch -regexp $argv {^-+no-ta.*}]]>=0} {
   if {$no_table_index+1>=[llength $argv]} { Usage }
   set deselect_str [lindex $argv [expr {$no_table_index+1}]]
   set argv [lreplace $argv $no_table_index [expr {$no_table_index+1}]]
   foreach item [split $deselect_str ","] {
      set pair [split $item ":"]
      if {[llength $pair]==1} { lappend pair [lindex $pair 0] }
      if {[llength $pair]!= 2 \
             || ![regexp {^[0-9]+$} [lindex $pair 0]] \
             || ![regexp {^[0-9]+$} [lindex $pair 1]] } {
         puts stderr "ERROR Invalid no-table deselect spec: \"$deselect_str\""
         puts stderr "      should be like 0 or 3:6 or 2:4,7,9:12"
         exit 1
      }
      lappend no_table_select_ranges $pair
   }
}

# Output format (aka "type"): ODT, Comma Separated Value (CSV) or bare?
set output_type odt
while {[set cmd_index [lsearch -regexp $argv {^-+t.*}]]>=0} {
   if {$cmd_index+1>=[llength $argv]} { Usage }
   set output_type [lindex $argv [expr {$cmd_index+1}]]
   set argv [lreplace $argv $cmd_index [expr {$cmd_index+1}]]
}
set output_type [string tolower $output_type]
set type_odt  0
set type_csv  0
set type_bare 0
switch -exact $output_type {
   odt  { set type_odt 1 }
   csv  { set type_csv 1 }
   bare { set type_bare 1 }
   default {
      puts stderr "ERROR invalid output type: $output_type"
      puts stderr "      should be either ODT (default), CSV, or BARE"
      exit 1
   }
}


# String to use to indicate missing data (output)
set missing_data_string "{}"
while {[set missing_index [lsearch -regexp $argv {^-+m.*}]]>=0} {
   if {$missing_index+1>=[llength $argv]} { Usage }
   set missing_data_string [lindex $argv [expr {$missing_index+1}]]
   set argv [lreplace $argv $missing_index [expr {$missing_index+1}]]
}

# Tweak stdio streams
fconfigure stdin -buffering full -buffersize 30000
fconfigure stdout -buffering full  -buffersize 30000
if {$type_csv} {
   # CSV spec calls for network (CRLF) line-endings
   fconfigure stdout -translation {auto crlf}
}

# Process intermixed column widths, data formats, and column requests.
# Resulting colspecs holds a list of triples: globpat colwidth data_fmt
set colwidth 15                   ;# Default
set data_format "%${colwidth}s"   ;# Default
set colspecs {}
set index 0
while {$index < [llength $argv]} {
   set elt [lindex $argv $index]
   # Check for column width and data format settings
   if {$index+2 < [llength $argv]} {
      if {[regexp {^-+w.*} $elt]} {
         set colwidth [lindex $argv [expr {$index+1}]]
         set argv [lreplace $argv $index [expr {$index+1}]]
         continue
      }
      if {[regexp {^-+f.*} $elt]} {
         set data_format [lindex $argv [expr {$index+1}]]
         set argv [lreplace $argv $index [expr {$index+1}]]
         continue
      }
   }
   # Otherwise assume column selection request
   lappend colspecs [list $elt $colwidth $data_format]
   incr index
}
if {[llength $colspecs]==0} {
   # Default is colspec is full list
   lappend colspecs [list * $colwidth $data_format]
}
unset colwidth data_format index



# Proc to substitute column glob-style selection expressions
# to numerical indices.
proc GenerateColumnIndexList { subset full } {
   # Do case insensitive matching.  It might be preferable to
   # match uppercase in subset only against uppercase in full,
   # but to match lowercase in subset against either uppercase
   # or lowercase in full.
   set full [string tolower $full]
   set indices {}
   foreach item $subset {
      lassign $item id colwidth data_format
      set id [string tolower $id]
      if {[regexp -- {^[0-9]+$} $id] && $id<[expr {[llength $full]}]} {
         # id is a number; assume this is a direct index request
         lappend indices [list $id $colwidth $data_format]
      } else {
         # Otherwise, try a glob-style match
         for {set i 0} {$i<[llength $full]} {incr i} {
            if {[string match $id [lindex $full $i]]} {
               lappend indices [list $i $colwidth $data_format]
            }
         }
      }
   }
   return $indices
}

# Header list request
if {$show_summary} {
    proc DumpHeaders {column_subset column_list units_list \
            table_number table_title row_count} {
       global show_summary
       # If $show_summary==1, the summary table includes only
       # those columns in $column_subset.  If $show_summary==2,
       # then $column_subset is ignored, and all columns are
       # reported.

       # Compute column display width
       set cdw 0
       if {$show_summary==2 || [llength $column_subset]==0} {
          # All columns
          foreach c $column_list {
             set width [string length [list $c]]
             if {$width>$cdw} {
                set cdw $width
             }
          }
       } else {
          # Column subset
          foreach _ $column_subset {
             lassign $_ index colwidth_request data_format
             set c [list [lindex $column_list $index]]
             set width [string length $c]
             if {$width>$cdw} {
                set cdw $width
             }
          }
       }

       if {$table_number>0} { puts "" } ;# Table separator
       if {[string match {} $table_title]} {
          puts [format "*** Table %2d ***" $table_number]
       } else {
          puts [format "*** Table %2d: $table_title ***" $table_number]
       }
       if {[llength column_list]<1} {
          if {$row_count>0} {
             puts "WARNING: No column header line detected."
          }
       } else {
          set index 0
          puts [format "Column %-${cdw}s  %s" Label Units]
          if {$show_summary==2 || [llength $column_subset]==0} {
             # List all columns
             foreach c $column_list u $units_list {
                set c [list $c]
                set u [list $u]
                puts [format "%4d:  %-${cdw}s  %s" $index $c $u]
                incr index
             }
          } else {
             # Dump column subset
             foreach _ $column_subset {
                lassign $_ index colwidth_request data_format
                set c [list [lindex $column_list $index]]
                set u [list [lindex $units_list $index]]
                puts [format "%4d:  %-${cdw}s  %s" $index $c $u]
                incr index
             }
             puts  "Columns:   [llength $column_list]"
          }
          puts      "Data rows: $row_count"             
       }
    }

    set inside_table 0
    set row_count 0
    set table_count 0
    set table_label {}
    set column_list {}
    set units_list {}
    set columns {}
    set line {}
    while {[gets stdin newline]>=0} {
        # Is new line a continuation of previous line?
        if {![string match {} $line]} {
            # Yes: strip any leading #'s
            set newline [string trimleft $newline "#"]
        }
        # Is new line continued on next read?
        if {[regexp -- {^(.*)\\$} $newline dummy match]} {
            # Yes: append w/o trailing slash to current line
            # and immediately read next line.
            append line $match
            continue
        }
        # Append new line to current line
        append line $newline

        # Replace tabs (if any) with spaces.  This simplifies regexp
        # processing below
        regsub -all -- "\t" $line " " line

        # Process current line
        if {[string match "#" [string index $line 0]]} {
            # Comment line
            if {[regexp -nocase -- {^# *columns[ :]*(.*)} $line dum ent]} {
                # Column header line
                set column_list [concat $column_list $ent]
                set columns [GenerateColumnIndexList $colspecs $column_list]
            } elseif {[regexp -nocase -- {^# *units[ :]*(.*)} $line dum ent]} {
                set units_list [concat $units_list $ent]
            } elseif {[regexp -nocase -- {^# *table *start} $line]} {
                # New table.  Copy title string, if any
                if {[regexp {^[^:]*: *(.*[^ ]) *$} $line dummy new_title]} {
                    set table_label $new_title
                }
                if {$inside_table} {
                    # Previous table not explicitly closed, so
                    # close it here.
                    DumpHeaders $columns $column_list $units_list \
                            $table_count $table_label $row_count
                    incr table_count
                }
                set row_count 0
                set table_label {}
                set column_list {}
                set units_list {}
                set inside_table 1
            } elseif {[regexp -nocase -- {^# *title} $line]} {
                # New title
                if {[regexp {^[^:]*: *(.*[^ ]) *$} $line dummy new_title]} {
                    set table_label $new_title
                }
            } elseif {[regexp -nocase -- {^# *table *end} $line]} {
                # End of current table
                DumpHeaders $columns $column_list $units_list \
                        $table_count $table_label $row_count
                incr table_count
                set row_count 0
                set table_label {}
                set column_list {}
                set units_list {}
                set inside_table 0
            } else {
                # Plain or otherwise unprocessed comment line; skip
            }
        } else {
            # Data line
            if {![regexp {^ *$} $line]} {
                # Non-blank data line
                incr row_count
            }
        }
        set line {}
    }
    if {$inside_table} {
        DumpHeaders $columns $column_list $units_list \
                $table_count $table_label $row_count
        incr table_count
    }
    if {$table_count>1} {
        puts [format "\nTotal number of tables: %2d" $table_count]
    }
    exit 0
}

# Collimated output routines
proc ColumnDump { cols {leader " "}} {
   global type_csv
   set outline $leader
   if {$type_csv} {
      set columns_remaining [llength $cols]
      foreach _ $cols {
         lassign $_ elt colwidth data_format
         incr columns_remaining -1
         # Does field need to be quoted?
         if {[regexp -- "\[,\r\n\",]" $elt]} {
            regsub -all -- \" $elt {""} elt
            set elt "\"$elt\""
         }
         if {$columns_remaining>0} {append elt ","}
         append outline [format " %${colwidth}s" $elt]
      }
   } else {
      foreach _ $cols {
         lassign $_ elt colwidth data_format
         set elt [list $elt]  ;# Protect spaces, if any
         if {[string compare # $leader]==0} {
            # Center
            set colwidth [expr {abs($colwidth)}]
            set head [expr {($colwidth - [string length $elt])/2}]
            if {$head<0} { set head 0 }
            set tail [expr {$colwidth-$head}]
            append outline [format " %*s%-*s" $head {} $tail $elt]
         } else {
            append outline [format " %${colwidth}s" $elt]
         }
      }
   }
   quiet_puts [string trimright $outline]
}

proc ColumnDumpCSV { column_select column_list unit_list} {
   quiet_puts -nonewline " "
   set columns_remaining [llength $column_select]
   foreach _ $column_select {
      lassign $_ i colwidth data_format 
      incr columns_remaining -1
      set celt [lindex $column_list $i]
      set uelt [lindex $unit_list $i]
      if {[string match {} $uelt]} {
         set field $celt  ;# Units not specified
      } else {
         set field "$celt ($uelt)"
      }
      # Does field need to be quoted?
      if {[regexp -- "\[,\r\n\",]" $field]} {
         regsub -all -- \" $field {""} field
         set field "\"$field\""
      }
      if {$columns_remaining>0} {append field ","}
      quiet_puts -nonewline [format " %${colwidth}s" $field]
   }
   quiet_puts {}
}

proc ColumnDataDump { cols {leader " "}} {
   global missing_data_string
   global type_csv
   quiet_puts -nonewline $leader
   set columns_remaining [llength $cols]
   foreach _ $cols {
      lassign $_ elt colwidth data_format 
      incr columns_remaining -1
      if {[string match {} $elt]} {
         # Preserve missing items
         set entry $missing_data_string
      } else {
         if {[catch {set entry [format $data_format $elt]}]} {
            # Bad data.  Fill field with stars
            quiet_puts stderr "ERROR Data doesn't match format: \"$elt\""
            set entry [string repeat "*" [expr {$colwidth == 0 ? 1 : abs($colwidth)}]]
         }
      }
      if {$type_csv && $columns_remaining>0} {
         # Add separating commas for CSV format
         regsub -- {( |)( *)$} $entry {,\2} entry
      }
      quiet_puts -nonewline [format " %${colwidth}s" $entry]
   }
   quiet_puts {}
}

proc TableSelected { tableno } {
   global table_select_ranges no_table_select_ranges
   set selected 0
   if {[llength $table_select_ranges]==0} {
      set selected 1
   } else {
      foreach range $table_select_ranges {
         if {[lindex $range 0]<=$tableno && $tableno<=[lindex $range 1]} {
            set selected 1
            break
         }
      }
   }
   if {$selected} {
      foreach range $no_table_select_ranges {
         if {[lindex $range 0]<=$tableno && $tableno<=[lindex $range 1]} {
            set selected 0
            break
         }
      }
   }
   return $selected
}

proc CheckListStruct { line record } {
   # Check for proper list structure
   if {[catch {llength $record}]} {
      puts stderr "ERROR: Invalid line structure: -->$line<--"
      puts stderr "       This line skipped."
      return 0
   }
   return 1
}

proc ProcessInput {} {
    # Loop through input lines
    global colspecs
    global type_odt type_csv type_bare
    set line {}
    set columns {} ;# Note: columns is populated by GenerateColumnIndexList,
    ## and holds a list of triples: column_index colwidth data_format
    set dataline 0
    set table_number 0
    set print_table 1
    set tables_printed 0
    while {[gets stdin newline]>=0} {
        # Is new line a continuation of previous line?
        if {![string match {} $line]} {
            # Yes: strip any leading #'s
            set newline [string trimleft $newline "#"]
        }
        # Is new line continued on next read?
        if {[regexp -- {^(.*)\\$} $newline dummy match]} {
            # Yes: append w/o trailing slash to current line
            # and immediately read next line.
            append line $match
            continue
        }
        # Append new line to current line
        append line $newline

        # Remove leading and trailing whitespace
        set line [string trim $line]

        # Replace tabs (if any) with spaces.  This simplifies regexp
        # processing below
        regsub -all -- "\t" $line " " line

        # Process current line
        if {[string match {} $line]} {
           # Blank line
           if {$print_table} { quiet_puts {} }
        } elseif {[string match "#" [string index $line 0]]} {
            # Comment line
            if {[regexp -nocase -- \
                    {^# *columns[ :]*(.*)} $line dum entries]} {
                # Column header line
                if {!$print_table || ![CheckListStruct $line $entries]} {
                   set line {}; continue
                }
                set columns [GenerateColumnIndexList $colspecs $entries]
                if {$type_odt} {
                    if {[info exists unit_entries]} {
                        # Units line previously read; process and dump
                        set outlist {}
                        foreach _ $columns {
                           lassign $_ i colwidth data_format
                           lappend outlist [list [lindex $unit_entries $i] $colwidth $data_format]
                        }
                        quiet_puts "# Units: \\"
                        ColumnDump $outlist "#"
                        unset unit_entries
                    }
                    set outlist {}
                    foreach _ $columns {
                       lassign $_ i colwidth data_format
                       lappend outlist [list [lindex $entries $i] $colwidth $data_format]
                    }
                    if {$type_odt} {
                        quiet_puts "# Columns: \\"
                        ColumnDump $outlist "#"
                    } else {
                        ColumnDump $outlist ""
                    }
                } elseif {$type_csv} {
                    set column_entries $entries
                }
            } elseif {[regexp -nocase -- \
                    {^# *units[ :]*(.*)} $line dum entries]} {
                # Units header line
                if {!$print_table || ![CheckListStruct $line $entries]} {
                   set line {}; continue
                }
                if {$type_odt} {
                    if {[string match {} $columns]} {
                        # Columns line not processed yet.  Save Units line
                        # in unit_entries variable, and process it when
                        # columns line is read.
                        set unit_entries $entries
                    } else {
                       set outlist {}
                       foreach _ $columns {
                          lassign $_ i colwidth data_format
                          lappend outlist [list [lindex $entries $i] $colwidth $data_format]
                       }
                       quiet_puts "# Units: \\"
                       ColumnDump $outlist "#"
                    }
                } elseif {$type_csv} {
                    set unit_entries $entries
                }
            } elseif {[regexp -nocase -- {^# *table *(start|end)} $line]} {
                # Table start or end.  Release column assumptions.
                set columns {}
                catch {unset column_entries}
                catch {unset unit_entries}
                set dataline 0
                if {[regexp -nocase -- {^# *table *start} $line]} {
                   set print_table [TableSelected $table_number]
                   incr table_number
                   if {$print_table} {
                      incr tables_printed
                      if {!$type_odt && $tables_printed>1} {
                         # In non-odt formats, separate tables
                         # with a blank line
                         quiet_puts {}
                      }
                   }
                }
                if {$print_table && $type_odt} {
                   quiet_puts $line
                }
            } else {
                # Plain comment line
                if {$print_table && $type_odt} {
		   quiet_puts $line
                }
            }
        } else {
            # Data line
            if {!$print_table} { set line {}; continue }
            if {![CheckListStruct $line $line]} {
               set line {}; continue
            }
            if {$dataline==0} {
                if {[llength $columns]==0} {
                   # Assume input file is missing "Columns" entry
                   catch {unset entries}
                   foreach x $line { lappend entries {} }
                   set columns [GenerateColumnIndexList $colspecs $entries]
                }
                if {$type_csv && $dataline==0 \
                       && [info exists column_entries]} {
                    if {![info exists unit_entries]} { set unit_entries {} }
                   ColumnDumpCSV $columns $column_entries $unit_entries
                }
            }
            set outlist {}
            foreach _ $columns {
               lassign $_ i colwidth data_format
               lappend outlist [list [lindex $line $i] $colwidth $data_format]
            }
            ColumnDataDump $outlist " "
            incr dataline
        }
        set line {}
    }
}

if {$quiet_flag} {
    catch {ProcessInput}
} else {
    ProcessInput
}
