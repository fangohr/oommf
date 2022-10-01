# FILE: mmdisp.tcl
#
# Main Tcl/Tk script for OOMMF 2D display program

# Library support (let them eat options)
package require Oc 2   ;# Error & usage handling below requires Oc
package require Nb 2
package require Ow 2
package require Mmdispcmds 2

# Disable Oc's user-invoked auto-size feature---mmDisp handles its own
# resizing.
Oc_DisableAutoSize .

########################### PROGRAM DOCUMENTATION ###################
Oc_Main SetAppName mmDisp
Oc_Main SetVersion 2.0b0
regexp \\\044Date:(.*)\\\044 {$Date: 2015/11/24 23:19:21 $} _ date
Oc_Main SetDate [string trim $date]
# regexp \\\044Author:(.*)\\\044 {$Author: donahue $} _ author
# Oc_Main SetAuthor [Oc_Person Lookup [string trim $author]]
Oc_Main SetAuthor [Oc_Person Lookup donahue]
Oc_Main SetExtraInfo "Built against Tcl\
        [[Oc_Config RunPlatform] GetValue compiletime_tcl_patch_level]\n\
        Running on Tcl [info patchlevel]"
Oc_Main SetHelpURL [Oc_Url FromFilename [file join [file dirname \
        [file dirname [file dirname [file dirname [Oc_DirectPathname [info \
        script]]]]]] doc userguide userguide \
        Vector_Field_Display_mmDisp.html]]
Oc_Main SetDataRole consumer

Oc_CommandLine ActivateOptionSet Net

Oc_CommandLine Option net {
        {flag {expr {![catch {expr {$flag && $flag}}]}} {= 0 or 1}}
    } {
        global enableServer; set enableServer $flag
} {Disable/enable data from other apps; default=enabled}
set enableServer 1

# Build list of configuration files.  Note: The local/mmdisp.config
# file, if it exists, is automatically sourced by the main mmdisp.config
# file.
set configFiles [list [file join [file dirname \
        [Oc_DirectPathname [info script]]] mmdisp.config]]
Oc_CommandLine Option config {file} {
        global configFiles; lappend configFiles $file
} "Append file to list of configuration files"

Oc_CommandLine Option [Oc_CommandLine Switch] {
        {{file optional} {} {Vector field file to display}}
    } {
        global firstfile;  set firstfile $file
} {End of options; next argument is file}

wm withdraw .
Oc_CommandLine Parse $argv

wm title . [Oc_Main GetInstanceName]  ;# Window title to display while
                                      ## program is initializing.

# Global arrays: mmdisp, _mmdisp, _mmdispcmds

# If the -server option appeared on the command line, start a server.
if {$enableServer} {
   package require Net 2
    Net_Protocol New mmd_protocol -name [list OOMMF vectorField protocol 0.1]
    $mmd_protocol AddMessage start datafile { fnlist } {
        # CAUTION!!! -- The command [AsyncTempFileDisplayRequest]
        # re-enters the event loop, so when an instance of Net_Connection
        # calls this proc via a call to [$net_protocol Reply], it can
        # be re-entered, and do such nasty things such as destroy itself.
        global _mmdispcmds
        lappend _mmdispcmds(atfdr_filelist) $fnlist ;# Add file to
        ## filelist immediately, without going through the event
        ## loop. Otherwise, a zwischenzug shutdown can trigger
        ## mmDispShutdownCleanup with an incomplete atfdr_filelist and
        ## result in file droppings.
        after idle AsyncTempFileDisplayRequest
        return [list start [list 0]]
    }
    Net_Server New mmd_server -protocol $mmd_protocol \
            -register 1 -alias [Oc_Main GetAppName]
    # Start server on an OS-selected port.
    $mmd_server Start 0

    Net_Protocol New mmd_sf_protocol -name [list OOMMF scalarField protocol 0.1]
    $mmd_sf_protocol AddMessage start datafile { fnlist } {
        # CAUTION!!! -- The command [AsyncTempFileDisplayRequest]
        # re-enters the event loop, so when an instance of Net_Connection
        # calls this proc via a call to [$net_protocol Reply], it can
        # be re-entered, and do such nasty things as destroy itself.
        global _mmdispcmds
        lappend _mmdispcmds(atfdr_filelist) $fnlist ;# Add file to
        ## filelist immediately, without going through the event
        ## loop. Otherwise, a zwischenzug shutdown can trigger
        ## mmDispShutdownCleanup with an incomplete atfdr_filelist and
        ## result in file droppings.
        after idle AsyncTempFileDisplayRequest
        return [list start [list 0]]
    }
    Net_Server New mmd_sf_server -protocol $mmd_sf_protocol \
            -register 1 -alias [Oc_Main GetAppName]
    # Start server on an OS-selected port.
    $mmd_sf_server Start 0
}

set _mmdispcmds(atfdr_filelist) {}   ;# File display request stack
set _mmdispcmds(atfdr_procsem) 0     ;# Processing semaphore
proc AsyncTempFileDisplayRequest {} {
    global _mmdispcmds

     # If semaphore already taken, just return
    if {$_mmdispcmds(atfdr_procsem)} return

    # Otherwise, schedule delete of all files on list except last,
    # which is displayed and then deleted.  After this check the
    # list for new names, which may have been added during display
    # processing.
    set _mmdispcmds(atfdr_procsem) 1     ;# Grab semaphore, and
    ## wrap rest up in a catch so we are sure to unset semaphore
    catch {
        update
        while {[llength $_mmdispcmds(atfdr_filelist)]>0} {
            set worklist $_mmdispcmds(atfdr_filelist)
            set _mmdispcmds(atfdr_filelist) {}  ;# Clear stack

            set namepair [lindex $worklist end]
            set filename [lindex $namepair 0]
            set title    [lindex $namepair 1]

            # Display file
            if {[string match "Vf_EmptyMesh" [GetMeshType]]} {
                # If no file currently displayed, use auto-display
                ReadFile $filename $title
            } else {
                # Otherwise, keep all current size and display parameters
                global plot_config DisplayRotation
                set autodisplay(sample) 0
                set autodisplay(datascale) 0
                set autodisplay(zoom) 0
                set autodisplay(viewaxis) 0
                set autodisplay(slice) 0
                ReadFile $filename $title \
                    $plot_config(misc,viewportwidth) \
                    $plot_config(misc,viewportheight) \
                    $DisplayRotation autodisplay
            }

            # Delete all files on stack
            foreach namepair $worklist {
                catch {file delete [lindex $namepair 0]}
            }

            # Loop back and check list for more files
            update
        }
    }  ;# End of semaphore-set catch
    set _mmdispcmds(atfdr_procsem) 0     ;# Release semaphore
}

proc mmDispShutdownCleanup {} {
   global _mmdispcmds
   # Delete any (temporary) file on display request stack.  In
   # practice this doesn't always catch everything.  Work is needed to
   # determine if the lost files are "owned" by this process or the
   # caller, and to take steps to delete those too.
   foreach namepair $_mmdispcmds(atfdr_filelist) {
      catch {file delete [lindex $namepair 0]}
   }
   set _mmdispcmds(atfdr_filelist) {}
}
Oc_EventHandler New _ Oc_Main Shutdown \
    [list mmDispShutdownCleanup] -oneshot 1

########################### GLOBAL VARIABLES ########################
# It would probably be better to have many of these sitting inside the
# _mmdisp array variable.
if {![info exists DEBUGLVL]} { set DEBUGLVL 0 } ;# Value of C++ compile-
                      ## time macro DEBUGLVL.  Should be set inside
                      ## Oommf_Init() (currently in OOMMF/oomf/oomfInit.cc)
set DrawFrameCount  0 ;# This variable is incremented by DrawFrame each
                      ## time it is called, so this variable can be used
                      ## for profiling.
set UpdateViewCount 0 ;# Any time the screen display is modified, this
                      ## value will be incremented.  A trace on this
                      ## variable can be used by any routine (e.g. the
                      ## print dialog) that needs to take action upon
                      ## such events.  Currently this variable is written
                      ## to only by the PackViewport proc.  NOTE: Scrolling
                      ## changes are NOT tracked by this variable at this
                      ## time. (-mjd 97/2/11)
set empty_mesh_fake_size 10000. ;## Pseudo dimension used in place of
## actual canvas width and height when dealing with EmptyMesh's.  This
## allows scroll positions to be preserved across loading of an
## EmptyMesh.  The effective margin size is 0 in this circumstance
## (i.e., plot_config(misc,margin) is ignored).

set canvas_frame .canvas_frame

set DefaultViewportWidth  640 ;# Safety.  These should be
set DefaultViewportHeight 480 ;# set during Init phase.

set MinViewportWidth  1
set MinViewportHeight 1
set MaxViewportWidth  1
set MaxViewportHeight 1

# NOTE: There are 3 different width/height pairs, and at any time they
#   may have very different values.  plot_config(misc,viewportwidth) and
#   plot_config(misc,viewportheight) are requested viewport width/height,
#   *including* scrollbars as needed.  The requested value will be
#   modified as necessary to meet the Min/Max/ViewportWidth/Height
#   restrictions.  The actual size of the viewing area, *not* including
#   the scrollbars, is stored in plot_config(misc,width) and
#   plot_config(misc,height).  The whole canvas size can be obtained from
#   the canvas "-scrollregion" configuration option, or from the "frame"
#   C++ class by calling GetFrameBox, or the Tcl procs "GetPlotBox" and
#   "GetPlotExtents".  The GetFrameBox returns a 6-tuple,
#                   xmin ymin zmin xmax ymax zmax
#   which is an overestimate of the box needed to enclose all drawn points.
#   GetPlotBox returns a 4-tuple, "xmin ymin xmax ymax", which is drawn
#   from the GetFrameBox result but further expanded by adding in the user
#   specified margin.  GetPlotExtents returns the width and height of the
#   box returned by GetPlotBox.
#

set AllowViewportResize 1  ;# If 1, then user may interactively change the
## viewpoint size, "Wrap" is allowed, and viewport dimensions follow the
## rotation angle.  See procs UpdateViewportResizePermission, WrapDisplay,
## and DisplayRotationTrace.

if {$AllowViewportResize==0} {
    wm resizable . 0 0
}

# Windows supports a "zoomed" state which inhibits window resizing.
proc CanResize {} {
    global AllowViewportResize
    if {$AllowViewportResize==0 || [string match zoomed [wm state .]]} {
        return 0
    }
    return 1
}

# Plot and print configuration arrays.  The 'plot_config_extras' list
# consists of items determined at runtime, not expected to be set from
# the configuration file.
# Notes:
#   misc,scrollcrossdim  --- Width of yscroll, height of xscroll
#
array set plot_config_extras {
    quantitylist       {}
    misc,dataunits     {}
    misc,spaceunits    {}
    misc,default_datascale 0
    misc,scrollcrossdim    0
    misc,confine           1
    misc,viewportwidth     0
    misc,viewportheight    0
    viewaxis,center    {}
}
array set plot_config [array get plot_config_extras]
foreach fn $configFiles { source $fn }
foreach elt [array names plot_config] {
    if {[string match {} [string trim $plot_config($elt)]]} {
        # Collapse empty strings
        set plot_config($elt) {}
    }
}
array set startup_plot_config [array get plot_config] ;# Save startup values.
foreach elt [array names print_config] {
    if {[string match {} [string trim $print_config($elt)]]} {
        # Collapse empty strings
        set print_config($elt) {}
    }
}

set SmartDialogs 1 ;# If 1, then when a dialog box is created, rather
## that disabling the associated menubutton, instead the behavior
## of the button is changed to raise that dialog box to the top of
## the window stacking order.

set DisplayRotation \
        [expr {90*round($plot_config(misc,rotation)/90.)}]  ;# Safety
set FileId            0    ;# Incremented each time a new file is loaded.
                           ## Traces may be set on this variable to update
                           ## file specific quantities, e.g., filename,
                           ## file description, mesh type, etc.
set RealFileId       0     ;# Incremented each time a non-empty file is
                           ## read.
set SliceCompat     1      ;# This needs to be set for Tcl/Tk older than
                           ## 8.3.  It causes slice display to use canvas
                           ## raise/lower commands instead of itemconfigure
                           ## -state hidden/normal.  The latter is faster,
                           ## but first appears in Tcl/Tk 8.3.
if {[package vcompare [package provide Tk] 8.3]>=0} {
    set SliceCompat 0
}

# Initialize view_transform array, which is used for out-of-plane
# rotations.
InitViewTransform

# Misc. constants
set SmallZoom 1.1486984  ;# Amt to enlarge on Page_Up (Prior). This is 2^(1/5)
set LargeZoom 2          ;# Amount to enlarge on Shift-Page_Up

# These scroll state variables should be modified *only* in
# proc PackViewport
set XScrollState      0
set YScrollState      0

# Whether or not the control bar is displayed
set omfCtrlBarState 1   ;# 1->on, 0->off

# Display state recover stack
# (Currently only 1 entry deep, but may be made deeper in future
set omfSavedDisplayState(zoom)      $plot_config(misc,zoom)
set omfSavedDisplayState(xcenter)   0.5
set omfSavedDisplayState(ycenter)   0.5
set omfSavedDisplayState(rot)       $DisplayRotation
set omfSavedDisplayState(datascale) $plot_config(misc,datascale)
proc OMF_SaveDisplayState { {zoom {}} {xcenter {}} {ycenter {}} \
        {rot {}} {datascale {}}} {
    global omfSavedDisplayState plot_config DisplayRotation
    if {[string match {} $zoom]} {
        set zoom $plot_config(misc,zoom)
    }
    if {[string match {} $xcenter] || [string match {} $ycenter]} {
        GetScrollCenter curx cury
        if {[string match {} $xcenter]} { set xcenter $curx }
        if {[string match {} $ycenter]} { set ycenter $cury }
    }
    if {[string match {} $rot]} { set rot $DisplayRotation }
    if {[string match {} $datascale]} {
        set datascale $plot_config(misc,datascale)
    }
    set omfSavedDisplayState(zoom)      $zoom
    set omfSavedDisplayState(xcenter)   $xcenter
    set omfSavedDisplayState(ycenter)   $ycenter
    set omfSavedDisplayState(rot)       $rot
    set omfSavedDisplayState(datascale) $datascale
}
proc OMF_RecoverDisplayState {} {
    global omfSavedDisplayState DisplayRotation
    # Note: SetScale calls OMF_SaveDisplayState
    #   & thereby reset these values
    set oldzoom      $omfSavedDisplayState(zoom)
    set oldxcenter   $omfSavedDisplayState(xcenter)
    set oldycenter   $omfSavedDisplayState(ycenter)
    set oldrot       $omfSavedDisplayState(rot)
    set olddatascale $omfSavedDisplayState(datascale)
    # Make rotation adjustments to xcenter & ycenter
    set dummywidth 0; set dummyheight 0
    OMF_RotateDimensions [expr {$DisplayRotation-$oldrot}] \
            dummywidth dummyheight oldxcenter oldycenter
    # Undo
    SetDataScaling $olddatascale
    SetScale $oldzoom
    SetScrollCenter $oldxcenter $oldycenter
}

##### Window geometry requirements #####################################
set RootHeadHeight 0           ;# Height of menu + action bar

proc CheckMinWidth {} {
    global menubar omfCtrlBarState ctrlbar

    set minwidth [winfo reqwidth $menubar]
    if {$omfCtrlBarState} {
	set temp [winfo reqwidth $ctrlbar]
	if {$temp>$minwidth} {set minwidth $temp}
    }

    global MinViewportWidth MinViewportHeight RootHeadHeight
    if {$minwidth != $MinViewportWidth} {
	update idletasks ;# Without this update some window
	## managers hang up for a few seconds on the wm minsize
	## set command.  This routine may be called from inside
	## a <Configure> event binding, so I'm not certain calling
	## 'update idletasks' is safe.
	set minheight [expr {$RootHeadHeight + $MinViewportHeight}]
	wm minsize . $minwidth $minheight
	foreach {minwidth minheight} [wm minsize .] {break}
	if {$minwidth != $MinViewportWidth} {
	    set MinViewportWidth $minwidth
	}
    }

    return $minwidth
}

proc OMF_SetGeometryRequirements {} {
    global MinViewportWidth MinViewportHeight RootHeadHeight
    global MaxViewportWidth MaxViewportHeight
    global omfCtrlBarState menubar ctrlbar
    global boxne rotbox

    if {$omfCtrlBarState} {
        WaitVisible $ctrlbar 500
        set size [winfo reqheight $boxne]
        $rotbox configure -height $size -width $size
    }

    set MinViewportHeight 1
    set MinViewportWidth [winfo reqwidth $menubar]
    if {[string compare $menubar [winfo toplevel $menubar]]==0} {
       # New menu scheme in effect --> menu doesn't affect
       # positioning of "." subwindows.
       set RootHeadHeight 0
    } else {
       # Old menu scheme being used.  Adjust vertical offset
       # to allow space for menu.
       set RootHeadHeight [winfo reqheight $menubar]
    }
    if {$omfCtrlBarState} {
        set RootHeadHeight [expr {$RootHeadHeight \
                + [winfo reqheight $ctrlbar]}]
        set temp [winfo reqwidth $ctrlbar]
        if {$temp>$MinViewportWidth} {
            set MinViewportWidth $temp
        }
    }

    foreach {MaxViewportWidth MaxViewportHeight} [wm maxsize .] { break }
    set MaxViewportHeight [expr {$MaxViewportHeight-$RootHeadHeight}]

    set minwidth $MinViewportWidth
    set minheight [expr {$RootHeadHeight + $MinViewportHeight}]
    wm minsize . $minwidth $minheight
    foreach {minwidth minheight} [wm minsize .] {break}
    if {$minwidth != $MinViewportWidth} {
	set MinViewportWidth $minwidth
    }
#    wm resizable . 1 1     ;# This is supposed to be the default.
    ## On Tcl/Tk 8.3.2 (others?), it has the side effect of lower
    ## the window and losing focus.
}

##### Combined interface to mesh object ################################
proc OMF_MeshInfo { command option } {
    # Currently only supports "cget" command, but in the future
    # "configure" should be added.
    if { ![string match "cget" "$command"] } {
        Ow_NonFatalError "Error in OMF_MeshInfo: Invalid command $cmd"
        return
    }
    set value {}
    switch -exact -- $option {
        -filename { set value [GetMeshName]  }
        -title    { set value [GetMeshTitle] }
        -gridtype { set value [GetMeshType]  }
        -description { set value [GetMeshDescription] }
        default   {
            Ow_NonFatalError "Error in OMF_MeshInfo: Invalid option $option"
            return
        }
    }
    return $value
}

########################## DEBUGGING PROC'S ############################
proc PrintDimensions { window } {
    set width [$window cget -width]
    if {[catch { set height [$window cget -height] }]} {
        set height "-1"
    }
    Oc_Log Log [format "%25s  Width=%3d, Height=%3d" $window $width \
            $height] status
}

proc PrintSizes {} {
    global canvas_frame canvas
    global MinViewportWidth MinViewportHeight
    global MaxViewportWidth MaxViewportHeight
    global plot_config
    Oc_Log Log \
        "MinViewportWidth/Height: $MinViewportWidth/$MinViewportHeight" \
        status
    Oc_Log Log \
        "MaxViewportWidth/Height: $MaxViewportWidth/$MaxViewportHeight" \
        status
    Oc_Log Log "   ViewportWidth/Height:\
        $plot_config(misc,viewportwidth)/$plot_config(misc,viewportheight)" \
        status
    GetRootDimensions width height
    Oc_Log Log [format "%25s  Width=%3d, Height=%3d" . $width $height] \
            status
    PrintDimensions $canvas
    PrintDimensions $canvas_frame.xscroll
    PrintDimensions $canvas_frame.yscroll
    Oc_Log Log [$canvas configure -scrollregion] status
}

