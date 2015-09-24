# FILE: makeextras.tcl
#
# This file lists extra files required during builds for local
# extensions.  Sourced by ../makerules.tcl.
#
# Verify that this script is being sourced by pimake
if {[llength [info commands MakeRule]] == 0} {
    error "'[info script]' must be evaluated by pimake"
}
#
########################################################################

# Add any additional libraries installed under pkg
#lappend oxslibs cvode
