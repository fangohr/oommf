# FILE: bug80.tcl
#
#	Patches for Tcl 8.0
#
# Last modified on: $Date: 2000/08/25 17:01:25 $
# Last modified by: $Author: donahue $
#
# This file contains Tcl code which when sourced in a Tcl 8.0 interpreter
# will patch bugs in Tcl 8.0 fixed in later versions of Tcl.  It is not
# meant to be complete.  Bugs are patched here only as we discover the need
# to do so in writing other OOMMF software.

# Continue with bug patches for the next Tcl version
source [file join [file dirname [info script]] bug81.tcl]