proc PrintDrawFrameCount { name elt op } {
    global DrawFrameCount
    Oc_Log Log "DrawFrame call count: $DrawFrameCount" status
}
trace variable DrawFrameCount w PrintDrawFrameCount

proc PrintUpdateViewCount { name elt op } {
    global UpdateViewCount
    Oc_Log Log "UpdateView call count: $UpdateViewCount" status
}
trace variable UpdateViewCount w PrintUpdateViewCount

########################## DISPLAY PROC'S ###########################
proc UpdateWindowTitle {name elt op} {
    if {![regexp {^<[0-9]+> mmDisp} [wm title .] pfx]} {
        set pfx [Oc_Main GetInstanceName]
    }
    set title [file tail [GetMeshTitle]]
    if {"$title"==""} {
        set title $pfx
    } else {
        set title "$pfx: $title"
    }
    wm title . $title     ;## This is a memory leak in Tk 8.0p0
    wm iconname . $title  ;## through 8.0.3.  It is fixed in Tk 8.0.4.
}
trace variable FileId w UpdateWindowTitle

proc GetCanvasBox {} {
    # Returns plot coordinates of canvas viewport
    global canvas plot_config
    return [list [$canvas canvasx 0] [$canvas canvasy 0] \
            [$canvas canvasx $plot_config(misc,width)]   \
            [$canvas canvasy $plot_config(misc,height)]]
}

proc GetPlotBox {} {
    # Returns the actual drawing region {xmin ymin xmax ymax},
    # plus margin.  SPECIAL CASE: If xmin>=xmax and ymin>=ymax,
    # then the margin is not added.
    global canvas plot_config
    set bbox [GetFrameBox]  ;# Drawing region
    foreach { xmin ymin zmin xmax ymax zmax } $bbox { break }
    set margin $plot_config(misc,margin)
    if { $xmin >= $xmax && $ymin >= $ymax } {
        return "$xmin $ymin $xmin $ymin"
    }
    set pbox [list \
            [expr {$xmin-$margin}] \
            [expr {$ymin-$margin}] \
            [expr {$xmax+$margin}] \
            [expr {$ymax+$margin}]]     ;# Add margin
    return $pbox
}

proc GetBackingBox {} {
    global plot_config
    # Returns coordinates of a rectangle that should be large enough
    # to completely enclosed the plot area, regardless of panning.
    # Note: If a backing box of the size specified by this routine
    # is put in place, and then the Viewport is resized, the box
    # might not cover the new Viewport.
    foreach {xmin ymin xmax ymax} [GetPlotBox] { break }
    set xrange [expr {$xmax - $xmin}]
    if {$xrange < $plot_config(misc,viewportwidth)} {
        set xrange $plot_config(misc,viewportwidth)
    }
    set yrange [expr {$ymax - $ymin}]
    if {$yrange < $plot_config(misc,viewportheight)} {
        set yrange $plot_config(misc,viewportheight)
    }
    return [list [expr {-1*$xrange}] [expr {-1*$yrange}] \
            [expr {2*$xrange}] [expr {2*$yrange}] ]
}

proc GetPlotExtents { {wname {}} {hname {}} } {
    set pbox [GetPlotBox]
    if {![string match {} $wname]} { upvar $wname width }
    if {![string match {} $hname]} { upvar $hname height }
    set width [expr {[lindex $pbox 2] - [lindex $pbox 0]}]
    set height [expr {[lindex $pbox 3] - [lindex $pbox 1]}]
    return [list $width $height]
}

proc GetScrollCenter { {xcenter_name {}} {ycenter_name {}} } {
    # Note: This code should work even if canvas "confine"
    #  is false.
    global canvas plot_config
    if {![string match {} $xcenter_name]} { upvar $xcenter_name xcenter }
    if {![string match {} $ycenter_name]} { upvar $ycenter_name ycenter }
    foreach {xmin ymin xmax ymax} [$canvas cget -scrollregion] {}
    set xdelta [expr {$xmax-$xmin}]
    set ydelta [expr {$ymax-$ymin}]
    if {$xdelta<=0. || $ydelta<=0.} {
        # This block handles empty mesh case.  In this situation,
        # use a fictitious delta values to retain scroll settings.
        # The size of this value determines the precision of that
        # holding.  It should be the same as the holding value used
        # in SetScrollCenter
        global empty_mesh_fake_size
        set xdelta $empty_mesh_fake_size
        set ydelta $empty_mesh_fake_size
    }

    # Note: The input to the canvas canvasx (and canvasy) command
    #  is a screen coordinate, in particular an integer.  The
    #  command doesn't complain if you give it a non-integral value,
    #  but the return value is integral (but with a decimal point).
    set x [expr {[$canvas canvasx 0] + $plot_config(misc,width)/2.}]
    set xcenter [expr {($x-$xmin)/$xdelta}]
    set y [expr {[$canvas canvasy 0] + $plot_config(misc,height)/2.}]
    set ycenter [expr {($y-$ymin)/$ydelta}]

    return [list $xcenter $ycenter]
}

proc SetScrollCenter { xcenter ycenter } {
    # All imports should be in range [0,1].  This code
    # should work even if canvas "confine" is turned off.
    global canvas plot_config
    foreach {xmin ymin xmax ymax} [$canvas cget -scrollregion] {}
    set xdelta [expr {$xmax-$xmin}]
    set ydelta [expr {$ymax-$ymin}]
    if {$xdelta<=0. || $ydelta<=0.} {
        # This block handles empty mesh case.  In this situation,
        # use a fictitious xdelta value to retain scroll settings.
        # The size of this value determines the precision of that
        # holding.  It should be the same as the holding value used
        # in GetScrollCenter
        global empty_mesh_fake_size
        set xdelta $empty_mesh_fake_size
        set ydelta $empty_mesh_fake_size
    }
    set xstart [expr {$xcenter - $plot_config(misc,width)/(2.*$xdelta)}]
    set ystart [expr {$ycenter - $plot_config(misc,height)/(2.*$ydelta)}]

    global plot_config
    if {0 && $plot_config(misc,confine)} {
        # If active, this block adjusts the canvas position as
        # necessary (and possible) to keep empty regions outside
        # of the canvas from being displayed.
        if {$xstart<0} {set xstart 0} elseif {$xstart>1} {set xstart 1}
        if {$ystart<0} {set ystart 0} elseif {$ystart>1} {set ystart 1}
    }

    # --- Position display ---
    #   According to the canvas documentation, the "fraction" input to
    # "$canvas xview moveto _fraction_," which is the fraction of the
    # total width off-screen to the left, must be between 0 and 1.  But
    # this is dumb, because if set to 1 then the whole image is offscreen
    # to the left, but to get the entire image offscreen to the right
    # requires a negative value for fraction.
    #  Regardless, Tk versions 4.1-8.0 all appear to allow negative
    # values for fraction.  However, the displayed offset is often,
    # but not always, one pixel farther to the left than requested.
    # Analogous comments apply to "$canvas yview moveto _fraction_."
    #   Best solution I've (mjd) been able to come up with is to use
    # "$canvas xview scroll _incr_ units" instead.  This relies on
    # [$canvas cget -xscrollincrement] being 1, which should be set
    # in the $canvas constructor.  If this changes, appropriate
    # adjustments must be made here.
    set xleft [expr {$xstart*$xdelta+$xmin}]
    set xincr [expr {int(floor($xleft - [$canvas canvasx 0] + 0.5))}]
    if {$xincr != 0 } { $canvas xview scroll $xincr units }
    set ytop [expr {$ystart*$ydelta+$ymin}]
    set yincr [expr {int(floor($ytop - [$canvas canvasy 0] + 0.5))}]
    if {$yincr != 0 } { $canvas yview scroll $yincr units }
}

proc DetermineScrollStates { viewwidth viewheight framewidth frameheight \
        newxstatename newystatename } {
    # Determines which scrollbars will need to be activated, given a
    # canvas size of framewidth x frameheight, a viewport size of
    # viewwidth x viewheight, and a scrollbar cross dimension of
    # plot_config(misc,scrollcrossdim).  States (0=>off, 1=>on) are
    # returned in newxstatename and newystatename.
    # Note: This code does *not* take MinViewportWidth/Height into account.
    global plot_config
    upvar $newxstatename newxstate
    upvar $newystatename newystate
    set minheight [expr {$viewheight - $plot_config(misc,scrollcrossdim)}]
    set minwidth  [expr {$viewwidth  - $plot_config(misc,scrollcrossdim)}]
    if {($framewidth<=$viewwidth) && ($frameheight<=$viewheight)} {
        set newxstate 0; set newystate 0;
    } elseif {($framewidth>$viewwidth) && ($frameheight<=$minheight)} {
        set newxstate 1; set newystate 0;
    } elseif {($framewidth<=$minwidth) && ($frameheight>$viewheight)} {
        set newxstate 0; set newystate 1;
    } else {
        set newxstate 1; set newystate 1;
    }
}

proc PackViewport { {newviewwidth {}} {newviewheight {}} \
        {change_geometry 0}} {
    global plot_config
    global MinViewportWidth MinViewportHeight
    global MaxViewportWidth MaxViewportHeight
    global XScrollState YScrollState
    global canvas_frame canvas

    # Save current center for use in case scrollbars toggle on or off
    GetScrollCenter cx cy

    # Set default values
    if {[CanResize]} {
        if {[string match {} $newviewwidth]} {
            set newviewwidth $plot_config(misc,viewportwidth)
        } else {
            set newviewwidth [expr {round($newviewwidth)}]
        }
        if {[string match {} $newviewheight]} {
            set newviewheight $plot_config(misc,viewportheight)
        } else {
            set newviewheight [expr {round($newviewheight)}]
        }
        # Some Tk routines don't like window sizes smaller than 1
        if {$newviewwidth<1} {set newviewwidth 1}
        if {$newviewheight<1} {set newviewheight 1}

        set plot_config(misc,viewportheight) $newviewheight
        set plot_config(misc,viewportwidth)  $newviewwidth
    } else {
        set newviewheight $plot_config(misc,viewportheight)
        set newviewwidth  $plot_config(misc,viewportwidth)
    }

    # Determine real (displayed) viewport width & height
    if { $newviewwidth < $MinViewportWidth } {
        set vwidth $MinViewportWidth
    } elseif {  $newviewwidth > $MaxViewportWidth } {
        set vwidth $MaxViewportWidth
    } else {
        set vwidth $newviewwidth
    }
    if { $newviewheight < $MinViewportHeight } {
        set vheight $MinViewportHeight
    } elseif { $newviewheight > $MaxViewportHeight } {
        set vheight $MaxViewportHeight
    } else {
        set vheight $newviewheight
    }

    # Determine new scroll states & canvas display size
    set slop 1 ;# Amount of lap over to allow before requiring
               ## scrollbars.
    GetPlotExtents fwidth fheight
    DetermineScrollStates $vwidth $vheight \
            [expr {$fwidth-$slop}] [expr {$fheight-$slop}] \
            newxstate newystate
    if { $newystate } {
        set cwidth [expr {$vwidth - $plot_config(misc,scrollcrossdim)}]
    } else {
        set cwidth $vwidth
    }
    if { $newxstate } {
        set cheight [expr {$vheight - $plot_config(misc,scrollcrossdim)}]
    } else {
        set cheight $vheight
    }
    set cwidth [expr {round($cwidth)}]   ;# Canvas automatically rounds
    set cheight [expr {round($cheight)}] ;# to integers.

    # It appears that some (all?) window managers won't allow
    # windows with dimensions less than 1.  Since we want the
    # canvas width and height to reflect that of the viewport,
    # adjust input values to meet this minimum size.
    if {$cwidth<1}  { set cwidth 1 }
    if {$cheight<1} { set cheight 1 }

    # Set new scrollregion
    $canvas configure -scrollregion [GetPlotBox]

    # Set canvas size
    set plot_config(misc,width)  $cwidth
    set plot_config(misc,height) $cheight

    # If scrollstatus has changed, we need to repack display
    if { ($newxstate != $XScrollState) \
            || ($newystate != $YScrollState) } {
        # Unpack as needed
        if { $XScrollState && !$newxstate} {
            pack forget $canvas_frame.xscroll
            set cx 0.5
        }
        if { $YScrollState } {
            # If $YScrollState is true, then y-scroll needs unpacking:
            # At least one of x-scroll or y-scroll is changing state,
            # and either way y-scroll packing changes.
            pack forget $canvas_frame.yscroll
            if { $XScrollState } {
                pack forget $canvas_frame.yscrollbox
            }
            if {!$newystate} {
                set cy 0.5
            }
        }
        # Now pack
        if { !$XScrollState && $newxstate } {
            pack $canvas_frame.xscroll -side bottom -fill x \
                    -before $canvas
        }
        if { $newystate } {
            # If $newystate is true, then y-scroll needs packing:
            # At least one of x-scroll or y-scroll is changing state,
            # and either way y-scroll packing changes.
            if { $newxstate } {
                # Both x & y scrollbars are displayed; Use
                # corner box in lower righthand corner.
                pack $canvas_frame.yscroll -side right -fill y \
                        -in $canvas_frame.yscrollbox
                pack $canvas_frame.yscrollbox -side right -fill y \
                        -before $canvas_frame.xscroll
            } else {
                pack $canvas_frame.yscroll -side right -fill y \
                        -before $canvas
            }
        }
        set XScrollState $newxstate
        set YScrollState $newystate
        SetScrollCenter $cx $cy
    }

    if {$change_geometry} {
        # Change canvas geometry request.  NOTE: This *must* not
        # be activated from inside <Configure> event processing,
        # as it will likely generate a new <Configure> event, and
        # then the possibility exists for an infinite loop of
        # <Configure> events.  (Note: There appears to be less
        # redraw "bouncing" if 'wm geometry . {}' is placed _after_
        # the $canvas_frame resizing
        $canvas_frame configure -width $vwidth -height $vheight
        Ow_ForcePropagateGeometry .
    }

    global UpdateViewCount
    incr UpdateViewCount
}

###################### ACTION PROC'S ###############################
# ResizeViewport: Change viewport size.  This is bound to
# $canvas_frame <Configure>
proc ResizeViewport { width height } {
    # Skip if requested dimensions match current displayed dimensions.
    # (This helps protect stored sizes that are smaller than minimum
    # sizes.)
    global plot_config
    global MinViewportWidth MinViewportHeight
    global MaxViewportWidth MaxViewportHeight
    CheckMinWidth
    if {$plot_config(misc,viewportwidth) < $MinViewportWidth } {
        set curviewwidth $MinViewportWidth
    } elseif {$plot_config(misc,viewportwidth) > $MaxViewportWidth } {
        set curviewwidth $MaxViewportWidth
    } else {
        set curviewwidth $plot_config(misc,viewportwidth)
    }
    if {$plot_config(misc,viewportheight) < $MinViewportHeight } {
        set curviewheight $MinViewportHeight
    } elseif {$plot_config(misc,viewportheight) > $MaxViewportHeight } {
        set curviewheight $MaxViewportHeight
    } else {
        set curviewheight $plot_config(misc,viewportheight)
    }
    if { $curviewwidth != $width || $curviewheight != $height } {
        PackViewport $width $height
        # Note that we must not call PackViewport from here with
        # change_geometry true, because that could generate an
        # infinite loop.
    }
}

proc DisplayRotationTrace { name elt op } {
    global DisplayRotation plot_config
    global canvas SliceCompat

    # Round new rotation direction to nearest multiple of 90
    set rot [expr {90*round($DisplayRotation/90.)}]

    # Adjust to range [0,360)
    set rot [expr {round(fmod($rot,360.))}]
    if { $rot < 0 } { incr rot 360 }

    # Compare to previous rotation
    GetFrameRotation old_rotation
    set angle [expr {$rot - $old_rotation}]
    # Determine rotation step: 0, 1, 2 or 3
    set irotstep [expr {round($angle/90.)%4}]
    if { $irotstep == 0 } {
        # No change
        GetFrameRotation DisplayRotation
        return
    }

    # Get current scroll positions
    GetScrollCenter xscrcenter yscrcenter

    # Rotate display
    SetFrameRotation $rot
    GetFrameRotation DisplayRotation
    UpdateCoordDisplay $rot

    # Addjust viewport width, height & scroll positions
    # Keep track of scrollbar centers rather than ends to
    # better handle case where displayed viewport width-height
    # values change due to MinViewportWidth/Height restrictions.
    set new_xscrcenter $xscrcenter
    set new_yscrcenter $yscrcenter
    set new_width      $plot_config(misc,viewportwidth)
    set new_height     $plot_config(misc,viewportheight)
    OMF_RotateDimensions $angle new_width new_height \
            new_xscrcenter new_yscrcenter

    global watchcursor_windows
    Ow_PushWatchCursor $watchcursor_windows
    DrawFrame $canvas $SliceCompat
    PackViewport $new_width $new_height 1
    SetScrollCenter $new_xscrcenter $new_yscrcenter
    Ow_PopWatchCursor
}

proc SetScale { scale {redraw 1}} {
    global plot_config canvas SliceCompat

    # Enforce value limits
    set config [Oc_Config RunPlatform]
    set valmin [$config GetValue compiletime_flt_min]
    if {$scale<$valmin} { set scale $valmin }
    set valmax [$config GetValue compiletime_flt_max]
    if {$scale>$valmax} { set scale $valmax }

    if { $scale == $plot_config(misc,zoom) } { return }
    GetScrollCenter xcenter ycenter
    if { $scale > 0 } {
        OMF_SaveDisplayState $plot_config(misc,zoom) $xcenter $ycenter
        SetZoom $scale
    }
    GetZoom plot_config(misc,zoom)
    if {$redraw} {
        global watchcursor_windows
        Ow_PushWatchCursor $watchcursor_windows
        DrawFrame $canvas $SliceCompat
        PackViewport
        global XScrollState YScrollState
        if {!$XScrollState} { set xcenter 0.5 }
        if {!$YScrollState} { set ycenter 0.5 }
        SetScrollCenter $xcenter $ycenter
        Ow_PopWatchCursor
    }
}

proc RefreshMaxDimensions {} {
   if {![catch Ow_CheckScreenDimensions dimensions]} {
      # Silently ignore errors in Ow_CheckScreenDimensions
      set maxchk [lindex $dimensions 2]
      set maxcur [wm maxsize .]
      if {[lindex $maxchk 0] != [lindex $maxcur 0] \
             || [lindex $maxchk 1] != [lindex $maxcur 1]} {
         # Dimensions changed. Reset maxsize and refresh geometry
         wm maxsize . {*}$maxchk
         OMF_SetGeometryRequirements
      }
   }
}

proc RedrawDisplay {} {
    global canvas SliceCompat
    global watchcursor_windows
    Ow_PushWatchCursor $watchcursor_windows
    RefreshMaxDimensions
    UpdatePlotConfiguration
    DrawFrame $canvas $SliceCompat
    PackViewport
    Ow_PopWatchCursor
}

