# FILE: avfdiff.tcl
#
# This file must be evaluated by mmDispSh

package require Oc 2
package require Mmdispcmds 2
Oc_ForceStderrDefaultMessage
catch {wm withdraw .}

Oc_Main SetAppName avfdiff
Oc_Main SetVersion 2.0b0

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

Oc_CommandLine Option cross {} {global cross; set cross 1} \
   {Compute ptwise filek x file0 rather than filek - file0}
set cross 0

Oc_CommandLine Option info {} {global info; set info 1} \
   {Print mesh info to stdout (no difference files created)}
set info 0

Oc_CommandLine Option odt {
   {label {} {is index column header}}
   {units {} {is index units header}}
   {valexpr {} {is expr cmd with $i, $f1, $d1, ...}}
} { global wma index_valexpr odt
   set wma(index) [list $label $units {}]
   set index_valexpr $valexpr
   set odt 1
} "\n\tPrint ODT output of differences to stdout (no difference files created)"
set odt 0

Oc_CommandLine Option numfmt {
   {fmt {expr {![catch {format $fmt 1.2}]}}
      {a C-style format string (default: "% #20.15g")}}
} { global numfmt; set numfmt $fmt
} {Format for numeric output if info or odt output is selected}
set numfmt "% #20.15g"

set filesort none
Oc_CommandLine Option filesort {
    {method {
	expr [string match none $method] || ![catch "lsort $method {}"]
} {is lsort option string or "none"}}
} { global filesort; set filesort $method } {Sort order for file1 through filen}

proc IsFloat { x } {
    if {[regexp -- {^-?[0-9]+\.?[0-9]*((e|E)(-|\+)?[0-9]+)?$} $x] || \
            [regexp -- {^-?[0-9]*\.?[0-9]+((e|E)(-|\+)?[0-9]+)?$} $x]} {
        if {![catch {expr $x}]} { return 1 }
    }
    return 0
}

proc ClipItemCheck { item } {
    # Format check
    if {[string match "-" $item] || [IsFloat $item]} {
	return 1
    }
    return 0
}

proc ClipStringCheck { clipstr } {
    set okaystr 0
    catch {
	if {[llength $clipstr]==0} {
	    set okaystr 1  ;# No clipping
	} elseif {[llength $clipstr]==6} {
	    set okaystr 1
	    # Format check
	    foreach item $clipstr {
		if {![ClipItemCheck $item]} {
		    set okaystr 0
		    break
		}
	    }
	    # Relative order check
	    for {set i 0} {$i<3} {incr i} {
		set min [lindex $clipstr $i]
		set max [lindex $clipstr [expr {$i+3}]]
		if {![string match "-" $min] && \
			![string match "-" $max] && \
			$min>$max} {
		    set okaystr 0
		    break
		}
	    }
	}
    }
    return $okaystr
}

Oc_CommandLine Option clip {
    {xmin { ClipItemCheck $xmin } {is minimum x}}
    {ymin { ClipItemCheck $ymin } {is minimum y}}
    {zmin { ClipItemCheck $zmin } {is minimum z}}
    {xmax { ClipItemCheck $xmax } {is maximum x}}
    {ymax { ClipItemCheck $ymax } {is maximum y}}
    {zmax { ClipItemCheck $zmax } {is maximum z}}
} {
    global clipstr
    set clipstr [list $xmin $ymin $zmin $xmax $ymax $zmax]
} "\n\tClipping box"
set clipstr {}

Oc_CommandLine Option resample {
   {fileselect {regexp {^(0|n)$} $fileselect} {is file to change (0 or n)}}
   {interp_order {regexp {^(0|1|3|ave)$} $interp_order}
      {is interpolation order (0, 1, 3 or ave)}}
} {
   global resample; set resample $fileselect
   global resample_order; set resample_order $interp_order
} "\n\tResample either base file0 (0) or other files (n) to make compatible"
set resample {}
set resample_order 0

Oc_CommandLine Parse $argv

# Check clip string is sensible
if {![ClipStringCheck $clipstr]} {
    puts stderr "ERROR: Bad clip request"
    exit 10
}

if {$info && $odt} {
   error "ERROR: Both -info and -odt options are set. Pick one."
}

if {$odt} {
   global wma odtmode numfmt
   set wma(average) "space"
   set wma(axis) "x"   ;# Dummy value
   set wma(defaultpos) 0
   set wma(defaultvals) 1
   set wma(extravals) 1
   set wma(normalize) 0
   set wma(numfmt)  $numfmt
   set wma(header) "shorthead"
   set wma(trailer) "notail"
   set odtmode 0
}

# On Windows %g can change Inf into a string not recognized by Tcl as
# Inf, such as "1.#INF".  Correct and adjust
proc ResetInf { x } {
   if {[string match "*1.#INF" $x]} {
      set x Inf
   }
   return $x
}

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

