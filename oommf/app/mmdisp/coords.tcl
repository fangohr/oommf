# FILE: coords.tcl
#
# Routines to draw coordinate axes.
#
# These routines could be wrapped up in an Oc_Class, but were originally
# written before the existence of Oc or em classes.
#
# Rendering of text is can be slow if a font server across a network
# is accessed, so rather than redraw the coordinate axes on each rotation,
# we draw four sets of axis for each view direction during initialization,
# and store them in different sections of the canvas.  The canvas is
# "scrollregion"'ed sized to 8x12 times the canvas width/height.  On
# receipt of a show coords request, we just scroll the canvas to display
# the appropriate axes.
#
# The coordinates axes are positioned in the canvas as a whole, as
# illustrated below (I=0, II=90, III=180, IV=270):
#
#  additional
#  view directions
#   ^    ^
#   |    |--------------------------------------------------
#   |    |     |     |       |     |     |     |     |
#        |     |     |       |     |     |     |     |
#        |     |     |       |     |     |     |     |
#        |--------------------------------------------------
#        |     |     |       |     |     |     |     |
#  +y => |  I  |     |  II   |     | III |     | IV  |
#        |     |     |       |     |     |     |     |
#        |--------------------------------------------------
#        |     |     |       |     |     |     |     |
#        |     |     |       |     |     |     |     |
#        |     |     |       |     |     |     |     |
#        |--------------------------------------------------
#        |     |     |       |     |     |     |     |
#  +z => |  I  |     |  II   |     | III |     | IV  |
#        |     |     |       |     |     |     |     |
#
#  The view direction ordering, from the bottom up, is
#
#                      +z  +y  +x  -x  -y  -z
#
# NOTE 1: This routine gets the coords display size from the canvas
#   -height value.  This *must* be set before calling this routine.
#
#   Each element in the coordinate canvas is tagged with one of the 6
# identifiers xp, xm, yp, ym, zp, zm, for "plus x-axis," "minus x-axis",
# etc.  Theses tags are used to color the coordinates inside the
# coloring proc SetCoordColors.
proc InitCoords { canvas } {
    $canvas delete all  ;# In case this isn't the first call

    # Check for square canvas viewport
    set size [$canvas cget -height]
    set width [$canvas cget -width]
    if { $size<1 } {
        set size $width
        $canvas configure -height $size
    } elseif {$width != $size} { 
        $canvas configure -width $size
    }

    # Set up size of whole canvas
    set scrollx [expr {8*$size}]
    set scrolly [expr {12*$size}]
    $canvas configure \
            -scrollregion "0 0 $scrollx $scrolly" \
            -xscrollincrement $size -yscrollincrement $size \
            -confine 1

    # Draw each axis configuration.  For better user-apparent
    # responsiveness, draw & show the first angle now, and
    # put the other drawing requests on the event stack.
    set viewcount 0
    foreach outaxis [list +z +y +x -x -y -z] {
	# Determine labels for in-plane axes
	switch -glob -- $outaxis {
	    {?z} { set axis1 {+x} ; set axis2 {+y} }
	    {?y} { set axis1 {+z} ; set axis2 {+x} }
	    {?x} { set axis1 {+y} ; set axis2 {+z} }
	}
	set rot_offset 0
	if {[string match {-?} $outaxis]} {
	    set temp $axis1 ; set axis1 $axis2 ; set axis2 $temp
	    set rot_offset 2 ;# Base in-plane position for orientations
	    ## with "-" out-of-plane axis are 180 degrees from "+"
	    ## orientations.
	}
	for { set rot 0 } { $rot < 4 } { incr rot } {
	    set xc [expr {$size*(2*$rot+0.5)}]
	    set yc [expr {$size*(2*$viewcount+0.5)}]
	    set angle [expr {90*(($rot+$rot_offset)%4)}]
	    if { $angle == 0 && [string match {+z} $outaxis] } {
		DrawCoords0 $canvas $size $xc $yc $outaxis $axis1 $axis2
		ShowCoords $canvas 0 $outaxis
	    } else {
		after idle DrawCoords$angle \
			$canvas $size $xc $yc $outaxis $axis1 $axis2
	    }
	}
	incr viewcount
    }
}

