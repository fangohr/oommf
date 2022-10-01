# FILE: avf2ovf.tcl
#
# This file must be evaluated by mmDispSh

package require Oc 2        ;# Oc_TempName
package require Mmdispcmds 2
Oc_ForceStderrDefaultMessage
catch {wm withdraw .}

Oc_Main SetAppName avf2ovf
Oc_Main SetVersion 2.0b0

Oc_CommandLine Option console {} {}

proc FlipStringCheck { str } {
    set okaystr 0
    catch {
	if {[regexp {^[-+]?([xyz]):[-+]?([xyz]):[-+]?([xyz])$} \
		$str dummy c1 c2 c3] \
		&& ![string match $c1 $c2] \
		&& ![string match $c1 $c3] \
		&& ![string match $c2 $c3]} {
	    set okaystr 1  ;# Good flip string
	}
    }
    return $okaystr
}

proc ReadFile { filename } {
    if {[string match {} $filename]} {
        # Copy stdin to a temporary file
        set tempfile [Oc_TempName any2ovf]
        set outchan [open $tempfile "w"]
        fconfigure stdin -buffering full -translation binary
        fconfigure $outchan -buffering full -translation binary
        fcopy stdin $outchan
        close $outchan
        set workname $tempfile
    } else {
        if {![file exists $filename]} {
            return -code error "Input file \"$filename\" does not exist."
        }
        if {![file readable $filename]} {
            return -code error "Input file \"$filename\" is not readable."
        }

        # Apply filter (based on filename extension), if applicable.
        set tempfile [Nb_InputFilter FilterFile $filename decompress ovf]
        if { [string match {} $tempfile] } {
            # No filter match
            set workname $filename
        } else {
            # Filter match
            set workname $tempfile
        }
    }

    # Read file
    ChangeMesh $workname 0 0 0 -1
    
    # Delete tempfile, if any
    if { ![string match {} $tempfile] } {
	catch { file delete $tempfile }
    }

    if {[string match "Vf_EmptyMesh" [GetMeshType]]} {
	return -code error \
		"ERROR loading input file \"$filename\": Empty mesh"
    }

}

Oc_CommandLine Option flip {
    {flipstr { FlipStringCheck $flipstr } {is like z:-y:x (axes permutation)}}
} {
    upvar #0 flipstr globalflipstr
    regsub -all -- {[+]} $flipstr {} globalflipstr
} {Coordinate tranform}
set flipstr x:y:z

proc IsFloat { x } {
    if {[regexp -- {^-?[0-9]+\.?[0-9]*((e|E)(-|\+)?[0-9]+)?$} $x] || \
            [regexp -- {^-?[0-9]*\.?[0-9]+((e|E)(-|\+)?[0-9]+)?$} $x]} {
        if {![catch {expr $x}]} { return 1 }
    }
    return 0
}

proc IsPosFloat { x } {
   if {[IsFloat $x] && $x>0.0} {
      return 1
   }
   return 0
}

