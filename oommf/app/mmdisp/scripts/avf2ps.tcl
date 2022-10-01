# FILE: avf2ps.tcl
#
# This file must be evaluated by mmDispSh

package require Oc 2		;# [Oc_OpenUniqueFile]
package require Nb 2
package require Mmdispcmds 2
Oc_ForceStderrDefaultMessage
catch {wm withdraw .}

Oc_Main SetAppName avf2ps
Oc_Main SetVersion 2.0b0

Oc_CommandLine Option console {} {}

Oc_CommandLine Option v {
        {level {regexp {^[0-9]+$} $level} {is a decimal integer}}
    } {
        global verbosity;  scan $level %d verbosity
} {Verbosity level (default = 1)}
set verbosity 1

Oc_CommandLine Option f {} {
        global forceOutput; set forceOutput 1
} {Force overwrite of output files}
set forceOutput 0

# Build list of configuration files.  Note: The local/avf2ps.config
# file, if it exists, is automatically source by the main avf2ps.config
# file.
set configFiles [list [file join [file dirname \
        [Oc_DirectPathname [info script]]] avf2ps.config]]
Oc_CommandLine Option config {file} {
        global configFiles; lappend configFiles $file
} "Append file to list of configuration files"

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
set opatexp {(\.[^.]?[^.]?[^.]?(.gz)?$|$)}

Oc_CommandLine Option opatsub {sub} {
        global opatsub;  set opatsub $sub
} {Replacement in input filename to get output filename}

Oc_CommandLine Option filter {
        {pipeline {} {is an output pipeline}}
    } {
        global filter;  set filter $pipeline
} {Post-processing programs to apply to output}

Oc_CommandLine Option [Oc_CommandLine Switch] {
        {{file list} {} {Input vector field file(s)}}
    } {
        global infile; set infile $file
} {End of options; next argument is file}
Oc_CommandLine Parse $argv

