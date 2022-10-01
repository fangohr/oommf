# FILE: avf2odt.tcl
#
# This file must be evaluated by mmDispSh
#
# Base WriteMeshAverages config array.  Check function notes
# in mmdispcmds.cc for definitive information.
#   average   -- one of (space|plane|line|point|ball)
#   axis      -- one of (x|y|z)
#   ball_radius -- floating point value denoting radius
#                 of averaging ball, in problem coordinates.
#                 Required iff average is "ball".
#   range     -- 6-tuple xmin ymin zmin xmax ymax zmax (prob. coords.)
#   rrange    -- 6-tuple, xmin ymin zmin xmax ymax zmax,
#                 each in range [0,1] (relative coordinates)
#   normalize -- 1 or 0.  If 1, then each output point is
#                 divided by the maximum magnitude that would occur
#                 if all vectors in the manifold are aligned.  Thus
#                 the magnitude of the output vectors are all <=1.
#   header    -- one of (fullhead|shorthead|nohead)
#   trailer   -- one of (tail|notail)
#   numfmt    -- Format for numeric output
#   descript  -- description string
#   index     -- 3-tuple: label units value
#   vallab    -- Value label.  Default is "M".
#   valfuncs  -- List of triplets.  Each triplet is
#                    label unit expr-expression
#                where "label" and "unit" are headers for a column
#                in the output data table.  "expr-expression" is a
#                Tcl expr expression which is applied point-by-point
#                on the input file before any averaging is done.
#                Available variables are x, y, z, vx, vy, vz.  A
#                couple of examples are
#                    Ms A/m {sqrt($vx*$vx+$vy*$vy+$vz*$vz)}
#                    M110 A/m {($vx+$vy)*0.70710678}
#   defaultvals -- 1 or 0.  If 1, the vx, vy, and vz are included
#                automatically in output table.  If 0, then only
#                columns specified by the "functions" option are
#                output.
#
#   defaultpos  -- 1 or 0.  If 1, the x, y, and/or z point coordinate
#                values (as appropriate for averaging type) are
#                automatically included in output table.  If 0, then
#                these values are not automatically output, but may be
#                included as desired through the valfuncs functionality.
#
# The active volume is set from range, if set.  Otherwise,
# rrange is used.  If neither is set, the default is the
# entire mesh volume.

package require Oc 2
package require Nb 2
package require Mmdispcmds 2
Oc_ForceStderrDefaultMessage
catch {wm withdraw .}

Oc_Main SetAppName avf2odt
Oc_Main SetVersion 2.0b0

Oc_CommandLine Option console {} {}

Oc_CommandLine Option v {
  {level {regexp {^[0-9]+$} $level} {is a decimal integer (default = 1)}} 
} {
  global verbosity;  scan $level %d verbosity
} {Verbosity level}
set verbosity 1

Oc_CommandLine Option average {
    {manifold {regexp {^(space|plane|line|point|ball)$} $manifold}
    {is space, plane, line, point or ball} }
} { global wma; set wma(average) $manifold } {Averaging type}
set wma(average) "space"

Oc_CommandLine Option ball_radius {
    {brad {regexp {^[-+.e0-9]+} $brad} {is ball radius (ball manifold only)} }
} { global wma; set wma(ball_radius) $brad } {Averaging ball radius}

Oc_CommandLine Option axis {
    {spec {regexp {^[xyz]$} $spec} {is one of {x,y,z}} }
} { global wma; set wma(axis) $spec } {Axis averaging specification}
set wma(axis) "x"

Oc_CommandLine Option region {
    {xmin {regexp {^[-+.e0-9]+$} $xmin} {}}
    {ymin {regexp {^[-+.e0-9]+$} $ymin} {}}
    {zmin {regexp {^[-+.e0-9]+$} $zmin} {}}
    {xmax {regexp {^[-+.e0-9]+$} $xmax} {}}
    {ymax {regexp {^[-+.e0-9]+$} $ymax} {}}
    {zmax {regexp {^[-+.e0-9]+$} $zmax} {}}
} { global wma; set wma(range) [list $xmin $ymin $zmin $xmax $ymax $zmax]
} {  Volume select}