proc IsResampleType { x } {
   if {[string compare "ave" $x]==0
       || [regexp -- {^[0-9]+$} $x]} {
      return 1
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

Oc_CommandLine Option keepbb {} {
    global cliprange; set cliprange 0
} {Don't clip bounding box.}
set cliprange 1

# For backwards compatibility, offer both -format and -dataformat
# options.  The new (2020-08-31) preference is -dataformat.
Oc_CommandLine Option format {
   {format {regexp {text|b4|b8} $format} \
       {is one of {text, "text %fmt", b4, b8}}}
} {
   global dataformat; set dataformat $format
} {Data format of output file (deprecated; use -dataformat instead)}
Oc_CommandLine Option dataformat {
   {format {regexp {text|b4|b8} $format} \
       {is one of {text, "text %fmt", b4, b8}}}
} {
   global dataformat; set dataformat $format
} {Data format of output file: text, 4-, or 8-byte binary}
set dataformat {}

# For backwards compatibility, offer both -ovf and -fileformat options.
# The new (2020-08-31) preference is -fileformat.
Oc_CommandLine Option ovf {
   {version {regexp {1|2} $version} {is either 1 (default) or 2}}
} {
   global filetype fileversion ; set filetype ovf ; set fileversion $version
} {OVF version of output file (deprecated; use -fileformat instead)}

Oc_CommandLine Option fileformat {
   {type    { regexp {ovf|npy} $type } {is either ovf (default) or npy}}
   {version { regexp {1|2} $version } {is 1 (ovf, npy; default) or 2 (ovf)}}
} {
   global filetype fileversion ; set filetype $type ; set fileversion $version
} {File format type and version}
set filetype ovf
set fileversion {}

Oc_CommandLine Option grid {
   {type {regexp {^(rect|irreg|reg)} $type} {is one of {rect,irreg}}}
    } {
       global grid ; regexp -- {^(rect|irreg|reg)} $type grid
} {Grid type of output file: rectangular or irregular}
set grid {}

Oc_CommandLine Option info {} {global info; set info 1} \
   {Print mesh info (no file conversion)}
set info 0

Oc_CommandLine Option mag {} {
   global writemags; set writemags 1
} {Write mesh magnitudes instead of vectors (OVF 2 output only)}
set writemags 0

Oc_CommandLine Option q {} {global quiet; set quiet 1} {Quiet}
set quiet 0

Oc_CommandLine Option subsample {
        {period {regexp {^[0-9]+} $period} {is an integer value\
                                        (rectangular grids only)}}
     } {
       global subsample ; set subsample $period
} {Subsampling period (default is 1 = no subsampling)}
set subsample 1

Oc_CommandLine Option resample {
   {xstep { IsPosFloat $xstep } {is x-step}}
   {ystep { IsPosFloat $ystep } {is y-step}}
   {zstep { IsPosFloat $zstep } {is z-step}}
   {order { IsResampleType $order } {is interpolation order (0, 1, 3 or ave)}}
} {
   global resample
   set resample [list $xstep $ystep $zstep $order]
} "\n\tResample grid"
set resample {}

Oc_CommandLine Option pertran {
    {xoff { IsFloat $xoff } {is x-offset}}
    {yoff { IsFloat $yoff } {is y-offset}}
    {zoff { IsFloat $zoff } {is z-offset}}
} {
    global pertran
    set pertran [list $xoff $yoff $zoff]
} "\n\tPeriodic translation (rounds to nearest cell)"
set pertran [list 0.0 0.0 0.0]

Oc_CommandLine Option rpertran {
    {rxoff { IsFloat $rxoff } {is relative x-offset}}
    {ryoff { IsFloat $ryoff } {is relative y-offset}}
    {rzoff { IsFloat $rzoff } {is relative z-offset}}
} {
    global rpertran
    set rpertran [list $rxoff $ryoff $rzoff]
} "\n\tRelative periodic translation (rounds to nearest cell)"
set rpertran [list 0.0 0.0 0.0]


Oc_CommandLine Option [Oc_CommandLine Switch] {
    {{infile optional}  {} {Input vector field file (default: stdin)}}
    {{outfile optional} {} {Output vector field file (default: stdout)}}
} {
    global inputfile outputfile
    set inputfile $infile
    set outputfile $outfile
} {End of options; next argument is input file}
set inputfile {}
set outputfile {}

Oc_CommandLine Parse $argv

if {[string compare "-" $inputfile]==0} { set inputfile {} }
if {[string compare "-" $outputfile]==0} { set outputfile {} }

# Output file format
if {$writemags} {
   if {[string match ovf $filetype] && [string match {} $fileversion]} {
      set fileversion 2
   }
   if {![string match ovf $filetype] || $fileversion<2} {
      puts stderr "ERROR: -mag output only accepted for OVF 2 format"
      exit 11
   }
}
if {[string match {} $fileversion]} {
   set fileversion 1  ;# Default for both ovf and npy
}
if {[string match npy $filetype] && ($fileversion != 1)} {
    puts stderr "ERROR: Only NPY version 1 is currently supported."
    exit 13
}

# Output data format
if {[string match {} $dataformat]} {
   if {[string match npy $filetype]} {
      set dataformat b8
   } else {
      set dataformat text
   }
}

if {[regexp {^text.+$} $dataformat] && \
       (![string match ovf $filetype] || $fileversion<2)} {
    puts stderr "ERROR: Data format string only accepted for OVF 2 format"
    exit 15
}
if {[regexp {^text.+$} $dataformat] && $writemags} {
    puts stderr "ERROR: Data format string not supported for magnitude output"
    exit 17
}

array set formatmap {
	text	text
	b4	binary4
	b8	binary8
}
array set gridmap {
	rect	rectangular
	irreg	irregular
	reg	rectangular
}
# Note: "reg" is recognized only for backwards compatibility

# Check clip string is sensible
if {![ClipStringCheck $clipstr]} {
    puts stderr "ERROR: Bad clip request"
    exit 10
}

# Read file into mesh
if {![string match {} $inputfile]} {
    set inputfile [Oc_DirectPathname $inputfile]
}
if {[catch {ReadFile $inputfile} errmsg]} {
    puts stderr $errmsg
    exit 20
}

set title [GetMeshTitle]
set desc [GetMeshDescription]

if {$info} {
    if {[string match {} $outputfile]} {
        set outchan stdout
    } else {
        set outchan [open $outputfile "a"]
    }
    if {[string match {} $inputfile]} {
        puts $outchan "File: <stdin>"
    } else {
        puts $outchan "File: $inputfile"
    }
    if {![catch {file size $inputfile} filesize]} {
	puts $outchan "File size: $filesize bytes"
    }
    puts $outchan "Mesh title: $title"
    if {[string first "\n" $desc]<0} {
	# Single line description
	puts $outchan "Mesh  desc: $desc"
    } else {
	# Mult-line description
	regsub -all "\n" $desc "\n: " desc
	puts $outchan "Mesh desc---\n: $desc"
    }
    puts $outchan [GetMeshStructureInfo]
    if {![string match {} $outputfile]} {
        close $outchan
    }
    exit 0
}


if {[string match {} $grid]} {
   # No grid request by user; match import mesh type
   if {[string match Vf_GeneralMesh3f [GetMeshType]]} {
      set grid irreg
   } else {
      set grid rect
   }
}

if {[string match Vf_GeneralMesh3f [GetMeshType]] &&
	![string match irregular $gridmap($grid)]} {
   puts stderr "ERROR: Input file has irregular grid; so must output."
   exit 25
}

# Combine translations, if any, into one absolute translation
foreach {tx ty tz} $pertran break
foreach {rx ry rz} $rpertran break
if {$rx!=0.0 || $ry!=0.0 || $rz!=0.0} {
   foreach {xmin ymin zmin xmax ymax zmax} [GetMeshRange] break
   set tx [expr {$tx + $rx*($xmax-$xmin)}]
   set ty [expr {$ty + $ry*($ymax-$ymin)}]
   set tz [expr {$tz + $rz*($zmax-$zmin)}]
}
unset rx ry rz

if {[string match Vf_GeneralMesh3f [GetMeshType]] &&
    ($tx!=0.0 || $ty!=0.0 || $tz!=0.0)} {
   puts stderr "ERROR: Periodic translations not supported on irregular grids"
   exit 30
}
    
if {!$quiet} {
    puts stderr "Copying Title field: $title\nYou may want to edit this\
	    field in the output by hand."
}

if {[llength $resample]==4} {
   foreach {xstep ystep zstep method_order} $resample break
   if {$xstep<=0 || $ystep<=0 || $zstep<0} {
      puts stderr "ERROR: resampling step sizes must be positive"
      exit 41
   }
   if {[string compare "ave" $method_order]!=0
       && $method_order!=0 && $method_order!=1 && $method_order!=3} {
      puts stderr "ERROR: resampling interpolation order must be 0, 1, 3 or ave"
      exit 43
   }
   if {[llength $clipstr]!=6} {
      set xmin [set ymin [set zmin -]]
      set xmax [set ymax [set zmax -]]
   } else {
      foreach {xmin ymin zmin xmax ymax zmax} $clipstr break
   }
   foreach {cxmin cymin czmin cxmax cymax czmax} [GetMeshRange] break

   if {[string compare "-" $xmin]==0} { set xmin $cxmin }
   if {[string compare "-" $ymin]==0} { set ymin $cymin }
   if {[string compare "-" $zmin]==0} { set zmin $czmin }
   if {[string compare "-" $xmax]==0} { set xmax $cxmax }
   if {[string compare "-" $ymax]==0} { set ymax $cymax }
   if {[string compare "-" $zmax]==0} { set zmax $czmax }

   set xcount [expr {($xmax-$xmin)/$xstep}]
   set ycount [expr {($ymax-$ymin)/$ystep}]
   set zcount [expr {($zmax-$zmin)/$zstep}]
   if {$xcount<0.9 || $ycount<0.9 || $zcount<0.9} {
      puts stderr "ERROR: resampling step sizes must be smaller than\
                   output mesh dimensions"
      puts stderr "$xcount $ycount $zcount"
      exit 45
   }
   set icount [expr {int(round($xcount))}]
   set jcount [expr {int(round($ycount))}]
   set kcount [expr {int(round($zcount))}]
   if {abs($xcount-$icount)>1e-8
       || abs($ycount-$jcount)>1e-8
       || abs($zcount-$kcount)>1e-8} {
      puts stderr \
        "ERROR: Resample step sizes must evenly divide output mesh dimensions."
      puts stderr [format " Try -resample %.9g %.9g %.9g" \
                      [expr {($xmax-$xmin)/$icount}] \
                      [expr {($ymax-$ymin)/$jcount}] \
                      [expr {($zmax-$zmin)/$kcount}]]
      exit 47
   }
   if {[string compare "ave" $method_order]==0} {
      ResampleAverage 0 0 $xmin $ymin $zmin $xmax $ymax $zmax \
         $icount $jcount $kcount
   } else {
      Resample 0 0 $xmin $ymin $zmin $xmax $ymax $zmax \
         $icount $jcount $kcount $method_order
   }
}

if {![string match "x:y:z" $flipstr] || ![string match {} $clipstr]
    || ($subsample != 1 && $subsample != 0)} {
    # Perform coordinate transform, clipping, and/or subsampling
    CopyMesh 0 0 $subsample $flipstr $clipstr $cliprange
}

if {$tx!=0.0 || $ty!=0.0 || $tz!=0.0} {
   # Perform periodic translation
   PeriodicTranslate 0 0 $tx $ty $tz
}

# $filename == "" --> write to stdout
if {!$writemags} {
   if {[string match npy $filetype]} {
      WriteMeshNPY $outputfile $formatmap($dataformat) $gridmap($grid)
   } elseif { $fileversion == 1 } { ;# OVF output
      WriteMesh $outputfile $formatmap($dataformat) $gridmap($grid) $title $desc
   } else {
      # OVF2 supports user-defined data format specification in text mode
      if {[regexp "^text\[ \t\r\n\]*(.*)$" $dataformat dummy fmtstr]} {
         set fmtspec "$formatmap(text) $fmtstr"
      } else {
         set fmtspec $formatmap($dataformat)
      }
      WriteMeshOVF2 $outputfile $fmtspec $gridmap($grid) \
         $title $desc
   }
} else {
   WriteMeshMagnitudes $outputfile $formatmap($dataformat) $gridmap($grid) \
      $title $desc
}

exit 0
