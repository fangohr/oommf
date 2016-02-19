# FILE: glibc-support.tcl
#
# This file contains a Tcl proc that returns the glibc version,
# as a two element list { major minor }.  The return value is
# { 0 0} if the version can't be determined.
proc GetGlibcVersion {} {
   set version [list 0 0]
   if {![catch {
         set fptr [open "|ldd --version" r]
         set verstr [read $fptr]
         close $fptr}]} {
      set verstr [string tolower $verstr]
      if {[regexp {ldd *\([^)]*glibc[^)]*\) *([0-9]+)\.([0-9]+)} \
              $verstr dummy a b]} {
         set version [list $a $b]
      } elseif {[regexp {ldd *\([^)]*gnu *libc[^)]*\) *([0-9]+)\.([0-9]+)} \
                   $verstr dummy a b]} {
         set version [list $a $b]
      } elseif {[regexp {gnu libc[^\n0-9]*([0-9]+)\.([0-9]+)} \
                    $verstr dummy a b]} {
         set version [list $a $b]
      }
   }
   return $version
}
