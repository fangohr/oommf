# FILE: pkgIndex.tcl
#
# Last modified on: $Date: 2012-09-25 17:12:02 $
# Last modified by: $Author: dgp $
#
# Do not override an existing ifneeded script (from C, for example).
if {![string match "" [package ifneeded Oc 1.2.0.5]]} {return}
package ifneeded Oc 1.2.0.5 [list uplevel #0 [list source [file join $dir oc.tcl]]]
