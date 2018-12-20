#!/bin/sh
# FILE: badgeom.tcl
# Usage: badgeom.tcl
#
# Checks for bad window geometry propagation.  Should start up with a
# simple window displaying some sample text.  Every three seconds
# after start the phrase changes, and the window width should change
# to match the text.  On buggy systems the window width stops
# adjusting at some point, after which only part of the sample text is
# visible.
#
# \
exec tclsh "$0" ${1+"$@"}

package require Tk

set pause 3000

set txt1 "The quick brown dog jumps over the lazy fox."
set txt2 "Now is the time for all good men to come to\
                         the aid of their party."

label .lab -text $txt1
pack .lab
after $pause {set abit 1}
vwait abit

for {set i 0} {$i<5} {incr i} {
   .lab configure -text $txt2
   after $pause {set abit 1}
   vwait abit
   .lab configure -text $txt1
   after $pause {set abit 1}
   vwait abit
}

exit
