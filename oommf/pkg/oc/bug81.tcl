# FILE: bug81.tcl
#
#	Patches for Tcl 8.1
#
# Last modified on: $Date: 2000/12/15 23:06:54 $
# Last modified by: $Author: dgp $
#
# This file contains Tcl code which when sourced in a Tcl 8.1 interpreter
# will patch bugs in Tcl 8.1 fixed in later versions of Tcl.  It is not
# meant to be complete.  Bugs are patched here only as we discover the need
# to do so in writing other OOMMF software.

# Continue with bug patches for the next Tcl version
source [file join [file dirname [info script]] bug82.tcl]

