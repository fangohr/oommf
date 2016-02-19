# FILE: pkgIndex.tcl
#
# This file has only one purpose.  It is here to support the loading
# of the Oc extension into Tcl 7.5 interpreters.  The search path for
# pkgIndex.tcl files was changed from Tcl 7.5 to Tcl 7.6.  The directory
# structure for OOMMF software is based on the search path used in 
# Tcl 7.6+.  One purpose of the Oc extension is to patch over any
# differences between Tcl versions, and it does patch Tcl 7.5 interpreters
# to use the new search path convention.  This file serves as the bootstrap
# by which Tcl 7.5 interpreters may load the Oc extension.
#
# Last modified on: $Date: 2007/03/21 01:17:02 $
# Last modified by: $Author: donahue $

global tcl_version
if {[string match 7.5 $tcl_version] && [string match Oc $name]} {
    foreach file [glob -nocomplain [file join $dir oc* pkgIndex.tcl]] {
        set dir [file dirname $file]
        source $file
    }
}

