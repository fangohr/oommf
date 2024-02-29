#!/bin/sh
# FILE: odtcalc.tcl
#
# This Tcl script takes an .odt (OOMMF Data Table) file on stdin, and
# prints to stdout the same Data Table augmented with new columns as
# specified by the command line arguments
#
# Example Usage: tclsh odtcalc.tcl z "$y+$z" kg <infile.odt >outfile.odt
#
#    v--Edit here if necessary \
exec tclsh "$0" ${1+"$@"}

package require Tcl 8.4

proc Usage {} {
   puts stderr {Usage: tclsh odtcalc.tcl [-h] [-supfile file.tcl]\
      var units expression ... <infile >outfile}
   exit
}

# Note on -supfile: Complicated expressions can be difficult to
# express inline, especially in a shell-friendly manner. The -supfile
# option allows one to define such expressions as procedures in one or
# more standalone files and import them for use in odtcalc.  (NB:
# odtcalc expressions are evaluated inside a namespace, so variables
# external to procs should be declared using the 'variable' command.)
# As an example, suppose the foo.tcl file consists of:
#
#   variable mindot 1.5
#   proc Octant { x y z } {
#      variable mindot
#      if {abs($x)+abs($y)+abs($z)>$mindot} {
#         return -1
#      }
#      set octant 0
#      if {$x<0} { incr octant 1 }
#      if {$y<0} { incr octant 2 }
#      if {$z<0} { incr octant 4 }
#      return [expr {($x<0?1:0) + ($y<0?2:0) + ($z<0?4:0)}]
#   }
#
# Then the odtcalc.tcl command might look like
#
#  oommf odtcalc -supfile foo.tcl Octant '[Octant $mx $my $mz]' '' < bar.odt
#


# Check for usage request
if {[lsearch -regexp $argv {^-+h.*}]>=0} {
   Usage
}

# Check for one or more support file load requests
while {[set index [lsearch -regexp $argv {^-+supfile$}]]>=0 \
          && $index+1<[llength $argv]} {
   set supfile [lindex $argv [expr {$index + 1}]]
   if {![file exists $supfile]} {
      puts stderr "Requested support file $supfile does not exist"
      Usage
   }
   if {![file readable $supfile]} {
      puts stderr "Requested support file $supfile not readable"
      Usage
   }
   if {[catch {namespace eval temp [list source $supfile]} errmsg]} {
      puts stderr "Error sourcing Tcl support file $supfile: $errmsg"
      exit 10
   }
   unset supfile errmsg
   set argv [lreplace $argv $index [expr {$index + 1}]]
}
unset index

# Cmd line must be collection of "var units expr" triples.
if {[llength $argv]%3} {
   Usage
}

# Tweak stdio streams (copied from odtcols)
fconfigure stdin  -buffering full -buffersize 30000
fconfigure stdout -buffering full -buffersize 30000

# Version of puts that exits quietly on "broken pipe" errors
rename puts orig_puts
proc puts { args } {
   if {[catch {eval orig_puts $args} errmsg]} {
      if {[regexp {^error writing [^:]+: broken pipe} $errmsg]} {
         # Ignore broken pipe and exit
         exit
      } else {
         error $errmsg
      }
   }
}

proc Fatal {msg} {
    puts stderr "Invalid ODT file @ line $::number: $msg"
    exit 1
}


# Add common variables pi and mu0 to evaluation namespace
namespace eval temp {variable pi [expr {4*atan(1.0)}]}
namespace eval temp {variable mu0 [expr {4*$pi*1e-7}]}

proc Data {line source} {
    if {[catch {llength $line}]} {
	Fatal "\nData line is not a valid list\n$source"
    }
    if {![info exists ::columns]} {
	Fatal "\nNo Columns header line active"
    }
    if {[llength $::argv] == 0} {
	puts -nonewline $source
	return
    }
    foreach c $::columns v $line {
	catch {namespace eval temp [list namespace eval \
		[namespace qualifiers $c] {}]}
	namespace eval temp [list variable $c $v]
	if {[string first : $c] >= 0} {
	    namespace eval temp [list upvar 0 $c [lindex [split $c :] end]]
	}
    }
    puts -nonewline "[string trim $source \n]"
    foreach {c u e} $::argv {
	set val [namespace eval temp [list expr $e]]
	puts -nonewline \t$val
	namespace eval temp [list variable $c $val]
    }
    puts {}
}

proc Columns {labels source} {
    if {[catch {llength $labels}]} {
	Fatal "\nColumn labels are not a valid list\n$source"
    }
    set newlabels {}
    foreach {label unit expr} $::argv {
	lappend labels $label
	append newlabels \t[list $label]
    }
    set ::columns $labels
    puts "[string trim $source \n]$newlabels"
}

proc Units {labels source} {
    if {[catch {llength $labels}]} {
	Fatal "\nColumn units are not a valid list\n$source"
    }
    set newlabels {}
    foreach {label unit expr} $::argv {
	append newlabels \t[list $unit]
    }
    puts "[string trim $source \n]$newlabels"
}

proc Comment {line source} {
    if {[scan $line {# %[^ :] : %n} name index] != 2} {

	if {[scan $line {# %s %n} name index] != 2} {
	    # Not any recognized header line format.
	    # Just pass it through in its original source form.
	    puts -nonewline $source
	    return
	}

	set value [string range $line $index [expr {$index+2}]]
	if {[string tolower $name] eq "table"
		&& [string tolower $value] eq "end"} {
	    unset -nocomplain ::units ::columns
	}
    } 
    set value [string range $line $index end]
    switch -exact -- [string tolower $name] {
	columns		{Columns $value $source}
	units		{Units $value $source}
	default		{puts -nonewline $source}
    }
}

proc Main {} {
    set ::number 1
    set source {}
    set line {}
    while {[gets stdin buf]>=0} {

	# Keep track of what original source looks like
	append source $buf\n

	# Continuation line processing
	if {[string match #* $line]} {
	    set buf [string trimleft $buf #]
	}
	if {[string match {*\\} $buf]} {
	    append line [string range $buf 0 end-1]
	    continue
	}
	append line $buf

	# Normalize all white space runs to single space character
	# DANGER - This could corrupt column names
	regsub -all -- {\s+} $line " " line

	# Remove leading/trailing space
	set line [string trim $line " "]
	
	if {$line eq {}} {
	    # Blank line; pass through
	    puts -nonewline $source
	} elseif {[string match #* $line]} {
	    Comment $line $source
	} else {
	    Data $line $source
	}
	incr ::number [llength [split $source \n]]
	set source {}
	set line {}
    }
}

Main
exit
