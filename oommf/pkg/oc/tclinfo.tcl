#!/bin/sh
# FILE: tclinfo.tcl
#
# This short Tcl script prints to stdout information about the Tcl
# shell that is running it.
#
#    v--Edit here if necessary \
exec tclsh "$0" ${1+"$@"}
########################################################################

puts "Tcl patchlevel = [info patchlevel]"
parray tcl_platform
set tkver [package provide Tk]
if {[string match {} $tkver]} {
   puts "Tk version = n/a"
} else {
   global tk_patchLevel
   if {[info exists tk_patchLevel]} {
      # In Tcl/Tk prior to version 8.5, [package provide Tk]
      # only returns major.minor.
      puts "Tk version = $tk_patchLevel"
   } else {
      puts "Tk version = $tkver"
   }
}
exit   ;# In case this script is sourced by wish