# Sort input file list
if {![string match none $filesort]} {
    set filelist [eval lsort $filesort {$filelist}]
}

proc AreMeshesCompatible { meshA meshB } {
   set baserange [GetMeshRange $meshA]
   set basecells [GetMeshCellSize $meshA]
   set changerange [GetMeshRange $meshB]
   set changecells [GetMeshCellSize $meshB]
   set compat 1
   foreach bi [concat $baserange $basecells] \
      ci [concat $changerange $changecells] {
      if {abs($bi-$ci)>1e-12*abs($bi+$ci)} {
         set compat 0
         break
      }
   }
   return $compat
}

proc MakeCompatibleMesh { baseid changeid outid method_order} {
   # Makes mesh changeid compatible with mesh baseid
   set baserange [GetMeshRange $baseid]
   set basecells [GetMeshCellSize $baseid]

   foreach {xmin ymin zmin xmax ymax zmax} $baserange break
   foreach {xstep ystep zstep} $basecells break
   set icount [expr {int(round(($xmax-$xmin)/$xstep))}]
   set jcount [expr {int(round(($ymax-$ymin)/$ystep))}]
   set kcount [expr {int(round(($zmax-$zmin)/$zstep))}]

   if {[string compare "ave" $method_order]==0} {
      ResampleAverage $changeid $outid $xmin $ymin $zmin \
         $xmax $ymax $zmax $icount $jcount $kcount
   } else {
      Resample $changeid $outid $xmin $ymin $zmin $xmax $ymax $zmax \
         $icount $jcount $kcount $method_order
   }
}

# Routine to extract numbers embedded in a text string.  Return value is
# the list of extracted numbers.  (This code cribbed from avf2odt, q.v.)
proc ExtractNumbers { x } {
   set pat1 {-?[0-9]+\.?[0-9]*((e|E)(-|\+)?[0-9]+)?}
   set pat2 {-?[0-9]*\.?[0-9]+((e|E)(-|\+)?[0-9]+)?}
   set nlst {}
   while {[regexp -- "($pat1|$pat2)(.*)" $x dummy n a2 a3 a4 a5 a6 a7 x]} {
      set neg 0
      if {[string match -* $n]} {
         set neg 1
         set n [string range $n 1 end]
      }
      # Trim leading zeros.  Is this necessary if there is a
      # leading minus sign?  If there is a leading minus sign
      # and then also unnecessary zeros, perhaps the minus sign
      # is actually a hyphen that should be removed from the
      # number?
      set n [string trimleft $n 0]
      if {[string match ".*" $n] || [string length $n]==0} {
         set n "0$n"  ;# Affix leading zero
      }
      if {$n!=0 && $neg} { set n "-$n" }
      lappend nlst $n
   }
   return $nlst
}


# Construct a value to put into the index column of the ODT output file.
# This is built using an expr command with numeric data derived from the
# file index number, filename, and description field in the input vector
# field file.  The index number is referenced in the expr command by the
# variable $i.  The first number embedded in the filename is $f1, the
# second is $f2, etc.  Similarly, numbers embedded in the file
# description field are referenced as $d1, $d2, ...  The variable $f is
# the same as $f1 if the latter is defined, 0 otherwise.  Likewise $d is
# an alias for $d1, or 0 if there are no numbers found in the
# description field.  (This code cribbed from avf2odt, q.v.)
proc ConstructIndexValue { valexpr fileno filename description } {
   set nameseq [ExtractNumbers $filename]
   set descseq [ExtractNumbers $description]

   # Substitution variables i, f, and d are always defined
   set i $fileno
   if {[llength $nameseq]>0} {
      set f [lindex $nameseq 0]
   } else {
      set f 0
   }
   if {[llength $descseq]>0} {
      set d [lindex $descseq 0]
   } else {
      set d 0
   }

   # Define indexed filename variables: f1, f2, ...
   set k 0
   foreach number $nameseq {
      incr k
      set f$k $number
   }

   # Define indexed description variables: d1, d2, ...
   set k 0
   foreach number $descseq {
      incr k
      set d$k $number
   }

   # Build index value
   if {[catch {expr $valexpr} result]} {
      set msg "Bad index expression on input file \"$filename\": "
      append msg $result
      return -code error  $msg
   }
   return $result
}

# proc wrapper for WriteMeshAverages.  Using a proc makes variable
# accesses inside WriteMeshAverages local references, which is
# slightly faster.  Any support values, such as pi or mu0, should
# be defined inside this proc before the WriteMeshAverages call
proc WriteOdtData { outchan arrname } {
   set pi [expr 4*atan(1.0)]
   set mu0 [expr 4*$pi*1e-7]
   upvar $arrname arr
   WriteMeshAverages $outchan arr
}


