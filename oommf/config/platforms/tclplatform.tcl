# File: tclplatform.tcl
# Purpose: Dumps the output of the tcl_platform array.
#    This is used by the runseq.tcl script to check the capabilities of
#    the tclsh specified for build/run tests.

puts [array get tcl_platform]