# Translate canvas in viewport to show corresponding coordinate axes.
proc ShowCoords { canvas angle outaxis } {
    set rot [expr {round($angle/90.)%4}]
    set xfrac [expr $rot*0.25]
    switch -exact -- $outaxis {
	+z { set yfrac 0 }
	+y { set yfrac 1 }
	+x { set yfrac 2 }
	-x { set yfrac 3 }
	-y { set yfrac 4 }
	-z { set yfrac 5 }
    }
    set yfrac [expr {$yfrac/6.}]
    $canvas xview moveto $xfrac
    $canvas yview moveto $yfrac
}

#
# Draws a 3D coordinate axes with orientation 'angle' (should be one of
# 0, 90, 180 or 270) on 'canvas', inside a square of edge length 'size',
# centered at (xc,yc).  Coordinate colors should be set by a separate
# call to SetCoordColors.
#
# Note 1: It is the responsibility of the calling routine to erase
#         'canvas' *before* calling this routine.
#
# Note 2: These routines have been optimized for size==50.
#
label .temporary_label
set _mmdisp(label_font) [.temporary_label cget -font]
destroy .temporary_label

proc DrawCoordsSetVars { size arrbasename arrsizename deltaname} {
    # Set variables used in DrawCoords* routines
    upvar $arrbasename arrbase
    upvar $arrsizename arrsize
    upvar $deltaname   delta

    set arrbase 5
    set arrsize [list $arrbase $arrbase [expr {floor($arrbase/2.)}]]
    set delta [expr {floor(($size-$arrbase)/2.)}]
}

# Coord axis draw routines.
# Note: These routines were originally written for axis="+z" only.
#  This accident of history is reflected in the variable names used
#  inside the routines.
proc DrawOutOfPlaneAxis { canvas size xc yc delta \
	arrbase arrsize axis font } {
    # Import axis should be [+-][xyz], e.g., "+z" or "-y".
    set outlabel $axis
    if {[string match {+?} $axis]} {
        # Axis string has form "+z"
        regsub -- {\+} $axis {-} inlabel        ;# Looks like "-z"
    } else {
        # Axis string has form "-z"
        regsub -- {\-} $axis {+} inlabel        ;# Looks like "+z"
    }
    set outtag "zp"
    set intag "zm"

    # Out-of-plane axis
    set diag [expr {floor($delta/1.414)}]
    set x1 [expr {$xc-$diag}];  set y1 [expr {$yc+$diag}]
    $canvas create line $xc $yc $x1 $y1 \
        -arrow last -arrowshape $arrsize -tags $outtag
    $canvas create text [expr {$xc-$size/2.+2}] [expr {$y1-3}] \
            -anchor nw -text $outlabel -font $font \
	    -tags [list text $outtag]
    # In-to-plane axis
    set x1 [expr {$xc+$diag}];  set y1 [expr {$yc-$diag}]
    set step [expr {round((($delta-$arrbase)/1.414)/4.)}]
    set xi [expr {$xc}];  set yi [expr {$yc}]
    set xj [expr {$xi+$step+1}];  set yj [expr {$yi-$step-1}]
    $canvas create line $xi $yi $xj $yj -arrow none -tags $intag
    set xi [expr {$xj+round($step/2.)}]
    set yi [expr {$yj-round($step/2.)}]
    set xj [expr {$xi+$step}];  set yj [expr {$yi-$step}]
    $canvas create line $xi $yi $xj $yj -arrow none -tags $intag
    set xi [expr {$xj+round($step/2.)}]
    set yi [expr {$yj-round($step/2.)}]
    set xj $x1;  set yj $y1
    $canvas create line $xi $yi $xj $yj \
            -arrow last -arrowshape $arrsize -tags $intag
    $canvas create text [expr {$xc+$size/2.-1}] [expr {$yj+4}] \
            -anchor se -text $inlabel -font $font \
	    -tags [list text $intag]
}

