# FILE: misc.tcl
#
# Various Tcl/Tk utility scripts for the OOMMF 2D display program
#

# Automatically select/deselect entire text upon Entry widget FocusIn/Out
if {![string match [package provide Tk] {}]} {
   #bind Entry <FocusIn> {+%W selection from 0 ; %W selection to end }
    bind Entry <FocusOut> {+%W selection clear}
}
#  I took out these binding on 97/2/16, because I don't like the
# effect.  -mjd
#  It appears that Tcl 8.0b1 does the <FocusIn> binding by default,
# but not the <FocusOut>, which I _really_ dislike, so I reinstated
# the latter on 97/9/26.   -mjd
#  In order to make use of the filtered-load capability from avf2ppm,
# which doesn't load Tk, the bind commands are moved behind a check
# for Tk. (-mjd, 4-Feb-2002)

# Converts width, height, xcenter & ycenter across rotations
proc OMF_RotateDimensions { angle widthname heightname \
        xcentername ycentername } {
    upvar $widthname   width
    upvar $heightname  height
    upvar $xcentername xcenter
    upvar $ycentername ycenter
    set irotstep [expr {round($angle/90.)%4}]
    set curwidth $width;     set curheight $height
    set curxcenter $xcenter; set curycenter $ycenter
    switch -exact $irotstep {
        1 { # 90 degrees
            set width   $curheight
            set height  $curwidth
            set xcenter $curycenter
            set ycenter [expr {1. - $curxcenter}]
        }
        2 { # 180 degrees
            set xcenter [expr {1. - $curxcenter}]
            set ycenter [expr {1. - $curycenter}]
        }
        3 { # 270 degrees
            set width   $curheight
            set height  $curwidth
            set xcenter [expr {1. - $curycenter}]
            set ycenter $curxcenter
        }
    }  ;# No changes if $irotstep==0

    # Note: There is some "grittiness" in the 'x/yview moveto'
    #   command, probably due to the scrollbar stepsize resolution
    #   (cf. the 'delta' scrollbar command).  As a result, sometimes
    #   the above calculations cause the slider bar to be off by 1
    #   step from where it should be.
}

####################################################################
# Rotations for out-of-plane transforms
proc InitViewTransform {} {
    global view_transform
    foreach index {{+z,+y} {+y,+x} {+x,+z} {-y,-z} {-z,-x} {-x,-y}} {
	set view_transform($index) {+y:+z:+x}
	set view_transform($index,ipr) 270  ;# Extra in-plane-rotation;
	## This makes it appear to the user that the transformation
	## was achived by a single 90 degree rotation about one of the
	## coordinate axes.
    }
    foreach index {{+z,+x} {+y,+z} {+x,+y} {-y,-x} {-z,-y} {-x,-z}} {
	set view_transform($index) {+z:+x:+y}
	set view_transform($index,ipr) 90
    }
    foreach index {{+z,-y} {+y,-x} {+x,-z} {-y,+z} {-z,+x} {-x,+y}} {
	set view_transform($index) {-x:-z:-y}
	set view_transform($index,ipr) 180
    }
    foreach index {{+z,-z} {+y,-y} {+x,-x} {-y,+y} {-z,+z} {-x,+x}} {
	set view_transform($index) {-y:-x:-z}
	# In this case, there is not a single 90 degree rotation that
	# achieves the transform.  It can be realized with a 180
	# degree rotation, but the rotation axis is not unique.  So
	# we just punt and pick one.  The two cases below provide a
	# consistent experience in the case the user is switching
	# back and forth between the same pair.
	if {[string compare {+} [string index $index 0]]==0} {
	    set view_transform($index,ipr) 90
	} else {
	    set view_transform($index,ipr) -90
	}
    }
    foreach index {{+z,-x} {+y,-z} {+x,-y} {-y,+x} {-z,+y} {-x,+z}} {
	set view_transform($index) {-z:-y:-x}
	set view_transform($index,ipr) 180
    }
    foreach index {{+x,+x} {+y,+y} {+z,+z} {-x,-x} {-y,-y} {-z,-z}} {
	set view_transform($index) {+x:+y:+z}
	set view_transform($index,ipr) 0
    }
}

proc ApplyTransform { transform x y z } {
    foreach i [list $x $y $z] j [split $transform :] {
	if {[string compare "-" [string index $j 0]]==0} {
	    set out([string index $j 1]) [expr {-1*$i}]
	} else {
	    set out([string index $j 1]) $i
	}
    }
    return [list $out(x) $out(y) $out(z)]
}

proc ApplyAxisTransform { base_axis new_axis x y z } {
    global view_transform
    set vtindex "$base_axis,$new_axis"
    if {[catch {set tnfm $view_transform($vtindex)}]} {
	InitViewTransform
	set tnfm $view_transform($vtindex)
    }
    return [ApplyTransform $tnfm $x $y $z]
}

####################################################################
# Find dimensions of root window
proc GetRootDimensions { widthname heightname } {
    upvar $widthname width
    upvar $heightname height
    set width 0;  set height 0
    regexp {^([^x]*)x([^-+]*).*$} [wm geometry .] match width height
}

####################################################################
# Proc to wait for a window to become visible.
# Timeouts after "timeout" milliseconds.  Default is 1000.
# Returns 1 on timeout, 0 otherwise.  Returns immediately
# if [winfo viewable $win] is true.
#
proc WaitVisible { win {timeout 1000} {timecheck 100}} {
    if { ![winfo exists $win] } { return 2 }
    if { [winfo viewable $win] } { return 0 }

    update idletasks
    if { [winfo viewable $win] } { return 0 }

    global WaitVisibleFlag WaitVisibleCheckFlag
    set WaitVisibleFlag "Wait"

    set id_timeout [after $timeout { set WaitVisibleFlag "Timeout" }]
    while { [string match $WaitVisibleFlag "Wait"] } {
        set WaitVisibleCheckFlag 0
        after $timecheck { set WaitVisibleCheckFlag 1 }
	vwait WaitVisibleCheckFlag
	if { [winfo viewable $win] } { 
	    set WaitVisibleFlag "Viewable"
	}
    }
    after cancel $id_timeout

    if { [string match $WaitVisibleFlag "Timeout"] } { return 1 }
    return 0
}

# Removed - Replaced by the proc Ow_Dialog
# proc OMF_InfoDialog args {}
# proc OMF_InfoDialogHelper args {}
# proc OMF_SetFocus args {}

# Removed - Can't find evidence that anything has used this in a long time.
# proc OMF_GetID args {}

# Produce an entry window collection:
#     Removed - Can't find evidence that anything has used this in a long time.
# proc OMF_MakeEntryWindow args {}
# proc OMF_EntryFollow args {}

# Produce an entry+scale window collection
# 	Removed - These have now been replaced by the Oc_Class Ow_EntryScale
# proc OMF_MakeEntryScaleWindow args {}
# proc OMF_EntryScaleBinding args {}
# proc OMF_ScaleScaleBinding args {}
# proc OMF_EntryScaleEscapeBinding args {}
# proc OMF_EntryScaleButtonReleaseBinding args {}
# proc OMF_ScaleMotionBinding args {}
# proc OMF_ScaleEntryFollow args {}
# proc OMF_ScaleFollow args {}
# proc OMF_IsNumber args {}

# Produce a radio box window collection
#     Removed - Can't find evidence that anything has used this in a long time.
# proc OMF_MakeRadioList args {}

# Print a canvas
# 	Removed - This has now been replaced by the Oc_Class Ow_PrintDlg
# proc OMF_PrintCanvas args {}
# proc OMF_ConvertUnits args {}