proc UpdateViewportResizePermission {} {
    global AllowViewportResize
    # It would be nice to only issue the resizable command
    # if the window resize state is changing.  However, some
    # window managers (e.g., Win98) do not report properly
    # to [wm resizable .].
    wm resizable . $AllowViewportResize $AllowViewportResize
    # Some window managers change window stacking order upon
    # receipt of a 'wm resizable ...' request, and even worse
    # transfers keyboard focus.  The latter can generally only
    # be regained with a 'focus -force .', which is a bit heavy
    # handed.  This is probably a Tk bug.  -mjd, 6-2001
    #
    global tcl_platform
    if {[string match windows $tcl_platform(platform)]} {
        raise . ; focus -force .  ;# Use at own risk!
    }
}

proc WrapDisplay {} {
# Set viewport size to framesize
    if [CanResize] {
        GetPlotExtents w h
        PackViewport $w $h 1
      # global canvas ;  $canvas xview moveto 0 ; $canvas yview moveto 0
        SetScrollCenter 0.5 0.5
    } else {
        CenterPlotView
    }
}

proc FillDisplay { {w {}} {h {}} {dopack 1}} {
# Set framesize to viewport size
    global plot_config
    global MinViewportWidth MinViewportHeight
    global MaxViewportWidth MaxViewportHeight

    if {[string match {} $w]} {
	set w $plot_config(misc,viewportwidth)
	if { $w < $MinViewportWidth } { set w $MinViewportWidth }
	if { $w > $MaxViewportWidth } { set w $MaxViewportWidth }
    } else {
	if { $w < $MinViewportWidth } { set w $MinViewportWidth }
	if { $w > $MaxViewportWidth } { set w $MaxViewportWidth }
	set plot_config(misc,viewportwidth) $w
    }

    if {[string match {} $h]} {
	set h $plot_config(misc,viewportheight)
	if { $h < $MinViewportHeight } { set h $MinViewportHeight }
	if { $h > $MaxViewportHeight } { set h $MaxViewportHeight }
    } else {
	if { $h < $MinViewportHeight } { set h $MinViewportHeight }
	if { $h > $MaxViewportHeight } { set h $MaxViewportHeight }
	set plot_config(misc,viewportheight) $h
    }

    set scale [SetZoom $w $h]

    if {$scale!=$plot_config(misc,zoom)} {
        global canvas SliceCompat watchcursor_windows
        OMF_SaveDisplayState
        set plot_config(misc,zoom) $scale
        Ow_PushWatchCursor $watchcursor_windows
        DrawFrame $canvas $SliceCompat
        if {$dopack} { PackViewport }
        $canvas xview moveto 0
        $canvas yview moveto 0
        Ow_PopWatchCursor
    }
}

proc HomeDisplay {} {
    FillDisplay {} {} 0
    WrapDisplay
}

proc ClearFrame {} {
    MakeCanvas ;# Start from scratch
    ReadFile {} {} 1 1 0
}

proc GetRelativeDisplayCenter {} {
    global DisplayRotation plot_config view_transform
    GetScrollCenter hor ver

    # Make margin corrections (i.e., remove margins)
    set margin $plot_config(misc,margin)
    GetPlotExtents hrange vrange
    if {$hrange>2*$margin && $vrange>2*$margin} {
        # The above test should always be true except in the
        # empty_mesh case; for empty_mesh the effective
        # margin size is 0, so no correction is needed.
        set hrange [expr {$hrange-2.*$margin}]
        set hor [expr {$hor + $margin*(2*$hor-1.)/$hrange}]
        set vrange [expr {$vrange-2.*$margin}]
        set ver [expr {$ver + $margin*(2*$ver-1.)/$vrange}]
    }

    # Coordinate conversions
    switch -exact -- $DisplayRotation {
        0   { set c(x) $hor ; set c(y) [expr {1.0-$ver}] }
        90  { set c(x) [expr {1.0-$ver}] ; set c(y) [expr {1.0-$hor}] }
        180 { set c(x) [expr {1.0-$hor}] ; set c(y) $ver }
        270 { set c(x) $ver ; set c(y) $hor }
    }
    foreach { slicemin slicemax slicerange} [GetSliceRange] { break }
    set c(z) [expr {($plot_config(viewaxis,center)-$slicemin)/$slicerange}]

    # At this point c() is relative (i.e., [0,1] based)
    # center in the apparent display coords.  So it remains
    # only to apply the appropriate coord transformation to
    # swap them around, and adjust for negative coordinates.

    set axis $plot_config(viewaxis)
    if {[string compare "+z" $axis]!=0} {
        foreach {c(x) c(y) c(z)} \
	    [ApplyAxisTransform $axis +z $c(x) $c(y) $c(z)] { break }
    }
    if {[string match "-?" $axis]} {
	set axistst [string index $axis 1]
	foreach i {x y z} {
            if {[string compare $axistst $i]==0} {
                set c($i) [expr {-1*$c($i)}]
            } else {
                set c($i) [expr {1+$c($i)}]
            }
        }
    }
    return [list $c(x) $c(y) $c(z)]
}

proc SetRelativeDisplayCenter { xc yc zc } {
    # Imports xc yc zc should be the desired relative (i.e., in the
    # range [0,1]) center point using the underlying data-driven
    # (i.e., not display) coordinate system.
    global plot_config DisplayRotation view_transform

    # First convert to display coordinates
    set axis $plot_config(viewaxis)
    if {[string match "-?" $axis]} {
	set axistst "[string index $axis 1]c"
	foreach i {xc yc zc} {
            if {[string compare $axistst $i]==0} {
                set $i [expr {-1*[set $i]}]
            } else {
                set $i [expr {[set $i]-1}]
            }
        }
    }
    if {[string compare "+z" $axis]!=0} {
        foreach {xc yc zc} \
	    [ApplyAxisTransform +z $axis $xc $yc $zc] { break }
    }
    switch -exact -- $DisplayRotation {
        0   { set hor $xc             ; set ver [expr {1-$yc}] }
        90  { set hor [expr {1-$yc}]  ; set ver [expr {1-$xc}] }
        180 { set hor [expr {1-$xc}]  ; set ver $yc             }
        270 { set hor $yc             ; set ver $xc             }
    }

    # Make margin corrections (i.e., introduce margins)
    GetPlotExtents hrange vrange
    if {$hrange>0 && $vrange>0} {
        # The above test fails only if the mesh is an EmptyMesh,
        # in which case the effective margin size is 0, so no
        # correction is needed.
        set hor [expr {$hor - $plot_config(misc,margin)*(2*$hor-1.)/$hrange}]
        set ver [expr {$ver - $plot_config(misc,margin)*(2*$ver-1.)/$vrange}]
    }

    # Apply translations
    SetScrollCenter $hor $ver
    foreach { slicemin slicemax slicerange} [GetSliceRange] { break }
    set slicecenter [expr {(1.0-$zc)*$slicemin+$zc*$slicemax}]
    SetSliceCenter $slicecenter
}

proc ChangeView { newaxis } {
    global plot_config
    if { [string match $newaxis $plot_config(viewaxis)] } {
        return ;# No change
    }
    # Put up watch cursor
    global watchcursor_windows
    Ow_PushWatchCursor $watchcursor_windows

    global view_transform slicewidget canvas SliceCompat
    global DisplayRotation plot_config

    global AllowViewportResize
    set keepAllowViewportResize $AllowViewportResize

    set AllowViewportResize 0        ;# Changing view shouldn't
    # UpdateViewportResizePermission ;# change viewport dimensions

    foreach {xc yc zc} [GetRelativeDisplayCenter] { break }

    set index "$plot_config(viewaxis),$newaxis"
    set extra_rotation $view_transform($index,ipr)
    CopyMesh 0 0 0 $view_transform($index)
    set plot_config(viewaxis) $newaxis
    SelectActiveMesh 0

    # Zoom level is based on the display plane cell width.
    # Changing the display plane potentially changes the
    # display plane cell width (if the cells aren't cubes),
    # so we need to reseat the zoom level.
    SetZoom $plot_config(misc,zoom)

    # Slice selection
    foreach { slicemin slicemax slicerange} [GetSliceRange] { break }
    $slicewidget AdjustRange $slicemin $slicemax
    $slicewidget Configure -marklist [list $slicemin $slicemax] \
            -scalestep [expr {$slicerange*0.01}]
    set axlet [string index $newaxis 1]
    set span $plot_config(viewaxis,${axlet}arrowspan)
    if {[string match {} $span] || $span==0} {
        set span [GetDefaultSliceSpan]
        set plot_config(viewaxis,${axlet}arrowspan) $span
        set plot_config(viewaxis,${axlet}pixelspan) $span
        set plot_config(viewaxis,center) \
                [expr {($slicemin+$slicemax)/2.}]
    } elseif {$span<0} {
        set span $slicerange
        set plot_config(viewaxis,${axlet}arrowspan) $span
        set plot_config(viewaxis,${axlet}pixelspan) $span
        set plot_config(viewaxis,center) \
                [expr {($slicemin+$slicemax)/2.}]
    } else {
        set center $plot_config(viewaxis,center)
        if {$center<$slicemin} \
                {set plot_config(viewaxis,center) $slicemin}
        if {$center>$slicemax} \
                {set plot_config(viewaxis,center) $slicemax}
        set step [ConvertToEng $slicerange 4]
        if {$plot_config(viewaxis,${axlet}arrowspan)>$slicerange} {
            set plot_config(viewaxis,${axlet}arrowspan) $step
        }
        if {$plot_config(viewaxis,${axlet}pixelspan)>$slicerange} {
            set plot_config(viewaxis,${axlet}pixelspan) $step
        }
    }

    UpdatePlotConfiguration

    # Note: DrawFrame (called below) does a call to
    # InitializeSliceDisplay, which will impress the plot_config
    # viewaxis,center and related settings above onto the display.
    set spaceunits [GetMeshSpatialUnitString]
    set plot_config(misc,spaceunits) $spaceunits
    set tlabel [string toupper $axlet]
    append tlabel "-slice"
    if {![string match {} $spaceunits]} {
        append tlabel " ($spaceunits)"
    }
    append tlabel ":"
    $slicewidget Configure -label $tlabel

    if {$extra_rotation!=0} {
        set DisplayRotation [expr {$DisplayRotation + $extra_rotation}]
        ## NB: DisplayRotationTrace has a DrawFrame call embedded in it.
    } else {
        DrawFrame $canvas $SliceCompat
    }

    UpdateCoordColors
    # PackViewport {} {} 1

    # Set to translations to original values.
    SetRelativeDisplayCenter $xc $yc $zc
    $slicewidget ScaleForce commit ;# Adjust slice center to
    ## value compatible with scale resolution

    # Reenable viewport resize switch to original setting
    set AllowViewportResize $keepAllowViewportResize
    # UpdateViewportResizePermission

    Ow_PopWatchCursor
}