proc DrawCoords0 { canvas size xc yc outaxis axis1 axis2 } {
    DrawCoordsSetVars $size arrbase arrsize delta
    set font [Oc_Font Get small_bold]
    # Plane normal
    DrawOutOfPlaneAxis $canvas $size $xc $yc $delta \
            $arrbase $arrsize $outaxis $font

    # Set up in-plane axis tags
    if {[string match {-?} $outaxis]} {
	set tagAsuf "m" ; set tagBsuf "p"
    } else {
	set tagAsuf "p" ; set tagBsuf "m"
    }

    # Axis-1
    set x1 [expr {$xc+$delta}];  set y1 $yc
    $canvas create line $xc $yc $x1 $y1 \
            -arrow last -arrowshape $arrsize -tags x$tagAsuf
    $canvas create text $x1 [expr {$y1+round($arrbase/4.)-1}] \
            -anchor ne -text $axis1 -font $font \
	    -tags [list text x$tagAsuf]
    set x1 [expr {$xc-$delta}]
    $canvas create line $xc $yc $x1 $y1 \
            -arrow none -arrowshape $arrsize -tags x$tagBsuf
    # Y-axis
    set x1 $xc;  set y1 [expr {$yc-$delta}]
    $canvas create line $xc $yc $x1 $y1 \
            -arrow last -arrowshape $arrsize -tags y$tagAsuf
    $canvas create text [expr {$x1-round($arrbase/2.)}] $y1 \
            -anchor ne -text $axis2 -font $font \
	    -tags [list text y$tagAsuf]
    set y1 [expr {$yc+$delta}]
    $canvas create line $xc $yc $x1 $y1 \
            -arrow none -arrowshape $arrsize -tags y$tagBsuf
}

proc DrawCoords90 { canvas size xc yc outaxis axis1 axis2 } {
    DrawCoordsSetVars $size arrbase arrsize delta
    set font [Oc_Font Get small_bold]
    # Plane normal
    DrawOutOfPlaneAxis $canvas $size $xc $yc $delta \
            $arrbase $arrsize $outaxis $font

    # Set up in-plane axis tags
    # Set up in-plane axis tags
    if {[string match {-?} $outaxis]} {
	set tagAsuf "m" ; set tagBsuf "p"
    } else {
	set tagAsuf "p" ; set tagBsuf "m"
    }

    # Axis-1
    set x1 $xc;  set y1 [expr {$yc-$delta}]
    $canvas create line $xc $yc $x1 $y1 \
            -arrow last -arrowshape $arrsize -tags x$tagAsuf
    $canvas create text [expr {$x1-round($arrbase/2.)}] \
            [expr {$yc-$size/2.-2}] \
            -anchor ne -text $axis1 -font $font \
	    -tags [list text x$tagAsuf]
    set y1 [expr {$yc+$delta}]
    $canvas create line $xc $yc $x1 $y1 \
            -arrow none -arrowshape $arrsize -tags x$tagBsuf
    # Axis-2
    set x1 [expr {$xc-$delta}];  set y1 $yc
    $canvas create line $xc $yc $x1 $y1 \
            -arrow last -arrowshape $arrsize -tags y$tagAsuf
    $canvas create text $x1 [expr {$y1-[lindex $arrsize 2]-1}] \
            -anchor sw -text $axis2 -font $font \
	    -tags [list text y$tagAsuf]
    set x1 [expr {$xc+$delta}]
    $canvas create line $xc $yc $x1 $y1 \
            -arrow none -arrowshape $arrsize -tags y$tagBsuf
}


