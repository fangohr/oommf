# FILE: pkgIndex.tcl
#
# Last modified on: $Date: 2012-09-25 17:12:02 $
# Last modified by: $Author: dgp $
#
package ifneeded Net 1.2.0.5 [list uplevel #0 [list source [file join $dir net.tcl]]]