# Load subtractor
set active_mesh 1
SelectActiveMesh $active_mesh
ChangeMesh $subfile 0 0 0 -1
if {[llength $clipstr]} {
   CopyMesh $active_mesh $active_mesh 0 "x:y:z" $clipstr 1
}
if {$info} {
   set namlen [string length $subfile]
   foreach infile $filelist {
      set len [string length $infile]
      if {$len>$namlen} { set namlen $len}
   }
   foreach {minval maxval} [GetMeshValueMagSpan] { break }
   set L1 [GetMeshValueL1]
   set rms [GetMeshValueRMS]
   set minval [ResetInf $minval] ; set maxval [ResetInf $maxval]
   set L1     [ResetInf $L1]
   set rms    [ResetInf $rms]
   set units [GetMeshValueUnit]
   set tabset [expr {$namlen + [string length " (in $units)"]}]
   if {[catch {format "%*s (in %s): \
            Min mag = $numfmt\n%*s   Max mag = $numfmt,\
            L1 = $numfmt,      RMS = $numfmt" \
            $namlen $subfile $units $minval $tabset " " $maxval $L1 $rms} msg]} {
      error "$msg\nVALUES: subfile=$subfile\n         \
                             units=$units\n        \
                            minval=$minval\n        \
                            maxval=$maxval\n            \
                                L1=$L1\n           \
                               rms=$rms"
   }
   puts $msg
}

set active_mesh 0
SelectActiveMesh $active_mesh
set passcount -1
foreach infile $filelist {
   incr passcount
   if {[catch {ChangeMesh $infile 0 0 0 -1} errmsg]} {
      error "ERROR loading file \"$infile\": $errmsg"
   }
   if {[llength $clipstr]} {
      CopyMesh $active_mesh $active_mesh 0 "x:y:z" $clipstr 1
   }
   set baseid 1
   if {[catch {
      switch -exact -- $resample {
         0  {
            if {![AreMeshesCompatible 0 1]} {
               # Change file0 to match filen
               MakeCompatibleMesh 0 1 2 $resample_order
               set baseid 2
            }
         }
         n  {
            if {![AreMeshesCompatible 0 1]} {
               # Change filen to match file0
               MakeCompatibleMesh 1 0 0 $resample_order
            }
         }
         default {}
      }
   } errmsg]} {
      error "ERROR resampling file \"$infile\": $errmsg"
   }
   if {$cross} {
      set errcode [catch {CrossProductMesh $baseid} msg]
   } else {
      set errcode [catch {DifferenceMesh $baseid} msg]
   }
   if {$errcode} {
      puts "Skipping file $infile: $msg"
   } else {
      if {$info} {
         foreach {minval maxval} [GetMeshValueMagSpan] { break }
         set L1 [GetMeshValueL1]
         set rms [GetMeshValueRMS]
         set minval [ResetInf $minval] ; set maxval [ResetInf $maxval]
         set L1     [ResetInf $L1]
         set rms    [ResetInf $rms]
         set units [GetMeshValueUnit]
         puts [format "%*s (in %s): Max diff = $numfmt,\
                       L1 = $numfmt, RMS diff = $numfmt" \
                  $namlen $infile $units $maxval $L1 $rms $units]
      } elseif {$odt} {
         if {$odtmode == 0} {
            set filecount [llength $filelist]
            if {$filecount<2} {
               set descript "Data from vector field file"
            } else {
               set descript "Data from $filecount vector field files:"
            }
            set names [lrange $filelist 0 2]
            if {$filecount>3} {
               lappend names "..."
            }
            foreach n $names {
               set xend $descript
               regsub -- "^.*\n" $xend {} xend
               if {[string length $xend] + [string length $n] > 63} {
                  append descript "\n## Desc:   "
               }
               append descript " $n,"
            }
            set wma(descript) [string trimright $descript ","]
            set odtmode 1
         }
         if {$passcount == [llength $filelist]-1} {
            set wma(trailer) "tail"
         }
         set meshdesc [GetMeshDescription]
         set index_value [ConstructIndexValue $index_valexpr \
                                $passcount $infile $meshdesc]
         set wma(index) [lreplace $wma(index) 2 2 $index_value]
         WriteOdtData stdout wma
         set wma(header) "nohead"
      } else {
         if {$cross} {
            set outfile "${infile}-cross"
            set descr "Cross product file: [list $infile] x [list $subfile]"
         } else {
            set outfile "${infile}-diff"
            set descr "Difference file: [list $infile] - [list $subfile]"
         }
         set fmt "binary8"
         # set fmt "text"
         if { [IsRectangularMesh] } {
            set struct "rectangular"
         } else {
             set struct "irregular"
         }
         set title $outfile
         puts "Writing output file: $outfile"
         WriteMesh $outfile $fmt $struct $title $descr
      }
   }
}

exit 0