Oc_CommandLine Option rregion {
    {xmin {regexp {^[-+.e0-9]+$} $xmin} {}}
    {ymin {regexp {^[-+.e0-9]+$} $ymin} {}}
    {zmin {regexp {^[-+.e0-9]+$} $zmin} {}}
    {xmax {regexp {^[-+.e0-9]+$} $xmax} {}}
    {ymax {regexp {^[-+.e0-9]+$} $ymax} {}}
    {zmax {regexp {^[-+.e0-9]+$} $zmax} {}}
} { global wma; set wma(rrange) [list $xmin $ymin $zmin $xmax $ymax $zmax]
} {  Relative volume select}

Oc_CommandLine Option index {
    {label {} {is column header}}
    {units {} {is units header}}
    {valexpr {} {is expr cmd with $i, $f1, $d1, ...}}
} { global wma index_valexpr
    lappend wma(index) $label $units {}
    lappend index_valexpr $valexpr
} {  Add file-based index column} multi

set wma(valfuncs) {}
Oc_CommandLine Option valfunc {
    {label {} {is column header}}
    {units {} {is units header}}
    {fcnexpr {} {is expr with $x, $vx, ..., $r, $vmag}}
} { global wma
    lappend wma(valfuncs) $label $units $fcnexpr
} {  Add ptwise functional output column} multi

set wma(defaultpos) 1
Oc_CommandLine Option defaultpos {
    {flag {expr {![catch {expr {$flag && $flag}}]}} "= 0 or 1 (default)"}
   } { global wma; set wma(defaultpos) $flag
} {Include (1) or exclude (0) default point positions}

set wma(defaultvals) 1
Oc_CommandLine Option defaultvals {
    {flag {expr {![catch {expr {$flag && $flag}}]}} "= 0 or 1 (default)"}
   } { global wma; set wma(defaultvals) $flag
} {Include (1) or exclude (0) default output values}

set wma(extravals) 0
Oc_CommandLine Option extravals {
    {flag {expr {![catch {expr {$flag && $flag}}]}} "= 0 (default) or 1"}
   } { global wma; set wma(extravals) $flag
} {Include (1) or exclude (0) extra output values (L1, L2, abs min and max)}

set inputPattern [list]
Oc_CommandLine Option ipat {
	{pattern {} {is in glob-style}}
    } {
	global inputPattern tcl_platform
	if {[string compare windows $tcl_platform(platform)]==0} {
	    # Convert to canonical form, replacing backslashes with
	    # forward slashes, so pattern can be fed to glob.
	    if {![catch {file join $pattern} xpat]} {
		set pattern $xpat
	    }
	}
	lappend inputPattern $pattern
} {Pattern specifying input files} multi

Oc_CommandLine Option opatexp {
	{regexp {} {is a regular expression}}
    } {
	global opatexp;  set opatexp $regexp
} {Pattern: part of input filename to replace}
set opatexp {(\.[^.]?[^.]?[^.]?$|$)}

Oc_CommandLine Option opatsub {sub} {
	global opatsub;  set opatsub $sub
} {Replacement in input filename to get output name} 
set opatsub .odt

set filesort none
Oc_CommandLine Option filesort {
    {method {
	expr [string match none $method] || ![catch "lsort $method {}"]
} {is lsort option string or "none"}}
} { global filesort; set filesort $method } {Input file list sort order}

Oc_CommandLine Option onefile {
    {outfile {} {is output filename or "-" for stdout}}
} {global onefile onefileFlag;  set onefile $outfile; set onefileFlag 1
} {Single output file} 
set onefileFlag 0

Oc_CommandLine Option truncate {
    {flag {expr {![catch {expr {$flag && $flag}}]}} "= 0 (default) or 1"}
   } {
       global truncateFlag; set truncateFlag $flag
} {Append to (0) or truncate (1) output file}
set truncateFlag 0

Oc_CommandLine Option headers {
    {style {regexp {^(full|collapse|none)$} $style}
    {is one of {full,collapse,none}} }
} { global headers; set headers $style } {Header style}
set headers full

Oc_CommandLine Option normalize {
    {flag {expr {![catch {expr {$flag && $flag}}]}} "= 0 (default) or 1"}
} {
    global wma; set wma(normalize) $flag
} {Output in file units (0) or normalized (1)}
set wma(normalize) 0

