# FILE: avfdiff.tcl
#
# This file must be evaluated by mmDispSh

package require Oc 1.1.1.0
package require Mmdispcmds 1.1.1.0
Oc_ForceStderrDefaultMessage
catch {wm withdraw .}

Oc_Main SetAppName avfdiff
Oc_Main SetVersion 1.2.0.5

Oc_CommandLine Option console {} {}
Oc_CommandLine Option [Oc_CommandLine Switch] {
	{file0 {} {Input vector field file to subtract from other files}}
	{file1 {} {Input vector field file from which to subtract file0}}
	{{filen list} {} {Optional list of additional files from which to subtract file0}}
    } {
	global subfile filelist
	set subfile $file0
	set filelist [concat [list $file1] $filen]
} {End of options; next argument is file}

Oc_CommandLine Option info {} {global info; set info 1} \
   {Print mesh info (no difference files created)}
set info 0

Oc_CommandLine Parse $argv

# On Windows, invoke glob-style filename matching.
# (On Unix, shells do this automatically.)
if {[string match windows $tcl_platform(platform)]} {
    set newlist {}
    foreach file [concat [list $subfile] $filelist] {
	if {[file exists $file] || [catch {glob -- $file} matches]} {
	    # Either $file exists as is, or glob returned empty list
	    lappend newlist $file
	} else {
	    set newlist [concat $newlist [lsort $matches]]
	}
    }
    set subfile [lindex $newlist 0]
    set filelist [lrange $newlist 1 end]
}

# Load subtractor
SelectActiveMesh 1
ChangeMesh $subfile 0 0 0 -1
if {$info} {
   set namlen [string length $subfile]
   foreach infile $filelist {
      set len [string length $infile]
      if {$len>$namlen} { set namlen $len}
   }
   foreach {minval maxval} [GetMeshValueMagSpan] { break }
   set rms [GetMeshValueRMS]
   set units [GetMeshValueUnit]
   if {[catch {format "%*s (in %s): \
               Min mag = %.8g, Max mag = %.8g, RMS = %.8g" \
               $namlen $subfile $units $minval $maxval $rms} msg]} {
      error "$msg\nVALUES: subfile=$subfile\n         \
                             units=$units\n       \
                            minval=$minval\n       \
                            maxval=$maxval\n          \
                               rms=$rms"
   }
   puts $msg
}

SelectActiveMesh 0
foreach infile $filelist {
   ChangeMesh $infile 0 0 0 -1
   if {[catch {DifferenceMesh 1} msg]} {
      puts "Skipping file $infile: $msg"
   } else {
      if {$info} {
         foreach {minval maxval} [GetMeshValueMagSpan] { break }
         set rms [GetMeshValueRMS]
         set units [GetMeshValueUnit]
         puts [format "%*s (in %s):  Max diff = %14.7e,  RMS diff = %14.7e" \
                  $namlen $infile $units $maxval $rms $units]
      } else {
         set outfile "${infile}-diff"
         set fmt "binary8"
         # set fmt "text"
         if { [IsRectangularMesh] } {
            set struct "rectangular"
         } else {
             set struct "irregular"
         }
         set title $outfile
         set descr "Difference file: [list $infile] - [list $subfile]"
         puts "Writing output file: $outfile"
         WriteMesh $outfile $fmt $struct $title $descr
      }
   }
}

exit 0