proc DrawCoords180 { canvas size xc yc outaxis axis1 axis2 } {
    DrawCoordsSetVars $size arrbase arrsize delta
    set font [Oc_Font Get small_bold]
    # Plane normal
    DrawOutOfPlaneAxis $canvas $size $xc $yc $delta \
            $arrbase $arrsize $outaxis $font

    # Set up in-plane axis tags
    if {[string match {-?} $outaxis]} {
	set tagAsuf "m" ; set tagBsuf "p"
    } else {
	set tagAsuf "p" ; set tagBsuf "m"
    }

    # Axis-1
    set x1 [expr {$xc-$delta}];  set y1 $yc
    $canvas create line $xc $yc $x1 $y1 \
            -arrow last -arrowshape $arrsize -tags x$tagAsuf
    $canvas create text $x1 [expr {$y1-[lindex $arrsize 2]-1}] \
            -anchor sw -text $axis1 -font $font \
	    -tags [list text x$tagAsuf]
    set x1 [expr {$xc+$delta}]
    $canvas create line $xc $yc $x1 $y1 \
            -arrow none -arrowshape $arrsize -tags x$tagBsuf
    # Axis-2
    set x1 $xc;  set y1 [expr {$yc+$delta}]
    $canvas create line $xc $yc $x1 $y1 \
            -arrow last -arrowshape $arrsize -tags y$tagAsuf
    $canvas create text [expr {$x1+$arrbase-1}] [expr {$yc+$size/2.-1}] \
            -anchor sw -text $axis2 -font $font \
	    -tags [list text y$tagAsuf]
    set y1 [expr {$yc-$delta}]
    $canvas create line $xc $yc $x1 $y1 \
            -arrow none -arrowshape $arrsize -tags y$tagBsuf
}

proc DrawCoords270 { canvas size xc yc outaxis axis1 axis2 } {
    DrawCoordsSetVars $size arrbase arrsize delta
    set font [Oc_Font Get small_bold]
    # Plane normal
    DrawOutOfPlaneAxis $canvas $size $xc $yc \
            $delta $arrbase $arrsize $outaxis $font

    # Set up in-plane axis tags
    if {[string match {-?} $outaxis]} {
	set tagAsuf "m" ; set tagBsuf "p"
    } else {
	set tagAsuf "p" ; set tagBsuf "m"
    }

    # Axis-1
    set x1 $xc;  set y1 [expr {$yc+$delta}];
    $canvas create line $xc $yc $x1 $y1 \
            -arrow last -arrowshape $arrsize -tags x$tagAsuf
    $canvas create text [expr {$x1+$arrbase-1}] [expr {$yc+$size/2.-1}] \
            -anchor sw -text $axis1 -font $font \
	    -tags [list text x$tagAsuf]
    set y1 [expr {$yc-$delta}];
    $canvas create line $xc $yc $x1 $y1 \
            -arrow none -arrowshape $arrsize -tags x$tagBsuf
    # Axis-2
    set x1 [expr {$xc+$delta}];  set y1 $yc
    $canvas create line $xc $yc $x1 $y1 \
            -arrow last -arrowshape $arrsize -tags y$tagAsuf
    $canvas create text [expr {$xc+$size/2.-1}] \
            [expr {$y1+round($arrbase/4.)}] \
            -anchor ne -text $axis2 -font $font \
	    -tags [list text y$tagAsuf]
    set x1 [expr {$xc-$delta}]
    $canvas create line $xc $yc $x1 $y1 \
            -arrow none -arrowshape $arrsize -tags y$tagBsuf
}

proc SetCoordColors { canvas {bgcolor {}} {axiscolors {}}} {
    # axiscolors should be a 6 element list of colors:
    #   +x color, -x color, +y color, -y color, +z color, -z color
    # Any empty or missing color will be painted black, unless bgcolor
    # is black in which case the default color will be white.

    if {[string match {} $bgcolor]} { set bgcolor white }
    $canvas configure -bg $bgcolor

    set defaultcolor black
    if {[string match black $bgcolor]} { set defaultcolor white }
    foreach { xp xm yp ym zp zm } $axiscolors {}
    foreach tag { xp xm yp ym zp zm } {
        if {![info exists $tag] || \
                [string match {} [set color [set $tag]]]} {
            set color $defaultcolor
        }
        $canvas itemconfig $tag -fill $color
    }
}