Oc_CommandLine Option numfmt {
    {fmt {expr {![catch {format $fmt 1.2}]}} "a C-style format string"}
} { global wma; set wma(numfmt) $fmt
} {Format for numeric output}

Oc_CommandLine Option [Oc_CommandLine Switch] {
	{{file list} {} {Input vector field file(s)}}
    } {
	global infile; set infile $file
} {End of options; next argument is file}
Oc_CommandLine Parse $argv

# If averaging type is ball, then ball_radius must be set
if {[string match "ball" $wma(average)]} {
   if {![info exists wma(ball_radius)]} {
      puts stderr "ERROR: -ball_radius must be set for averaging type ball"
      exit 1
   }
   if {$wma(ball_radius)<=0.0} {
      puts stderr "ERROR: ball_radius must be positive"
      exit 1
   }
}

# Turn -ipat pattern into list of input files
set inGlob [list]
if {[llength $inputPattern]>0} {
   foreach pat $inputPattern {
      set inGlob [concat $inGlob [lsort -dictionary [glob -nocomplain -- $pat]]]
   }
}

# On Windows, check infile list for and expand wildcards, to better
# agree with Unix behavior. Unix shells automatically expand wildcards,
# Windows does not. One concession to Windows: If a potential "pattern"
# matches a file directly (verbatim), then that match is used and
# wildcard matching is not attempted on that pattern. This protects
# unwary Windows users not familiar with glob matching patterns beyond ?
# and *.
if {[string match windows $tcl_platform(platform)]} {
   set wildfiles {}
   foreach f $infile {
      if {![file exists $f]} {
         # Convert any '\' to '/' before feeding to glob
         if {![catch {file join $f} xf]} {set f $xf}
         set wildfiles [concat $wildfiles \
                        [lsort -dictionary [glob -nocomplain -- $f]]]
      } else {
         lappend wildfiles $f
      }
   }
   set infile $wildfiles
}

# Form complete input file list
set infilelist [concat $inGlob $infile]
if {[llength $infilelist]<1} {
    puts stderr "ERROR: Empty input file list"
    exit 1
}

# Sort input file list
if {![string match none $filesort]} {
    set infilelist [eval lsort $filesort {$infilelist}]
}

set maxnamelength 1
foreach file $infilelist {
    set length [string length $file]
    if {$length>$maxnamelength} {set maxnamelength $length}
}

# Dump processing info, if requested
if {$verbosity >= 3} {
    puts stderr "Input  files: $infile $inGlob"
}
if {$verbosity >= 4} {
    puts stderr "ipat: $inputPattern"
    puts stderr "opatexp: $opatexp"
    puts stderr "opatsub: $opatsub"
}

########################################################################
### Support procs ######################################################
#
proc ConvertToEng { z sigfigs } {
    # Note: This routine differs from the similarly named routine
    #  in mmdisp.tcl in that trailing zeros in the mantissa are
    #  truncated.
    set sigfigs [expr int(round($sigfigs))]
    if {$sigfigs<1} { set sigfigs 1 }
    set z [format "%.*g" $sigfigs $z] ;# Remove extra precision

    if {$z==0} {
	return 0
    }
    set exp1 [expr {int(floor(log10(abs($z))))}]
    set exp2 [expr {int(round(3*floor($exp1/3.)))}]
    set prec [expr int(round($sigfigs-($exp1-$exp2+1)))]
    if { $prec<0 } { set prec 0 }
    set mantissa [format "%.*f" $prec [expr {$z*pow(10.,-1*$exp2)}]]
    regsub -- {0*$} $mantissa {} mantissa
    regsub -- {\.$} $mantissa {} mantissa
    if {$exp2==0} {return $mantissa}
    return [format "%se%d" $mantissa $exp2]
}

# Routine to extract numbers embedded in a text string.  Return value is
# the list of extracted numbers. (There is similar code in avfdiff.tcl.)
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
# description field.  (There is similar code in avfdiff.tcl.)
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

