# FILE: bug83.tcl
#
#	Patches for Tcl 8.3
#
# Last modified on: $Date: 2015/09/08 05:03:51 $
# Last modified by: $Author: donahue $
#
# This file contains Tcl code which when sourced in a Tcl 8.3 interpreter
# will patch bugs in Tcl 8.3 fixed in later versions of Tcl.  It is not
# meant to be complete.  Bugs are patched here only as we discover the need
# to do so in writing other OOMMF software.

# Before Tcl 8.4, [expr rand()] can return an out of range result
# the first time it is called on a 64-bit platform.  Workaround that
# problem by always calling it and discarding the possibly bad result
# here before good results are expected.  Use [catch] to cover Tcl 7
# where rand() did not yet exist.
catch {expr {rand()}}

# Some new options were added to [lsearch] in 8.4 that are handy to
# make use of.

rename lsearch Tcl8.3_lsearch
;proc lsearch {args} {
    if {[llength $args] < 2} {
	return [uplevel 1 Tcl8.3_lsearch $args]
    }
    set argcount [llength $args]
    set pattern [lindex $args [expr {$argcount-1}]]
    set list [lindex $args [expr {$argcount-2}]]
    set options [lrange $args 0 [expr {$argcount-3}]]

    set useAll 0
    set legalOptions {}
    foreach o $options {
	switch -exact -- $o {
	    -ascii	-
	    -decreasing	-
	    -dictionary	-
	    -increasing	-
	    -inline	-
	    -integer	-
	    -not	-
	    -real	-
	    -sorted 	-
	    -start	{
		return -code error "\[lsearch] option \"$o\" not supported;\
			Upgrade to Tcl 8.4 or higher."
	    }
	    -all {
		set useAll 1
	    }
	    default {
		lappend legalOptions $o
	    }
	}
    }
    if {!$useAll} {
	return [uplevel 1 Tcl8.3_lsearch $options [list $list $pattern]]
    }
    set result {}
    set idx [uplevel 1 Tcl8.3_lsearch $legalOptions [list $list $pattern]]
    set offset 0
    while {$idx >= 0} {
	incr offset $idx
	lappend result $offset
	incr offset
	set list [lrange $list [incr idx] end]
	set idx [uplevel 1 Tcl8.3_lsearch $legalOptions [list $list $pattern]]
    }
    return $result
}


# In Tcl 8.4, the varName argument to regsub is optional

rename regsub Tcl8.3_regsub
;proc regsub {args} {
   set index [lsearch -regexp $args {^--$|^[^-]}]
   if {[string compare "--" [lindex $args $index]]} {
      incr index
   }
   if {[llength $args] - $index == 2} {
      set novar " _ Tcl8.3_regsub dummy variable "
      upvar 1 $novar foo
      if {[info exists foo]} {
         return -code error \
            "replacement regsub can't create unused variable name"
      }
      lappend args $novar
      uplevel 1 Tcl8.3_regsub $args
      set foocopy $foo
      uplevel 1 [list unset $novar]
      return $foocopy
   }
   return [uplevel 1 Tcl8.3_regsub $args]
}

# Tcl command lset was introduced in 8.4.  This old-Tcl equivalent is
# MUCH slower than the true lset.
if {![llength [info command lset]]} {
   proc K_combinator {x y} {set x}
   proc lset {varname args} {
      upvar $varname var
      set newval [lindex $args end]
      set indices [join [lrange $args 0 [expr {[llength $args]-2}]]]
      set index [lindex $indices 0]
      set record [lindex $var $index]
      if {[llength $indices]>1} {
         set record [lset record [lrange $indices 1 end] $newval]
      } else {
         set record $newval
      }
      set var [lreplace [K_combinator $var [set var {}]] $index $index $record]
   }
}


# Tcl 8.3 is the current stable version.
# Continue on to the generic patches.
source [file join [file dirname [info script]] bug.tcl]