proc ReadFile { filename title {width {}} {height {}} {rot {}} \
        {autodispname {}}} {
    global FileId RealFileId canvas SliceCompat
    global DisplayRotation
    global DefaultViewportWidth DefaultViewportHeight
    global plot_config datascalewidget slicewidget
    global view_transform
    global watchcursor_windows

    # Put up watch cursor.  Note: Ow_PushWatchCursor includes
    # makes an 'update idletasks' call.
    Ow_PushWatchCursor $watchcursor_windows

    # Store current translations, relative to data region of
    # the display, i.e., without the margins.  If r is a ratio
    # with margin m and total length d, then the data ratio is
    # r' = (rd-m)/(d-2m); the inverse of this relation is
    # r  = (r'(d-2m)+m)/d.
    GetScrollCenter cx cy
    GetPlotExtents w h
    set margin $plot_config(misc,margin)
    if {$w<=2*$margin || $h<=2*$margin} {
        global empty_mesh_fake_size
        set margin 0
        set w $empty_mesh_fake_size
        set h $empty_mesh_fake_size
    }
    set cx [expr {($cx*$w-$margin)/($w-2*$margin)}]
    set cy [expr {($cy*$h-$margin)/($h-2*$margin)}]

    # Store current mesh type
    set old_mesh_type [GetMeshType]

    set shrinkwrap 0
    if {[string match {} $width] || $width<1} {
        set width $DefaultViewportWidth
        set shrinkwrap 1
    }
    if {[string match {} $height] || $height<1} {
        set height $DefaultViewportHeight
        set shrinkwrap 1
    }
    if {[string match {} $rot]} {
        set rot $DisplayRotation
    }

    # Set defaults for all display parameters
    if {![string match {} $autodispname]} {
        upvar $autodispname autodisplay
    } else {
        # Default is to enable autodisplay
        set autodisplay(sample)    1
        set autodisplay(datascale) 1
        set autodisplay(zoom)      1
        set autodisplay(viewaxis)  1
        set autodisplay(slice)     1
    }

    if {$autodisplay(zoom) || $plot_config(misc,zoom)==0.} {
        set scale -1
        set cx 0.5 ; set cy 0.5  ;# Auto-center too.
    } else {
        set scale $plot_config(misc,zoom)
    }

    # If auto-sampling is requested, temporarily reconfigure
    # plot_config to invoke auto-sampling.  (Note: The values
    # in plot_config are reset directly into the new mesh, so
    # it is not necessary to call UpdatePlotConfiguration.
    if {$autodisplay(sample)} {
        set arrow_autosample_save $plot_config(arrow,autosample)
        set pixel_autosample_save $plot_config(pixel,autosample)
        set plot_config(arrow,autosample) 1
        set plot_config(pixel,autosample) 1
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
    ChangeMesh $workname $width $height $rot $scale
    set is_empty_mesh [string match "Vf_EmptyMesh" [GetMeshType]]
    if {![string match {} $title]} {
        SetMeshTitle $title
    }

    # Delete tempfile, if any
    if { ![string match {} $tempfile] } {
        file delete $tempfile
    }

    incr FileId
    if {!$is_empty_mesh} {
        incr RealFileId
    }

    if {$plot_config(misc,viewportheight)<3} {
        GetPlotExtents plot_config(misc,viewportwidth) \
                plot_config(misc,viewportheight)
    }

    # Data scaling
    set maxdatavalue [GetDataValueScaling]
    set plot_config(misc,default_datascale) $maxdatavalue
    if {$autodisplay(datascale)} {
        set plot_config(misc,datascale) $maxdatavalue
    } else {
        SetDataValueScaling $plot_config(misc,datascale)
    }

    # View axis selection
    if {$autodisplay(viewaxis) || \
            [string compare {+z} $plot_config(viewaxis)]==0 } {
        # Default view is from +z
        set viewaxis {+z}
        set plot_config(viewaxis) $viewaxis
    } else {
        # Mesh loads in with +z forward, so transform
        # mesh to show desired axis
        set viewaxis $plot_config(viewaxis)
        CopyMesh 0 0 0 $view_transform(+z,$viewaxis)
        SelectActiveMesh 0
        # Zoom level is based on the display plane cell width.
        # Changing the display plane potentially changes the
        # display plane cell width (if the cells aren't cubes),
        # so we need to reseat the zoom level.
        if {!$autodisplay(zoom)} {
            SetZoom $scale
        } else {
            SetZoom $w $h
        }
    }

    # Slice selection
    foreach { slicemin slicemax slicerange} [GetSliceRange] { break }
    $slicewidget AdjustRange $slicemin $slicemax
    $slicewidget Configure -marklist [list $slicemin $slicemax] \
            -scalestep [expr {$slicerange*0.01}]
    set axis [string index $plot_config(viewaxis) 1]
    set span $plot_config(viewaxis,${axis}arrowspan)
    if {$autodisplay(slice) || [string match {} $span]} {
        set span [GetDefaultSliceSpan]
        foreach i {x y z} {
            set plot_config(viewaxis,${i}arrowspan) {}
            set plot_config(viewaxis,${i}pixelspan) {}
        }
        set plot_config(viewaxis,${axis}arrowspan) $span
        set plot_config(viewaxis,${axis}pixelspan) $span
        set plot_config(viewaxis,center) \
                [expr {($slicemin+$slicemax)/2.}]
    } else {
        set center $plot_config(viewaxis,center)
        if {$center<$slicemin} \
                {set plot_config(viewaxis,center) $slicemin}
        if {$center>$slicemax} \
                {set plot_config(viewaxis,center) $slicemax}

        set span $plot_config(viewaxis,${axis}arrowspan)
        if {$span==0} {
            set span [GetDefaultSliceSpan]
        } elseif {$span<0 || $span>$slicerange} {
            set span [ConvertToEng $slicerange 4]
        }
        set plot_config(viewaxis,${axis}arrowspan) $span

        set span $plot_config(viewaxis,${axis}pixelspan)
        if {$span==0} {
            set span [GetDefaultSliceSpan]
        } elseif {$span<0 || $span>$slicerange} {
            set span [ConvertToEng $slicerange 4]
        }
        set plot_config(viewaxis,${axis}pixelspan) $span
    }
    $slicewidget ScaleForce commit ;# Adjust slice center to
    ## value compatible with scale resolution

    # Note: DrawFrame (called below) does a call to
    # InitializeSliceDisplay, which will impress the plot_config
    # slice center and span settings above onto the display.
    set spaceunits [GetMeshSpatialUnitString]
    set plot_config(misc,spaceunits) $spaceunits
    set tlabel [string toupper [string index $plot_config(viewaxis) 1]]
    append tlabel "-slice"
    if {![string match {} $spaceunits]} {
        append tlabel " ($spaceunits)"
    }
    append tlabel ":"
    $slicewidget Configure -label $tlabel

    DrawFrame $canvas $SliceCompat
    GetFrameRotation DisplayRotation
    UpdateCoordDisplay $DisplayRotation
    GetZoom plot_config(misc,zoom)
    SetArrowSubsamplingIncrement

    set tunits [GetDataValueUnit]
    set tlabel "Data Scale"
    if {![string match {} $tunits]} {
        append tlabel " ($tunits)"
    }
    append tlabel ":"
    $datascalewidget Configure -label $tlabel \
            -marklist $plot_config(misc,default_datascale)
    set plot_config(misc,dataunits) $tunits

    # Subsample values that weren't in auto-sample mode should
    # be removed from auto-sample mode.
    if {$autodisplay(sample)} {
        if {!$arrow_autosample_save} {
            set plot_config(arrow,subsample) \
                    [expr {abs($plot_config(arrow,subsample))}]
            if {$plot_config(arrow,subsample)<=0.01} {
                set plot_config(arrow,subsample) 0
            }
        }
        if {!$pixel_autosample_save} {
            set plot_config(pixel,subsample) \
                    [expr {abs($plot_config(pixel,subsample))}]
            if {$plot_config(pixel,subsample)<=0.01} {
                set plot_config(pixel,subsample) 0
            }
        }
        set plot_config(arrow,autosample) $arrow_autosample_save
        set plot_config(pixel,autosample) $pixel_autosample_save
        UpdatePlotConfiguration
    }
    UpdateCoordColors

    GetPlotExtents w h
    if {$shrinkwrap} {
        if {$width>$w}  { set width $w }
        if {$height>$h} { set height $h }
    }
    PackViewport $width $height 1

    # Reset translations to original values.  As noted above in
    # conjuction with the GetScrollCenter call (q.v.), cx and cy
    # store relative translation w/o margin, so we need to adjust
    # for the margin before calling SetScrollCenter, using the
    # relation
    #   r  = (r'(d-2m)+m)/d.
    # Note that w & h have been set by the GetPlotExtents call
    # immediately preceding the PackViewport call.
    set margin $plot_config(misc,margin)
    if {$w<=2*$margin || $h<=2*$margin} {
        global empty_mesh_fake_size
        set margin 0
        set w $empty_mesh_fake_size
        set h $empty_mesh_fake_size
    }
    set cx [expr {($cx*($w-2*$margin)+$margin)/$w}]
    set cy [expr {($cy*($h-2*$margin)+$margin)/$h}]
    SetScrollCenter $cx $cy

    Ow_PopWatchCursor
}

# Save file dialog box variables and procs
array set savefiledialog_subwidgets {
   optsbox {}
   descbox {}
}

proc SaveFileDialogCleanup {} {
   global FileId savefiledialog_subwidgets

   trace vdelete FileId w SaveFileDialogTrace
   ## This trace is set at the place where the save file dialog box is
   ## launched.

   set optsbox $savefiledialog_subwidgets(optsbox)
   $optsbox Delete
   set savefiledialog_subwidgets(optsbox) {}

   set descbox $savefiledialog_subwidgets(descbox)
   $descbox Delete
   set savefiledialog_subwidgets(descbox) {}
}

proc SaveFileDialogTrace { name elt op } {
   global savefiledialog_subwidgets
   set optsbox $savefiledialog_subwidgets(optsbox)
   set gridtype [OMF_MeshInfo cget -gridtype]
   if { [string match "Vf_GridVec3f" $gridtype] } {
      $optsbox GridsAllowedUpdate 1
   } else {
      $optsbox GridsAllowedUpdate 0
   }
   set descbox $savefiledialog_subwidgets(descbox)
   $descbox Set "Title:" [OMF_MeshInfo cget -title]
   $descbox Set "Desc:" [OMF_MeshInfo cget -description]
}

proc SaveOptionBoxSetup { widget frame } {
   global savefiledialog_subwidgets
   OmfOpts New optsbox $frame \
      -outer_frame_options "-bd 0 -relief flat"
   set gridtype [OMF_MeshInfo cget -gridtype]
   if { [string match "Vf_GridVec3f" $gridtype] } {
      $optsbox GridsAllowedUpdate 1
   } else {
      $optsbox GridsAllowedUpdate 0
   }
   pack [$optsbox Cget -winpath]
   set savefiledialog_subwidgets(optsbox) $optsbox
}

proc SaveDescBoxSetup { widget frame } {
   global savefiledialog_subwidgets
   Ow_MultiBox New descbox $frame -labelwidth 5 \
      -valuewidth 10 \
      -outer_frame_options "-bd 0 -relief flat" \
      -labellist [list [list "Title:" {}] [list "Desc:" {}]]
   $descbox Set "Title:" [OMF_MeshInfo cget -title]
   $descbox Set "Desc:" [OMF_MeshInfo cget -description]
   pack [$descbox Cget -winpath] -side top \
      -fill x -expand 0 -anchor ne
   set savefiledialog_subwidgets(descbox) $descbox
}

proc SaveFileCallback { widget } {
    global plot_config view_transform savefiledialog_subwidgets

    set errmsg {}

    # Get filename
    set filename [$widget GetFilename]

    # Store OMF type data & desc strings into omfspec array,
    # using the option name (w/o leading hyphen) as array index.
    foreach { name value } [$savefiledialog_subwidgets(optsbox) GetState] {
        set omfspec([string range $name 1 end]) $value
    }

    set desclist {}
    lappend desclist -title \
       [$savefiledialog_subwidgets(descbox) LookUp "Title:"]
    lappend desclist -desc \
       [$savefiledialog_subwidgets(descbox) LookUp "Desc:"]
    foreach { name value } $desclist {
        set omfspec([string range $name 1 end]) $value
    }

    # Check to see if file already exists
    if { [file exists $filename] } {
        set answer [Ow_Dialog 1 "File Save: File exists" question \
                "File $filename already exists.\nOverwrite?" \
                {} 1 "Yes" "No" ]
        if { $answer != 0 } {
            lappend errmsg "ERROR: File exists; no overwrite requested."
            return $errmsg
        }
    }
    if { [string length $filename] == 0 } {
        lappend errmsg "PROGRAMMING ERROR: Can't write to stdout in\
                [Oc_Main GetAppName]"
        return $errmsg
    }

    global watchcursor_windows
    Ow_PushWatchCursor $watchcursor_windows

    # If viewaxis isn't +z, then make copy of mesh with +z forward,
    # and write that.
    set viewaxis $plot_config(viewaxis)
    if {[string compare {+z} $viewaxis]!=0} {
        CopyMesh 0 1 0 $view_transform($viewaxis,+z)
        SelectActiveMesh 1
    }

    # Write file
    if { [catch {WriteMesh $filename $omfspec(datatype) $omfspec(gridtype) \
            $omfspec(title) $omfspec(desc)} writeresult] } {
        lappend errmsg $writeresult
    }

    if {[string compare {+z} $viewaxis]!=0} {
        # Release mesh copy
        SelectActiveMesh 0
        FreeMesh 1
    }

    Ow_PopWatchCursor

    return $errmsg
}

array set open_autobox {
   sample    1
   datascale 1
   zoom      1
   slice     1
}

proc OpenOptionBoxSetup { widget frame } {
    global open_autobox
    set bdframe [frame $frame.bdframe -relief raised -bd 2]
    set lab [label $bdframe.lab -text Auto:]
    set inframe [frame $bdframe.inframe -bd 2 -relief sunken]
    set sample [checkbutton $inframe.sample -text "Subsample" \
                -variable open_autobox(sample) \
                -offvalue 0 -onvalue 1]
    set data [checkbutton $inframe.data -text "Data Scale" \
                -variable open_autobox(datascale) \
                -offvalue 0 -onvalue 1]
    set zoom  [checkbutton $inframe.zoom -text "Zoom" \
                -variable open_autobox(zoom) \
                -offvalue 0 -onvalue 1]
    set slice [checkbutton $inframe.slice -text "Slice" \
                -variable open_autobox(slice) \
                -offvalue 0 -onvalue 1]
    pack $bdframe
    pack $lab $inframe -side left -anchor nw
    pack $sample $data $zoom $slice -side top -anchor w
}

array set plot_config_writecheck {
    misc,centerpt    0
    misc,relcenterpt 1
    misc,zoom        1
    misc,datascale   1
    *span            1
}

proc WriteConfigOptionBoxSetup { widget frame } {
    global plot_config_writecheck
    set bdframe [frame $frame.bdframe -relief raised -bd 2]
    set lab [label $bdframe.lab -text Save:]
    set inframe [frame $bdframe.inframe -bd 2 -relief sunken]
    set cpt [checkbutton $inframe.cpt -text "center point" \
                -variable plot_config_writecheck(misc,centerpt) \
                -offvalue 0 -onvalue 1]
    set rcpt [checkbutton $inframe.rcpt -text "relative center" \
                -variable plot_config_writecheck(misc,relcenterpt) \
                -offvalue 0 -onvalue 1]
    set data [checkbutton $inframe.data -text "data scale" \
                -variable plot_config_writecheck(misc,datascale) \
                -offvalue 0 -onvalue 1]
    set span [checkbutton $inframe.span -text "spans" \
                -variable plot_config_writecheck(*span) \
                -offvalue 0 -onvalue 1]
    set zoom [checkbutton $inframe.zoom -text "zoom" \
                -variable plot_config_writecheck(misc,zoom) \
                -offvalue 0 -onvalue 1]
    pack $bdframe
    pack $lab $inframe -side left -anchor nw
    pack $cpt $rcpt $data $span $zoom -side top -anchor w
}

proc WriteConfigFileCallback { widget } {
    global plot_config plot_config_extras plot_config_writecheck
    global print_config

    set errmsg {}

    # Get filename
    set filename [$widget GetFilename]

    # Check to see if file already exists
    if { [file exists $filename] } {
        set answer [Ow_Dialog 1 "File Save: File exists" question \
                "File $filename already exists.\nOverwrite?" \
                {} 1 "Yes" "No" ]
        if { $answer != 0 } {
            lappend errmsg "ERROR: File exists; no overwrite requested."
            return $errmsg
        }
    }
    if { [string length $filename] == 0 } {
        lappend errmsg "PROGRAMMING ERROR: Can't write to stdout in\
                [Oc_Main GetAppName]"
        return $errmsg
    }

    global watchcursor_windows
    Ow_PushWatchCursor $watchcursor_windows

    array set myconfig [array get plot_config]

    # Special values
    global DisplayRotation
    set myconfig(misc,rotation) $DisplayRotation
    foreach {xrel yrel zrel} [GetRelativeDisplayCenter] break
    set myconfig(misc,relcenterpt) [list $xrel $yrel $zrel]
    foreach {xmin ymin zmin xmax ymax zmax} [GetMeshRange] break
    foreach {xmin ymin zmin} [ApplyAxisTransform \
            $myconfig(viewaxis) +z $xmin $ymin $zmin] break
    foreach {xmax ymax zmax} [ApplyAxisTransform \
            $myconfig(viewaxis) +z $xmax $ymax $zmax] break
    if {[string match {-?} $plot_config(viewaxis)]} {
        # Negative view axis
        set tmp $xmin ; set xmin $xmax ; set xmax $tmp
        set tmp $ymin ; set ymin $ymax ; set ymax $tmp
        set tmp $zmin ; set zmin $zmax ; set zmax $tmp
    }
    set xc [expr {(1-$xrel)*$xmin+$xrel*$xmax}]
    set yc [expr {(1-$yrel)*$ymin+$yrel*$ymax}]
    set zc [expr {(1-$zrel)*$zmin+$zrel*$zmax}]
    set myconfig(misc,centerpt) [list $xc $yc $zc]

    # Remove elements from plot_config_extras, which represent
    # non-configurable items.
    foreach elt [array names plot_config_extras] {
        if {[info exists myconfig($elt)]} {
            unset myconfig($elt)
        }
    }

    # Remove elements as specified in plot_config_writecheck.
    # Note that the search in myconfig is a glob-style match.
    foreach elt [array names plot_config_writecheck] {
        if {!$plot_config_writecheck($elt)} {
            foreach subelt [array names myconfig $elt] {
                unset myconfig($subelt)
            }
        }
    }

    # Update print_config print widths
    if {$print_config(croptoview)} {
	set aspect $print_config(aspect_crop)
    } else {
	set aspect $print_config(aspect_nocrop)
    }
    if {![string match {} $aspect] && $aspect>0} {
	set print_config(pheight) \
	    [expr round(1000.*$print_config(pwidth)/$aspect)/1000.]
    }

    # Write header
    set chan [open $filename w]
    if {[catch {
       puts $chan "# FILE: [file tail $filename]           -*-Mode: tcl-*-"
       puts $chan \
{#
# mmDisp configuration.  This file must be valid Tcl code.

# This file should only be sourced from within another Tcl application.
# Check to make sure we aren't at the top application level
if {[string match [info script] $argv0]} {
    error "'[info script]' must be evaluated by an mmdisp-style application"
}

# Plot configuration array}

       # Write plot_config label-value pairs
       puts $chan "array set plot_config \{"
       foreach elt [lsort [array names myconfig]] {
             puts $chan [format "   %-25s %s" $elt [list $myconfig($elt)]]
       }
       puts $chan "\}"

       # Write print_config label-value pairs
       puts $chan "\narray set print_config \{"
       foreach elt [lsort [array names print_config]] {
	   if {[string match "aspect_*" $elt]} {
	       continue  ;# Don't save aspect_crop and aspect_nocrop
	   }
           puts $chan [format "   %-25s %s" $elt [list $print_config($elt)]]
       }
       puts $chan "\}"

       # Close
       flush $chan  ;# Make sure error is raised on disk full.
       close $chan
    } msg]} {
       error "Error writing file \"$filename\": $msg"
    }
    Ow_PopWatchCursor

    return $errmsg
}

proc OpenFileCallback { widget } {
    global plot_config open_autobox

    set errmsg {}

    # Get filename from File|Open widget
    set filename [$widget GetFilename]

    if {![info exists open_autobox(viewaxis)]} {
        set open_autobox(viewaxis) 0
    }

    if { ![file exists $filename] } {
        Ow_Dialog 0 "[Oc_Main GetInstanceName]:Error" warning \
                "File $filename does not exist." 10c
        lappend errmsg "ERROR: File does not exists"
    } elseif { ![file isfile $filename] } {
        Ow_Dialog 0 "[Oc_Main GetInstanceName]:Error" warning \
                "File $filename is not an ordinary file." 10c
        lappend errmsg "ERROR: Special file"
    } elseif { ![file readable $filename] } {
        Ow_Dialog 0 "[Oc_Main GetInstanceName]:Error" warning \
                "Don't have read access to file $filename." 10c
        lappend errmsg "ERROR: No read access"
    } else {
        # Read file
	if {[string match "Vf_EmptyMesh" [GetMeshType]]} {
	    set width [set height {}]
	} else {
	    set width $plot_config(misc,viewportwidth)
	    set height $plot_config(misc,viewportheight)
	}
        GetFrameRotation rot
        ReadFile $filename {} $width $height $rot open_autobox

        # Initialize omfSavedDisplayState
        OMF_SaveDisplayState
    }
    return $errmsg
}

proc DialogCallback { widget actionid args } {
    # Re-enable menubutton when the dialog widget is destroyed
    if {[string match DELETE $actionid]} {
        eval [join $args]
        return
    }

    # Call appropriate action proc
    set errmsg "ERROR (proc DialogCallback): Invalid actionid: $actionid"
    if {[string match OPEN $actionid]} {
        set errmsg [OpenFileCallback $widget]
    } elseif {[string match SAVE $actionid]} {
        set errmsg [SaveFileCallback $widget]
    } elseif {[string match WRITECONFIG $actionid]} {
        set errmsg [WriteConfigFileCallback $widget]
    }

    return $errmsg
}

proc UpdatePrintAspectRatios { args } {
    global print_config
    # Set non-cropped aspect ratio
    GetPlotExtents width height
    if { $height>0 && $width>0 } {
        set aspect [expr {double($width)/double($height)}]
        if {![info exists print_config(aspect_nocrop)]
     	    || $print_config(aspect_nocrop)!=$aspect} {
            set print_config(aspect_nocrop) $aspect
        }
    }
    # Set cropped aspect ratio
    global plot_config
    if { $plot_config(misc,height)>0 && $plot_config(misc,width)>0 } {
        set aspect [expr {double($plot_config(misc,width)) \
                / double($plot_config(misc,height))}]
        if {![info exists print_config(aspect_crop)]
     	    || $print_config(aspect_crop)!=$aspect} {
            set print_config(aspect_crop) $aspect
        }
    }
}

trace variable UpdateViewCount w UpdatePrintAspectRatios

proc PrintCallback { printwidget arrname } {
    upvar $arrname dialogprn
    global canvas print_config
    if {[info exists dialogprn]} {
        array set print_config [array get dialogprn]
    }
    global watchcursor_windows
    Ow_PushWatchCursor $watchcursor_windows
    set errcode [catch \
            {Ow_PrintDlg Print $printwidget $canvas print_config} msg]
    Ow_PopWatchCursor
    if {$errcode} {
        Oc_Log Log $msg error
    }
}

proc LaunchDialog { menuname itemlabel action } {
   if { [string match [$menuname entrycget $itemlabel -state] "disabled"] } {
      return   ;# Item not enabled
   }
   global FileId SmartDialogs

   set basetitle [Oc_Main GetTitle]

   if {[string match OPEN $action]} {
      Ow_FileDlg New dialogbox -callback DialogCallback \
         -dialog_title "Open File -- $basetitle" \
         -allow_browse 1 \
         -optionbox_setup OpenOptionBoxSetup \
         -selection_title "Open file..." \
         -select_action_id "OPEN" \
         -filter "*.o?f *.svf *.vio" \
         -compress_suffix [Nb_InputFilter GetExtensionList decompress] \
         -menu_data [list $menuname $itemlabel] \
         -smart_menu $SmartDialogs
   } elseif {[string match SAVE $action]} {
      Ow_FileDlg New dialogbox -callback DialogCallback \
         -dialog_title "Save File -- $basetitle" \
         -allow_browse 1 \
         -optionbox_setup SaveOptionBoxSetup \
         -descbox_setup SaveDescBoxSetup \
         -selection_title "Save File As..." \
         -select_action_id "SAVE" \
         -filter "*.o?f *.svf *.vio" \
         -compress_suffix [Nb_InputFilter GetExtensionList decompress] \
         -menu_data [list $menuname $itemlabel] \
         -smart_menu $SmartDialogs \
         -delete_callback_arg SaveFileDialogCleanup
       trace variable FileId w SaveFileDialogTrace
   } elseif {[string match WRITECONFIG $action]} {
      Ow_FileDlg New dialogbox -callback DialogCallback \
         -optionbox_setup WriteConfigOptionBoxSetup \
         -dialog_title "Write Config -- $basetitle" \
         -selection_title "Save Config File As..." \
         -select_action_id "WRITECONFIG" \
         -filter "*.config" \
         -menu_data [list $menuname $itemlabel] \
         -smart_menu $SmartDialogs
   } elseif {[string match PRINT $action]} {
      global print_config
      Ow_PrintDlg New dialogbox \
         -apply_callback PrintCallback \
         -dialog_title "Print -- $basetitle" \
         -menu_data [list $menuname $itemlabel] \
         -smart_menu $SmartDialogs \
         -crop_option 1 \
         -import_arrname print_config
   } else {
      Ow_NonFatalError "Invalid action code: $action" \
         "Error in LaunchDialog proc"
      return
   }

   # Set icon
   Ow_SetIcon [$dialogbox Cget -winpath]
}

proc UpdateCoordColors { args } {
    global plot_config coords_win
    set bgcolor $plot_config(misc,background)
    set t [GetDataValueScaling]
    if {$t<=0.0} {set t 1.0} ;# Safety
    set mt [expr {-1*$t}]
    set axiscolors [list \
            [GetVecColor [list  $t  0.  0.]] \
            [GetVecColor [list $mt  0.  0.]] \
            [GetVecColor [list  0.  $t  0.]] \
            [GetVecColor [list  0. $mt  0.]] \
            [GetVecColor [list  0.  0.  $t]] \
            [GetVecColor [list  0.  0. $mt]]]
    SetCoordColors $coords_win $bgcolor $axiscolors
}

proc SetPlotConfiguration { cfgname } {
    global plot_config canvas SliceCompat
    upvar $cfgname cfg

    # Take appropriate actions
    global watchcursor_windows
    Ow_PushWatchCursor $watchcursor_windows

    # Replace any empty strings in cfg with current plot_config values
    foreach elt [array names cfg] {
       if {[string match {} $cfg($elt)] && [info exists plot_config($elt)]} {
            set cfg($elt) $plot_config($elt)
        }
    }

    GetScrollCenter xcenter ycenter  ;# Except for 'Margin', plot
    ## cfg doesn't change center.  Could put in Margin correction,
    ## but probably not worth the bother.
    SetScale $cfg(misc,zoom) 0
    SetDataScaling $cfg(misc,datascale) 0
    set cfg(misc,datascale) $plot_config(misc,datascale)

    # Check for special values
    set defaultspan {}
    set fullrange {}
    set fullcenter {}
    set axis [string index $cfg(viewaxis) \
            [expr {[string length $cfg(viewaxis)]-1}]]
    set aspan viewaxis,${axis}arrowspan
    if {[string match {} $cfg($aspan)] || $cfg($aspan)==0} {
        set defaultspan [GetDefaultSliceSpan]
        set cfg($aspan) $defaultspan
    } elseif {$cfg($aspan)<0} {
        foreach {rmin rmax range} [GetSliceRange] {break}
        set fullrange [ConvertToEng $range 4]
        set fullcenter [ConvertToEng [expr {($rmin+$rmax)/2.}] 4]
        set cfg($aspan) $fullrange
        if {$cfg(arrow,status)} {
            # On full range request, recenter slice selection.
            # if arrow display is active.
            set cfg(viewaxis,center) $fullcenter
        }
    }
    set pspan viewaxis,${axis}pixelspan
    if {[string match {} $cfg($pspan)] || $cfg($pspan)==0} {
        if {[string match {} $defaultspan]} {
            set defaultspan [GetDefaultSliceSpan]
        }
        set cfg($pspan) $defaultspan
    } elseif {$cfg($pspan)<0} {
        if {[string match {} $fullrange]} {
            foreach {rmin rmax range} [GetSliceRange] {break}
            set fullrange [ConvertToEng $range 4]
            set fullcenter [expr {($rmin+$rmax)/2.}]
        }
        set cfg($pspan) $fullrange
        if {$cfg(pixel,status) && !$cfg(arrow,status)} {
            # On full range request, recenter slice selection
            # if pixel display is active but arrow display is not.
            set cfg(viewaxis,center) $fullcenter
        }
    }

    # Copy values over to plot_config
    array set plot_config [array get cfg]

    UpdatePlotConfiguration
    DrawFrame $canvas $SliceCompat
    UpdateCoordColors
    PackViewport
    global XScrollState YScrollState
    if {!$XScrollState} { set xcenter 0.5 }
    if {!$YScrollState} { set ycenter 0.5 }
    SetScrollCenter $xcenter $ycenter
    Ow_PopWatchCursor
}

###################### WIDGETS (Definitions & Bindings) #############
# On X, changing the cursor in a top-level window is immediately
# propagated down to all children.  On Windows, the propagation is
# lazy, so we need to set cursor changes a bit more aggressively
# there.
#  Note 1: This needs to be reset any time the canvas changes,
#   e.g., from ClearFrame.
#  Note 2: Swapping the watch cursor in and out can be disabled
#   by setting the owDisableWatchCursor feature true in the
#   oommf/config/local/options file.  This swapping appears to
#   exact a roughly 10% performance penalty on an average display
#   update.  Another reason to set owDisableWatchCursor true is
#   to work around a bad memory leak in some Tk+X platforms.
set watchcursor_windows {}
if {[string match windows $tcl_platform(platform)]} {
    proc UpdateWatchCursorWindows {} {
        global watchcursor_windows
        set watchcursor_windows {}
        set testwins [concat . [Ow_Descendents .]]
        foreach w $testwins {
            if {[llength [winfo children $w]]==0 && \
                    [string compare . [winfo toplevel $w]]==0} {
                # Change cursor on only the leaves of the widget tree,
                # because otherwise wrapper frames are seen to flash
                # annoyingly, at least on Win2K with Tcl/Tk 8.4.5.
                # Using only the leaves works pretty well, except
                # for little bits of underlying frames that peek
                # out; the cursor on those bits doesn't change.
                #  Also, don't change the cursor under toplevel
                # windows other that ".", both for cosmetic reasons,
                # but also so we don't have to modify this list each
                # time such a window (e.g., the configure dialog)
                # is created and destroyed.
                lappend watchcursor_windows $w
            }
        }
    }
} else {
    proc UpdateWatchCursorWindows {} {
        global watchcursor_windows
        set watchcursor_windows .
    }
}

# Canvas widget
proc MakeCanvas {} {
    # This routine creates a fresh canvas and associated scrollbars
    # inside the "canvas frame" widget.  The $canvas_frame frame must
    # be created before calling this routine.
    global canvas_frame canvas plot_config
    if {![info exists canvas]} {
	set canvas $canvas_frame.canvas
    }
    if {[winfo exists $canvas]} {
	# Temporarily disable canvas frame <Configure> binding
	set confscript [bind $canvas_frame <Configure>]
	bind $canvas_frame <Configure> {}

        # Save any current bindings on $canvas
        foreach b [bind $canvas] {
            lappend canvas_bindings [list $b [bind $canvas $b]]
        }

	destroy $canvas_frame.yscrollbox.corner
	destroy $canvas_frame.yscroll
	destroy $canvas_frame.xscroll
	destroy $canvas_frame.yscrollbox
	destroy $canvas
    }
    canvas $canvas -confine 0 \
	    -borderwidth 0 -selectborderwidth 0 \
	    -highlightthickness 0 \
	    -xscrollcommand "$canvas_frame.xscroll set" \
	    -yscrollcommand "$canvas_frame.yscroll set" \
	    -xscrollincrement 1 -yscrollincrement 1 \
	    -scrollregion "0 0 1 1" \
	    -width 0 -height 0
    # Note: 1) -confine is turned off so translations can take place
    #  without requiring an update of the scrollbars and scrollregion
    # (via a call to 'update idletasks').  In particular, this allows
    # for cleaner rotation processing.
    #       2) <x|y>scrollincrement is used in "SetScrollCenter" to set
    #  canvas translation.  If the values set in the constructor above
    #  are changed, then that proc will have to be changed as well.
    #    The following scroll filter restores -confine type behavior
    #  for user scrollbar interaction.

    frame $canvas_frame.yscrollbox -borderwidth 0 ;# This needs to be created
    # *before* yscroll so it gets mapped under yscroll.
    scrollbar $canvas_frame.xscroll \
	    -command "CanvasConfine xview"  -orient horizontal
    scrollbar $canvas_frame.yscroll \
	    -command "CanvasConfine yview"  -orient vertical
    pack $canvas -side left -fill both -expand 1
    set plot_config(misc,scrollcrossdim) [expr \
	    {[$canvas_frame.yscroll cget -width] \
	    + 2*[$canvas_frame.yscroll cget -borderwidth] \
	    + 2*[$canvas_frame.yscroll cget -highlightthickness]}]
    frame $canvas_frame.yscrollbox.corner -borderwidth 2 -relief raised \
	    -width $plot_config(misc,scrollcrossdim) \
	    -height $plot_config(misc,scrollcrossdim)
    pack $canvas_frame.yscrollbox.corner -side bottom
    # The yscrollbox.corner is used if both x & y scrollbars are displayed.
    # By making the scrollbars symmetric, it eases the calculations needed
    # to keep the same part of the image displayed under rotations.
    # NOTE: The yscroll widget is directly packed into $canvas_frame if
    #    xscroll is not displayed, but packed into $canvas_frame.yscrollbox
    #    if xscroll is displayed.  See 'proc UpdateScrollBarDisplay'.

    # NOTE: $canvas <Configure> binding is set below in
    #       INITIALIZATION section, and may get temporarily unset
    #       and set in other places as well.

    # Re-enable canvas bindings, if any
    if {[info exists canvas_bindings]} {
        foreach elt $canvas_bindings {
            foreach {event script} $elt {}
            bind $canvas $event $script
        }
    }

    # Re-enable canvas <Configure> binding, if any
    if {[info exists confscript]} {
	bind $canvas_frame <Configure> $confscript
    }

    UpdateWatchCursorWindows
}
proc CanvasConfine { args } {
    global canvas plot_config
    if {$plot_config(misc,confine)} {
        $canvas configure -confine 1
        eval $canvas $args
        $canvas configure -confine 0
    } else {
        eval $canvas $args
    }
}

set canvas_frame [frame .canvas_frame -borderwidth 0]
MakeCanvas

# Option frame and widgets
set ctrlbar [frame .ctrlbar -borderwidth 2 -relief raised]
set ctrlbar_entry_widgets {}
trace variable omfCtrlBarState w OMF_ChangeCtrlBarState
proc OMF_ChangeCtrlBarState { name elt op } {
    global omfCtrlBarState ctrlbar canvas_frame
    if {$omfCtrlBarState} {
        pack $ctrlbar -side top -before $canvas_frame -fill x -expand 0
    } else {
        pack forget $ctrlbar
        focus $canvas_frame     ;# Shift focus off $ctrlbar
    }
    OMF_SetGeometryRequirements
}

# Rotation
set rotbox [frame $ctrlbar.rotation -borderwidth 2 -relief ridge]
set coords_win [canvas $rotbox.coords \
        -width 48 -height 0 -bg white]
# The -width 48 is just a size guess; the canvas gets enlarged
# to a square region that just fills the height required
# by the other control bar lines. NOTE: The -height 0 value
# seems likely to be best behaved value wrt the <Configure>
# bindings.
pack $coords_win -side left -fill y
proc UpdateCoordDisplay { newrot } {
    global coords_win plot_config
    set win $coords_win
    ShowCoords $win $newrot $plot_config(viewaxis)
}
trace variable DisplayRotation w DisplayRotationTrace
catch {bind $coords_win <<Ow_LeftButton>> {
    set temp [expr {(round($DisplayRotation/90.)+1)%%4}]
    set DisplayRotation [expr {$temp*90}]
}} ;# Catch is for Tcl 7.5/Tk 4.1, which doesn't support virtual events.
bind $coords_win <Key-Left> {
    # Same as <Button-1>
    set temp [expr {(round($DisplayRotation/90.)+1)%%4}]
    set DisplayRotation [expr {$temp*90}]
}
catch {bind $coords_win <<Ow_RightButton>> {
    set temp [expr {(round($DisplayRotation/90.)-1)%%4}]
    set DisplayRotation [expr {$temp*90}]
}} ;# Catch is for Tcl 7.5/Tk 4.1, which doesn't support virtual events.
bind $coords_win <Key-Right> {
    # Same as <Button-3>
    set temp [expr {(round($DisplayRotation/90.)-1)%%4}]
    set DisplayRotation [expr {$temp*90}]
}
bind $coords_win <Configure> {
    set bw [%W cget -borderwidth]
    set ht [%W cget -highlightthickness]
    set size [expr {%h - 2*$bw - 2*$ht}]
    set cwidth [%W cget -width]
    set cheight [%W cget -height]
    if {$cwidth!=$size || $cheight!=$size} {
        %W configure -width $size -height $size
    }
}

# Control bar BoxNE frame; Holds "Subsample", "Auto sample",
# "Data Scale" and "Zoom" controls
set boxne [frame $ctrlbar.boxne]

# Arrow subsampling
proc SetArrowSubsampling { subsample } {
    global plot_config canvas SliceCompat watchcursor_windows
    if { $subsample == $plot_config(arrow,subsample) \
        || $plot_config(arrow,autosample) } { return }
    set plot_config(arrow,subsample) $subsample
    Ow_PushWatchCursor $watchcursor_windows
    UpdatePlotConfiguration
    DrawFrame $canvas $SliceCompat
    Ow_PopWatchCursor
}
set valmax [[Oc_Config RunPlatform] GetValue compiletime_flt_max]
Ow_EntryScale New arrowsswidget $boxne.subsample \
        -label "Arrow Subsample:" \
        -variable plot_config(arrow,subsample) -valuewidth 5 \
        -scalewidth 50 -rangemin 0. -rangemax 20. -scalestep 0.5 \
        -outer_frame_options "-bd 2 -relief ridge" \
        -command SetArrowSubsampling \
        -hardmin [expr {-1 * $valmax}] -hardmax $valmax
lappend ctrlbar_entry_widgets [$arrowsswidget Cget -winentry]
proc SetArrowSubsamplingIncrement { {incr {}} } {
    global arrowsswidget
    if {[string match {} $incr]} { set incr [GetMeshIncrement] }
    $arrowsswidget Configure -scalestep $incr
}
proc ArrowAutosamplingTrace { args } {
    global plot_config arrowsswidget
    if {$plot_config(arrow,autosample)} {
        $arrowsswidget Configure -state disabled
    } else {
        $arrowsswidget Configure -state normal
    }
}
trace variable plot_config(arrow,autosample) w ArrowAutosamplingTrace
ArrowAutosamplingTrace

# Arrow autosampling
set arrowautosampleframe \
        [frame $boxne.asframe -bd 2 -relief ridge]
set arrowautosamplewidget \
        [checkbutton $boxne.asframe.as -text "Auto sample" \
        -borderwidth 2 -pady 0 \
        -variable plot_config(arrow,autosample) \
        -onvalue 1 -offvalue 0 \
        -command ArrowAutosamplingUpdate]
# Note: RedrawDisplay includes an UpdatePlotConfiguration call
pack $arrowautosamplewidget -side left
proc ArrowAutosamplingUpdate {} {
    global plot_config
    UpdatePlotConfiguration
    if {$plot_config(arrow,autosample)} {
        foreach {arr_rate pix_rate} [GetAutosamplingRates] { break }
        if {$plot_config(arrow,subsample)!=$arr_rate} {
            global canvas SliceCompat watchcursor_windows
            Ow_PushWatchCursor $watchcursor_windows
            DrawFrame $canvas $SliceCompat
            PackViewport
            Ow_PopWatchCursor
        }
    }
}


# Data value scaling
proc SetDataScaling { newscale {redraw 1}} {
    global plot_config canvas SliceCompat
    if {[string match {} $newscale] || $newscale<=0} {
        # Illegal value; Reset to default
        set newscale $plot_config(misc,default_datascale)
    }

    set config [Oc_Config RunPlatform]
    set valmin [expr {sqrt([$config GetValue compiletime_dbl_min])}]
    if {$newscale<$valmin} { set newscale $valmin } ;# Safety
    set valmax [expr {[$config GetValue compiletime_dbl_max]/4.0}]
    if {$newscale>$valmax} { set newscale $valmax } ;# Safety
    if {$newscale!=$plot_config(misc,datascale)} {
        set tempscale [SetDataValueScaling $newscale]
        if {$tempscale!=$plot_config(misc,datascale)} {
            OMF_SaveDisplayState
            set plot_config(misc,datascale) [format "%.8g" $newscale]
            if {$redraw} {
                global watchcursor_windows
                Ow_PushWatchCursor $watchcursor_windows
                DrawFrame $canvas $SliceCompat
                Ow_PopWatchCursor
            }
        }
    }
}

set plot_config(misc,datascale) [GetDataValueScaling]
if {[catch {
   set minval [expr {pow(10,round(log10($plot_config(misc,datascale)))-1)}]
}]} {
   set minval 0.0  ;# Safety
}
if {$minval == 0.0} {
   set maxval 1.0
} else {
   set maxval [expr {$minval*100}]
}
set plot_config(misc,dataunits) [GetDataValueUnit]
set tunits $plot_config(misc,dataunits)
set tlabel "Data Scale"
if {![string match {} $tunits]} {
    append tlabel " ($tunits)"
}
append tlabel ":"
Ow_EntryScale New datascalewidget $boxne.datascale \
        -label $tlabel \
        -variable plot_config(misc,datascale) -valuewidth 6 \
        -scalewidth 50 -rangemin $minval -rangemax $maxval \
        -scalestep 0.01 -displayfmt "%.3g" \
        -outer_frame_options "-bd 2 -relief ridge" \
        -logscale 1 -autorange 1 \
        -marklist $plot_config(misc,default_datascale) -markwidth 2 \
        -command SetDataScaling \
        -hardmax [expr \
           {[[Oc_Config RunPlatform] GetValue compiletime_dbl_max]/4.0}]
lappend ctrlbar_entry_widgets [$datascalewidget Cget -winentry]

# Zoom (formerly known as FrameScale)
proc ZoomCallback { widget action args } {
    if {[string compare $action UPDATE]} {return}
    SetScale [lindex $args 1]
}
Ow_EntryBox New zoomwidget $boxne.zoom -label "Zoom:" \
        -callback ZoomCallback -autoundo 0 \
         -valuewidth 5 -variable plot_config(misc,zoom) \
        -valuetype posfloat -valuejustify right \
        -coloredits 1 -writethrough 0 \
        -outer_frame_options "-bd 2 -relief ridge" \
        -hardmin [[Oc_Config RunPlatform] GetValue compiletime_flt_min] \
        -hardmax [[Oc_Config RunPlatform] GetValue compiletime_flt_max]
lappend ctrlbar_entry_widgets [$zoomwidget Cget -winentry]


# Slice control #################################

# SliceHideShow provides slice selection capability for older
# versions of Tk.  SliceCompat should be set true for Tk older than
# 8.3, in which case items are hidden/displayed by lowering/raising
# them behind/above a backing rectangle. This is slower than using
# the canvas itemconfigure -state hidden/normal command, which
# appeared in Tcl/Tk 8.3.
if {$SliceCompat} {
    proc SliceLevelInit {} {
        # NB: A side effect of SliceLevelInit is that all pixel
        # and arrow objects are hidden
        global canvas plot_config

        # Lay down backing rectangle for slice display, and
        # move all pixel and arrow objects behind it.
        $canvas delete zslicerect  ;# Safety

        eval {$canvas create rectangle} [GetBackingBox] \
                -fill [$canvas cget -background] \
                {-outline {} -width 0 -tag zslicerect}

        $canvas lower zslicerect
        $canvas lower arrows
        $canvas lower pixels

        # Create base level scaffolding
        $canvas delete zslicescaf  ;# Safety
        set levelcount [GetZsliceCount]

        if {[string match {-?} $plot_config(viewaxis)]} {
            # Position layers in reversed order for negative views
            set i $levelcount
            while { $i > 0 } {
                incr i -1
                $canvas create rectangle 0 0 0 0 \
                        -fill {} -width 0 -outline {} \
                        -tag "zslicescaf floorp$i"
                $canvas create rectangle 0 0 0 0 \
                        -fill {} -width 0 -outline {} \
                        -tag "zslicescaf floora$i"
            }
        } else {
            for {set i 0} {$i<$levelcount} {incr i} {
                $canvas create rectangle 0 0 0 0 \
                        -fill {} -width 0 -outline {} \
                        -tag "zslicescaf floorp$i"
                $canvas create rectangle 0 0 0 0 \
                        -fill {} -width 0 -outline {} \
                        -tag "zslicescaf floora$i"
            }
        }
        $canvas raise "bdry"
    }
    proc SliceHideShow { hide_list show_list } {
        global canvas
        foreach item $hide_list { $canvas lower $item }
        foreach item $show_list {
            $canvas raise "$item" "floor$item"
        }
    }
} else {
    proc SliceLevelInit {} {
        # NB: A side effect of SliceLevelInit is that all pixel
        # and arrow objects are hidden
        global canvas plot_config
        $canvas itemconfigure {pixels||arrows} -state hidden

        set levelcount [GetZsliceCount]
        if {[string match {-?} $plot_config(viewaxis)]} {
            # Position layers in reversed order for negative views
            set i $levelcount
            while { $i > 0 } {
                incr i -1
                $canvas raise p$i
                $canvas raise a$i
            }
        } else {
            for {set i 0} {$i<$levelcount} {incr i} {
                $canvas raise p$i
                $canvas raise a$i
            }
        }
        $canvas raise "bdry"
    }
    proc SliceHideShow { hide_list show_list } {
        global canvas
        # In Tcl/Tk 8.3, itemconfigure accepts a tag sequence
        # containing embedded boolean operators.
        set hide_list [join $hide_list "||"]
        $canvas itemconfigure $hide_list -state hidden
        set show_list [join $show_list "||"]
        $canvas itemconfigure $show_list -state normal
    }
}

# GetMeshSliceLevels provides sign adjustment needed for
# interfacing with the GetZsliceLevels command
proc GetMeshSliceLevels { center span } {
    global plot_config
    if {[string match {-?} $plot_config(viewaxis)]} {
        set center [expr {-1*$center}]
    }
    set slicemin [expr {$center-$span/2.}]
    set slicemax [expr {$slicemin+$span}]]
    return [GetZsliceLevels $slicemin $slicemax]
}

proc InitializeSliceDisplay { \
        {slicecenter {}} {arrowspan {}} {pixelspan {}}} {
    global plot_config canvas slicewidget
    set axis [string index $plot_config(viewaxis) 1]
    if {[string match {} $slicecenter]} {
        set slicecenter $plot_config(viewaxis,center)
    } else {
        set plot_config(viewaxis,center) $slicecenter
    }
    if {[string match {} $arrowspan]} {
        set arrowspan $plot_config(viewaxis,${axis}arrowspan)
    } else {
        set plot_config(viewaxis,${axis}arrowspan) $arrowspan
    }
    if {[string match {} $pixelspan]} {
        set pixelspan $plot_config(viewaxis,${axis}pixelspan)
    } else {
        set plot_config(viewaxis,${axis}pixelspan) $pixelspan
    }

    # Set slicewidget slider width
    set rangemin [$slicewidget Cget -rangemin]
    set rangemax [$slicewidget Cget -rangemax]
    set range [expr {$rangemax - $rangemin}]
    if {$range<=0.0} {set range 1.0} ;# Safety

    if {$plot_config(pixel,status) && !$plot_config(arrow,status)} {
        # Arrow displayed disabled, but pixel display enabled,
        # so take slice span from pixel span.
        set newspan $pixelspan
    } else {
        # Otherwise take slice span from arrow span.
        set newspan $arrowspan
    }
    $slicewidget Configure -relsliderwidth [expr {$newspan/$range}]

    if {!$plot_config(pixel,status) && !$plot_config(arrow,status)} {
        return
    }

    # Initialize canvas slice display
    set show_list {}
    if {$plot_config(pixel,status)} {
        foreach {show_pix_low show_pix_high} \
                [GetMeshSliceLevels $slicecenter $pixelspan] { break }
        for {set i $show_pix_low} {$i<$show_pix_high} {incr i} {
            lappend show_list "p$i"
        }
    }

    if {$plot_config(arrow,status)} {
        foreach {show_arr_low show_arr_high} \
                [GetMeshSliceLevels $slicecenter $arrowspan] { break }
        for {set i $show_arr_low} {$i<$show_arr_high} {incr i} {
            lappend show_list "a$i"
        }
    }

    SliceLevelInit
    SliceHideShow {} $show_list
}

proc SliceDisplaySelect { \
        zcenterold zpixspanold zarrspanold \
        zcenter    zpixspan    zarrspan    } {

    global plot_config

    set hide_list {}
    set show_list {}
    if {$plot_config(pixel,status)} {
        foreach {hide_pix_low hide_pix_high} \
                [GetMeshSliceLevels $zcenterold $zpixspanold] { break }
        foreach {show_pix_low show_pix_high} \
                [GetMeshSliceLevels $zcenter $zpixspan] { break }
        if {$hide_pix_low <= $show_pix_low && \
                $show_pix_low < $hide_pix_high && \
                $hide_pix_high <= $show_pix_high } {
            # Ranges overlap, with show_pix higher
            set temp $hide_pix_high
            set hide_pix_high $show_pix_low
            set show_pix_low $temp
        } elseif {$show_pix_low <= $hide_pix_low && \
                $hide_pix_low < $show_pix_high && \
                $show_pix_high <= $hide_pix_high } {
            # Ranges overlap, with hide_pix higher
            set temp $show_pix_high
            set show_pix_high $hide_pix_low
            set hide_pix_low $temp
        }

        for {set i $hide_pix_low} {$i<$hide_pix_high} {incr i} {
            lappend hide_list "p$i"
        }
        for {set i $show_pix_low} {$i<$show_pix_high} {incr i} {
            lappend show_list "p$i"
        }
    }

    if {$plot_config(arrow,status)} {
        foreach {hide_arr_low hide_arr_high} \
                [GetMeshSliceLevels $zcenterold $zarrspanold] { break }
        foreach {show_arr_low show_arr_high} \
                [GetMeshSliceLevels $zcenter $zarrspan] { break }
        if {$hide_arr_low <= $show_arr_low && \
                $show_arr_low < $hide_arr_high && \
                $hide_arr_high <= $show_arr_high } {
            # Ranges overlap, with show_arr higher
            set temp $hide_arr_high
            set hide_arr_high $show_arr_low
            set show_arr_low $temp
        } elseif {$show_arr_low <= $hide_arr_low && \
                $hide_arr_low < $show_arr_high && \
                $show_arr_high <= $hide_arr_high } {
            # Ranges overlap, with hide_arr higher
            set temp $show_arr_high
            set show_arr_high $hide_arr_low
            set hide_arr_low $temp
        }

        for {set i $hide_arr_low} {$i<$hide_arr_high} {incr i} {
            lappend hide_list "a$i"
        }
        for {set i $show_arr_low} {$i<$show_arr_high} {incr i} {
            lappend show_list "a$i"
        }
    }

    SliceHideShow $hide_list $show_list
}

proc ConvertToEng { z sigfigs } {
   set sigfigs [expr int(round($sigfigs))]
   if {$sigfigs<1} { set sigfigs 1 }
   set z [format "%.*g" $sigfigs $z] ;# Remove extra precision

   if {$z==0} {
      return [format "%#.*g" $sigfigs 0]
   }
   if {[catch {
      set exp1 [expr {int(floor(log10(abs($z))))}]
      set exp2 [expr {int(round(3*floor($exp1/3.)))}]
      set prec [expr int(round($sigfigs-($exp1-$exp2+1)))]
      if { $prec<0 } { set prec 0 }
      set mantissa [string trimright \
                       [format "%.*f" $prec [expr {$z*pow(10.,-1*$exp2)}]] \
                       ". "]
   }]} {
      # Error occurred, probably overflow.  Punt
      return [format "%#.*g" $sigfigs 0]
   }
   if {$exp2==0} {return $mantissa}
   return [format "%se%d" $mantissa $exp2]
}

proc GetSliceRange {} {
    global plot_config
    foreach { slicemin slicemax } [GetMeshZRange] { break }
    if {[string compare {-} [string index $plot_config(viewaxis) 0]]==0} {
        set temp     [expr {-1*$slicemin}]
        set slicemin [expr {-1*$slicemax}]
        set slicemax $temp
    }
    if {$slicemax<=$slicemin} {
        # No zspan.  Expand slice to size zstep about zslicemin.
        # If zstep is 0., the use the smaller of xstep or ystep
        # which is also strictly positive.
        foreach { xstep ystep zstep } [GetMeshCellSize] { break }
        if {$zstep<=0.} {
            if {$ystep>0.} {
                if {$ystep<$xstep || $xstep<=0.} {
                    set zstep $ystep
                } else {
                    set zstep $xstep
                }
            } else {
                if {$xstep>0.} {
                    set zstep $xstep
                } else {
                    set zstep 1.0   ;# Punt: all of [xyz]step are zero
                }
            }
        }
        set slicemax [expr {$slicemin+$zstep/2.}]
        set slicemin [expr {$slicemin-$zstep/2.}]
    }
    set slicerange [expr {$slicemax-$slicemin}]
    return [list $slicemin $slicemax $slicerange]
}

proc GetDefaultSliceSpan {} {
    # Returns default (auto) slice thickness for current view

    # Note 1: Mesh uses rotated coordinates, so slice
    #   direction is *always* z
    # Note 2: Currently (6/2001) slice selection doesn't work
    #   properly on irregular meshes (the subsampling routine,
    #   GetClosest2D, ignores the z coordinate), so set the default
    #   slice span for such meshes to the full z range.
    foreach { slicemin slicemax slicerange } [GetSliceRange] { break }
    if {![IsRectangularMesh]} {
        set zstep $slicerange
    } else {
        foreach { xstep ystep zstep } [GetMeshCellSize] { break }
        if {$zstep<=0.0} {set zstep 1} ;# Safety
        if {$zstep>$slicerange} {set zstep $slicerange}
        if {20*$zstep<$slicerange} {
           # zstep is rather small to use directly, so change to
           # a multiple of zstep such that 20*zstep is close to
           # slicerange.
           set zstep [expr {$zstep*round($slicerange/(20.*$zstep))}]
        }
    }
    return [ConvertToEng $zstep 4]
}

proc SetSliceCenter { newcenter } {
    global plot_config canvas
    set config [Oc_Config RunPlatform]
    set valmax [expr {[$config GetValue compiletime_dbl_max]/4.0}]
    set valmin [expr {-1 * $valmax}]
    if {$newcenter<$valmin} { set newcenter $valmin } ;# Safety
    if {$newcenter>$valmax} { set newcenter $valmax } ;# Safety
    set newcenter [ConvertToEng $newcenter 4]
    if {$newcenter!=$plot_config(viewaxis,center)} {
        OMF_SaveDisplayState
        set axis [string index $plot_config(viewaxis) 1]
        set pixelspan $plot_config(viewaxis,${axis}pixelspan)
        set arrowspan $plot_config(viewaxis,${axis}arrowspan)
        if {[string match {} $pixelspan]} {
            # Slice span not set; use defaults
            set span [GetDefaultSliceSpan]
            set plot_config(viewaxis,${axis}arrowspan) $span
            set plot_config(viewaxis,${axis}pixelspan) $span
            set pixelspan $span
            set arrowspan $span
        }
        SliceDisplaySelect \
                $plot_config(viewaxis,center) $pixelspan $arrowspan \
                $newcenter                    $pixelspan $arrowspan
        set plot_config(viewaxis,center) $newcenter
    }
}

proc SetSliceSpan { newpixelspan newarrowspan } {
    # Note: As of 29-June-2001, this proc does not appear
    #       to be used.  See InitializeSliceDisplay above. -mjd
    global plot_config slicewidget

    set bbox [GetFrameBox]  ;# Drawing region
    set thick [expr {[lindex $bbox 5] - [lindex $bbox 2]}]

    if {$newpixelspan<0} {
        set newpixelspan 0
    } elseif {$newpixelspan>2*$thick} {
        set newpixelspan 2*$thick
    }

    if {$newarrowspan<0} {
        set newarrowspan 0
    } elseif {$newarrowspan>2*$thick} {
        set newarrowspan 2*$thick
    }

    set axis [string index $plot_config(viewaxis) 1]

    if {$plot_config(pixel,status) && !$plot_config(arrow,status)} {
        # Arrow displayed disabled, but pixel display enabled,
        # so take slice span from pixel span.
        set newspan $newpixelspan
    } else {
        # Otherwise take slice span from arrow span.
        set newspan $newarrowspan
    }

    set rangemin [$slicewidget Cget -rangemin]
    set rangemax [$slicewidget Cget -rangemax]
    set range [expr {$rangemax - $rangemin}]
    if {$range<=0.0} {set range 1.0} ;# Safety
    set newwidth [expr {$newspan/$range}]

    if {$newwidth != [$slicewidget Cget -relsliderwidth]} {
        OMF_SaveDisplayState
        $slicewidget Configure -relsliderwidth $newwidth
    }

    set slicecenter $plot_config(viewaxis,center)
    SliceDisplaySelect $slicecenter \
            $plot_config(viewaxis,${axis}pixelspan) \
            $plot_config(viewaxis,${axis}arrowspan) \
            $slicecenter $newpixelspan $newarrowspan

    set plot_config(viewaxis,${axis}pixelspan) $newpixelspan
    set plot_config(viewaxis,${axis}arrowspan) $newarrowspan
}

set tunits $plot_config(misc,spaceunits)
set tlabel [string toupper [string index $plot_config(viewaxis) 1]]
append tlabel "-slice"
if {![string match {} $tunits]} {
    append tlabel " ($tunits)"
}
append tlabel ":"
Ow_EntryScale New slicewidget $ctrlbar.slice \
        -label $tlabel \
        -variable plot_config(viewaxis,center) -valuewidth 9 \
        -scalewidth 50 -rangemin 0. -rangemax 1. -scalestep 0.01 \
        -valuetype float -command SetSliceCenter \
        -logscale 0 -autorange 0 -markwidth 2 \
        -outer_frame_options "-bd 2 -relief ridge" \
        -hardmin [expr {-1 * $valmax}] -hardmax $valmax \
        -scaleautocommit 100
lappend ctrlbar_entry_widgets [$slicewidget Cget -winentry]

### $ctrlbar packing
pack [$slicewidget Cget -winpath] -side bottom -fill x
pack $rotbox -side left -fill y
pack $boxne  -side top -fill x
grid [$arrowsswidget Cget -winpath] $arrowautosampleframe \
        -sticky sewn
grid [$datascalewidget Cget -winpath] [$zoomwidget Cget -winpath] \
        -sticky sewn
grid columnconfigure $boxne 0 -weight 1
UpdateWatchCursorWindows

######################## COMMAND CONSOLE ###############################
proc LaunchCommandConsole { menuname itemlabel } {
   global command_console SmartDialogs
   set menucolor red

   if {![info exists command_console]} {
      set command_console [interp create -- command_interp]
      $command_console eval {
         package require Tk
         package require Oc
         Oc_CommandLine Parse [list -console]
         console hide
         wm withdraw .
         proc hide {} {
            console hide
         }
         interp alias {} exit {} hide
      }

      proc RaiseCommandConsole {} [subst -nocommands {
         $command_console eval { console eval {
            if {[string compare iconic [wm state .]]==0} {
               wm state . normal
            }
            if {[string match {} [focus -lastfor .]]} {
               focus .
            } else {
               focus [focus -lastfor .]
            }
            raise .
         }}
      }]

      interp alias $command_console MenuDeleteCleanup {} MenuDeleteCleanup

      set child_command_console [$command_console eval interp slaves]
      $command_console eval {console eval {
         proc CheckMenu {} {
            if {[string match withdrawn [wm state .]]} {
               MenuDeleteCleanup
            }
         }
      }}
      $command_console eval \
         [list interp alias \
             $child_command_console MenuDeleteCleanup {} MenuDeleteCleanup]
      if {0} {
      $command_console eval \
         [list console eval \
             [list wm protocol . WM_DELETE_WINDOW MenuDeleteCleanup]]
      }
      $command_console eval \
         [list console eval \
             [list bind . <Destroy> +MenuDeleteCleanup]]
      $command_console eval \
         [list console eval \
             [list bind . <Unmap> +CheckMenu]]

      interp alias $command_console CopyFrame {} CopyFrame
      interp alias $command_console FrameInfo {} FrameInfo
      interp alias $command_console SubtractFrame {} SubtractFrame
      interp alias $command_console ClipFrame {} ClipFrame

      proc CopyFrame { frame1 frame2 } {
         set activemesh [ReportActiveMesh]
         CopyMesh $frame1 $frame2
         if {$frame2 == $activemesh } RedrawDisplay
      }
      proc FrameInfo { frame } {
         set infostr "Mesh title: [GetMeshTitle $frame]"
         set desc [GetMeshDescription $frame]
         append infostr
         if {[string first "\n" $desc]<0} {
            # Single line description
            append infostr "\nMesh  desc: $desc"
         } else {
            # Mult-line description
            regsub -all "\n" $desc "\n: " desc
            append infostr "\nMesh desc---\n: $desc"
         }
         append infostr "\n[GetMeshStructureInfo $frame]"
         return $infostr
      }
      proc SubtractFrame { src_dest_frame subtract_frame } {
         set activemesh [ReportActiveMesh]
         DifferenceMesh $src_dest_frame $subtract_frame
         if {$src_dest_frame == $activemesh } RedrawDisplay
      }
      proc ClipFrame { frame xmin ymin zmin xmax ymax zmax } {
         set clipstr [list $xmin $ymin $zmin $xmax $ymax $zmax]
         set activemesh [ReportActiveMesh]
         CopyMesh $frame $frame 0 "x:y:z" $clipstr 1
         if {$frame == $activemesh } RedrawDisplay
      }
   }

   if {$SmartDialogs} {
      set origcmd [$menuname entrycget $itemlabel -command]
      set menuorigfgcolor [$menuname entrycget $itemlabel -foreground]
      set menuorigafgcolor [$menuname entrycget $itemlabel -activeforeground]
      set menudeletecleanup \
         [list $menuname entryconfigure $itemlabel \
             -command $origcmd \
             -foreground $menuorigfgcolor\
             -activeforeground $menuorigafgcolor]
      $menuname entryconfigure $itemlabel \
         -command RaiseCommandConsole \
         -foreground $menucolor \
         -activeforeground $menucolor
   } else {
      $menuname entryconfigure $itemlabel -state disabled
      set menudeletecleanup \
         [list $menuname entryconfigure $itemlabel -state normal]
   }
   proc MenuDeleteCleanup {} $menudeletecleanup

   set title "[Oc_Main GetTitle] Console"
   $command_console eval [list console title $title]


   $command_console eval console show
}

########################### MENUS ###################################
proc SetupTearOff { rootbindkey menu win } {
    $menu entryconfigure 0 -state disabled
    set orig_action [bind . $rootbindkey]
    bind . $rootbindkey "Ow_RaiseWindow $win"
    bind $win <Control-Key-period> {Ow_RaiseWindow .}
    bind $win <Control-Key-space> { CenterPlotView } ;# This binding
    ## is rather Viewpoint tearoff specific, but since that is the
    ## only tearoff at present (6/2001), we'll leave it here for now.
    bind $win <Destroy> \
            "Ow_MoveFocus .
             bind . $rootbindkey {$orig_action}
             $menu entryconfigure 0 -state normal"
    focus $win
}


set menubar .menu
foreach {filemenu viewmenu optionmenu helpmenu} \
        [Ow_MakeMenubar . $menubar File View Options Help] {}

$filemenu add command -label "Open..." -underline 0 \
        -command { LaunchDialog $filemenu "Open..." OPEN }
bind . <Control-Key-o> { $filemenu invoke "Open..." }
$filemenu add command -label "Clear" -underline 0 -command { ClearFrame }
$filemenu add command -label "Save as..." -underline 0 \
        -command { LaunchDialog $filemenu "Save as..." SAVE }
bind . <Control-Key-s> { $filemenu invoke "Save as..." }
$filemenu add command -label "Print..." -underline 0 \
        -command { LaunchDialog $filemenu "Print..." PRINT }
bind . <Control-Key-p> { $filemenu invoke "Print..." }

$filemenu add separator
if {![Oc_Option Get Menu show_console_option _] && $_} {
   $filemenu add command -label "Show Console" \
       -command { LaunchCommandConsole $filemenu "Show Console" } -underline 1
}
$filemenu add separator
$filemenu add command -label "Write config..." -underline 0 \
        -command { LaunchDialog $filemenu "Write config..." WRITECONFIG }
$filemenu add separator
$filemenu add command -label "Exit" -command { exit } -underline 1
wm protocol . WM_DELETE_WINDOW { exit }

set viewaxes [menu $viewmenu.viewaxes -tearoff 1 \
        -tearoffcommand {SetupTearOff <Control-Key-v>}]
bind . <Control-Key-v> "$viewaxes invoke 0"  ;# Launches viewpoint menu
set viewaxis_menuselect $plot_config(viewaxis)
trace variable plot_config(viewaxis) w \
   { global viewaxis_menuselect ; \
     set viewaxis_menuselect $plot_config(viewaxis) ;# }
$viewaxes add radio -label "+x" \
        -variable viewaxis_menuselect -command {ChangeView "+x"}
$viewaxes add radio -label "+y" \
        -variable viewaxis_menuselect -command {ChangeView "+y"}
$viewaxes add radio -label "+z" \
        -variable viewaxis_menuselect -command {ChangeView "+z"}
$viewaxes add radio -label "-x" \
        -variable viewaxis_menuselect -command {ChangeView "-x"}
$viewaxes add radio -label "-y" \
        -variable viewaxis_menuselect -command {ChangeView "-y"}
$viewaxes add radio -label "-z" \
        -variable viewaxis_menuselect -command {ChangeView "-z"}

$viewmenu add command -label "Wrap Display" -command { WrapDisplay } \
        -underline 0
$viewmenu add command -label "Fill Display" \
    -command { FillDisplay ; CenterPlotView } -underline 0
$viewmenu add command -label "Center Display" \
    -command { CenterPlotView } -underline 0
$viewmenu add command -label "Rotate ccw" -underline 0 \
        -command {set DisplayRotation [expr {round($DisplayRotation+90)%360}]}
$viewmenu add command -label "Rotate cw" -underline 1 \
        -command {set DisplayRotation [expr {round($DisplayRotation-90)%360}]}
$viewmenu add command -label "reDraw" -underline 2 -command { RedrawDisplay }
$viewmenu add cascade -label "Viewpoint" -underline 0 -menu $viewaxes

$optionmenu add command -label "Configure..." -underline 0 \
        -command { Mmd_PlotSelectConfigure New _ \
                  -menu "$optionmenu {Configure...}" \
                  -menuraise $SmartDialogs \
                  -import_arrname plot_config \
                  -apply_callback SetPlotConfiguration
              }
bind . <Control-Key-c> { $optionmenu invoke "Configure..." }
$optionmenu add checkbutton -label "Control Bar" -underline 8 \
        -variable omfCtrlBarState
$optionmenu add checkbutton -label "Lock size" -underline 0 \
        -offvalue 1 -onvalue 0 -variable AllowViewportResize \
        -command UpdateViewportResizePermission

Ow_StdHelpMenu $helpmenu

set menuwidth [Ow_GuessMenuWidth $menubar]
set bracewidth [Ow_SetWindowTitle . [Oc_Main GetInstanceName]]
if {$bracewidth<$menuwidth} {
   set bracewidth $menuwidth
}
set brace [canvas .brace -width $bracewidth -height 0 -borderwidth 0 \
        -highlightthickness 0]
pack $brace -side top
# Resize root window when OID is assigned:
Oc_EventHandler New _ Net_Account NewTitle [subst -nocommands {
  $brace configure -width \
    [expr {%winwidth<$menuwidth ? $menuwidth : %winwidth}]
}]


####################### ADDITIONAL KEY BINDINGS #####################
# Allow the canvas frame to acquire keyboard focus, and prohibit
# canvas or canvas scrollbars from same (all necessary key bindings
# are bound to the canvas frame.
set cf $canvas_frame
$cf.xscroll configure -takefocus 0
$cf.yscroll configure -takefocus 0
$cf.canvas  configure -takefocus 0
$cf configure -takefocus 1

bind $cf <Key-Escape>      { OMF_RecoverDisplayState }

bind $cf <Key-Insert> {
    set newrate [expr {$plot_config(arrow,subsample)-1}]
    if {$newrate<0} {set newrate 0}
    SetArrowSubsampling $newrate
}
bind $cf <Key-Delete> {
    SetArrowSubsampling [expr {$plot_config(arrow,subsample)+1}]
}
bind $cf <Shift-Key-Insert> {
    SetArrowSubsampling [expr {$plot_config(arrow,subsample)/2.}]
}
bind $cf <Shift-Key-Delete> {
    SetArrowSubsampling [expr {$plot_config(arrow,subsample)*2}]
}
# Ditto for KeyPad; Note that the <Shift> keypad values are funky,
# and might be wrong for some keyboards.
catch { bind $cf <Key-KP_Insert> {
    set newrate [expr {$plot_config(arrow,subsample)-1}]
    if {$newrate<0} {set newrate 0}
    SetArrowSubsampling $newrate
}   }
catch {bind $cf <Key-KP_Delete> {
    SetArrowSubsampling [expr {$plot_config(arrow,subsample)+1}]
}   }
catch {bind $cf <Shift-Key-KP_0> {
    SetArrowSubsampling [expr {$plot_config(arrow,subsample)/2.}]
}   }
catch {bind $cf <Shift-Key-KP_Decimal> {
    SetArrowSubsampling [expr {$plot_config(arrow,subsample)*2}]
}   }

# Proc to scroll canvas from keyboard.  direction should be one of
# left|right|up|down, and stride should be one of units|pages.
proc KeyboardScrollCanvas { direction stride } {
    global canvas
    switch -- $direction {
        left     { CanvasConfine xview scroll -1 $stride }
        right    { CanvasConfine xview scroll  1 $stride }
        up       { CanvasConfine yview scroll -1 $stride }
        down     { CanvasConfine yview scroll  1 $stride }
        default  {
            Ow_FatalError "Invalid direction ($direction) detected in \
                    proc KeyboardScrollCanvas"
        }
    }
}
bind $cf <Key-Left>  { KeyboardScrollCanvas left  units }
bind $cf <Key-Right> { KeyboardScrollCanvas right units }
bind $cf <Key-Up>    { KeyboardScrollCanvas up    units }
bind $cf <Key-Down>  { KeyboardScrollCanvas down  units }
bind $cf <Shift-Key-Left>  { KeyboardScrollCanvas left  pages }
bind $cf <Shift-Key-Right> { KeyboardScrollCanvas right pages }
bind $cf <Shift-Key-Up>    { KeyboardScrollCanvas up    pages }
bind $cf <Shift-Key-Down>  { KeyboardScrollCanvas down  pages }
# Ditto for KeyPad; Note that the <Shift> keypad values are funky,
# and might be wrong for some keyboards.
catch {bind $cf <Key-KP_Left>  { KeyboardScrollCanvas left  units }}
catch {bind $cf <Key-KP_Right> { KeyboardScrollCanvas right units }}
catch {bind $cf <Key-KP_Up>    { KeyboardScrollCanvas up    units }}
catch {bind $cf <Key-KP_Down>  { KeyboardScrollCanvas down  units }}
catch {bind $cf <Shift-Key-KP_4> { KeyboardScrollCanvas left  pages }}
catch {bind $cf <Shift-Key-KP_6> { KeyboardScrollCanvas right pages }}
catch {bind $cf <Shift-Key-KP_8> { KeyboardScrollCanvas up    pages }}
catch {bind $cf <Shift-Key-KP_2> { KeyboardScrollCanvas down  pages }}

# Try to handle <Shift-Key-Tab> variants.  The [bind all <Shift-Tab>]
# parameter expands to the proc that is the current binding to
# <Shift-Tab> (probably 'tkTabToWindow [tk_focusPrev %W]').  Some
# X servers return a distinct keysym for the Shift-Tab event, e.g.,
# ISO_Left_Tab _instead_ Tab.  One can use the X utility program
# xev to see what keysyms are being generated.
#  One problem that occurs, is if one is running a binary on one
# machine, say, a Sun, that does not know the ISO_Left_Tab keysym
# (cf. the system <X11/keysymdef.h> file), and displaying on a
# different system, say recent Linux machine, that does.  Then the
# X server on the Linux machine generates the keysym number corresponding
# to ISO_LEFT_TAB (0xfe20), but the program built on the Sun doesn't
# know the symbolic interpretation for that keysym.  Therefore we
# provide a numeric binding if the symbolic binding is not known.  (I'm
# assuming the symbolic bindings, if defined, are the same across all
# platforms.)
#  In addition to xev, the following wish script is useful in diagnosing
# and solving such problems:
#
# bind . <KeyPress> {puts stdout {%%K=%K, %%A=%A, %%k=%k, %%s=%s, %%N=%N.}}
#
# This prints to stdout the  keysym (%K), ASCII character (%A), keycode (%k)
# state (%s, which for key events is modifier information, like shift key
# status), and keysym as a decimal number (%N).  The last should be defined
# even when the first isn't.          -mjd, Sep-1997
#
if {[catch {bind all <Shift-Key-ISO_Left_Tab> [bind all <Shift-Tab>]}]} {
    # ISO_Left_Tab probably not defined.  Try numeric binding
    catch {
        bind all <Shift-Any-Key> \
                "+if {\"%N\"==65056} \{[bind all <Shift-Tab>]\}"
        # ISO_Left_Tab = 0xFE20 = 65056
        # Man, talk about quoting hell.  We need the braces on the
        # second term of the "if" to keep "if" happy, but we need
        # to get the [bind all <Shift-Tab>] expanded at bind time to
        # get the imbedded %W (from inside [bind all <Shift-Tab]) in
        # place for binding substitution...
    }
}
if {[catch {bind all <Shift-Key-3270_BackTab> [bind all <Shift-Tab>]}]} {
    # 3270_BackTab probably not defined.  Try numeric binding
    catch {
        bind all <Shift-Any-Key> \
                "+if {\"%N\"==64773} \{[bind all <Shift-Tab>]\}"
        # 3270_BackTab = 0xFD05 = 64773
        # See the notes in the previous stanza about "quoting hell."
    }
}

# Bindings on root window.  These use to be tied to the canvas,
# but aren't otherwise used, so we moved them up to '.'.
proc Zoom { amt } {
    global plot_config
    SetScale [expr {$amt*$plot_config(misc,zoom)}]
}
bind . <Key-Prior>       { Zoom $SmallZoom }
bind . <Shift-Key-Prior> { Zoom $LargeZoom }
bind . <Key-Next>        { Zoom [expr {1./$SmallZoom}] }
bind . <Shift-Key-Next>  { Zoom [expr {1./$LargeZoom}] }
## NOTE: SunOS apparently doesn't have Page_Up & Page_Down.  For the
## record, Prior == Page_Up, and Next == Page_Down.
# Ditto for KeyPad; Note that the <Shift> keypad values are funky,
# and might be wrong for some keyboards.
catch {bind . <Key-KP_Prior>    { Zoom $SmallZoom }}
catch {bind . <Shift-Key-KP_9>  { Zoom $LargeZoom }}
catch {bind . <Key-KP_Next>     { Zoom [expr {1./$SmallZoom}] }}
catch {bind . <Shift-Key-KP_3>  { Zoom [expr {1./$LargeZoom}] }}

bind . <Control-Key-f> { FillDisplay ; CenterPlotView }
bind . <Control-Key-w> { WrapDisplay }

# Bind <Control-Key-Home> to HomeDisplay. We do this on the
# root window, but Enter and Scale widgets have <Key-Home> bindings
# that will trigger on <Control-Key-Home> unless we provide an
# explicit <Control-Key-Home> binding.
foreach tag {. Entry Scale} {
   bind $tag <Control-Key-Home> HomeDisplay
}
# We also want <Key-Home> to trigger HomeDisplay generally, except in
# Entry and Scale widgets where that would trigger confusion with the
# widget default behavior. One complication is that in Tk 8.5 and
# earlier there are bindings on <Key-Home> directly, but in Tk 8.6 those
# are replaced with bindings on the virtual event <<LineStart>>
# instead. Either way, append a break to short-circuit the root window
# binding. Entry widgets also have a default binding on <Shift-Key-Home>
# or <<SelectLineStart>>, so handle those too.
bind . <Key-Home>             { HomeDisplay }
catch {bind . <Key-KP_Home>   { HomeDisplay }}
foreach tag {Entry Scale} {
   if {![string match {} [bind $tag <<LineStart>>]]} {
      bind $tag <<LineStart>> {+break}
      if {![string match {} [bind $tag <<SelectLineStart>>]]} {
         bind $tag <<SelectLineStart>> {+break}
      }
   } else {
      bind $tag <Key-Home> {+break}
      if {![string match {} [bind $tag <Shift-Key-Home>]]} {
         bind $tag <Shift-Key-Home> {+break}
      }
   }
}

bind . <Control-Key-d> { RedrawDisplay }

bind . <Control-Key-r> { ;# Ctrl-r
    set DisplayRotation [expr {round($DisplayRotation+90)%%360}]
}
bind . <Control-Key-R> { ;# Shift-Ctrl-R
    set DisplayRotation [expr {round($DisplayRotation-90)%%360}]
}

bind . <Control-Key-space> { CenterPlotView }

###################### ADDITIONAL MOUSE BINDINGS ####################
### Canvas rectangle zoom-in
# Zoom box foreground line color, A -> enlarge, B -> contract
set omfZoomBoxForeColorA [Ow_GetRGB red]
set omfZoomBoxForeColorB [Ow_GetRGB MediumBlue]
# For reasons unknown to me (mjd 97/2/22), when drawing on an 8-bit
# pseudocolor visual with a shared colormap, under interactive
# conditions (redrawing zoom boxes with mouse motion) it took noticeable
# longer to erase and draw "red" boxes than "MediumBlue" boxes.  I
# discovered that the difference goes away if "red" is changed to, e.g.,
# "IndianRed", or to the form #xxx.  I also discovered that you can
# apparently find out what color you will actually get with the command
# "winfo rgb . color", where color can be in any of the usual Tk color
# formats.  For example,
#   %winfo rgb . MediumBlue
#  > 0 0 52685
#   %winfo rgb . #FF0000
#  > 65535 0 0
#   %winfo rgb . #FE0000
#  > 65535 0 0
#
set omfZoomBoxForeThick  1       ;# Zoom box foreground line thickness
set omfZoomBoxBackColor  white   ;# Zoom box background line color
set omfZoomBoxBackThick  3       ;# Zoom box background line thickness
set omfZoomBoxMinWidth   5;# This must be >0.
set omfZoomBoxArrowSize  12
set omfZoomDirection     0       ;# When active, should be +1 => enlarge
                                  #                     or -1 => contract
set omfZoomBoxLoiterTime 3000    ;# Number of milliseconds to keep zoom box
                                  # visible after zooming.
proc OMF_InitZoomBox { v1x v1y direction } {
    global canvas omfP1x omfP1y omfZoomDirection
    if {$omfZoomDirection!=0} return  ;# Zoom (other direction) already
    set omfZoomDirection $direction   ;# in progress
    set omfP1x [$canvas canvasx $v1x]
    set omfP1y [$canvas canvasy $v1y]
}

proc OMF_StrokeZoomBox { x1 y1 x2 y2 boxtag} {
    global omfZoomBoxBackColor omfZoomBoxBackThick
    global omfZoomBoxForeThick omfZoomDirection
    global omfZoomBoxForeColorA omfZoomBoxForeColorB
    global canvas
    if {$omfZoomDirection==0} return
    if {$omfZoomDirection>0} { set forecolor $omfZoomBoxForeColorA } \
            else             { set forecolor $omfZoomBoxForeColorB }
    $canvas create rectangle $x1 $y1 $x2 $y2 \
            -outline $omfZoomBoxBackColor -width $omfZoomBoxBackThick \
            -tag $boxtag
    $canvas create rectangle $x1 $y1 $x2 $y2 \
            -outline $forecolor -width $omfZoomBoxForeThick \
            -tag $boxtag
}

proc OMF_DrawZoomBox { v2x v2y } {
    global plot_config zoomwidget
    global omfP1x omfP1y canvas
    global omfZoomBoxMinWidth
    global omfZoomDirection omfZoomBoxForeColorA omfZoomBoxForeColorB
    if {$omfZoomDirection==0} return
    if {$omfZoomDirection>0} {
        set forecolor $omfZoomBoxForeColorA
    } else {
        set forecolor $omfZoomBoxForeColorB
    }
    global omfZoomBoxBackColor omfZoomBoxForeThick omfZoomBoxBackThick
    global omfZoomBoxArrowSize
    $canvas delete zoombox ;# erase old zoom box
    set p2x [$canvas canvasx $v2x]
    set p2y [$canvas canvasy $v2y]
    set zw [expr {abs($p2x-$omfP1x)}]
    set zh [expr {abs($p2y-$omfP1y)}]
    if {$zw<$omfZoomBoxMinWidth || $zh<$omfZoomBoxMinWidth} {
        $zoomwidget Set $plot_config(misc,zoom)
        return
    }
    OMF_StrokeZoomBox $omfP1x $omfP1y $p2x $p2y zoombox

    # Determine which side (x- or y-) will limit expansion
    global plot_config
    set vw $plot_config(misc,width)
    set vh $plot_config(misc,height)
    if {$omfZoomDirection>0} {
        # Make adjustments to viewport dimensions under the
        # assumption that any non-active scroll bars will be
        # turn on as a result of the zoom.
        global XScrollState YScrollState
        if {!$XScrollState} {
            set vh [expr {$vh-$plot_config(misc,scrollcrossdim)}]
        }
        if {!$YScrollState} {
            set vw [expr {$vw-$plot_config(misc,scrollcrossdim)}]
        }
    }
    if {$vw<1} {set vw 1} ; if {$vh<1} {set vh 1} ;# Safety
    set xlimit [expr {$vw*$zh}]; set xfudge [expr {2*$vw}]
    set ylimit [expr {$vh*$zw}]; set yfudge [expr {2*$vh}]
    if {$omfZoomDirection<0} {
        set temp $xlimit; set xlimit $ylimit; set ylimit $temp;
        set temp $xfudge; set xfudge $yfudge; set yfudge $temp;
    }

    # Draw zoom direction arrows on expansion limit sides
    if {$omfP1x<$p2x} { set xmin $omfP1x; set xmax $p2x    } \
            else      { set xmin $p2x;    set xmax $omfP1x }
    if {$omfP1y<$p2y} { set ymin $omfP1y; set ymax $p2y    } \
            else      { set ymin $p2y;    set ymax $omfP1y }
    set xmid [expr {round(($xmin+$xmax)/2.)}]
    set ymid [expr {round(($ymin+$ymax)/2.)}]
    set arrlength [expr {round($omfZoomBoxArrowSize)}]
    if {$omfZoomDirection>=0} { set offset $arrlength } \
            else              { set offset -$arrlength }
    set arrheadsize [expr {round($arrlength/2)}]
    set arrheadshape "$arrheadsize  $arrheadsize [expr {$arrheadsize/2}]"
    if { $ylimit < $xlimit + $xfudge} {
        # Draw arrows on top & bottom sides
        $canvas create line \
                $xmid [expr {$ymin+$offset}] $xmid $ymin \
                -fill $forecolor -width $omfZoomBoxForeThick \
                -arrow last -arrowshape $arrheadshape -tag zoombox
        $canvas create line \
                $xmid [expr {$ymax-$offset}] $xmid $ymax \
                -fill $forecolor -width $omfZoomBoxForeThick \
                -arrow last -arrowshape $arrheadshape -tag zoombox
    }
    if { $xlimit < $ylimit + $yfudge} {
        # Draw arrows on left & right sides
        $canvas create line \
                [expr {$xmin+$offset}] $ymid $xmin $ymid \
                -fill $forecolor -width $omfZoomBoxForeThick \
                -arrow last -arrowshape $arrheadshape -tag zoombox
        $canvas create line \
                [expr {$xmax-$offset}] $ymid $xmax $ymid \
                -fill $forecolor -width $omfZoomBoxForeThick \
                -arrow last -arrowshape $arrheadshape -tag zoombox
    }
    # Reflect anticipated zoom state in scale widgets
    if {$vw>0 && $vh>0 && $zw>0 && $zh>0} { ;# Safety check
        if {$ylimit < $xlimit} {
            set scale [expr {double($vh)/double($zh)}]
        } else {
            set scale [expr {double($vw)/double($zw)}]
        }
        if {$omfZoomDirection<0} { set scale [expr {1./double($scale)}]}
        set newsize [format "%4.2f" [expr {$plot_config(misc,zoom)*$scale}]]
        $zoomwidget SetEdit $newsize
    }
}
proc OMF_DoZoom { v2x v2y } {
    global canvas omfP1x omfP1y
    global plot_config
    global omfZoomBoxMinWidth omfZoomDirection
    global plot_config
    global XScrollState YScrollState
    global SliceCompat
    if {$omfZoomDirection==0} return

    set clean_return "$canvas delete zoombox ; return"

    set p2x [$canvas canvasx $v2x]
    set p2y [$canvas canvasy $v2y]
    set zwidth  [expr {abs($p2x-$omfP1x)}]
    set zheight [expr {abs($p2y-$omfP1y)}]
    if {$zwidth<$omfZoomBoxMinWidth \
            || $zheight<$omfZoomBoxMinWidth} {
        unset omfP1x omfP1y
        set omfZoomDirection 0
        eval $clean_return
    }
    set vwidth  $plot_config(misc,width)
    set vheight $plot_config(misc,height)
    set xscrollstate_orig $XScrollState
    set yscrollstate_orig $YScrollState
    if {$omfZoomDirection>0} {
        # Make adjustments to viewport dimensions under the
        # assumption that any non-active scroll bars will be
        # turned on as a result of the zoom.
        if {!$XScrollState} {
            set vheight [expr {$vheight-$plot_config(misc,scrollcrossdim)}]
        }
        if {!$YScrollState} {
            set vwidth [expr {$vwidth-$plot_config(misc,scrollcrossdim)}]
        }
    }
    set xscale [expr {double($vwidth)/double($zwidth)}]
    set yscale [expr {double($vheight)/double($zheight)}]
    if {$xscale<0.001 || $xscale>1000  || $yscale<0.001 || $yscale>1000} {
        # Safety check
        unset omfP1x omfP1y
        set omfZoomDirection 0
        eval $clean_return
    }
    if {$omfZoomDirection<0} {
        set xscale [expr {1./double($xscale)}]
        set yscale [expr {1./double($yscale)}]
    }
    if {$xscale<$yscale} {
        set scale $xscale
    } else {
        set scale $yscale
    }
    # Note: $scale is magnification relative to current
    #       $plot_config(misc,zoom), which is magnification
    #       relative to mesh scaling.

    OMF_SaveDisplayState
    set newzoom [expr {floor($scale*$plot_config(misc,zoom)*100.)/100.}]
    if {$newzoom>0.} {
        set plot_config(misc,zoom) $newzoom
        SetZoom $plot_config(misc,zoom)
    } else {
        unset omfP1x omfP1y
        set omfZoomDirection 0
        eval $clean_return
    }

    global watchcursor_windows
    Ow_PushWatchCursor $watchcursor_windows
    $canvas delete zoombox ;# Put old zoom box erase command on event
    ## loop *after* 'update idletask' call inside Ow_PushWatchCursor.
    DrawFrame $canvas $SliceCompat
    PackViewport

    # Center middle of zoom box.  If a scrollbar has been turned
    # on or off, the canvas widget won't know about it until after
    # an update idletask call.  To remove the display+scroll action,
    # handle this case by adjusting the centering request.
    set a1x [expr {$omfP1x*$scale}]
    set a1y [expr {$omfP1y*$scale}]
    set a2x [expr {$p2x*$scale}]
    set a2y [expr {$p2y*$scale}]
    set acenterx [expr {($a1x+$a2x)/2.}]
    set acentery [expr {($a1y+$a2y)/2.}]
    set xscrolladj [expr {($XScrollState-$xscrollstate_orig)
                          *$plot_config(misc,scrollcrossdim)/2.}]
    set yscrolladj [expr {($YScrollState-$yscrollstate_orig)
                          *$plot_config(misc,scrollcrossdim)/2.}]
    CenterPlotView [expr {$acenterx+$xscrolladj}] \
        [expr {$acentery+$yscrolladj}]

    # Draw zoom "loiter" box
    if {$omfZoomDirection>0} {
        # Adjust coordinates so box fits inside viewport
        if {$a2x<$a1x} {set temp $a1x; set a1x $a2x; set a2x $temp}
        if {$a2y<$a1y} {set temp $a1y; set a1y $a2y; set a2y $temp}
        foreach {xmin ymin xmax ymax} [GetCanvasBox] {}
        set xmax [expr {$xmax - 1.0}]
        set ymax [expr {$ymax - 1.0}]
        if {$a1x<$xmin} {set a1x $xmin}
        if {$a1y<$ymin} {set a1y $ymin}
        if {$a2x>$xmax} {set a2x $xmax}
        if {$a2y>$ymax} {set a2y $ymax}
        set a1x [expr  {ceil($a1x)}] ;   set a1y [expr  {ceil($a1y)}]
        set a2x [expr {floor($a2x)}] ;   set a2y [expr {floor($a2y)}]
    }
    global UpdateViewCount
    set boxtag "afterzoombox$UpdateViewCount"
    # The above creates an essentially unique tagname for the zoom
    # loiter box.  If all loiter boxes shared the same tag, then if
    # the user did a second zoom too quick, then the second zoom loiter
    # box could be deleted by the command deleting the first.  OK, this
    # is not a serious problem, but it's easy to fix, and I'm here now
    # anyway...
    OMF_StrokeZoomBox $a1x $a1y $a2x $a2y $boxtag

    # Clean up
    unset omfP1x omfP1y
    set omfZoomDirection 0
    global omfZoomBoxLoiterTime
    after $omfZoomBoxLoiterTime "$canvas delete $boxtag"
    Ow_PopWatchCursor
}

proc CancelZoom {} {
    # Wipes out any in-progress zoom
    global omfZoomDirection
    if {$omfZoomDirection==0} return

    global canvas omfP1x omfP1y
    unset omfP1x omfP1y
    set omfZoomDirection 0
    $canvas delete zoombox

    global zoomwidget plot_config
    $zoomwidget Set $plot_config(misc,zoom)
}

proc CenterPlotView { {px {}} {py {}}} {
    foreach {xmin ymin xmax ymax} [GetPlotBox] {}
    if { $xmax<=$xmin || $ymax<=$ymin } { return }
    if {[string match {} $px]} { set px [expr {($xmin+$xmax)/2.}] }
    if {[string match {} $py]} { set py [expr {($ymin+$ymax)/2.}] }
    set xcenter [expr {($px-$xmin)/($xmax-$xmin)}]
    if {$xcenter<0} {set xcenter 0}
    if {$xcenter>1} {set xcenter 1}
    set ycenter [expr {($py-$ymin)/($ymax-$ymin)}]
    if {$ycenter<0} {set ycenter 0}
    if {$ycenter>1} {set ycenter 1}
    SetScrollCenter $xcenter $ycenter
}
proc CenterScreenView { vx vy } {
    global canvas
    CenterPlotView [$canvas canvasx $vx] [$canvas canvasy $vy]
}
# Following catch is for Tcl 7.5/Tk 4.1, which doesn't support virtual
# events.
catch {
   bind $canvas <<Ow_LeftButton>> { OMF_InitZoomBox %x %y 1 }
   bind $canvas <<Ow_RightButton>> { OMF_InitZoomBox %x %y -1 }
   bind $canvas <<Ow_LeftButtonMotion>> [Oc_SkipWrap {OMF_DrawZoomBox %x %y}]
   bind $canvas <<Ow_RightButtonMotion>> [Oc_SkipWrap {OMF_DrawZoomBox %x %y}]
   bind $canvas <ButtonRelease> { OMF_DoZoom %x %y }
   bind $canvas <<Ow_ShiftLeftButtonRelease>> { CancelZoom }
   bind $canvas <<Ow_ShiftRightButtonRelease>> { CancelZoom }
   bind $canvas <<Ow_MiddleButtonRelease>> {
      CancelZoom ; CenterScreenView %x %y
   }
   bind $canvas <<Ow_MiddleButton>> {
      continue ;# Make this a nop so is doesn't interact with other bindings.
   }
   bind $canvas <<Ow_ShiftLeftButton>> { OMF_ShowPosition %x %y }
   bind $canvas <<Ow_ShiftRightButton>> { OMF_ShowPosition %x %y 1 }
   bind $canvas <<Ow_ShiftLeftButtonRelease>> +OMF_UnshowPosition
   bind $canvas <<Ow_ShiftRightButtonRelease>> +OMF_UnshowPosition
}

proc OMF_ShowPosition { wx wy {verbose 0}} {
    set boxmargin 5
    set dotrad 2

    # Convert (wx,wy) to display coords
    global canvas
    set odx [$canvas canvasx $wx]
    set ody [$canvas canvasy $wy]

    # Convert to mesh coordinates, with dummy z value
    foreach {x y z} [GetMeshCoordinates $odx $ody 0] { break }

    # Grab z value from plot_config, which uses mesh coordinates,
    # except when it doesn't.
    global plot_config
    set z $plot_config(viewaxis,center)
    set viewaxis $plot_config(viewaxis)
    if {[string compare {-} [string index $viewaxis 0]]==0} {
        set z [expr {-1*$z}]
    }

    # Get vector data on closest point
    foreach {x y z vx vy vz} [FindMeshVector $x $y $z] break
    foreach {dx dy dz} [ApplyAxisTransform +z $viewaxis $x $y $z] break
    foreach {dx dy dz} [GetDisplayCoordinates $dx $dy $dz] break

    # Form display string
    if {!$verbose} {
        set sigfigs 4 ;# Number of digits in output display
        set x [ConvertToEng $x $sigfigs]
        set y [ConvertToEng $y $sigfigs]
        set z [ConvertToEng $z $sigfigs]
        set vx [ConvertToEng $vx $sigfigs]
        set vy [ConvertToEng $vy $sigfigs]
        set vz [ConvertToEng $vz $sigfigs]
        set displaystr "($vx,$vy,$vz)"
    } else {
        set sigfigs 8 ;# Number of digits in output display
        set x [ConvertToEng $x $sigfigs]
        set y [ConvertToEng $y $sigfigs]
        set z [ConvertToEng $z $sigfigs]
        set vx [ConvertToEng $vx $sigfigs]
        set vy [ConvertToEng $vy $sigfigs]
        set vz [ConvertToEng $vz $sigfigs]
        set valueunits [string trim [GetDataValueUnit]]
        if {[string match unknown $valueunits]} {
            set valueunits {}
        }
        if {[string length $valueunits]>0} {
            set valueunits " $valueunits"
        }
        set meshunits [string trim [GetMeshSpatialUnitString]]
        if {[string match unknown $meshunits]} {
            set meshunits {}
        }
        if {[string length $meshunits]>0} {
            set meshunits " $meshunits"
        }
        set displaystr "($vx,$vy,$vz)$valueunits\n@($x,$y,$z)$meshunits"
    }

    # Write display string to canvas
    if {$ody<$dy} {
        set tdy [expr {$dy-$dotrad}]
        set anchor s
    } else {
        set tdy [expr {$dy+$dotrad}]
        set anchor n
    }
    set bgclr yellow; set txtclr black; set bdryclr black ;# Colors
    set textid [$canvas create text $dx $tdy -text $displaystr \
            -fill $txtclr -justify center -anchor $anchor -tags position]
    foreach {bx1 by1 bx2 by2} [$canvas bbox $textid] { break }
    set bx1 [expr {$bx1-$boxmargin}]
    set bx2 [expr {$bx2+$boxmargin}]
    set by1 [expr {$by1-$boxmargin}]
    set by2 [expr {$by2+$boxmargin}]
    set boxid [$canvas create rectangle $bx1 $by1 $bx2 $by2 \
                       -fill $bgclr -outline $bdryclr -width 2 \
                       -tags position]
    $canvas raise $textid

    # Move to fit in display
    set xadj 0 ; set yadj 0
    foreach {cx1 cy1 cx2 cy2} [GetCanvasBox] { break }
    if {$bx2-$bx1 >= $cx2-$cx1} {
        set xadj [expr {(($cx1+$cx2)-($bx1+$bx2))/2.}]
    } else {
        if {$bx1<$cx1} { set xadj [expr {$cx1-$bx1}] }
        if {$bx2>$cx2} { set xadj [expr {$cx2-$bx2}] }
    }
    if {$by2-$by1 >= $cy2-$cy1} {
        set yadj [expr {(($cy1+$cy2)-($by1+$by2))/2.}]
    } else {
        if {$by1<$cy1} { set yadj [expr {$cy1-$by1}] }
        if {$by2>$cy2} { set yadj [expr {$cy2-$by2}] }
    }
    # Move to fit on plot.  Fit x/y2 first, so that if text
    # box is too big to fit on plot, we honor x/y1 limits.
    foreach {px1 py1 px2 py2} [GetPlotBox] { break }
    if {$bx2+$xadj>$px2} { set xadj [expr {$px2-$bx2}] }
    if {$bx1+$xadj<$px1} { set xadj [expr {$px1-$bx1}] }
    if {$by2+$yadj>$py2} { set yadj [expr {$py2-$by2}] }
    if {$by1+$yadj<$py1} { set yadj [expr {$py1-$by1}] }

    $canvas move $boxid $xadj $yadj
    $canvas move $textid $xadj $yadj

    $canvas create rectangle \
            [expr {$dx-$dotrad}] [expr {$dy-$dotrad}] \
            [expr {$dx+$dotrad}] [expr {$dy+$dotrad}] \
            -fill red -outline blue -width 1 -tags position
}

proc OMF_UnshowPosition {} {
    global canvas
    $canvas delete position
}


########################### TOP_LEVEL PACKING #######################
pack $canvas_frame -side top -fill both -expand 1
pack propagate $canvas_frame 0
# Note 1: Any extra vertical spacing is divvied up equally among
#   all the first level widgets that have the -expand 1 option.
#   The net result is that frame-canvas resizing won't work properly
#   if any widget besides $canvas_frame has -expand 1.
# Note 2: The outer frame is used as a geometry size support for all
#   its children.  Therefore it is important that propagation be
#   turned off.

# Ctrlbar
if {$omfCtrlBarState} {
    pack $ctrlbar -side top -fill x -expand 0 -before $canvas_frame
}

########################### INITIALIZATION ##########################
# Determine minimum viewport size, and get fixed widget geometry values
OMF_SetGeometryRequirements
wm deiconify .

# Initialize global variables
set DefaultViewportWidth  \
        [expr {$plot_config(misc,defaultwindowwidth)-10}]
set DefaultViewportHeight \
        [expr {$plot_config(misc,defaultwindowheight)-$RootHeadHeight-20}]
# The '-10' & '-20' in the above lines is just so we don't fill up the
# *whole* display screen (in case the display *is* only 640x480).

# Bind canvas resize proc
bind $canvas_frame <Configure> {
    set border \
       [expr {2*[%W cget -borderwidth]+2*[%W cget -highlightthickness]}]
    ResizeViewport [expr {%w-$border}] [expr {%h-$border}]
    # Note: The <Configure> resize request *includes* both -borderwidth
    #       & -highlightthickness!
}
# The <Unmap> event is triggered when the user resizes the
# display to 0, or if the root window is minimized.  In the
# former case, resize window and zoom to defaults.
bind $canvas_frame <Unmap> {
    if {[winfo ismapped .]} {
	FillDisplay $DefaultViewportWidth $DefaultViewportHeight 0
	WrapDisplay
    }
}

# Extract working directory and filename from command line argument
set firstpath {}
if {[file isdirectory $firstfile]} {
    # No file, just directory
    set firstpath $firstfile
    set firstfile {}
} else {
    # File and potentially directory
    set firstpath [file dirname $firstfile]
    set firstfile [file tail $firstfile]
}

# Change to requested working directory
if {[catch {cd $firstpath} msg]} {
    Ow_NonFatalError "WARNING: $msg"
    set firstfile {}  ;# Presumably we don't want to load
    ## $firstfile out of current directory
}

update  ;# This update is needed for some reason to prevent a
## fatal error in the case where firstfile is non-empty but
## points to a non-existent file.  This situation triggers
## a modal Ow_Dialog warning (actually, an "info" message)
## that generates a fatal error under Windows XP, with
## ActiveTcl 8.4.14.

# Load first file
if {[string match {} $firstfile]} {
    ReadFile {} {} 1 1 0
} else {
    set plot_config(misc,viewportwidth) $DefaultViewportWidth
    set plot_config(misc,viewportheight) $DefaultViewportHeight
    # Select autodisplay settings based on initial plot_config values
    set ad(sample)    0  ;# Use default sampling settings
    if {$startup_plot_config(misc,datascale)!=0} {
	set ad(datascale) 0
	set plot_config(misc,datascale) $startup_plot_config(misc,datascale)
    } else {
	set ad(datascale) 1
    }
    if {$startup_plot_config(misc,zoom)!=0} {
	set ad(zoom)      0
	set plot_config(misc,zoom) $startup_plot_config(misc,zoom)
    } else {
	set ad(zoom)      1
    }
    set ad(viewaxis)  0
    set ad(slice)     0
    ReadFile $firstfile {} \
	    $plot_config(misc,viewportwidth)  \
	    $plot_config(misc,viewportheight) \
	    $DisplayRotation ad
}

set xrel 0.5;  set yrel 0.5;  set zrel 0.5
if {[info exists startup_plot_config(misc,centerpt)] \
        && ![string match {} $startup_plot_config(misc,centerpt)]} {
    foreach {xc yc zc} $startup_plot_config(misc,centerpt) break
    foreach {xmin ymin zmin xmax ymax zmax} [GetMeshRange] break
    foreach {xmin ymin zmin} [ApplyAxisTransform \
            $plot_config(viewaxis) +z $xmin $ymin $zmin] break
    foreach {xmax ymax zmax} [ApplyAxisTransform \
            $plot_config(viewaxis) +z $xmax $ymax $zmax] break
    if {[string match {-?} $plot_config(viewaxis)]} {
        # Negative view axis
        set tmp $xmin ; set xmin $xmax ; set xmax $tmp
        set tmp $ymin ; set ymin $ymax ; set ymax $tmp
        set tmp $zmin ; set zmin $zmax ; set zmax $tmp
    }
    if {$xmax!=$xmin} {
        catch {set xrel [expr {($xc-$xmin)/($xmax-$xmin)}]}
    }
    if {$ymax!=$ymin} {
        catch {set yrel [expr {($yc-$ymin)/($ymax-$ymin)}]}
    }
    if {$zmax!=$zmin} {
        catch {set zrel [expr {($zc-$zmin)/($zmax-$zmin)}]}
    }
} elseif {[info exists startup_plot_config(misc,relcenterpt)] \
        && ![string match {} $startup_plot_config(misc,relcenterpt)]} {
    foreach {xrel yrel zrel} $startup_plot_config(misc,relcenterpt) break
}
SetRelativeDisplayCenter $xrel $yrel $zrel
$slicewidget ScaleForce commit ;# Adjust slice center to
## value compatible with scale resolution

# Initialize and show coords display
bind $coords_win <Configure> {}  ;# Not needed
WaitVisible $coords_win
InitCoords $coords_win
UpdateCoordDisplay $DisplayRotation
after idle UpdateCoordColors
## All the axes except the first are drawn through the
## after idle event loop.

# Release firstpath & firstfile variables
unset firstpath firstfile

focus $canvas_frame

# Initialize omfSavedDisplayState
OMF_SaveDisplayState


after idle Ow_SetIcon .