# Get the configuration settings from config file list
foreach c $configFiles {
    source $c
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

# Set default output filename substitution
if {![info exists opatsub]} {
    set opatsub .eps
}

if {$verbosity >= 3} {
    puts stderr "##############"
    puts stderr "Input  files: $infile $inGlob"
    puts stderr "##############"
}
if {$verbosity >= 2} {
    puts stderr "ipat: $inputPattern"
    puts stderr "opatexp: $opatexp"
    puts stderr "opatsub: $opatsub"
    if {[info exists filter]} {
        puts stderr "Filter command: $filter"
    }
    set centerstr {}
    if {[info exists plot_config(misc,relcenterpt)]} {
	append centerstr "\n Relative center pt:"
	append centerstr " $plot_config(misc,relcenterpt)"
    }
    if {[info exists plot_config(misc,centerpt)]} {
	append centerstr "\n Absolute center pt:"
	append centerstr " $plot_config(misc,centerpt)"
    }
    puts stderr "Arrow Configuration---
 Display: $plot_config(arrow,status)
 Colormap name: $plot_config(arrow,colormap)
 Colormap size: $plot_config(arrow,colorcount)
 Quantity: $plot_config(arrow,quantity)
 Color phase: $plot_config(arrow,colorphase)
 Color reverse: $plot_config(arrow,colorreverse)
 Autosample: $plot_config(arrow,autosample)
 Subsample: $plot_config(arrow,subsample)
 Size: $plot_config(arrow,size)
 View axis size adj: $plot_config(arrow,viewscale)
 Antialias: $plot_config(arrow,antialias)"
    puts stderr "Pixel Configuration---
 Display: $plot_config(pixel,status)
 Colormap name: $plot_config(pixel,colormap)
 Colormap size: $plot_config(pixel,colorcount)
 Opaque: $plot_config(pixel,opaque)
 Quantity: $plot_config(pixel,quantity)
 Color phase: $plot_config(pixel,colorphase)
 Color reverse: $plot_config(pixel,colorreverse)
 Autosample: $plot_config(pixel,autosample)
 Subsample: $plot_config(pixel,subsample)
 Size: $plot_config(pixel,size)"
    puts stderr "Misc Configuration----
 Background color: $plot_config(misc,background)
 Draw boundary: $plot_config(misc,drawboundary)
 Margin: $plot_config(misc,margin)
 Dimensions: $plot_config(misc,width) x $plot_config(misc,height)
 Zoom: $plot_config(misc,zoom)
 Crop: $plot_config(misc,crop)
 CropToView: $print_config(croptoview)
 Rotation: $plot_config(misc,rotation)
 Datascale: $plot_config(misc,datascale)$centerstr"
    set axis [string index $plot_config(viewaxis) end]
    set centerstr {}
    if {[info exists plot_config(viewaxis,center)]} {
	append centerstr "\n    Axis center:"
	append centerstr " $plot_config(viewaxis,center)"
    }
    puts stderr "View axis Configuration----
 View axis: $plot_config(viewaxis)$centerstr
    Arrow  span: $plot_config(viewaxis,${axis}arrowspan)
    Pixel  span: $plot_config(viewaxis,${axis}pixelspan)"
}

########################################################################
### Support variables and procs ########################################

# Initialize view_transform array, which is used for out-of-plane
# rotations.
InitViewTransform

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

proc ReadFile { filename } {
    global plot_config print_config view_transform verbosity

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
    set viewaxis $plot_config(viewaxis)
    if {[string length $viewaxis] == 1} {
        set viewaxis "+$viewaxis"
        set plot_config(viewaxis) $viewaxis
    }
    set rot $plot_config(misc,rotation)
    set rot [expr {(round($rot/90.) % 4)*90}]
    set width $plot_config(misc,width)
    set height $plot_config(misc,height)
    set zoom $plot_config(misc,zoom)
    ChangeMesh $workname $width $height $rot $zoom

    if {$verbosity >= 1} {
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

    # Adjust data scaling
    SetDataValueScaling $plot_config(misc,datascale)

    # Mesh loads in with +z forward, so transform
    # mesh to show desired axis
    CopyMesh 0 0 0 $view_transform(+z,$viewaxis)
    SetZoom $zoom
    SelectActiveMesh 0
}

########################################################################

# Loop through input files
foreach in [concat $inGlob $infile] {
    if {[regsub -- $opatexp $in $opatsub out] != 1} {
        set out $in$opatsub
    }
    if {!$forceOutput} {
	set next 0
        foreach {outChan out next} \
                [Oc_OpenUniqueFile -start $next -pfx $out -sep1 -] break
    } else {
        set outChan [open $out w]
    }
    fconfigure $outChan -buffersize 10000
    if {[info exists filter]} {
        # Note: $filter might itself be a pipeline, or a command
        # with arguments.  It might also include a path to a filter program
        # that contains spaces, which will confuse [open] unless it is
        # structured in proper Tcl list syntax.
        set endChan $outChan
        
	if {[string compare windows $tcl_platform(platform)]!=0} {
            # On non-windows platforms, we expect the command line to be
            # quoted so that the value of $filter can be passed to [open]
            # as is.
            
            set cmdLine [concat 2>@stderr $filter [list >@ $endChan]]
            
        } else { 
            # On Windows, the facilities of quoting on the command line
            # are primitive, the use of backslashes in paths is a mismatch
            # to Tcl syntax, and spaces in paths are far more common.  We
            # attempt some probing and processing to try to work around these
            # limitations. It is unlikely this is 100% foolproof.
            
            foreach cmd [split $filter |] {
                set parts [split [string trim $cmd] { }]
                set numParts [llength $parts] 
                for {set pgmEnd 0} {$pgmEnd < $numParts} {incr pgmEnd} {
                    set program [join [lrange $parts 0 $pgmEnd] { }]
                    set toExec [auto_execok $program]
                    if {[llength $toExec]} {
                        break
                    }
                }
                if {$pgmEnd < $numParts} {
                    lappend cmds [concat $toExec [lrange $parts 1+$pgmEnd end]]
                } else {
		    lappend cmds $cmd
		}
            }
            set cmdLine [concat 2>@stderr [join $cmds { | }] [list >@ $endChan]]
        }
        set outChan [open |$cmdLine w]
        fconfigure $outChan -buffersize 10000
    }
    if {$verbosity >= 1} {
        puts -nonewline stderr "$in --> "
        if {[info exists filter]} {
            puts -nonewline stderr "$filter --> "
        }
        puts stderr $out
    }

    # Read input file into frame
    ReadFile $in

    # Dump PostScript
    PSWriteMesh $outChan

    if {[catch {close $outChan} msg]} {
        return -code error $msg
    }
    if {[info exists filter]} {
       if {[catch {close $endChan} msg]} {
          return -code error $msg
       }
    }
}

exit 0
