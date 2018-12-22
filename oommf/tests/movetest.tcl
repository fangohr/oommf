# FILE: movetest.tcl
# This script mimics the file move procedure used by mmArchive.
#
# NB: There is a companion file, movecheck.tcl, that does file tests
#     here but does try to move the file.  Changes here should be
#     reflected in that file.

proc Usage {} {
   puts stderr "Usage: tclsh movetest.tcl file1 file2"
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

# Check admin/root account
switch -exact $tcl_platform(platform) {
   windows {
      # Windows platform
      if {![catch {exec whoami /groups /fo csv /nh} data]} {
         set data [split $data "\n"]
         set index [lsearch -glob $data {*"S-1-5-32-544"*}]
         if {$index>=0 && \
                [string match {*Enabled group*} [lindex $data $index]]} {
            puts stderr "ERROR: Script running as administrator"
            incr errorcount
         } else {
            puts stderr "OK: Script not running as administrator"
         }
      }
   }
   darwin -
   unix {
      # Unix-like systems
      if {[string compare root $tcl_platform(user)]==0} {
         puts stderr "ERROR: Script running as root"
         incr errorcount
      } else {
         puts stderr "OK: Script not running as root"
      }
   }
   default {}
}

# Try to move the file
if {[catch {file rename -force $f1 $f2} errmsg]} {
   puts stderr "ERROR: Unable to move file: $errmsg"
   incr errorcount
} else {
   puts stderr "OK: File moved"
}

if {$errorcount>0} {
   puts "$errorcount error[expr {$errorcount>1 ? {s} : {}}] detected"
   exit $errorcount
}

puts "All tests passed."
