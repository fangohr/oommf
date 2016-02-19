# FILE: bug82.tcl
#
#	Patches for Tcl 8.2
#
# Last modified on: $Date: 2009/11/04 22:34:27 $
# Last modified by: $Author: donahue $
#
# This file contains Tcl code which when sourced in a Tcl 8.2 interpreter
# will patch bugs in Tcl 8.2 fixed in later versions of Tcl.  It is not
# meant to be complete.  Bugs are patched here only as we discover the need
# to do so in writing other OOMMF software.

# The -unique option was added to [lsort] in 8.3

rename lsort Tcl8.2_lsort
;proc lsort {args} {
   if {[llength $args] < 2} {
      return [uplevel 1 Tcl8.2_lsort $args]
   }
   set argcount [llength $args]
   set list [lindex $args [expr {$argcount-1}]]
   set options [lrange $args 0 [expr {$argcount-2}]]

   set uniq_flag 0
   set sort_index -1
   set sort_method ascii
   for {set i 0} {$i<$argcount-1} {incr i} {
      set o [lindex $options $i]
      switch -exact -- $o {
         -ascii -
         -dictionary -
         -increasing -
         -decreasing {}
         -integer { set sort_method integer }
         -real    { set sort_method real }
         -command {
            set sort_method command
            set sort_command [lindex $options [incr i]]
         }
         -index {
            set sort_index [lindex $options [incr i]]
         }
         -unique {
            set uniq_flag 1
            set options [lreplace $options $i $i]
            incr i -1
            incr argcount -1
         }
         default {
            return -code error "\[lsort] option \"$o\" not supported."
         }
      }
   }

   set result [uplevel 1 Tcl8.2_lsort $options [list $list]]
   if {!$uniq_flag || [llength $result]<2} {
      return $result
   }

   set new_result {}
   set oldcheck [set oldelt [lindex $result 0]]
   if {$sort_index>=0} { set oldcheck [lindex $oldcheck $sort_index] }
   foreach elt $result {
      set check $elt
      if {$sort_index>=0} { set check [lindex $check $sort_index] }
      switch -exact -- $sort_method {
         ascii {
            if {[string compare $oldcheck $check]!=0} {
               lappend new_result $oldelt
            }
         }
         integer {
            if {int($oldcheck) != int($check)} {
               lappend new_result $oldelt
            }
         }
         real {
            if {double($oldcheck) != double($check)} {
               lappend new_result $oldelt
            }
         }
         command {
            if {[eval $sort_command [list $oldcheck $check]]!=0} {
               lappend new_result $oldelt
            }
         }
         default {
            return -code error "Bad bug82.tcl \[lsort]\
                                sort method: $sort_method"
         }
      }
      set oldelt $elt
      set oldcheck $check
   }
   lappend new_result $elt

   return $new_result
}

# Continue with bug patches for the next Tcl version
source [file join [file dirname [info script]] bug83.tcl]

