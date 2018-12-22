# FILE: movecheck.tcl
# This script mimics the file move checks used by mmArchive.
# 
# NB: There is a companion file, movetest.tcl, that does all the tests
#     here and then tries to actually move the file.  Any changes here
#     should be reflected in that file.

proc Usage {} {
   puts stderr "Usage: tclsh movecheck.tcl file1 file2"
   exit 99
}

if {[llength $argv]!=2 || [string match "-*" [lindex $argv 0]]} {
   Usage
}

set f1 [lindex $argv 0]
set f2 [lindex $argv 1]

set errorcount 0

# Check that file extension doesn't change
if {[string compare [file extension $f1] [file extension $f2]]!=0} {
   puts "ERROR: Changing file extension not allowed."
   incr errorcount
} else {
   puts "OK: File extensions match"
}

# Check file type
if {![file isfile $f1]} {
   puts stderr "ERROR: File \"$f1\" is not a regular file"
   incr errorcount
} else {
   puts stderr "OK: File \"$f1\" is a regular file"
}
if {[file executable $f1]} {
   puts stderr "ERROR: File1 is executable"
   incr errorcount
} else {
   puts stderr "OK: File1 is not executable"
}

# Check ownership
if {![file owned $f1]} {
   puts stderr "ERROR: File1 is not owned by current user"
   incr errorcount
} else {
   puts stderr "OK: File1 is owned by current user"
}

if {$errorcount>0} {
   puts "$errorcount error[expr {$errorcount>1 ? {s} : {}}] detected"
   exit $errorcount
}

puts "All tests passed."