proc ReadFile { filename } {
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

    # Read file
    ChangeMesh $workname 0 0 0 -1
    
    global verbosity
    if {$verbosity >= 2} {
	foreach i [GetMeshRange] j {xmin ymin zmin xmax ymax zmax} {
	    set $j [ConvertToEng $i 6]
	}
	puts stderr \
	   "Bounding box: ($xmin,$ymin,$zmin) x ($xmax,$ymax,$zmax)"
    }

    # Delete tempfile, if any
    if { ![string match {} $tempfile] } {
	catch { file delete $tempfile }
    }
}

########################################################################

if {$truncateFlag} {
    set writeMode " (truncate)"
} else {
    set writeMode " (append)"
}

if {$onefileFlag} {
    set out $onefile
    if {[string match - $out]} {
	set outChan stdout
    } else {
	set outChan {}
    }
}

if {[string match none $headers]} {
    set wma(header)  "nohead"
    set wma(trailer) "notail"
} elseif {[string match collapse $headers] && $onefileFlag} {
    set wma(header)  "shorthead"
    set wma(trailer) "notail"
    ## In this case, headchoice and tailchoice get modified
    ## on the fly inside the input file loop.
} else {
    set wma(header)  "fullhead"
    set wma(trailer) "tail"
}

# proc wrapper for WriteMeshAverages.  Using a proc makes variable
# accesses inside WriteMeshAverages local references, which is
# slightly faster.  Any support values, such as pi or mu0, should
# be defined inside this proc before the WriteMeshAverages call
proc WriteData { outchan arrname } {
   set pi [expr 4*atan(1.0)]
   set mu0 [expr 4*$pi*1e-7]
   upvar $arrname arr
   WriteMeshAverages $outchan arr
}

# Loop through input files
set fileno 0
foreach in $infilelist {
    if {[catch {
	set wma(descript) "Data from vector field file $in"

	# Header adjustments for "-headers collapse" case
	if {$onefileFlag && [string match collapse $headers]} {
	    if {$fileno == 0} {
		set filecount [llength $infilelist]
		if {$filecount<2} {
		    set descript "Data from vector field file"
		} else {
		    set descript "Data from $filecount vector field files:"
		}
		set names [lrange $infilelist 0 2]
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
	    }
	    if {$fileno == 1} {
		set wma(header) "nohead"  ;# No headers after
		## first file is processed
	    }
	    if {$fileno == $filecount-1} {
		set wma(trailer) "tail"
		## Enable tail output on last file
	    }
	}

	if {!$onefileFlag} {
	    if {[regsub -- $opatexp $in $opatsub out] != 1} {
		set out $in$opatsub
	    }
	    if {[string match - $out]} {
		set outChan stdout
	    } else {
		set outChan {}
	    }
	}

	if {$verbosity >= 1} {
	    if {[string match - $out]} {
		puts stderr [format "%*s --> stdout" $maxnamelength $in]
	    } elseif {![file exists $out]} {
		puts stderr [format "%*s --> %s" $maxnamelength $in $out]
	    } else {
		if {$truncateFlag && [string match {} $outChan]} {
		    puts stderr [format "%*s --> %s (truncate)" \
			    $maxnamelength $in $out]
		} else {
		    puts stderr [format "%*s --> %s (append)" \
			    $maxnamelength $in $out]
		}
	    }
	}
	
	ReadFile $in

	if {[string match {} $outChan]} {
	    if {$truncateFlag} {
		set outChan [open $out w]
	    } else {
		set outChan [open $out a]
	    }
	}

        if {[info exists index_valexpr]} {
           set meshdesc [GetMeshDescription]
           set index_column 0
           foreach valexpr $index_valexpr {
              set index_value [ConstructIndexValue $valexpr \
                                  $fileno $in $meshdesc]
              set offset [expr {3*$index_column+2}]
              set wma(index) [lreplace $wma(index) \
                                 $offset $offset $index_value]
              incr index_column
           }
        }

        WriteData $outChan wma

	if {![string match - $out] && !$onefileFlag} {
	    close $outChan
	}

	incr fileno
    } errmsg]} {
	puts stderr "--- ERROR PROCESSING INPUT FILE \"$in\" ---"
	puts stderr [string trimright $errmsg]
	puts stderr "----"
	exit 2
    }
}

if {$onefileFlag && ![string match - $out]} {
    catch {close $outChan}
}

exit 0
