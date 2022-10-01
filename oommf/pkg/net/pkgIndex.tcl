# FILE: pkgIndex.tcl
#
# Last modified on: $Date: 2015/03/25 16:45:09 $
# Last modified by: $Author: dgp $
#
package ifneeded Net 2.0b0 [list uplevel #0 [list source [file join $dir net.tcl]]]
