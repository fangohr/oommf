# FILE: graphwin.tcl
#
# Entry graph canvas sub-widget
#
# Last modified on: $Date: 2014/01/21 05:47:04 $
# Last modified by: $Author: donahue $
#
# The usual stacking order is:
#        Key (if any)
#        Horizontal and vertical rules
#        Axes tics and labels
#        Border matting
#        Curves (if any)
#        Base axes box
#
# Two sets of coordinates are used: display (canvas, pixel) coordinates,
# and graph (data) coordinates.  Relevant variables for display coords
# are lmargin, tmargin, rmargin, bmargin, plotwidth and plotheight. Here
# lmargin+plotwidth+rmargin = [$graph cget -width], and similarly for
# canvas height.  The graph limits in data coords are held in xgraphmin,
# ygraphmin, y2graphmin, xgraphmax, ygraphmax, y2graphmax. The conversion
# between the two coords
# (x,y)->(cx,cy) is given by (cx,cy) = (xoff,yoff)+ (x*xscale,y*yscale)
#                                or (xoff,y2off) + (x*xscale,y*y2scale).
#
#
# Oc_Class GraphWin methods:
#
#    callback method ResizeCanvas { width height }
#       Bound to the $graph <Configure> event.  Includes an
#      'update idletask' call.
#    method SetSmoothState { newstate }
#       If smooth state is 1, then use the "-smooth" option to 'canvas
#       create line' when plotting curves.  In this case the curves
#       will not generally pass through the interior data points.
#    method SetKeyState { newstate }
#    private method HideKey {}
#    method GetKeyPosition {}
#       Returns coords of upper righthand corner of current key, relative
#       to upper righthand corner of canvas.  (In particular, xpos will
#       generally be negative.)  The return value can be later fed back
#       into SetKeyState.
#    private method DrawKey { { coords {} } }
#       Routines for changing key (legend) display.
#    private method GetUserCoords { cx cy yaxis }
#       Converts from canvas to user coordinates
#    callback method ShowPosition { cx cy button state convert tags }
#    callback method UnshowPosition { state }
#       Routines for displaying position information.
#    callback method InitZoomBox { x y button shiftmod }
#    callback method DrawZoomBox { x2 y2 }
#    callback method DoZoomBox { x2 y2 }
#       Routines for mouse-based graph scaling
#    public method Zoom { factor }
#       Rescales about center of display.
#    private method DrawRules { x y }
#       Draws vertical rule at x=$x, and a horizontal rule at y=$y
#    callback method DragInit { obj x y button state }
#    callback method Drag { obj x y button state }
#    callback method ZoomBlock { script }
#    callback method BindWrap { script }
#       Bindings to allow the dragging of the key and rules.
#    method SetMargins { left right top bottom }
#       Sets minimum margin values, in pixels.
#    method DrawAxes {}
#       Clears canvas, then renders title, axes labels, axes,
#       horizontal and vertical rules, and key.  Also sets
#       {t,b,l,r}margin, plotwidth/plotheight, and {x,y}scale,
#       {x,y}off values.
#    method NewCurve { curvelabel yaxis}
#    method DeleteCurve { curvelabel yaxis }
#    method AddDataPoints { curvelabel yaxis newpts }
#       The above 3 methods are the only public methods that
#       access individual curves. In the internal data structure
#       "curve", each curve is uniquely identified by the string
#       "$curvelabel,$yaxis".
#    method DeleteAllCurves {}
#    private method DeleteCurveByName { curvename }
#       Private DeleteCurve interface that uses "curvename"
#       directly, as opposed to the public DeleteCurve method
#       that constructs "curvename" from "curvelabel" and "yaxis".
#    private method GetCurveId { curvename } { id }
#       Curve creation, deletion and identification routines.
#       Adds new points to data list, and marks display list
#       as out-of-date.  "newpts" is a list of the form
#       "x1 y1 x2 y2 x3 y3 ...".  Set x_i == {} to request
#       a break in the curve between points i-1 and i+1.
#       The return value is a bounding box for new data,
#       in graph coords, unless all x_i in newpts are {}, in
#       which case the return value is {}.
#    method ResetSegmentColors {}
#       Empties all curve($cid,segcolor) lists, and resets global
#       segment count (segcount) to 0.
#    method DeleteAllData {}
#       Keeps curve structures intact, but clears all data,
#       and removes curves from display.
#    private method AddDisplayPoints { curveid datapts }
#       Called by UpdateDisplayList to append "datapts" to current
#       curveid curve piece.
#    private method UpdateDisplayList { curveid }
#       Makes the curve display list up-to-date with respect to
#       the data list.  The return value is the index into the
#       display list necessary to update the graphed display, i.e.,
#       the 'startindex' value appropriate for calling PlotCurve.
#       A value of -1 indicates that the curve should be completely
#       redrawn, and -2 occurs if no points were added to the display
#       list.
#    private method PlotCurve { curveid { startindex 0 }}
#       Draws specified portion (startindex to 'end') of specified
#       curve onto the canvas.
#    method GetCurveList { yaxis }
#       Returns an ordered list of all non-hidden curves on specified
#       yaxis (1 or 2), by curvelabel
#    method DrawCurves {{update_only 1}}
#       Invokes PlotCurve for all non-hidden curves.  If update_only
#       is set, then only those portions of the curves not yet moved
#       to the display list are drawn.  After calling this function,
#       all non-hidden curve 'dispcurrent' elements are true.
#    private method InvalidateDisplay { curvelist }
#       Marks entire display portion of each curve in curvelist as
#       invalid.  Pass in $idlist to invalidate all curve displays.
#    private method GetDataExtents {}
#       Returns [list $xmin $xmax
#                     $ymin $ymax $y2min $y2max]
#       across all non-hidden curves.
#    private proc RoundExtents {xmin xmax ymin ymax y2min y2max}
#       Expands values to "round" coordinates.
#    private proc NiceExtents {ratio xmin xmax ymin ymax y2min y2max}
#       Adjusts ?min and ?max by up to +/-(?max-?min)/ratio to
#       make them "nice" when formatted as a base 10 decimal.
#       Similar to but generally less aggressive than RoundExtents
#    method SetGraphLimits {{xmin {}} {xmax {}} {ymin {}} {ymax {}}
#                           {y2min {}} {y2max {}}}
#       Allows client to set graph extents.  Empty-string valued imports
#       are filled with data from GetDataExtents, rounded outward using
#       RoundExtents.  The return value is 1 if the graph extents are
#       changed, 0 if the new values are the same as the old values.
#    method GetGraphLimits {}
#       Returns [list $xgraphmin $xgraphmax
#                     $ygraphmin $ygraphmax $ygraphmin $ygraphmax]
#    method RefreshDisplay {}
#       Redraws entire display, using current canvas size and graph
#       limits.
#    method Reset {}
#       Re-initializes most of the graph properties to initial defaults
#    private method PrintData { curvepat }
#        For debugging.
#    method WinDestroy { w } { winpath }
#        $winpath <Destroy> binding.
#
# Non-class procs (used to cut down on call overhead) :
#   proc OWgwMakeDispPt { xdatabase ydatabase xs ys xdispbase ydispbase \
#            xmin ymin xmax ymax previnc xprev yprev x y displistname }
#     Converts (xprev,yprev) (x,y) from data to display coords in
#     a manner suitable for appending to a canvas create line call.
#   proc OWgwPutInBox { x y xmin ymin xmax ymax } {
#     Returns a two element list [list $xnew $ynew], which shifts
#     x and y as necessary to put it into the specified box.

Oc_Class Ow_GraphWin {

    const private variable ShiftMask = 1  ;# Defined by X11.h
    # const private variable CtrlMask  = 4

    # Width of curve plot lines
    public common default_curve_width
    public variable curve_width {
        # Determine whether or not drawpiece hack is needed
        global tcl_platform
        if {$curve_width>1 && \
                [string match windows $tcl_platform(platform)]} {
            set usedrawpiece 1
        } else {
            set usedrawpiece 0
        }
	if {$boundary_clip_size<$curve_width} {
	    set boundary_clip_size $curve_width
	}
    }

    # Symbol display frequency and size
    public common default_symbol_freq
    public common default_symbol_size
    public variable symbol_freq
    public variable symbol_size

    # Select color by "curve", or by "segment"
    public common default_color_selection
    public variable color_selection

    const public variable boundary_clip_size = 32  ;# Draw margin
    ## size, in pixels.  This should be larger than the biggest
    ## curve width.
    const public variable winpath
    const public variable outer_frame_options = \
            "-bd 0 -relief ridge -background #0000cd"
    # Note: #0000cd is MediumBlue
    const public variable default_height = 320
    const public variable default_width = 320
    const public variable canvas_frame_options = \
            "-bd 0 -relief ridge -background #0000cd"
    const public variable canvas_options = "-bd 0 -relief flat"
    const public variable graphsize = 0.9  ;# % of canvas to draw graph in.
    const public variable axescolor = black
    const public variable axeswidth = 2
    const public variable keywidth = 2

    const private variable segmax = 31 ;# Maximum number of curve updates
    ## to allow without a complete curve redraw.

    const private variable usedrawpiece
    const private variable drawpiece = 61
    const private variable drawsmoothpiece = 201
    ## On Windows 9x (others?), Tcl 8.4.0 (others?), the performance
    ## of the canvas "create line" method drops precipitously if the
    ## coordList (i.e., the line vertices) size is too large.  So
    ## we break the curve up into pieces.  If smoothcurves is false,
    ## then make each piece up to the last have ($drawpiece-1)/2
    ## segments.  For smooth curves, there will be a discontinuity
    ## introduced in the first derivative between the pieces, so
    ## we tradeoff speed for fewer corners by breaking the drawn
    ## curve into ($drawsmoothpiece-1)/2 pieces, which should be
    ## set larger than $drawpiece.  Note that both should be odd.

    const private variable graph  ;# Graph canvas
    public method GetCanvas {} { return $graph }

    # Canvas background color
    public common default_canvas_color
    public variable canvas_color {
       if {[info exists graph]} {
          $graph configure -background $canvas_color
       }
    }

    public variable title  = ""
    public variable title_font
    public variable xlabel = ""
    public variable ylabel = "" {
        # Separate ylabel characters by newlines
        set ylabel [join [split $ylabel {}] "\n"]
    }
    public variable y2label = "" {
        # Separate y2label characters by newlines
        set y2label [join [split $y2label {}] "\n"]
    }

    public variable label_font

    private variable hrulestate = 1      ;# Horizontal rule (1=>display),
    private variable hrulepos = {}       ;# position (in graph coords).
    private variable vrulestate = 1      ;# Vertical rule   (0=>hide)
    private variable vrulepos = {}       ;# position (in graph coords)
    ## h(v)rulepos == {} => draw (hide) rule at bottommost (leftmost)
    ## edge of the graph.

    public variable showkey = 1
    public variable smoothcurves = 0     ;# Draw curves as splines?
    private variable firstkey = 1        ;# If true, then a non-empty key
    ## has not been drawn in the lifetime of this widget.
    private variable key_font

    private variable lmargin = 0    ;# Margins: left, top, right, bottom,
    private variable tmargin = 0    ;# in pixels.
    private variable rmargin = 0
    private variable bmargin = 0

    private variable lmargin_min = 0 ;# Minimum values for margins,
    private variable tmargin_min = 0 ;# in pixels.  Set by SetMargins
    private variable rmargin_min = 0 ;# method.
    private variable bmargin_min = 0

    private variable plotwidth  = 0 ;# Width and height of plotting region,
    private variable plotheight = 0 ;# in pixels.

    private variable matteid = 0   ;# Canvas id for matte surrounding graph
    # Note: The graph curves are layered directly below matteid

    private variable bindblock = 0 ;# Used to disable bind <Button-1>
    ## zoom binding when canvas item drag in progress.

    private variable curvecount = 0
    private variable segcount   = 0  ;# Note: segcount is reset
    ## by method ResetSegmentColors.
    private variable xoff    =  0.   ;# Used in conversion from data coords
    private variable yoff    =  0.   ;#  to graph (display) coords.  Setup
    private variable y2off   =  0.   ;#  in DrawAxes routine.
    private variable xscale  =  1.
    private variable yscale  = -1.
    private variable y2scale = -1.
    private variable xgraphmin  = 0. ;# Graph limits, in data coords.
    private variable xgraphmax  = 1.
    private variable ygraphmin  = 0.
    private variable ygraphmax  = 1.
    private variable y2graphmin = 0.
    private variable y2graphmax = 1.

    private variable but1x = 0       ;# Mouse coordinates when
    private variable but1y = 0       ;# Button-1 is pressed

    private variable zoombox_yaxis = 0        ;# Zoom box variables
    private variable zoombox_basept = {0 0}
    private variable zoombox_minsize = 5
    private variable zoombox_arrowsize = 12
    public variable zoombox_fg = #000   ;# Zoom box foreground and
    public variable zoombox_bg = #ff0   ;# background colors.
    public variable zoombox_ac = #000   ;# Zoom box axis indicator
                                       ;##  arrow color.

    private array variable color     ;# Color array

    private variable symbol_types = 6 ;# Number of different types of symbols

    # Limits on number of points to store.  The maximum number of points
    # (per curve) is ptlimit.  When that value is exceeded, the list is
    # reduced to $ptlimit * $ptlimitcut.  Set ptlimit to 0 for no limit.
    public variable ptlimit = 0 {
        set ptlimit [expr {int(floor($ptlimit))}]
        if {$ptlimit<1} {
            set ptlimit 0
        } else {
            foreach curveid $idlist {
                $this CutCurve $curveid
            }
        }
    }
    private variable ptlimitcut = 0.8

    private variable idlist = {}     ;# Ordered list of all valid $curveid's
    private array variable id        ;# Name -> id mapping array
    private array variable curve     ;# Curve data array
    ## Each curve has an entry of the following form:
    ##       curve($curveid,label)    = Label to display in key
    ##       curve($curveid,yaxis)    = Y-axis to plot against;
    ##                                   1 => left,  2 => right
    ##       curve($curveid,name)     = id -> Name mapping.  The
    ##                                  name has the form "$label,$yaxis"
    ##       curve($curveid,color)    = Drawing color, when using curve
    ##                                  color selection.
    ##       curve($curveid,segcolor)  = Drawing color, when using segment
    ##                                  color selection.  This is a list of
    ##                                  colors, indexed against the pts list.
    ##       curve($curveid,symbol)    = Drawing symbol for curve selection.
    ##       curve($curveid,segsymbol) = Drawing symbol for segment selection.
    ##                                  This is a list like segcolor, but a
    ##                                  list of integers.
    ##
    ##       curve($curveid,ptsbreak) = List of indices in pts of last
    ##             point in each "broken" segment of the display curve.
    ##             These result from x == {} entries in calls to
    ##             $this AddDataPoints.
    ##       curve($curveid,pts)      = Data point string of "x y" pairs
    ##       curve($curveid,lastpt)   = List pertaining to last data point
    ##             input:
    ##                { lastindex xlast ylast }
    ##             Here lastindex = expr [llength $curve($curveid,pts)]-2,
    ##             and { xlast ylast } =
    ##                        lrange $curve($curveid,pts) $lastindex end
    ##             This information is stored as an aid to Tcl 7.6, which
    ##             does not handle long lists very efficiently.
    ##       curve($curveid,xmin)     = Minimum value of x among data points
    ##       curve($curveid,xmax)     = Maximum value of x among data points
    ##       curve($curveid,ymin)     = Minimum value of y among data points
    ##       curve($curveid,ymax)     = Maximum value of y among data points
    ##       curve($curveid,ptsdisp)  = List of lists of display points.
    ##             Each separate "segment" of the curve is stored in its own
    ##             sublist.  These are all integer values.
    ##       curve($curveid,dnextpt)  = Index into curve($curveid,pts) of
    ##             next points to be processed for display.  Set to 0 if
    ##             pts has not been processed for display.
    ##       curve($curveid,lastdpt)  = List pertaining to last display
    ##             processing pass:
    ##               { xprev yprev previnc }
    ##             xprev yprev previnc are the data position and "included"
    ##             boolean value for the last "representative" point.  (If
    ##             the trailing points in 'pts' plot on the canvas at a
    ##             distance less than 1 pixel, then one "representative"
    ##             point will be chosen for each display pixel.  Because
    ##             of this, 'xprev yprev' may occur earlier in 'pts' than
    ##             'dnextpt-2'.)  'previnc' is true iff T(xprev,yprev) is
    ##             included in 'ptsdisp', where T represents the transform
    ##             from graph to display coordinates.  (All positions in
    ##             the 'lastpt' list are in graph coordinates.)
    ##             'previnc' is needed to handle the case where 'xprev yprev'
    ##             was excluded from the 'ptsdisp' because no part of the line
    ##             segment joining 'xprev yprev' to its predecessor lay inside
    ##             the graph viewport, but a portion of the line segment
    ##             between 'xprev yprev' and its successor does.  This can't
    ##             be determined until the successor is known, and so at that
    ##             time it may be necessary to retroactively add T(xprev,yprev)
    ##             to 'ptsdisp'.  lastdpt will be set to {} if a break in the
    ##             curve is to be positioned after the last entry in ptsdisp.
    ##       curve($curveid,dispcurrent)  = True iff ptsdisp is up-to-date
    ##                                  wrt pts and scaling.
    ##       curve($curveid,segcount) = Number of times PlotCurve has been
    ##             called for $curveid to perform only a partial display of
    ##             the curve.  This is typically equal to the number of
    ##             separate canvas items being used to display the curve.
    ##             When this number gets larger than segmax, PlotCurve
    ##             does a complete redraw rather than an update.
    ##       curve($curveid,hidden)   = Controls whether or not curve is drawn

    public variable callback = Oc_Nop
    public variable delete_callback_arg = {}

    Constructor {basewinpath args} {
        # Don't add '.' suffix if there already is one.
        # (This happens chiefly when basewinpath is exactly '.')
        if {![string match {.} \
                [string index $basewinpath \
                [expr {[string length $basewinpath]-1}]]]} {
            append basewinpath {.}
        }

	if {![info exists curve_width]} {
	    set curve_width $default_curve_width ;# Initialize to default
	}

        # Determine whether or not drawpiece hack is needed
        global tcl_platform
        if {$curve_width>1 && \
                [string match windows $tcl_platform(platform)]} {
            set usedrawpiece 1
        } else {
            set usedrawpiece 0
        }

	if {![info exists symbol_freq]} {
	    set symbol_freq $default_symbol_freq
	}

	if {![info exists symbol_size]} {
	    set symbol_size $default_symbol_size
	}

	if {![info exists color_selection]} {
	    set color_selection $default_color_selection
	}

	if {![info exists canvas_color]} {
	    set canvas_color $default_canvas_color
	}

	if {$boundary_clip_size<$curve_width} {
	    set boundary_clip_size $curve_width
	}

        set title_font [Oc_Font Get large_bold]
        set label_font [Oc_Font Get bold]
        set key_font $label_font

        eval $this Configure -winpath ${basewinpath}w$this $args

        # Temporarily disable callbacks until construction is complete
        set callback_keep $callback ;   set callback Oc_Nop

        # Pack entire subwidget into a subframe.  This makes
        # housekeeping easier, since the only bindings on
        # $winpath will be ones added from inside the class,
        # and also all subwindows can be destroyed by simply
        # destroying $winpath.
        eval frame $winpath $outer_frame_options
        set graph_frame [eval frame $winpath.canvasframe \
                $canvas_frame_options]
        set graph [eval canvas $graph_frame.graph \
                -width $default_width -height $default_height \
                -background $canvas_color \
                $canvas_options]
        pack $graph -side left -anchor w -fill both -expand 1
        pack $graph_frame -side left -anchor w -fill both -expand 1 

        # Initialize color array:
        #  red, blue, green, magenta, turquoise4, darkorange,
        #  black, cyan, brown, darkgray, purple3
        array set color [list \
                            0 #FF0000 \
                            1 #0000FF \
                            2 #00FF00 \
                            3 #FF00FF \
                            4 #00868B \
                            5 #FF8C00 \
                            6 #000000 \
                            7 #00FFFF \
                            8 #A52A2A \
                            9 #A9A9A9 \
                           10 #7D26CD \
                           ]
        set color(count) 11
 
        # Widget bindings
        bind $winpath <Destroy> "$this WinDestroy %W"
        bind $graph <Configure> "$this ResizeCanvas %w %h"

        # Mouse bindings
	# Show position
	$graph bind showpos <<Ow_LeftButton>>  "$this ShowPosition %x %y 1 %s 1 pos"
	$graph bind showpos <<Ow_RightButton>> "$this ShowPosition %x %y 3 %s 1 pos"
        bind $graph <ButtonRelease>  "+ $this UnshowPosition %s"
        bind $graph <Control-ButtonRelease>  "+ $this UnshowPosition %s"
	# Object dragging
	foreach obj [list key hrule vrule] {
	    $graph bind $obj <Button> "$this DragInit $obj %x %y %b %s"
	    $graph bind $obj <<Ow_LeftButtonMotion>> \
		    [Oc_SkipWrap "$this Drag $obj %x %y 1 %s"]
	    $graph bind $obj <<Ow_RightButtonMotion>> \
		    [Oc_SkipWrap "$this Drag $obj %x %y 3 %s"]
	}
	# Zoom box
        bind $graph <Control-Button> \
		"$this BindWrap \"$this ZoomBoxInit %x %y %b %s\""
        bind $graph <Control-B1-Motion> \
                [Oc_SkipWrap "$this ZoomBoxDraw %x %y"]
        bind $graph <Control-B3-Motion> \
                [Oc_SkipWrap "$this ZoomBoxDraw %x %y"]
        bind $graph <Control-ButtonRelease> "+ $this ZoomBoxDo %x %y"

        # Initialize scaling and axes
        $this SetGraphLimits
        $this RefreshDisplay

        # Re-enable callbacks
        set callback $callback_keep
    }
    method CanvasCoords {x y {axis 1}} {
        set cx [expr {$xoff + $x*$xscale}]
        if {$axis == 1} {
            set cy [expr {$yoff + $y*$yscale}]
        } elseif {$axis == 2} {
            set cy [expr {$y2off + $y*$y2scale}]
        } else {
            return -code error "Bad axis $axis: should be 1 or 2"
        } 
        return [list $cx $cy]
    }
    callback method ResizeCanvas { width height } { graph } {
        set border [expr {2*[$graph cget -borderwidth] \
                +2*[$graph cget -highlightthickness]}]
        # Note: The <Configure> resize request *includes* both
        #       -borderwidth and -highlightthickness!

        set newwidth [expr {round($width-$border)}]
        set newheight [expr {round($height-$border)}]
        if {[$graph cget -width]!=$newwidth || \
                [$graph cget -height]!=$newheight} {
            set confbind [bind $graph <Configure>]
            bind $graph <Configure> {}  ;# Temporarily disable
            $graph configure -width $newwidth -height $newheight
            update idletasks  ;# Need this to keep from generating
            ## and infinite <Configure> loop
            bind $graph <Configure> $confbind ;# Reenable
            $this RefreshDisplay
        }
    }
    method SetSmoothState { newstate } {
        if {$smoothcurves != $newstate} {
            set smoothcurves $newstate
            $this DrawCurves 0
        }
    }
    method SetKeyState { newstate {key_pos {}} } {
        set showkey $newstate
        if {$showkey} {
            $this DrawKey $key_pos
        } else {
            $this HideKey
        }
    }
    private method HideKey {} {
        $graph delete key
    }
    method GetKeyPosition {} {
        # Returns location of upper righthand corner of key, relative
        # to the upper righthand corner of the canvas.  The return
        # value can be fed into SetKeyState or DrawKey.
        set coords [$graph coords key_box]
        if {[string match {} $coords]} { return {} }
        set xpos [expr {[lindex $coords 2] - $lmargin \
                - $plotwidth - $rmargin}]
        set ypos [lindex $coords 1]
        return [list $xpos $ypos]
    }
    private method RenderKeyText { x y left_justify ylist } \
       {graph curve key_font color_selection color symbol_types segcount} {
       # Returns a two item list, {y linecount}, consisting of
       # the next y offset and the number of lines written.
       set linecount 0
       if {![string match "segment" $color_selection]} {
          # color by curve
          if {$left_justify} {
             set anchor "nw"
             set justify "left"
          } else {
             set anchor "ne"
             set justify "right"
          }
          foreach curveid $ylist {
             set text $curve($curveid,label)
             incr linecount [expr {1 + [regsub -all "\n" $text {} dummy]}]
             set tempid [$graph create text $x $y \
                            -anchor $anchor \
                            -fill $curve($curveid,color) \
                            -justify $justify \
                            -tags {key key_text} \
                            -font $key_font \
                            -text $text]
             set bbox [$graph bbox $tempid]
             set y [lindex $bbox 3]
          }
       } else {
          # Color by segment
          if {$left_justify} {
             foreach curveid $ylist {
                set segno [expr {[llength $curve($curveid,ptsbreak)]+1}]
                ## segno is the number of segments
                set text $curve($curveid,label)
                append text ": 0"
                incr linecount [expr {1 + [regsub -all "\n" $text {} dummy]}]
                set clr [lindex $curve($curveid,segcolor) 0]
                if {[string match {} $clr]} {
                   set clr $color([expr {$segcount % $color(count)}])
                   lappend curve($curveid,segcolor) $clr
                   lappend curve($curveid,segsymbol) \
                      [expr {$segcount % $symbol_types}]
                   incr segcount
                }
                set tempid [$graph create text $x $y \
                               -anchor nw \
                               -fill $clr \
                               -justify left \
                               -tags {key key_text} \
                               -font $key_font \
                               -text $text]
                set bbox [$graph bbox $tempid]
                set ynew [lindex $bbox 3]
                set xtemp [lindex $bbox 2]
                for {set i 1} {$i<$segno} {incr i} {
                   set clr [lindex $curve($curveid,segcolor) $i]
                   if {[string match {} $clr]} {
                      set clr $color([expr {$segcount % $color(count)}])
                      lappend curve($curveid,segcolor) $clr
                      lappend curve($curveid,segsymbol) \
                         [expr {$segcount % $symbol_types}]
                      incr segcount
                   }
                   set tempid [$graph create text $xtemp $y \
                                  -anchor nw \
                                  -fill $clr \
                                  -justify left \
                                  -tags {key key_text} \
                                  -font $key_font \
                                  -text [format %X $i]]
                   set bbox [$graph bbox $tempid]
                   set xtemp [lindex $bbox 2]
                }
                set y $ynew
             }
          } else {
             # Right justify
             foreach curveid $ylist {
                # Is curve($curveid,segcolor) completely populated?
                set number_of_segments \
                   [expr {[llength $curve($curveid,ptsbreak)]+1}]
                for {set i [llength $curve($curveid,segcolor)]} \
                      {$i<$number_of_segments} {incr i} {
                   set clr $color([expr {$segcount % $color(count)}])
                   lappend curve($curveid,segcolor) $clr
                   lappend curve($curveid,segsymbol) \
                      [expr {$segcount % $symbol_types}]
                   incr segcount
                }
                # Write out text string, from right to left
                set xtemp $x
                for {set i [expr {$number_of_segments-1}]} \
                      {$i>0} {incr i -1} {
                   set clr [lindex $curve($curveid,segcolor) $i]
                   set tempid [$graph create text $xtemp $y \
                                  -anchor ne \
                                  -fill $clr \
                                  -justify right \
                                  -tags {key key_text} \
                                  -font $key_font \
                                  -text [format %X $i]]
                   set bbox [$graph bbox $tempid]
                   set xtemp [lindex $bbox 0]
                }
                set text $curve($curveid,label)
                append text ": 0"
                incr linecount [expr {1 + [regsub -all "\n" $text {} dummy]}]
                set clr [lindex $curve($curveid,segcolor) 0]
                set tempid [$graph create text $xtemp $y \
                               -anchor ne \
                               -fill $clr \
                               -justify right \
                               -tags {key key_text} \
                               -font $key_font \
                               -text $text]
                set bbox [$graph bbox $tempid]
                set y [lindex $bbox 3]
             }
          }
       }
       return [list $y $linecount]
    }
    private method DrawKey { { key_pos {} } } {
        # Get coords from current key, if any, or otherwise
        # use graph extents
        if [string match {} $key_pos] {
            set key_pos [$this GetKeyPosition]
        }

        $this HideKey   ;# Delete any previously existing key
        if { !$showkey } { return }

        if {[string match "segment" $color_selection]} {
           set left_justify 0
        } else {
           set left_justify 1
        }

        set newbox 0 ;# If ==0, then we are overwriting old box
        if {$firstkey || [string match {} $key_pos]} {
            set key_box_xmax [expr {$lmargin+$plotwidth}]
            set key_box_ymin $tmargin
            set newbox 1
        } else {
            set key_box_xmax [expr {$lmargin + $plotwidth + \
                    $rmargin + [lindex $key_pos 0]}]
            set key_box_ymin [lindex $key_pos 1]
        }

        # Separate out y1 and y2 axis curves
        set y1list [set y2list {}]
        foreach curveid $idlist {
            if {$curve($curveid,hidden)} { continue }
            if {$curve($curveid,yaxis)!=2} {
                lappend y1list $curveid
            } else {
                lappend y2list $curveid
            }
        }
        set keycount [expr {[llength $y1list] + [llength $y2list]}]

        # Temporarily write all key text items, in order, anchored
        # on ne corner; this is used to determine box dimensions.
        set linecount 0
        set x $key_box_xmax
        set y $key_box_ymin
        foreach {y lineincr} \
              [$this RenderKeyText $x $y $left_justify $y1list] {
           incr linecount $lineincr
           break
        }
        set ytemp $y
        if {[llength $y1list]>0 && [llength $y2list]>0} {
            # Add separating line
            set y [expr {$y+1}]
            set tempid [$graph create line $x $y [expr $x+1] $y \
                    -tags {key key_text} -width 1]
            set bbox [$graph bbox $tempid]
            set y [lindex $bbox 3]
        }
        set separator_height [expr {$y-$ytemp}]
        foreach {y lineincr} \
              [$this RenderKeyText $x $y $left_justify $y2list] {
           incr linecount $lineincr
           break
        }

        # Create appropriately sized key box
        if {$keycount<1} {
            set x1 $x  ; set y1 $y
            set x2 $x1 ; set y2 $y1
            set textheight 0
            set textwidth 0
            set lineheight 0
        } else {
            set bbox [$graph bbox key_text]
            set x1 [lindex $bbox 0] ; set y1 [lindex $bbox 1]
            set x2 [lindex $bbox 2] ; set y2 [lindex $bbox 3]
            set textheight [expr {$y2-$y1}]
            set textwidth [expr {$x2-$x1}]
            set lineheight [expr {double($textheight-$separator_height)
                                  /double($linecount)}]
        }
        set x $key_box_xmax  ; set y $key_box_ymin
        if {$newbox} {
            # Add margin
            set x [expr {$x-$lineheight}]
            set y [expr {$y+0.75*$lineheight}]
        }
        set x1 [expr {$x-$textwidth-$lineheight/2.}]
        set y1 $y
        set x2 $x
        set y2 [expr {$y+$textheight+$lineheight/2.-1}]

        if {$y2<$y1} { set y2 $y1 } ;# This primarily protects against
        ## accidental movement of the key box when $keycount==0.  Since
        ## [lindex [$graph coords key_box] 1] returns min($y1,$y2), if
        ## we don't put this check in place then when the box position
        ## is next read the current value of $y1 will be replaced with
        ## $y2 if $y2<$y1.  This occurs if $keycount==0, with the result
        ## that the keybox is shifted upwards 1 pixel.

        if {$x1<0} { set x2 [expr {$x2-$x1}] ; set x1 0 } ;# Don't
        ## allow key to be placed off lefthand side of graph.  This
        ## provides better behaved canvas resize behavior.
        if {$keycount<1} {
            # Empty keybox
            $graph create rectangle $x1 $y1 $x2 $y2 -tag {key key_box}\
                    -outline {}
        } else {
            # Non-empty keybox
            $graph create rectangle $x1 $y1 $x2 $y2 -tag {key key_box}\
                    -fill white -outline black -width $keywidth
            set firstkey 0
        }

        # Rewrite all key text items, anchored on either nw  or ne
        # corner (depending on $left_justify), inside key box
        $graph delete key_text
        if {$left_justify} {
           set x [expr {$x1+$lineheight/4.}]
        } else {
           set x [expr {$x2-$lineheight/4.}]
        }
        set y [expr {$y1+$lineheight/4.}]
        foreach {y lineincr} [$this RenderKeyText $x $y $left_justify $y1list] {
           break
        }
        if {[llength $y1list]>0 && [llength $y2list]>0} {
            # Add separating line
           set y [expr {$y+1}]
            set tempid [$graph create line $x1 $y $x2 $y \
                    -tags {key} -width 1]
            set bbox [$graph bbox $tempid]
            set y [lindex $bbox 3]
        }
        $this RenderKeyText $x $y $left_justify $y2list
    }
    method SetColorSelection { newselection } { color_selection } {
       if {![string match segment $newselection]} {
          set newselection "curve"   ;# Default
       }
       if {[string compare $color_selection $newselection]==0} {
          return   ;# No change
       }
       set color_selection $newselection
       $this DrawCurves 0
       $this DrawKey
    }
    private method GetUserCoords { cx cy yaxis } { \
            xoff yoff y2off xscale yscale y2scale } {
        # Given canvas coordinates cx and cy,
        # returns corresponding user coordinates ux and uy.

        if {$xscale==0.} {
            set ux $cx ;# Safety
        } else {
            set ux [expr {($cx-$xoff)/double($xscale)}]
        }
        if {$yaxis!=2} {
            # Measure off y1 axis
            if {$yscale==0.} {
                set uy $cy   ;# Safety
            } else {
                set uy [expr {($cy-$yoff)/double($yscale)}]
            }
        } else {
            # Measure off y2 axis
            if {$y2scale==0.} {
                set uy $cy   ;# Safety
            } else {
                set uy [expr {($cy-$y2off)/double($y2scale)}]
            }
        }
        return [list $ux $uy]
    }
    callback method ShowPosition { cx cy button state convert tags } { \
	    graph xgraphmin xgraphmax ygraphmin ygraphmax \
	    y2graphmin y2graphmax bindblock ShiftMask } {

	if {$button==3 || ($button==1 && ($state & $ShiftMask))} {
	    set yaxis 2
	} elseif {$button==1} {
	    set yaxis 1
	} else {
	    return
	}

	if {$convert} {
            # Convert to canvas coordinates
            set cx [$graph canvasx $cx]
            set cy [$graph canvasy $cy]
        }

        # Convert to user coordinates
        foreach {ux uy} [$this GetUserCoords $cx $cy $yaxis] {break}

        # Determine whether to display text to left or right
        # of point.
        set xmid [expr {($xgraphmin+$xgraphmax)/2.}]
        if {$ux>=$xmid} {
            set textanchor se
            set cxoff -3
        } else {
            set textanchor sw
            set cxoff 3
        }
        set cyoff -1
        # Format text for display
        set fx [expr {[format "%.3g" [expr {$ux-$xgraphmin}]]+$xgraphmin}]
        if {$yaxis!=2} {
            set fy [expr \
                    {[format "%.3g" [expr {$uy-$ygraphmin}]]+$ygraphmin}]
        } else {
            set fy [expr \
                    {[format "%.3g" [expr {$uy-$y2graphmin}]]+$y2graphmin}]
        }
        # Do display
        set textid [$graph create text \
                [expr {$cx+$cxoff}] [expr {$cy+$cyoff}] \
                -text [format "(%.12g,%.12g)" $fx $fy] \
                -anchor $textanchor -tags $tags]
        set bbox [$graph bbox $textid]
        eval "$graph create rectangle $bbox -fill white -outline {} \
                -tags [list $tags]"
        $graph raise $textid
        return [list $fx $fy]
    }

    callback method UnshowPosition { state } { graph } {
	$graph delete pos
    }

    callback method ZoomBoxInit { x y button state } { \
            graph zoombox_yaxis zoombox_basept ShiftMask } {
        if {$zoombox_yaxis!=0} { return } ;# Zoom already in progress
        if {$button==3 || ($state & $ShiftMask) } {
            set zoombox_yaxis 2
        } elseif {$button==1} {
            set zoombox_yaxis 1
        } else {
	    return
	}
        set x [$graph canvasx $x]
        set y [$graph canvasy $y]
        set zoombox_basept [list $x $y]
    }

    callback method ZoomBoxDraw { x2 y2 } { graph \
            zoombox_yaxis zoombox_basept zoombox_minsize \
            zoombox_arrowsize zoombox_fg zoombox_bg zoombox_ac } {
        if {$zoombox_yaxis==0} return ;# No zoom in progress

        $graph delete zoombox

        set x1 [lindex $zoombox_basept 0]
        set y1 [lindex $zoombox_basept 1]
        set x2 [$graph canvasx $x2]
        set y2 [$graph canvasy $y2]
        set zw [expr {abs($x1-$x2)}]
        set zh [expr {abs($y1-$y2)}]
        if {$zw<$zoombox_minsize || $zh<$zoombox_minsize} {
            return ;# Don't draw tiny boxes
        }

        # Stroke new zoombox
        $graph create rectangle $x1 $y1 $x2 $y2 \
                -outline $zoombox_bg -width 3 -tag zoombox
        $graph create rectangle $x1 $y1 $x2 $y2 \
                -outline $zoombox_fg -width 1 -tag zoombox

        # Draw axis indicator arrow
        set ymid [expr {($y1+$y2)/2.}]
        if {$zoombox_yaxis==2} {
            set xarr1 $x2
            set xarr2 [expr {$x2+$zoombox_arrowsize}]
        } else {
            set xarr1 $x1
            set xarr2 [expr {$x1-$zoombox_arrowsize}]
        }
        set arrheadsize [expr {round($zoombox_arrowsize/2.)}]
        set arrheadshape "$arrheadsize  $arrheadsize [expr {$arrheadsize/2}]"
        $graph create line $xarr1 $ymid $xarr2 $ymid \
                -fill $zoombox_ac -width 1 \
                -arrow last -arrowshape $arrheadshape -tag zoombox
    }
    private proc RoundNicely { grit minname maxname } {
        # Utility proc for DoZoomBox
        upvar 1 $minname min
        upvar 1 $maxname max
        if {$grit <= 0.0} { set grit 1.0 } ;# Safety
        set slop [expr {pow(10,floor(log10($grit)))}]
        set min [expr {$slop*floor($min/$slop)}]
        set max [expr {$slop*ceil($max/$slop)}]
    }
    callback method ZoomBoxDo { x2 y2 } { graph zoombox_yaxis zoombox_basept zoombox_minsize xgraphmin xgraphmax ygraphmin ygraphmax y2graphmin y2graphmax } {
       if {$zoombox_yaxis==0} return ;# No zoom in progress

       $graph delete zoombox

        set x1 [lindex $zoombox_basept 0]
        set y1 [lindex $zoombox_basept 1]
        set x2 [$graph canvasx $x2]
        set y2 [$graph canvasy $y2]
        set zw [expr {abs($x1-$x2)}]
        set zh [expr {abs($y1-$y2)}]
        if {$zw<$zoombox_minsize || $zh<$zoombox_minsize} {
            set zoombox_yaxis 0   ;# Don't zoom
            return
        }

        # Working precision
        global tcl_precision
        if {![info exists tcl_precision] || $tcl_precision==0} {
           set working_precision 17
        } else {
           set working_precision $tcl_precision
        }

        # Convert from canvas to user coordinates, and round
        # to nice numbers.
        if {$zoombox_yaxis==2} {
            foreach {uxa uy2a} [$this GetUserCoords $x1 $y1 2] {break}
            foreach {uxb uy2b} [$this GetUserCoords $x2 $y2 2] {break}

	    # Don't zoom higher than computational precision.
	    set diff [expr {abs($uxa-$uxb)}]
	    set sum  [expr {abs($uxa)+abs($uxb)}]
	    set mindiff [expr {$sum*pow(10,-1*($working_precision-3))}]
	    if {$diff<$mindiff} {
		set midpt [expr {($uxa+$uxb)/2.}]
		set uxa [expr {$midpt-$mindiff/2.}]
		set uxb [expr {$midpt+$mindiff/2.}]
	    }
	    set diff [expr {abs($uy2a-$uy2b)}]
	    set sum  [expr {abs($uy2a)+abs($uy2b)}]
	    set mindiff [expr {$sum*pow(10,-1*($working_precision-3))}]
	    if {$diff<$mindiff} {
		set midpt [expr {($uy2a+$uy2b)/2.}]
		set uy2a [expr {$midpt-$mindiff/2.}]
		set uy2b [expr {$midpt+$mindiff/2.}]
	    }

            Ow_GraphWin RoundNicely \
                [expr {($xgraphmax-$xgraphmin)/100.}] uxa uxb
            Ow_GraphWin RoundNicely \
                [expr {($y2graphmax-$y2graphmin)/100.}] uy2a uy2b
            set uy1a $ygraphmin
            set uy1b $ygraphmax
        } else {
            foreach {uxa uy1a} [$this GetUserCoords $x1 $y1 1] {break}
            foreach {uxb uy1b} [$this GetUserCoords $x2 $y2 1] {break}

	    # Don't zoom higher than computational precision.
	    set diff [expr {abs($uxa-$uxb)}]
	    set sum  [expr {abs($uxa)+abs($uxb)}]
	    set mindiff [expr {$sum*pow(10,-1*($working_precision-3))}]
	    if {$diff<$mindiff} {
		set midpt [expr {($uxa+$uxb)/2.}]
		set uxa [expr {$midpt-$mindiff/2.}]
		set uxb [expr {$midpt+$mindiff/2.}]
	    }
	    set diff [expr {abs($uy1a-$uy1b)}]
	    set sum  [expr {abs($uy1a)+abs($uy1b)}]
	    set mindiff [expr {$sum*pow(10,-1*($working_precision-3))}]
	    if {$diff<$mindiff} {
		set midpt [expr {($uy1a+$uy1b)/2.}]
		set uy1a [expr {$midpt-$mindiff/2.}]
		set uy1b [expr {$midpt+$mindiff/2.}]
	    }

            Ow_GraphWin RoundNicely \
                [expr {($xgraphmax-$xgraphmin)/100.}] uxa uxb
            Ow_GraphWin RoundNicely \
                [expr {($ygraphmax-$ygraphmin)/100.}] uy1a uy1b
            set uy2a $y2graphmin
            set uy2b $y2graphmax
        }

        # Rescale
	if {[$this SetGraphLimits $uxa $uxb $uy1a $uy1b $uy2a $uy2b]} {
	    $this RefreshDisplay
	}

        # Turn off zoombox-in-progress indicator
        set zoombox_yaxis 0
    }

    method Zoom { factor } {
	if {$factor==0.0} {return}
	set ifactor [expr {1.0/abs(2.0*$factor)}]

	# Don't zoom beyond floating point precision
	global tcl_precision
        if {![info exists tcl_precision]
            || $tcl_precision==0 || $tcl_precision>16} {
           set minrat 5.0e-15
        } else {
           set minrat [expr {0.5*pow(10,-1*($tcl_precision-3))}]
        }

	# Rescale about display center
	set xmid [expr {($xgraphmin+$xgraphmax)/2.}]
	set xsize [expr {($xgraphmax-$xgraphmin)*$ifactor}]
	if {$xsize<$minrat*$xmid} {set xsize [expr {$minrat*$xmid}]}
	set xa [expr {$xmid-$xsize}]
	set xb [expr {$xmid+$xsize}]
	Ow_GraphWin RoundNicely \
                [expr {($xgraphmax-$xgraphmin)/100.}] xa xb

	set ymid [expr {($ygraphmin+$ygraphmax)/2.}]
	set ysize [expr {($ygraphmax-$ygraphmin)*$ifactor}]
	if {$ysize<$minrat*$ymid} {set ysize [expr {$minrat*$ymid}]}
	set ya [expr {$ymid-$ysize}]
	set yb [expr {$ymid+$ysize}]
	Ow_GraphWin RoundNicely \
                [expr {($ygraphmax-$ygraphmin)/100.}] ya yb

	set y2mid [expr {($y2graphmin+$y2graphmax)/2.}]
	set y2size [expr {($y2graphmax-$y2graphmin)*$ifactor}]
	if {$y2size<$minrat*$y2mid} {set y2size [expr {$minrat*$y2mid}]}
	set y2a [expr {$y2mid-$y2size}]
	set y2b [expr {$y2mid+$y2size}]
	Ow_GraphWin RoundNicely \
                [expr {($y2graphmax-$y2graphmin)/100.}] y2a y2b

        # Rescale
	if {[$this SetGraphLimits $xa $xb $ya $yb $y2a $y2b]} {
	    $this RefreshDisplay
	}
    }

    method Pan { xpan y1pan y2pan } {
        foreach {xa xb y1a y1b y2a y2b} [$this GetGraphLimits] { break }

        set xadj [expr {($xb-$xa)*$xpan}]
        set xa [expr {$xa+$xadj}]
        set xb [expr {$xb+$xadj}]

        set y1adj [expr {($y1b-$y1a)*$y1pan}]
        set y1a [expr {$y1a+$y1adj}]
        set y1b [expr {$y1b+$y1adj}]

        set y2adj [expr {($y2b-$y2a)*$y2pan}]
        set y2a [expr {$y2a+$y2adj}]
        set y2b [expr {$y2b+$y2adj}]

        foreach {txa txb ty1a ty1b ty2a ty2b} [Ow_GraphWin NiceExtents \
            1000 $xa $xb $y1a $y1b $y2a $y2b] {break}

	if {$xadj!=0}  { set xa  $txa  ; set xb  $txb  }
	if {$y1adj!=0} { set y1a $ty1a ; set y1b $ty1b }
	if {$y2adj!=0} { set y2a $ty2a ; set y2b $ty2b }

	if {[$this SetGraphLimits $xa $xb $y1a $y1b $y2a $y2b]} {
	    $this RefreshDisplay
	}
    }

    private method DrawRules { x y } {
        # Note: Imports x and y are in graph (not canvas) coordinates.
        #  If $x=={}, then the vertical rule is drawn on the leftmost
        # edge of the graph.  If $y=={}, then the horizontal rule is
        # drawn on the bottommost edge of the graph.
        set vrulepos $x
        set hrulepos $y
        foreach {cxmin cymin cxmax cymax} \
                [list $lmargin $tmargin \
                  [expr {$lmargin+$plotwidth}] \
                  [expr {$tmargin+$plotheight}]] {}
        set cx {}
        if {![string match {} $x]} {
            set cx [expr {$x*$xscale+$xoff}]
        }
        set cy {}
        if {![string match {} $y]} {
            set cy [expr {$y*$yscale+$yoff}]
        }
        if {[string match {} $cx]} { set cx $cxmin }
        if {[string match {} $cy]} { set cy $cymax }
        if {$cx<$cxmin} {set cx $cxmin}
        if {$cx>$cxmax} {set cx $cxmax}
        if {$cy<$cymin} {set cy $cymin}
        if {$cy>$cymax} {set cy $cymax}
        if {$hrulestate} {
            $graph raise [$graph create line \
                    $cxmin $cy $cxmax $cy \
                    -fill $axescolor -width 0 \
                    -tags {hrule showpos}]
        }
        if {$vrulestate} {
            $graph raise [$graph create line \
                    $cx $cymin $cx $cymax \
                    -fill $axescolor -width 0 \
                    -tags {vrule showpos}]
        }
    }
    callback method ZoomBlock {} { bindblock } {
	set bindblock 1
    }
    callback method BindWrap { script } { bindblock } {
	if {$bindblock} {
	    set bindblock 0
	} else {
	    eval $script
	}
    }
    callback method DragInit { obj x y button state } {
       graph but1x but1y lmargin tmargin plotwidth plotheight
    } {
        if {$button!=1 && $button!=3} { return }
        set but1x [$graph canvasx $x]
        set but1y [$graph canvasy $y]
        foreach { cx cy } [OWgwPutInBox $but1x $but1y \
                $lmargin $tmargin \
                [expr {$lmargin+$plotwidth}] \
                [expr {$tmargin+$plotheight}]] {}
        switch $obj {
            hrule {
                $graph lower hrule key
                set but1x $cx
                set but1y [lindex [$graph coords hrule] 1]
                ## Shift to line center
		$this ShowPosition $cx $cy $button $state 0 pos
            }
            vrule {
                $graph lower vrule key
                set but1x [lindex [$graph coords vrule] 0]
                set but1y $cy
                ## Shift to line center
		$this ShowPosition $cx $cy $button $state 0 pos
            }
	    key {
		$graph raise key
	    }
        }
	$this ZoomBlock
    }
    callback method Drag { obj x y button state } { graph \
	    but1x but1y vrulepos hrulepos lmargin tmargin \
	    plotwidth plotheight } {
        set cx [$graph canvasx $x]
        set cy [$graph canvasy $y]
        switch $obj {
            key   {
                $graph move key [expr {$cx - $but1x}] [expr {$cy - $but1y}]
            }
            default {
                foreach { cx cy } [OWgwPutInBox $cx $cy \
                        $lmargin $tmargin \
                        [expr {$lmargin+$plotwidth}] \
                        [expr {$tmargin+$plotheight}]] {}
                $graph delete pos
                switch $obj {
                    vrule {
                        $graph move vrule [expr {$cx - $but1x}]  0
                        set vrulepos [lindex [$this ShowPosition \
                                $cx $cy $button $state 0 pos] 0]
                    }
                    hrule {
                        $graph move hrule 0 [expr {$cy - $but1y}]
                        set hrulepos [lindex [$this ShowPosition \
                                $cx $cy $button $state 0 pos] 1]
                    }
                }
            }
        }
        set but1x $cx
        set but1y $cy
    }
    method SetMargins { left right top bottom } {
        set lmargin_min $left
        set rmargin_min $right
        set tmargin_min $top
        set bmargin_min $bottom
    }
    method DrawAxes {} {
        # Clears canvas, then renders title, axes labels, axes,
        # horizontal and vertical rules, and key.
        #   Also sets {t,b,l,r}margin, plotwidth/plotheight, and
        # {x,y,y2}scale, {x,y,y2}off values.

        # Save key position, *before* updating plotwidth and margins
        set key_pos [$this GetKeyPosition]

        # Are y1 and/or y2 axes active?
        set y1ccnt 0
        set y2ccnt 0
        foreach cid $idlist {
            if { ! $curve($cid,hidden) } {
                if { $curve($cid,yaxis)!=2 } {
                    incr y1ccnt
                } else {
                    incr y2ccnt
                }
            }
        }

        # Check to see if a rule is at or off the edge of the graph,
        # and is so  store {} as the rule position; this will cause
        # '$this DrawRules' to draw the rule at the left/bottom graph
        # edge.
        if {$xscale!=0.} {set slack [expr {1./abs($xscale)}]} \
                else     {set slack 0}
        if {![string match {} $vrulepos] && \
                $vrulepos<=$xgraphmin+$slack || \
                $vrulepos>=$xgraphmax-$slack} {
            set vrulepos {}
        }
        if {$yscale!=0.} {set slack [expr {1./abs($yscale)}]} \
                else     {set slack 0}
        if {![string match {} $hrulepos] && \
                $hrulepos<=$ygraphmin+$slack || \
                $hrulepos>=$ygraphmax-$slack} {
            set hrulepos {}
        }

        # Determine title height
        if {![string match {} $title]} {
            $graph delete title_test ;# Safety
            $graph create text 0 0 \
                    -text $title -font $title_font -tags title_test
            set bbox [$graph bbox title_test]
            set title_height [expr {1.5*([lindex $bbox 3]-[lindex $bbox 1])}]
            $graph delete title_test
        } else {
            set title_height 0
        }

        # Produce "cleaned up" rendition of {x|y|y2}graph{min|max}
        foreach v [list xgraphmin xgraphmax ygraphmin ygraphmax \
                y2graphmin y2graphmax] {
            set val [set $v]
            set test [format "%#.12g" $val] ;# The "#" form of the
            ## %g format guarantees a decimal point is presented.
            ## This protects against integer overflow problems.
            if {$test-$val == 0.0} {
                set ${v}_text [format "%.12g" $val]
            } else {
                set ${v}_text [format "%.15g" $val]
            }
        }

        # Calculate lower bound for margin size from axes coordinate text
        $graph delete margin_test   ;# Safety
        $graph create text 0 0 -text $xgraphmin_text -font $label_font \
                -anchor sw -tags margin_test
        $graph create text 0 0 -text $xgraphmax_text -font $label_font \
                -anchor sw -tags margin_test
        set bbox [$graph bbox margin_test]
        set xtext_height [expr {([lindex $bbox 3]-[lindex $bbox 1])*1.2+1}]
        set xtext_halfwidth [expr {([lindex $bbox 2]-[lindex $bbox 0])*0.6+1}]
        ## Include a little bit extra
        $graph delete margin_test

        if {$y1ccnt>0} {
            $graph create text 0 0 -text $ygraphmin_text -font $label_font \
                    -anchor sw -tags margin_test
            $graph create text 0 0 -text $ygraphmax_text -font $label_font \
                    -anchor sw -tags margin_test
            set bbox [$graph bbox margin_test]
            set ytext_halfheight \
                    [expr {([lindex $bbox 3]-[lindex $bbox 1])*0.6+1}]
            set ytext_width \
                    [expr {([lindex $bbox 2]-[lindex $bbox 0])*1.2+1}]
            ## Include a little bit extra
            $graph delete margin_test
        } else {
            set ytext_halfheight 0
            set ytext_width 0
        }
        if {$y2ccnt>0} {
            $graph create text 0 0 -text $y2graphmin_text -font $label_font \
                    -anchor sw -tags margin_test
            $graph create text 0 0 -text $y2graphmax_text -font $label_font \
                    -anchor sw -tags margin_test
            set bbox [$graph bbox margin_test]
            set y2text_halfheight \
                    [expr {([lindex $bbox 3]-[lindex $bbox 1])*0.6+1}]
            set y2text_width \
                    [expr {([lindex $bbox 2]-[lindex $bbox 0])*1.2+1}]
            ## Include a little bit extra
            $graph delete margin_test
        } else {
            set y2text_halfheight 0
            set y2text_width 0
        }

        # Adjust space as required by orthogonal axes
        if {$ytext_halfheight>$xtext_height} {
            set xtext_height $ytext_halfheight
        }
        if {$y2text_halfheight>$xtext_height} {
            set xtext_height $y2text_halfheight
        }
        if {$xtext_halfwidth>$ytext_width} {
            set ytext_width $xtext_halfwidth
        }
        if {$xtext_halfwidth>$y2text_width} {
            set y2text_width $xtext_halfwidth
        }

        # Set origin and scaling factors
        # NOTE: Canvas widget has +y downward
        set canwidth [$graph cget -width]
        set canheight [$graph cget -height]
        set lmargin [expr {$canwidth*(1-$graphsize)/2.}]
        set rmargin $lmargin
        set tmargin [expr {$title_height+$canheight*(1-$graphsize)/2.}]
        set bmargin [expr {$tmargin-$title_height}]
        if { $lmargin < $ytext_width }  {
            set lmargin $ytext_width
        }
        if { $rmargin < $y2text_width }  {
            set rmargin $y2text_width
        }
        if { $tmargin < $ytext_halfheight } {
            set tmargin $ytext_halfheight
        }
        if { $tmargin < $y2text_halfheight } {
            set tmargin $y2text_halfheight
        }
        if { $bmargin < $xtext_height }  {
            set bmargin $xtext_height
        }
        if {$lmargin < $lmargin_min} { set lmargin $lmargin_min }
        if {$rmargin < $rmargin_min} { set rmargin $rmargin_min }
        if {$tmargin < $tmargin_min} { set tmargin $tmargin_min }
        if {$bmargin < $bmargin_min} { set bmargin $bmargin_min }

	set lmargin [expr {round($lmargin)}]
	set rmargin [expr {round($rmargin)}]
	set tmargin [expr {round($tmargin)}]
	set bmargin [expr {round($bmargin)}]

        set plotwidth [expr {$canwidth-$lmargin-$rmargin}]
        set plotheight [expr {$canheight-$tmargin-$bmargin}]

        # Set instance variables used to transform from graph coords
        # to display coords:
        set xrange [expr {double($xgraphmax-$xgraphmin)}]   ;# Temporary
        if {$xrange==0.} {set xrange  1.}
        set xscale [expr {double($plotwidth)/$xrange}]
        if {$xscale==0.} {set xscale 1.}
        set xoff [expr {$lmargin - $xgraphmin * $xscale}]

        set yrange [expr {double($ygraphmax-$ygraphmin)}]   ;# Temporary
        if {$yrange==0.} {set yrange  1.}
        set yscale [expr {double(-$plotheight)/$yrange}]
        if {$yscale==0.} {set yscale -1.}
        set yoff [expr {$tmargin - $ygraphmax * $yscale}]

        set y2range [expr {double($y2graphmax-$y2graphmin)}] ;# Temporary
        if {$y2range==0.} {set y2range  1.}
        set y2scale [expr {double(-$plotheight)/$y2range}]
        if {$y2scale==0.} {set y2scale -1.}
        set y2off [expr {$tmargin - $y2graphmax * $y2scale}]

        set xdispmin $lmargin
        set ydispmin $tmargin
        set xdispmax [expr {$canwidth-$rmargin}]
        set ydispmax [expr {$canheight-$bmargin}]

        # Draw new graph
        if {1} {
           # Orig
           $graph delete all
        } else {
           # Try to delete only essential stuff, so that active drag
           # bindings on the key and horizontal and vertical rules
           # remain active.
           $graph delete axes
           foreach curveid $idlist {
              $graph delete tag$curveid
           }
           $graph delete hrule  ;# Needs work
           $graph delete vrule  ;# Needs work
           #$graph delete key   ;# Needs work
        }

        $this InvalidateDisplay $idlist

        # Draw base graph box at bottom of display stack
        $graph create rectangle \
           $xdispmin $ydispmin $xdispmax $ydispmax \
           -outline $axescolor -width $axeswidth -tags axes

        # Add border matting used to hide curve outliers.  Double outside
        # dimension to hide outliers better at boundary and when
        # resizing the display viewport.  Also overlap corners to make
        # certain there aren't any gaps for lines running outside plot
        # area to peak through.
        #  Curves are layered directly below the first of these boxes.
        set matteid [$graph create rectangle 0 0 \
                     [expr {2*$canwidth}] [expr {$ydispmin-$axeswidth/2.}] \
                     -outline {} -fill white -tags axes]
        $graph create rectangle 0 0 \
                [expr {$xdispmin-$axeswidth/2.}] $canheight \
                -outline {} -fill white -tags axes
        $graph create rectangle [expr {$xdispmax+$axeswidth/2.}] 0 \
                [expr {2*$canwidth}] $canheight \
                -outline {} -fill white -tags axes
        $graph create rectangle 0 [expr {$ydispmax+$axeswidth/2.}] \
                [expr {2*$canwidth}] [expr {2*$canheight}] \
                -outline {} -fill white -tags axes

        $graph create text $xdispmin [expr {$ydispmax+$bmargin*0.1}] \
                -font $label_font \
                -text $xgraphmin_text -anchor n -tags axes
        $graph create text $xdispmax [expr {$ydispmax+$bmargin*0.1}] \
                -font $label_font \
                -text $xgraphmax_text -anchor n -tags axes
        if {$y1ccnt>0} {
            $graph create text [expr {$xdispmin-$lmargin*0.1}] $ydispmax \
                    -font $label_font \
                    -text $ygraphmin_text -anchor se -tags axes
            $graph create text [expr {$xdispmin-$lmargin*0.1}] $ydispmin \
                    -font $label_font \
                    -text $ygraphmax_text -anchor ne -tags axes
        }
        if {$y2ccnt>0} {
            $graph create text [expr {$xdispmax+$rmargin*0.1}] $ydispmax \
                    -font $label_font \
                    -text $y2graphmin_text -anchor sw -tags axes
            $graph create text [expr {$xdispmax+$rmargin*0.1}] $ydispmin \
                    -font $label_font \
                    -text $y2graphmax_text -anchor nw -tags axes
        }

        # Render title
        if {![string match {} $title]} {
            $graph create text \
                    [expr {($xdispmin+$xdispmax)/2.}] \
                    [expr {$tmargin/2.}] \
                    -text $title -font $title_font \
                    -justify center -tags axes
            ## Use [expr {$canwidth/2.}] for x to get centering
            ## with respect to page instead of wrt graph.
        }

        # Place labels
        $graph create text \
                [expr {($xdispmin+$xdispmax)/2.}] \
                [expr {$canheight-$bmargin*0.5}]  \
                -font $label_font -justify center \
                -text $xlabel -tags axes
        if {$y1ccnt>0} {
            $graph create text \
                    [expr {$lmargin*0.5}]             \
                    [expr {($ydispmin+$ydispmax)/2.}] \
                    -font $label_font -justify center \
                    -text $ylabel -tags axes
        }
        if {$y2ccnt>0} {
            $graph create text \
                    [expr {$canwidth - $rmargin*0.5}] \
                    [expr {($ydispmin+$ydispmax)/2.}] \
                    -font $label_font -justify center \
                    -text $y2label -tags axes
        }
        
        # X-axes tics
        set xquart [expr {($xdispmax-$xdispmin)/4.}]
        set x1 [expr {$xdispmin+$xquart}]
        set y1 $ydispmax  ;  set y2 [expr {$y1-1.4*$axeswidth}]
        set y3 $ydispmin  ;  set y4 [expr {$y3+1.4*$axeswidth}]
        $graph create line $x1 $y1 $x1 $y2 \
                -fill $axescolor -width $axeswidth -tags {axes showpos}
        $graph create line $x1 $y3 $x1 $y4 \
                -fill $axescolor -width $axeswidth -tags {axes showpos}
        set x1 [expr {$xdispmax-$xquart}]
        $graph create line $x1 $y1 $x1 $y2 \
                -fill $axescolor -width $axeswidth -tags {axes showpos}
        $graph create line $x1 $y3 $x1 $y4 \
                -fill $axescolor -width $axeswidth -tags {axes showpos}
        set x1 [expr {$xdispmin+2*$xquart}]
        set y1 $ydispmax  ;  set y2 [expr {$y1-2*$axeswidth}]
        set y3 $ydispmin  ;  set y4 [expr {$y3+2*$axeswidth}]
        $graph create line $x1 $y1 $x1 $y2 \
                -fill $axescolor -width $axeswidth -tags {axes showpos}
        $graph create line $x1 $y3 $x1 $y4 \
                -fill $axescolor -width $axeswidth -tags {axes showpos}
        # Y1-axis tics
        if {$y1ccnt>0} {
            set yquart [expr {($ydispmax-$ydispmin)/4.}]
            set y1 [expr {$ydispmax-$yquart}]
            set x1 $xdispmin  ;  set x2 [expr {$x1+1.4*$axeswidth}]
            $graph create line $x1 $y1 $x2 $y1 \
                    -fill $axescolor -width $axeswidth -tags {axes showpos}
            if {$y2ccnt==0} {
                set x3 $xdispmax  ;  set x4 [expr {$x3-1.4*$axeswidth}]
                $graph create line $x3 $y1 $x4 $y1 -fill $axescolor \
                        -width $axeswidth -tags {axes showpos}
            }
            set y1 [expr {$ydispmin+$yquart}]
            $graph create line $x1 $y1 $x2 $y1 \
                    -fill $axescolor -width $axeswidth -tags {axes showpos}
            if {$y2ccnt==0} {
                $graph create line $x3 $y1 $x4 $y1 -fill $axescolor \
                        -width $axeswidth -tags {axes showpos}
            }
            set y1 [expr {$ydispmax-2*$yquart}]
            set x1 $xdispmin  ;  set x2 [expr {$x1+2*$axeswidth}]
            $graph create line $x1 $y1 $x2 $y1 \
                    -fill $axescolor -width $axeswidth -tags {axes showpos}
            if {$y2ccnt==0} {
                set x3 $xdispmax  ;  set x4 [expr {$x3-2*$axeswidth}]
                $graph create line $x3 $y1 $x4 $y1 -fill $axescolor \
                        -width $axeswidth -tags {axes showpos}
            }
        }
        # Y2-axis tics
        if {$y2ccnt>0} {
            set yquart [expr {($ydispmax-$ydispmin)/4.}]
            set y1 [expr {$ydispmax-$yquart}]
            if {$y1ccnt==0} {
                set x1 $xdispmin  ;  set x2 [expr {$x1+1.4*$axeswidth}]
                $graph create line $x1 $y1 $x2 $y1 -fill $axescolor \
                        -width $axeswidth -tags {axes showpos}
            }
            set x3 $xdispmax  ;  set x4 [expr {$x3-1.4*$axeswidth}]
            $graph create line $x3 $y1 $x4 $y1 -fill $axescolor \
                    -width $axeswidth -tags {axes showpos}
            set y1 [expr {$ydispmin+$yquart}]
            if {$y1ccnt==0} {
                $graph create line $x1 $y1 $x2 $y1 -fill $axescolor \
                        -width $axeswidth -tags {axes showpos}
            }
            $graph create line $x3 $y1 $x4 $y1 -fill $axescolor \
                    -width $axeswidth -tags {axes showpos}
            set y1 [expr {$ydispmax-2*$yquart}]
            if {$y1ccnt==0} {
                set x1 $xdispmin  ;  set x2 [expr {$x1+2*$axeswidth}]
                $graph create line $x1 $y1 $x2 $y1 -fill $axescolor \
                        -width $axeswidth -tags {axes showpos}
            }
            set x3 $xdispmax  ;  set x4 [expr {$x3-2*$axeswidth}]
            $graph create line $x3 $y1 $x4 $y1 -fill $axescolor \
                    -width $axeswidth -tags {axes showpos}
        }

        # Draw rules
        $this DrawRules $vrulepos $hrulepos

        # Draw key
        $this DrawKey $key_pos
    }

    method NewCurve { curvelabel yaxis } \
            { curvecount id idlist curve color symbol_types } {
        set curvename "$curvelabel,$yaxis"
        if {[info exists id($curvename)]} {
            error "Curve $curvename already exists"
        }

        lappend idlist $curvecount
        set curveid $curvecount
        set curvecolor $color([expr {$curvecount % $color(count)}])
               set symbol [expr {$curvecount % $symbol_types}]
        incr curvecount
        set segcolor {} ;# Segcolors are assigned in lazy fashion,
                          ## at time of initial rendering.
        set segsymbol {} ;# Ditto

        set id($curvename) $curveid
        array set curve [list              \
                $curveid,name  $curvename  \
                $curveid,label $curvelabel \
                $curveid,color $curvecolor \
                $curveid,segcolor  $segcolor \
                $curveid,symbol $symbol \
                $curveid,segsymbol  $segsymbol \
                $curveid,ptsbreak    {}      \
                $curveid,pts         {}      \
                $curveid,lastpt   {-2 0. 0.} \
                $curveid,ptsdisp     {}      \
                $curveid,lastdpt     {}      \
                $curveid,dnextpt     0       \
                $curveid,dispcurrent 1       \
                $curveid,segcount    0       \
                $curveid,hidden      0       \
                $curveid,yaxis     $yaxis    \
                $curveid,xmin        {}      \
                $curveid,ymin        {}      \
                $curveid,xmax        {}      \
                $curveid,ymax        {}       ]

        # Update key
        $this DrawKey

        return $curveid
    }
    private method GetCurveId { curvename } { id } {
        # Returns empty string if curvename is invalid
        if {[catch {set id($curvename)} curveid]} {
            return {}
        }
        return $curveid
    }
    private method DeleteCurveByName { curvename } {
        set curveid [$this GetCurveId $curvename]
        if {[string match {} $curveid]} { return } ;# No such curve

        set index [lsearch -exact $idlist $curveid]
        if {$index >=0} {
            set idlist [lreplace $idlist $index $index]
        }

        $graph delete tag$curveid
        foreach i [array names curve $curveid,*] {
            unset curve($i)
        }
        unset id($curvename)

        $this DrawKey      ;# Update key
    }
    method DeleteAllCurves {} {
        # Deletes all data and curves
        foreach name [array names id] {
            $this DeleteCurveByName $name
        }
	set curvecount 0
        set segcount 0
    }
    method DeleteCurve { curvelabel yaxis } {
        set curvename "$curvelabel,$yaxis"
        $this DeleteCurveByName $curvename
    }
    private method CutCurve { curveid } { curve ptlimit ptlimitcut } {
       if {$ptlimit<1} { return }
       foreach { lastindex x y } $curve($curveid,lastpt) {}
       if {$lastindex+2 <= 2*$ptlimit} { return }
       set newlength [expr {2 * int(floor($ptlimit * $ptlimitcut))}]
       set offset [expr {$lastindex+2-$newlength}]
       if {$offset<=0} { return }
       set curve($curveid,pts) [lrange $curve($curveid,pts) $offset end]
       set brklst {}
       foreach pt $curve($curveid,ptsbreak) {
          if {$pt>=$offset} {
             lappend brklst [expr {$pt-$offset}]
          }
       }
       set cutindex [expr {[llength $curve($curveid,ptsbreak)] \
                              -[llength $brklst]}]
       set curve($curveid,ptsbreak) $brklst
       set curve($curveid,segcolor) \
          [lrange $curve($curveid,segcolor) $cutindex end]
       set curve($curveid,segsymbol) \
          [lrange $curve($curveid,segsymbol) $cutindex end]
       set xmin $x ; set xmax $x ; set ymin $y ; set ymax $y
       foreach { x y } $curve($curveid,pts) {
          if {$x<$xmin} { set xmin $x }
          if {$x>$xmax} { set xmax $x }
          if {$y<$ymin} { set ymin $y }
          if {$y>$ymax} { set ymax $y }
       }
       set curve($curveid,xmin) $xmin
       set curve($curveid,xmax) $xmax
       set curve($curveid,ymin) $ymin
       set curve($curveid,ymax) $ymax
       set curve($curveid,lastpt) [list [expr {$newlength - 2}] $x $y]
       set curve($curveid,ptsdisp) {}
       set curve($curveid,lastdpt) {}
       set curve($curveid,dnextpt) 0
       set curve($curveid,dispcurrent) 0
       if {$cutindex>0} {
          # Segment count has changed.  This may affect key.
          $this DrawKey
       }
    }
    method AddDataPoints { curvelabel yaxis newpts } \
       { id curve ptlimit color } {
        # Returns xmin ymin xmax ymax of input data.  (This may
        # be different from extremes of appended data, if ptlimit
        # is smaller than the number of appended points.)
        # Also sets curve($curveid,dispcurrent) to 0 (false).
        # Returns {} if no data input.
        set curvename "$curvelabel,$yaxis"
        set argcount [llength $newpts]
        if {$argcount<2} { return {} }
        if {$argcount % 2 != 0} {
            error "List argument must have even number of elements\n\n\
             usage:\n\t\$instance AddDataPoints curvelabel yaxis list\n\n\
             Invalid call:\n\t$this AddDataPoints $curvelabel $yaxis $newpts"
        }

        if {[catch {set id($curvename)} curveid]} {
            set curveid [$this NewCurve $curvelabel $yaxis]
        }

        # Initialize {x|y}min to "z", {x|y}max to {}, so
        # $x<$xmin and $x>$xmax for *all* numeric $x.
        set xmin [set ymin "z"]
        set xmax [set ymax {}]
        foreach { lastindex x y } $curve($curveid,lastpt) {}
        foreach { tx ty } $newpts {
            if {[string match {} $tx]} {
                # Curve break request; chuck breaks at front and
                # duplicates
                if {$lastindex>=0 && \
                        [lindex $curve($curveid,ptsbreak) end] \
                        < $lastindex} {
                    # NOTE: If ptsbreak is {}, then the second
                    #  test above will always be true (as a
                    #  string comparison).
                    lappend curve($curveid,ptsbreak) $lastindex
                }
            } else {
                # Make sure input is represented as a floating
                # point value, and protect against underflow.
                if {[catch {scan $tx %g x}]} {set x 0.0}
                if {[catch {scan $ty %g y}]} {set y 0.0}
                # Track extreme values
                if {$x<$xmin} {set xmin $x}
                if {$x>$xmax} {set xmax $x}
                if {$y<$ymin} {set ymin $y}
                if {$y>$ymax} {set ymax $y}
                lappend curve($curveid,pts) $x $y
                incr lastindex 2
            }
        }
        set curve($curveid,lastpt) [list $lastindex $x $y]
        if {[string match {} $xmax]} {
            # No non-empty points entered
            set xmin [set ymin {}]
        } elseif {[string match {} $curve($curveid,xmin)]} {
            set curve($curveid,xmin) $xmin
            set curve($curveid,xmax) $xmax
            set curve($curveid,ymin) $ymin
            set curve($curveid,ymax) $ymax
        } else {
            if {$xmin<$curve($curveid,xmin)} {
                set curve($curveid,xmin) $xmin
            }
            if {$xmax>$curve($curveid,xmax)} {
                set curve($curveid,xmax) $xmax
            }
            if {$ymin<$curve($curveid,ymin)} {
                set curve($curveid,ymin) $ymin
            }
            if {$ymax>$curve($curveid,ymax)} {
                set curve($curveid,ymax) $ymax
            }
        }
        set curve($curveid,dispcurrent) 0
        if {$ptlimit>0 && $lastindex+2>[expr {2*$ptlimit}]} {
            $this CutCurve $curveid
        }
        if {[string match {} $xmin]} { return }
        return [list $xmin $ymin $xmax $ymax]
    }
    method ResetSegmentColors {} {
       foreach curveid $idlist {
          set curve($curveid,segcolor) {}
          set curve($curveid,segsymbol) {}
       }
       set segcount 0
    }
    method DeleteAllData {} {
        # Deletes all data, but leaves curve structures
        foreach curveid $idlist {
            array set curve [list                \
                    $curveid,ptsbreak   {}       \
                    $curveid,pts        {}       \
                    $curveid,lastpt   {-2 0. 0.} \
                    $curveid,ptsdisp  {}         \
                    $curveid,lastdpt  {}         \
                    $curveid,dnextpt   0         \
                    $curveid,dispcurrent 1       \
                    $curveid,xmin     {}         \
                    $curveid,ymin     {}         \
                    $curveid,xmax     {}         \
                    $curveid,ymax     {}          ]
            $graph delete tag$curveid
        }
        $this DrawKey
    }
    private method AddDisplayPoints { cid datapts } {
       boundary_clip_size curve xscale yscale y2scale lmargin tmargin
       plotwidth plotheight xgraphmin xgraphmax  ygraphmin ygraphmax
       y2graphmin y2graphmax } {
       # Converts datapts to display coordinates, and appends to
       # end of last display segment.
       #   To start a new segment, calling routines should lappend an empty
       # list to $curve($cid,ptsdisp) before calling this routine.
       #   The return value is a value suitable for passing to PlotCurve
       # for drawing only new points.  Special values: -2 => no
       # new points were added, -3 => last list elt of $curve($cid,ptsdisp)
       # removed.

       # NOTE: For improved accuracy, don't use xoff, yoff or y2off
       # directly, but replace with these relations:
       #
       #    xoff   = lmargin - xgraphmin * xscale
       #    yoff   = tmargin - ygraphmax * yscale
       #    y2off  = tmargin - ygraphmax * y2scale
       #
       # So, e.g.,
       #             xdisp = x*xscale + xoff
       #                   = x*xscale + (lmargin - xgraphmin*xscale)
       #                   = (x-xgraphmin)*xscale + lmargin
       #
       # The last form has one extra addition, but is more accurate
       # because x*scale and xgraphmin*xscale may be on a very
       # different scale from lmargin, so the parenthesized expression
       # above may loose all precision.   -mjd, 25-Jan-2001

       # NOTE: Although the canvas widget uses floating point coordinates,
       #  the canvas here is setup with pixels corresponding to integral
       #  coordinates.  So we go ahead and do the rounding ourselves to
       #  integer values.

       # Determine y-axis scaling
       if { $curve($cid,yaxis) != 2 } {
          set ys $yscale
          set ygmin $ygraphmin
          set ygmax $ygraphmax
       } else {
          set ys $y2scale
          set ygmin $y2graphmin
          set ygmax $y2graphmax
       }

       # Specify draw bounding box
       if {$xscale!=0.0} {
          set xfudge [expr {abs($boundary_clip_size/double($xscale))}]
       } else {
          set xfudge 0.0;
       }
       set xminbound [expr {$xgraphmin-$xfudge}]
       set xmaxbound [expr {$xgraphmax+$xfudge}]
       if {$ys!=0.0} {
          set yfudge [expr {abs($boundary_clip_size/double($ys))}]
       } else {
          set yfudge 0.0;
       }
       set yminbound [expr {$ygmin-$yfudge}]
       set ymaxbound [expr {$ygmax+$yfudge}]


       set displist [lindex $curve($cid,ptsdisp) end]
       set startindex [llength $displist]
       if {[llength $curve($cid,lastdpt)]==3} {
          foreach { xprev yprev previnc } \
             $curve($cid,lastdpt) {}
          if {$startindex>0} {
             if {$previnc} {incr startindex -2}
          } else {
             # No points drawn yet
             if {$xminbound<$xprev && $xprev<$xmaxbound \
                    && $yminbound<$yprev && $yprev<$ymaxbound} {
                lappend displist \
                   [expr {round(($xprev-$xgraphmin)*$xscale+$lmargin)}]
                lappend displist \
                   [expr {round(($yprev-$ygmax)*$ys+$tmargin)}]
                set previnc 1
             } else {
                set previnc 0
             }
          }
       } else {
          # Fresh segment
          set xprev [lindex $datapts 0]
          set yprev [lindex $datapts 1]
          if {[string match {} $yprev]} {
             # Hmm, no data... Handle case where displist is
             # too short, and return.  This should be a rare
             # circumstance.
             OWgwAssert 0 "ADP -- No data appended to new segment" ;###
             set dlength [llength $displist]
             if {$dlength==2} {
                # This *really* shouldn't happen!  If lastdpt
                # is empty, then displist should be too.
                return -code error \
                   "ADP -- Data structure corruption detected"
             }
             if {$dlength<4} {
                set curve($cid,ptsdisp) \
                   [lreplace $curve($cid,ptsdisp) end end]
                return -3
             }
             return -2
          } else {
             set datapts [lreplace $datapts 0 1]
             if {$xminbound<$xprev && $xprev<$xmaxbound \
                    && $yminbound<$yprev && $yprev<$ymaxbound} {
                lappend displist \
                   [expr {round(($xprev-$xgraphmin)*$xscale+$lmargin)}]
                lappend displist \
                   [expr {round(($yprev-$ygmax)*$ys+$tmargin)}]
                set previnc 1
             } else {
                set previnc 0
             }
          }
       }

       OWgwAssert {$startindex>=0} {  ADP Bad startindex: $startindex
          cid: $cid
          datapts: $datapts
          curve($cid,lastdpt): $curve($cid,lastdpt)
          displist: $displist}  ;###

       # Add all points that will display at distinct screen locations
       set xdiff 1.  ; set ydiff 1.   ;# Safety init
       if {$xscale!=0.} {set xdiff [expr {1./abs($xscale)}] }
       if {$ys!=0.} {set ydiff [expr {1./abs($ys)}] }
       foreach { x y } $datapts {
          if { abs($x-$xprev)<$xdiff && abs($y-$yprev)<$ydiff } {
             # Points too close to distinguish
             continue
          }
          if {($x<$xminbound && $xprev<$xminbound) \
                 || ($x>$xmaxbound && $xprev>$xmaxbound) \
                 || ($y<$yminbound && $yprev<$yminbound) \
                 || ($y>$ymaxbound && $yprev>$ymaxbound)} {
             # Bounding box of (xprev,yprev) and (x,y) doesn't
             # intersect display box, so no part of the line
             # segment between (xprev,yprev) and (x,y) is visible.
             set previnc 0
             set xprev $x
             set yprev $y
             continue
          }
          set previnc [OWgwMakeDispPt displist \
                          $xgraphmin $ygmax $xscale $ys \
                          $lmargin $tmargin \
                          $xminbound $yminbound $xmaxbound $ymaxbound \
                          $previnc $xprev $yprev $x $y]
          set xprev $x
          set yprev $y
       }
       # Update display point list.
       set newlength [llength $displist]
       set curve($cid,ptsdisp) \
          [lreplace $curve($cid,ptsdisp) end end $displist]
       if {$newlength<2} {
          # Short list.  Mark curve($cid,lastdpt) appropriately.
          set previnc 0
          set startindex -3
       } else {
          if {$startindex+2>$newlength} {
             set startindex -2 ;# No new data to display
          }
       }
       set curve($cid,lastdpt) [list $xprev $yprev $previnc]

       return $startindex
    }
    private method UpdateDisplayList { cid } { curve } {
        # Makes the curve display list up-to-date with respect to
        # the data list.  The return value is the index into the
        # display list necessary to update the graphed display, i.e.,
        # the 'startindex' value appropriate for calling PlotCurve.
        # A value of -1 indicates that the curve should be completely
        # redrawn, and -2 occurs if no points were added to the display
        # list.
        if {$curve($cid,dispcurrent)} { return -2 }

        set startpt $curve($cid,dnextpt)
        set lastpt [lindex $curve($cid,lastpt) 0]
        if {$startpt>$lastpt} {
            set curve($cid,dispcurrent) 1
            return -2
        }

        set segnum [llength $curve($cid,ptsdisp)]
        set breakcount [llength $curve($cid,ptsbreak)]
        if {$segnum>$breakcount} {
            # Append new points to current segment
            set startindex [$this AddDisplayPoints $cid \
                    [lrange $curve($cid,pts) $startpt end]]
            set curve($cid,dnextpt) [expr $lastpt + 2]
            set curve($cid,dispcurrent) 1
            return $startindex
        }

        # Otherwise we have to break pts into segments
        set writecount 0
        set startindex -2 ;# Safety
        if {$segnum>0} {
            set brklist [lrange $curve($cid,ptsbreak) \
                    [expr $segnum-1] end]
        } else {
            set brklist [concat -2 $curve($cid,ptsbreak)]
        }
        if {[llength $brklist]>0} {
            set bpt [lindex $brklist 0]
            if {$startpt<=$bpt} {
                if {$segnum<1} {
                    set segnum 1
                    set curve($cid,ptsdisp) [list {}]
                }
                set startindex [$this AddDisplayPoints $cid \
                        [lrange $curve($cid,pts) $startpt [expr $bpt+1]]]
                incr writecount
                set startpt [expr $bpt+2]
                set curve($cid,lastdpt) {} ;# End of segment reached
            } else {
                # Check lastdpt validity
                if {$startpt < $bpt+4} {
                    # $bpt+2 hasn't been processed yet, so start fresh
                    set curve($cid,lastdpt) {} ;# End of segment reached
                }
            }
        }
        foreach bpt [lrange $brklist 1 end] {
            incr segnum
            lappend curve($cid,ptsdisp) {}
            set startindex [$this AddDisplayPoints $cid \
                    [lrange $curve($cid,pts) $startpt [expr $bpt+1]]]
            incr writecount
            set startpt [expr $bpt+2]
            set curve($cid,lastdpt) {} ;# End of segment reached
        }
        if {$startpt<=$lastpt} {
            incr segnum
            lappend curve($cid,ptsdisp) {}
            set startindex [$this AddDisplayPoints $cid \
                    [lrange $curve($cid,pts) $startpt end]]
            if {$startindex==-3} {
                # New segment too short to keep
                incr segnum -1
                set startindex -2  ;# No need to redraw last segment
            } else {
                incr writecount
            }
        }
        if {$writecount>1} {
            set startindex -1 ;# Redraw all segments
        }
        set curve($cid,dnextpt) [expr $lastpt + 2]
        set curve($cid,dispcurrent) 1
        return $startindex
    }
    private proc PlotSymbols { graph color symbol size width freq \
                                  tags underid offset_index pts} {
       # This proc draws a '+' at a subset of pts, determined by freq.
       if {$freq<1} { return } ;# freq == 0 ==> draw no symbols
       set index [expr {$offset_index/2 - 1}]
       # offset_index is offset into ptlist, which is twice the points
       # count, because each (x,y) coordinate pair is stored as two
       # elements in ptlist.  'freq', though, is counted wrt points.
       #   Return value is gid list for all drawn points.  This will
       # be an empty list if no points were rendered.
       set gids {}
       foreach {x y} $pts {
          incr index
          if { $index % $freq != 0 } { continue }
          set x1 [expr {$x-0.5*$size}]
          set x2 [expr {$x1 + $size}]
          set y1 [expr {$y-0.5*$size}]
          set y2 [expr {$y1 + $size}]
          if {0 == $symbol} {
             # "+" symbol
             lappend gids [$graph create line $x1 $y $x2 $y \
                              -fill $color -width $width -tags $tags]
             lappend gids [$graph create line $x $y1 $x $y2 \
                              -fill $color -width $width -tags $tags]
          } elseif {1 == $symbol} {
             # "X" symbol
             lappend gids [$graph create line $x1 $y1 $x2 $y2 \
                              -fill $color -width $width -tags $tags]
             lappend gids [$graph create line $x2 $y1 $x1 $y2 \
                              -fill $color -width $width -tags $tags]
          } elseif {2 == $symbol} {
             # Square symbol
             lappend gids [$graph create line \
                              $x1 $y1 $x2 $y1 $x2 $y2 $x1 $y2 $x1 $y1 \
                              -fill $color -width $width -tags $tags]
          } elseif {3 == $symbol} {
             # Diamond symbol
             lappend gids [$graph create line \
                              $x1 $y $x $y1 $x2 $y $x $y2 $x1 $y \
                              -fill $color -width $width -tags $tags]
          } elseif {4 == $symbol} {
             # Triangle symbol
             lappend gids [$graph create line \
                              $x1 $y1 $x2 $y1 $x $y2 $x1 $y1 \
                              -fill $color -width $width -tags $tags]
          } elseif {5 == $symbol} {
             # Inverted triangle symbol
             lappend gids [$graph create line \
                              $x1 $y2 $x $y1 $x2 $y2 $x1 $y2 \
                              -fill $color -width $width -tags $tags]
          } else {
             error "Invalid symbol code $symbol; should be 0 - 5"
          }
       }
       foreach g $gids { $graph lower $g $underid }
       return $gids
    }
    private method PlotCurve { curveid update_only} \
            { graph curve curve_width segmax smoothcurves matteid \
              usedrawpiece drawpiece drawsmoothpiece \
              symbol_freq symbol_size color symbol_types \
              color_selection segcount} {
        # NOTE 1: If update_only is 0, then tag$curveid is
        #  first deleted, and then the entire curve is
        #  redrawn.  Otherwise only that portion of the
        #  curve not yet rendered is drawn.
        if {![info exists curve($curveid,ptsdisp)] || \
                $curve($curveid,hidden)} {
            $graph delete tag$curveid
            $this InvalidateDisplay $curveid
            return
        }
        set entry_segcount $segcount
        set startindex [$this UpdateDisplayList $curveid]
        if {!$update_only || $curve($curveid,segcount)>=$segmax} {
            $graph delete tag$curveid
            set startindex -1
        }

        if {$startindex == -2} {
            return  ;# Display already up-to-date
        } elseif {$startindex>0} {
            # Draw last section of last segment only
            incr curve($curveid,segcount)
            set segindex [expr {[llength $curve($curveid,ptsdisp)] - 1}]
            set ptlist [lindex $curve($curveid,ptsdisp) $segindex]
            set ptlist [lrange $ptlist $startindex end]
            OWgwAssert {[llength $ptlist]%2==0} \
                    {PC-a Bad ptlist: $ptlist}                     ;###
            if {![string match "segment" $color_selection]} {
               set clr $curve($curveid,color)
               set sym $curve($curveid,symbol)
            } else {
               while {[string match {} \
                [set clr [lindex $curve($curveid,segcolor) $segindex]]]} {
                  # Segment colors and symbols are assigned in lazy fashion
                  lappend curve($curveid,segcolor) \
                     $color([expr {$segcount % $color(count)}])
                  lappend curve($curveid,segsymbol) \
                     [expr {$segcount % $symbol_types}]
                  incr segcount
               }
               set sym [lindex $curve($curveid,segsymbol) $segindex]
            }
            if {[llength $ptlist]>=4} {
               set gid [eval $graph create line $ptlist \
                           -fill $clr \
                           -width $curve_width \
                           -smooth $smoothcurves \
                           -tags {[list tag$curveid showpos]}]
               $graph lower $gid $matteid
            }
            Ow_GraphWin PlotSymbols $graph $clr $sym \
               $symbol_size $curve_width $symbol_freq \
               [list tag$curveid showpos] $matteid $startindex $ptlist
        } else {
            # Redraw all segments
            set curve($curveid,segcount) 1
            $graph delete tag$curveid
            if {[string match "segment" $color_selection]} {
               set color_by_segment 1
            } else {
               set color_by_segment 0
               set clr $curve($curveid,color)
               set sym $curve($curveid,symbol)
            }
            set segindex 0
            foreach ptlist $curve($curveid,ptsdisp) {
               if {$color_by_segment} {
                  while {[string match {} [set clr \
                          [lindex $curve($curveid,segcolor) $segindex]]]} {
                     # Segment colors and symbols are assigned in lazy fashion
                     lappend curve($curveid,segcolor) \
                        $color([expr {$segcount % $color(count)}])
                     lappend curve($curveid,segsymbol) \
                        [expr {$segcount % $symbol_types}]
                     incr segcount
                  }
                  set sym [lindex $curve($curveid,segsymbol) $segindex]
                  incr segindex
               }
               set totalsize [llength $ptlist]
               if {$totalsize==0} { continue }
               OWgwAssert {$totalsize%2==0} \
                  {PC-b Bad ptlist: $ptlist}                     ;###
               if {!$usedrawpiece} {
                  if {$totalsize>=4} {
                     set cmd [linsert $ptlist 0 $graph create line]
                     lappend cmd -fill $clr \
                        -width $curve_width \
                        -smooth $smoothcurves \
                        -tags [list tag$curveid showpos]
                     set gid [eval $cmd]
                     $graph lower $gid $matteid
                  }
                  Ow_GraphWin PlotSymbols $graph $clr $sym \
                     $symbol_size $curve_width $symbol_freq \
                     [list tag$curveid showpos] $matteid 0 $ptlist
               } else {
                  if {$smoothcurves} {
                     set piecesize $drawsmoothpiece
                  } else {
                     set piecesize $drawpiece
                  }
                  set piecestart 0
                  set piecestop [expr {$piecestart+$piecesize}]
                  while {$piecestop<$totalsize} {
                     set cmd [linsert \
                                 [lrange $ptlist $piecestart $piecestop] \
                                 0 $graph create line]
                     lappend cmd -fill $clr \
                        -width $curve_width \
                        -smooth $smoothcurves \
                        -tags [list tag$curveid showpos]
                     set gid [eval $cmd]
                     $graph lower $gid $matteid
                     set piecestart [expr {$piecestop-1}]
                     set piecestop [expr {$piecestart+$piecesize}]
                  }
                  if {$piecestart<[expr {$totalsize-2}]} {
                     set cmd [linsert [lrange $ptlist $piecestart end] \
                                 0 $graph create line]
                     lappend cmd -fill $clr \
                        -width $curve_width \
                        -smooth $smoothcurves \
                        -tags [list tag$curveid showpos]
                     set gid [eval $cmd]
                     $graph lower $gid $matteid
                  }
                  Ow_GraphWin PlotSymbols $graph $clr $sym \
                     $symbol_size $curve_width $symbol_freq \
                     [list tag$curveid showpos] $matteid 0 $ptlist
               }
            }
         }
        if {$entry_segcount != $segcount} {
           # New segement(s) rendered; update key
           $this DrawKey
        }
    }
    method GetCurveList { yaxis } {
        # Returns an ordered list two-tuples of all non-hidden curves,
        # where each two-tuple is { curvelabel yaxis }
        set curvelist {}
        foreach curveid $idlist {
            if {! $curve($curveid,hidden) && \
                    $curve($curveid,yaxis) == $yaxis } {
                if {![regexp {^(.*),([12])$} $curve($curveid,name) \
                        dummy curvelabel yaxis]} {
                    error "Curvename wrong format: $curve($curveid,name)"
                }
                lappend curvelist $curvelabel
            }
        }
        return $curvelist
    }
    method DrawCurves {{update_only 1}} {
        # Note: We want to draw curves in order
        set curves_drawn 0

        # Separate out y1 and y2 axis curves
        set y1list [set y2list {}]
        foreach curveid $idlist {
            if {$curve($curveid,hidden)} { continue }
            if {$curve($curveid,yaxis)!=2} {
                lappend y1list $curveid
            } else {
                lappend y2list $curveid
            }
        }
        foreach curveid $y1list {
            $this PlotCurve $curveid $update_only
            incr curves_drawn
        }
        foreach curveid $y2list {
            $this PlotCurve $curveid $update_only
            incr curves_drawn
        }
        return $curves_drawn
    }
    private method InvalidateDisplay { curvelist } { curve } {
        # Invalidate display portion of all curves in curvelist.
        # Note: Pass in instance variable $idlist to invalidate
        #  all curves.
        foreach cid $curvelist {
            array set curve [list \
                    $cid,ptsdisp    {} \
                    $cid,lastdpt    {} \
                    $cid,dnextpt     0 \
                    $cid,dispcurrent 0 ]
            if {[lindex $curve($cid,lastpt) 0]<0} {
                set curve($cid,dispcurrent) 1
                ## No data points, so ptsdisp is actually up-to-date!
            }
        }
    }
    private method GetDataExtents {} {
        # Initialize {x|y|y2}min to "z", {x|y|y2}max to {}, so
        # $x<$xmin and $x>$xmax for *all* numeric $x, etc.
        set xmin [set ymin [set y2min "z"]]
        set xmax [set ymax [set y2max {}]]
        set ptcount 0
        foreach cid $idlist {
            if {[string match {} $curve($cid,xmin)] || \
                    $curve($cid,hidden) } {
                continue     ;# Empty or hidden curve
            }
            set test $curve($cid,xmin)
            if {$test<$xmin} {set xmin $test}
            set test $curve($cid,xmax)
            if {$test>$xmax} {set xmax $test}
            if {$curve($cid,yaxis)!=2} {
                set test $curve($cid,ymin)
                if {$test<$ymin} {set ymin $test}
                set test $curve($cid,ymax)
                if {$test>$ymax} {set ymax $test}
            } else {
                set test $curve($cid,ymin)
                if {$test<$y2min} {set y2min $test}
                set test $curve($cid,ymax)
                if {$test>$y2max} {set y2max $test}
            }
            set cidptcount [expr [llength $curve($cid,pts)]/2 - 1]
            if {$cidptcount>0} { incr ptcount $cidptcount }
        }
        # Fill in missing data with dummy values
        if {[string match {} $xmax]} {
            set xmin 0.0 ; set xmax 1.0
        }
        if {[string match {} $ymax]} {
            set ymin 0.0 ; set ymax 1.0
        }
        if {[string match {} $y2max]} {
            set y2min 0.0 ; set y2max 1.0
        }
        if {$ptcount == 0} {
            # No curve has more than 1 point in it, so expand
            # empty ranges by 10% so auto-limit calculations
            # don't try to display out to 17 digits.
            if { $xmin == $xmax } {
                set xmin [expr $xmin * 0.95]
                set xmax [expr $xmax * 1.05]
            }
            if { $ymin == $ymax } {
                set ymin [expr $ymin * 0.95]
                set ymax [expr $ymax * 1.05]
            }
            if { $y2min == $y2max } {
                set y2min [expr $y2min * 0.95]
                set y2max [expr $y2max * 1.05]
            }
        }

        return [list $xmin $xmax $ymin $ymax $y2min $y2max]
    }
    private proc RoundExtents {xmin xmax ymin ymax y2min y2max} {
        # Adjust to "round" numbers
        set xdelta [expr {($xmax-$xmin)*0.999}]  ;# Allow a *little* slack
        if {$xdelta<=0.0 || \
		[catch {expr {pow(10,ceil(-log10($xdelta)))}} xrangescale]} {
            # Probably underflow; treat the same as if $xdelta were 0,
            # i.e., set xdelta and xrangescale to 1
            set xdelta 1
            set xrangescale 1
        }
        set xmin [expr {(floor($xmin*$xrangescale))/$xrangescale}]
        set xmax [expr {(ceil($xmax*$xrangescale))/$xrangescale}]
        set ydelta [expr {($ymax-$ymin)*0.999}]  ;# Allow a *little* slack
        if {$ydelta<=0.0 || \
		[catch {expr {pow(10,ceil(-log10($ydelta)))}} yrangescale]} {
	    set ydelta 1.
            set yrangescale 1
	}
        set ymin [expr {(floor($ymin*$yrangescale))/$yrangescale}]
        set ymax [expr {(ceil($ymax*$yrangescale))/$yrangescale}]
        set y2delta [expr {($y2max-$y2min)*0.999}] ;# Allow a *little* slack
        if {$y2delta<=0.0 || \
		[catch {expr {pow(10,ceil(-log10($y2delta)))}} y2rangescale]} {
            set y2delta 1
            set y2rangescale 1
        }
        set y2min [expr {(floor($y2min*$y2rangescale))/$y2rangescale}]
        set y2max [expr {(ceil($y2max*$y2rangescale))/$y2rangescale}]
        
        # Round and save for future use. (Zero needs to be handled
        # specially because scaling adjustment above falls flat.)
        # Use "#" form of g format to insure decimal point is written.
        # Otherwise, it is possible to convert a floating point value
        # that is too large to represent as an integer into an integer
        # expression that Tcl won't handle, e.g.:
        #  set x 1e10 ; set y [format "%.12g" $x] ; expr double($y)
        # The last command produces an error because y has the
        # string value "10000000000".
        if {$xmin==0 && $xmax==0} {
            set xmin -0.5
            set xmax  0.5
        } else {
            set xtestmin [format "%#.12g" $xmin]
            set xtestmax [format "%#.12g" $xmax]
            if { $xtestmax<=$xtestmin } {
                # Too much rounding.  Try again
                set xtestmin [format "%#.15g" $xmin]
                set xtestmax [format "%#.15g" $xmax]
            }
            set xmin [expr {double($xtestmin)}] ;# Make _certain_ these
            set xmax [expr {double($xtestmax)}] ;# are f.p. values.
        }

        if {$ymin==0 && $ymax==0} {
            set ymin -0.5
            set ymax  0.5
        } else {
            set ytestmin [format "%#.12g" $ymin]
            set ytestmax [format "%#.12g" $ymax]
            if { $ytestmax<=$ytestmin } {
                # Too much rounding.  Try again
                set ytestmin [format "%#.15g" $ymin]
                set ytestmax [format "%#.15g" $ymax]
            }
            set ymin [expr {double($ytestmin)}] ;# Make _certain_ these
            set ymax [expr {double($ytestmax)}] ;# are f.p. values.
        }

        if {$y2min==0 && $y2max==0} {
            set y2min -0.5
            set y2max  0.5
        } else {
            set y2testmin [format "%#.12g" $y2min]
            set y2testmax [format "%#.12g" $y2max]
            if { $y2testmax<=$y2testmin } {
                # Too much rounding.  Try again
                set y2testmin [format "%#.15g" $y2min]
                set y2testmax [format "%#.15g" $y2max]
            }
            set y2min [expr {double($y2testmin)}] ;# Make _certain_ these
            set y2max [expr {double($y2testmax)}] ;# are f.p. values.
        }

        return [list $xmin $xmax $ymin $ymax $y2min $y2max]
    }
    private proc NiceExtents { ratio xmin xmax ymin ymax y2min y2max} {
        # Adjusts ?min and ?max by up to +/-(?max-?min)/ratio to
        # make them "nice" when formatted as a base 10 decimal.
        # Similar to but generally less aggressive than RoundExtents,
        # q.v.
        #   Note: We use floor(0.5+x) rather than round(x), because
        # the latter converts to an int, which has a smaller range
        # than double.
        if {$ratio<=0.0} {
            return [list $xmin $xmax $ymin $ymax $y2min $y2max]
        }
        set slop [expr {($xmax-$xmin)/double($ratio)}]
        if {$slop<=0.} {
            lappend results $xmin $xmax
        } else {
            set slop [expr {double(pow(10,floor(log10($slop))))}]
            lappend results [expr {$slop*floor(0.5+$xmin/$slop)}]
            lappend results [expr {$slop*floor(0.5+$xmax/$slop)}]
        }
        set slop [expr {($ymax-$ymin)/double($ratio)}]
        if {$slop<=0.} {
            lappend results $ymin $ymax
        } else {
            set slop [expr {double(pow(10,floor(log10($slop))))}]
            lappend results [expr {$slop*floor(0.5+$ymin/$slop)}]
            lappend results [expr {$slop*floor(0.5+$ymax/$slop)}]
        }
        set slop [expr {($y2max-$y2min)/double($ratio)}]
        if {$slop<=0.} {
            lappend results $y2min $y2max
        } else {
            set slop [expr {double(pow(10,floor(log10($slop))))}]
            lappend results [expr {$slop*floor(0.5+$y2min/$slop)}]
            lappend results [expr {$slop*floor(0.5+$y2max/$slop)}]
        }
        return $results
    }
    method SetGraphLimits {{xmin {}} {xmax {}} \
            {ymin {}} {ymax {}} {y2min {}} {y2max {}}} {
        # Sets the graph numerical limits.  Any blank values are
        # filled in using data values.  This routine does not update
        # the display.
        #  If any change is made, all curve displays are marked invalid,
        # and 1 is returned.  If no change is made, then 0 is returned.

        # Make certain inputs use floating point representation
        if {![string match {} $xmin]}  { set xmin [expr {double($xmin)}] }
        if {![string match {} $xmax]}  { set xmax [expr {double($xmax)}] }
        if {![string match {} $ymin]}  { set ymin [expr {double($ymin)}] }
        if {![string match {} $ymax]}  { set ymax [expr {double($ymax)}] }
        if {![string match {} $y2min]} { set y2min [expr {double($y2min)}] }
        if {![string match {} $y2max]} { set y2max [expr {double($y2max)}] }

	# Compute padding amount
        global tcl_precision
	if {[info exists tcl_precision]
            && 0<$tcl_precision && $tcl_precision<15} {
           set eps [expr {pow(10.,1-$tcl_precision)}]
	} else {
           # If tcl_precision is 17 (or 0, which means
           # 17 but with short display), then effective
           # precision is actually IEEE REAL*8 EPSILON,
           # which is approximately 2.22e-16.  We round
           # up to 1e-14 to be safe, and to play nicely
           # with RoundExtents, which uses %.15g format.
           set eps [expr {pow(10.,-14)}]
	}
        foreach {oxmin oxmax oymin oymax oy2min oy2max} \
                [list $xgraphmin $xgraphmax \
                $ygraphmin $ygraphmax $y2graphmin $y2graphmax] {}

        if {[string match {} $xmin] || [string match {} $xmax] \
                || [string match {} $ymin] || [string match {} $ymax] \
                || [string match {} $y2min] || [string match {} $y2max]} {
            foreach {xgraphmin xgraphmax \
                    ygraphmin ygraphmax y2graphmin y2graphmax} \
                    [$this GetDataExtents] {}
            # Add some luft in case data are constant (no variation)
            if {$xgraphmax<=$xgraphmin} {
                set xgraphmax [expr {$xgraphmin+$eps*abs($xgraphmin)}]
                set xgraphmin [expr {$xgraphmin-$eps*abs($xgraphmin)}]
            }
            if {$ygraphmax<=$ygraphmin} {
                set ygraphmax [expr {$ygraphmin+$eps*abs($ygraphmin)}]
                set ygraphmin [expr {$ygraphmin-$eps*abs($ygraphmin)}]
            }
            if {$y2graphmax<=$y2graphmin} {
                set y2graphmax [expr {$y2graphmin+$eps*abs($y2graphmin)}]
                set y2graphmin [expr {$y2graphmin-$eps*abs($y2graphmin)}]
            }
            # Make "nice" limits
            foreach { xgraphmin xgraphmax \
                    ygraphmin ygraphmax y2graphmin y2graphmax } \
                    [Ow_GraphWin RoundExtents $xgraphmin $xgraphmax \
                    $ygraphmin $ygraphmax $y2graphmin $y2graphmax] {}
            if {![string match {} $xmin]} { set xgraphmin $xmin }
            if {![string match {} $xmax]} { set xgraphmax $xmax }
            if {![string match {} $ymin]} { set ygraphmin $ymin }
            if {![string match {} $ymax]} { set ygraphmax $ymax }
            if {![string match {} $y2min]} { set y2graphmin $y2min }
            if {![string match {} $y2max]} { set y2graphmax $y2max }
        } else {
            if {$xmin<=$xmax} {
                set xgraphmin $xmin ; set xgraphmax $xmax
            } else {
                set xgraphmin $xmax ; set xgraphmax $xmin
            }
            if {$ymin<=$ymax} {
                set ygraphmin $ymin ; set ygraphmax $ymax
            } else {
                set ygraphmin $ymax ; set ygraphmax $ymin
            }
            if {$y2min<=$y2max} {
                set y2graphmin $y2min ; set y2graphmax $y2max
            } else {
                set y2graphmin $y2max ; set y2graphmax $y2min
            }
        }

        # Safety adjustment
        if {$xgraphmax<=$xgraphmin} {
            set xgraphmax [expr {$xgraphmin+$eps*abs($xgraphmin)}]
            set xgraphmin [expr {$xgraphmin-$eps*abs($xgraphmin)}]
        }
        if {$ygraphmax<=$ygraphmin} {
            set ygraphmax [expr {$ygraphmin+$eps*abs($ygraphmin)}]
            set ygraphmin [expr {$ygraphmin-$eps*abs($ygraphmin)}]
        }
        if {$y2graphmax<=$y2graphmin} {
            set y2graphmax [expr {$y2graphmin+$eps*abs($y2graphmin)}]
            set y2graphmin [expr {$y2graphmin-$eps*abs($y2graphmin)}]
        }

        if {$oxmin!=$xgraphmin || $oxmax!=$xgraphmax || \
                $oymin!=$ygraphmin || $oymax!=$ygraphmax || \
                $oy2min!=$y2graphmin || $oy2max!=$y2graphmax} {
            # Mark all curve displays invalid
            $this InvalidateDisplay $idlist
            return 1
        }

        # Otherwise, no change
        return 0
    }
    method GetGraphLimits {} { \
            xgraphmin xgraphmax \
            ygraphmin ygraphmax y2graphmin y2graphmax} {
        return [list $xgraphmin $xgraphmax \
                $ygraphmin $ygraphmax $y2graphmin $y2graphmax]
    }
    method RefreshDisplay {} {
        $this DrawAxes
        $this DrawCurves 0
        $this DrawKey
    }
    method Reset {} {
        # Re-initializes most of the graph properties to initial defaults
        $this DeleteAllCurves
        $this SetGraphLimits 0. 1. 0. 1.
        set hrule {} ; set vrule {}
        set title  {}
        set xlabel {}
        set ylabel {}
        $this DrawAxes
    }
    private method PrintData { curvepat } {
        foreach curvename [array names id $curvepat] {
            set curveid $id($curvename)
            foreach i [lsort -ascii [array names curve $curveid,*]] {
                puts "$i = $curve($i)"
            }
        }
        return
    }
    method WinDestroy { w } { winpath } {
        if {[string compare $winpath $w]!=0} {
            return    ;# Child destroy event
        }
        $this Delete
    }
    Destructor {
        # Notify client of impending destruction
        catch {eval $callback $this DELETE {$delete_callback_arg}}
        Oc_EventHandler Generate $this Delete
        if {[info exists winpath] && [winfo exists $winpath]} {
            # Remove <Destroy> binding
            bind $winpath <Destroy> {}
            # Destroy instance frame, all children, and bindings
            destroy $winpath
        }
    }
}

# Adjusts $x and $y as necessary to fit into the specified bounding box.
;proc OWgwPutInBox { x y xmin ymin xmax ymax } {
    if {$x<$xmin}     { set x $xmin } \
    elseif {$x>$xmax} { set x $xmax }
    if {$y<$ymin}     { set y $ymin } \
    elseif {$y>$ymax} { set y $ymax }
    return [list $x $y]
}

# OWgwMakeDispPt converts points from data coordinates to display
# coordinates.  The imports xdatamin, ydatamin, xs, ys, xdispbase,
# and ydispbase define the transformation, via
#     xdisp = (x-xdatabase)*xs + xdispbase
#     ydisp = (y-ydatabase)*ys + ydispbase
# The return value is a boolean (0 or 1) indicating whether
# or not the point (x,y) is inside the display box.  The
# display box is defined by the imports xmin, ymin, xmax,
# ymax, which are specified in data coordinates.  The other export
# are points in display coordinates, which are appended to the
# import displistname.  These can be used to build up a polyline
# vertex list for a canvas create line call.  The imports xprev,
# yprev are the preceding vertex on the polyline, in data coordinates.
# The import boolean previnc is true (1) if (xprev,yprev) is
# inside the display box, and has therefore already been
# included on the polyline display list.  In the usual case,
# previnc is 1 and (x,y) is inside the display box, the return
# value is 1 and display list has two elements appended: xdisp ydisp.
#   If previnc is 0, then the first point pair appended to the
# displist will be the display coordinates of the projection of
# (xprev,yprev) to the display box along the line connecting
# (xprev,yprev) to (x,y).
#   Following the display coordinates for (xprev,yprev), if any,
# on the display list are display coordinates for (x,y).  If
# curinc is true, then these values are (xdisp,ydisp) as defined
# above.  If curinc is false, then these values will be the
# display coordinates of the projection of (x,y) to the display
# box along the line connecting (x,y) to (xprev,yprev).  If
# curinc is false, and if (x,y) falls in one of the "corner"
# regions of the display box, then an additional point pair
# is included in the display list, representing the display
# coordinates of the corresponding corner point.  (A point
# is in a corner region of the display box if *both* x and
# y are outside the box bounds: x<xmin && y<ymin, or
# x<xmin && y>ymax, etc.  There are 4 corner regions.)  The
# extra corner point is needed on the display list to guard
# against the following circumstance:
#
#
#                       ...b
#                 b' ...  ,
#     +-----------...---+,<--- q = corner point
#     |        ...      |,
#     |       a         ,b"
#     |                 ,
#     |                c|
#     +-----------------+
#
# When given imports a and a, the return list holds a.
# When given imports a and b, the return list holds b' and q.
# When given imports b and c, the return list holds b" and c.
# Thus the full display list is "a b' q b" c".  Assuming the
# "display box" as specified to this proc is actually a little
# bit bigger than the canvas viewport, then the corner line
# running from b' to q to b" won't be visible.  However, if
# we don't include q, then a line would be drawn between b'
# and b", which might cross into the viewport.  This possibility
# can be avoided if the margin between the viewport and the
# stated display box is big enough---the margin has to be half
# the adjoining edge dimension---but this is probably an
# uncommon enough case that it shouldn't hurt too much to
# throw in the extra point q.
#   If the entire line segment (xprev,yprev) to (x,y) is outside
# the display box, then the display list will be unchanged,
# and the return value will be 0.
#
# NOTE: For best results, the calling routine should perform a simple
# overlap test between the display box and the rectangle bounding
# (xprev,yprev) and (x,y).  If there is no overlap, then no part
# of the line segment between (xprev,yprev) and (x,y) is visible,
# in which case it isn't necessary to call this routine.  This is a very
# common occurrence with zoomed views.  The speedup can be considerable;
# >50% has been observed with big data sets.  This proc originally
# included the overlap check within itself, but now it assumes this
# has been checked beforehand.  No testing has been done on the
# correctness of this proc if the overlap condition fails.
#
# NB: Imports must satisfy xmin<xmax, ymin<ymax
#
;proc OWgwMakeDispPt { displistname \
	xdatabase ydatabase xs ys xdispbase ydispbase \
	xmin ymin xmax ymax previnc xprev yprev x y } {
    set xlo [expr {$xmin-$x}]
    set xhi [expr {$xmax-$x}]
    set ylo [expr {$ymin-$y}]
    set yhi [expr {$ymax-$y}]
    if {$xlo*$xhi>0 || $ylo*$yhi>0} {
	set curinc 0
    } else {
	set curinc 1
    }
    upvar $displistname dsplst
    if {$curinc && $previnc} {
	# The canonical case: (xprev,yprev) already in display
	# list, and (x,y) inside display box.
	lappend dsplst [expr {round(($x-$xdatabase)*$xs+$xdispbase)}]
	lappend dsplst [expr {round(($y-$ydatabase)*$ys+$ydispbase)}]
	return 1
    }
    # Otherwise, either the previous or the current point
    # is not inside the display box

    set xP [expr {double($xprev-$x)}]
    if {$xP>0} {
	set xA $xlo ; set xB $xhi
    } else {
	set xA $xhi ; set xB $xlo
    }
    set yP [expr {double($yprev-$y)}]
    if {$yP>0} {
	set yA $ylo ; set yB $yhi
    } else {
	set yA $yhi ; set yB $ylo
    }

    if {!$previnc} {
	# The (prevx,prevy) projection to display box is
	# the point (xB,?) or (?,yB) which is closest to
	# (x,y).
	if {abs($xB)<abs($xP)} {
	    set tx [expr {$xB/$xP}]
	} else { ;# Overflow protection
	    if {$xB*$xP>=0} {set tx 1} else {set tx -1}
	}
	if {abs($yB)<abs($yP)} {
	    set ty [expr {$yB/$yP}]
	} else { ;# Overflow protection
	    if {$yB*$yP>=0} {set ty 1} else {set ty -1}
	}
	if {$tx<$ty} {
	    set test [expr {$y+$yP*$tx}]
	    if {$test<$ymin || $test>$ymax} {
		return 0 ;# No intersection with display box
	    }
	    lappend dsplst \
		    [expr {round((($x+$xB)-$xdatabase)*$xs+$xdispbase)}]
	    lappend dsplst \
		    [expr {round(($test-$ydatabase)*$ys+$ydispbase)}]
	} else {
	    set test [expr {$x+$xP*$ty}]
	    if {$test<$xmin || $test>$xmax} {
		return 0 ;# No intersection with display box
	    }
	    lappend dsplst \
		    [expr {round(($test-$xdatabase)*$xs+$xdispbase)}]
	    lappend dsplst \
		    [expr {round((($y+$yB)-$ydatabase)*$ys+$ydispbase)}]
	}
    }

    if {$curinc} {
	lappend dsplst [expr {round(($x-$xdatabase)*$xs+$xdispbase)}]
	lappend dsplst [expr {round(($y-$ydatabase)*$ys+$ydispbase)}]
    } else {
	# The (x,y) projection to display box is the
	# farther of (xA,?) and (?,yA).
	# Note: If we reach here, then we know we (x,y)
	#  has a valid projection to the display box
	#  edge because either (xprev,yprev) is inside
	#  the display box, or (xprev,yprev) has a valid
	#  projection to the display box (i.e., we didn't
	#  kick out of this proc in the !$previnc stanza
	#  above.  Well, we know this modulo rounding
	#  errors...)
	if {abs($xA)<abs($xP)} {
	    set tx [expr {$xA/$xP}]
	} else { ;# Overflow protection
	    if {$xA*$xP>0} {set tx 1} else {set tx -1}
	}
	if {abs($yA)<abs($yP)} {
	    set ty [expr {$yA/$yP}]
	} else { ;# Overflow protection
	    if {$yA*$yP>0} {set ty 1} else {set ty -1}
	}
	if {$tx>$ty} {
	    lappend dsplst \
		    [expr {round((($x+$xA)-$xdatabase)*$xs+$xdispbase)}]
	    lappend dsplst \
		    [expr {round(($y+$yP*$tx-$ydatabase)*$ys+$ydispbase)}]
	} else {
	    lappend dsplst \
		    [expr {round(($x+$xP*$ty-$xdatabase)*$xs+$xdispbase)}]
	    lappend dsplst \
		    [expr {round((($y+$yA)-$ydatabase)*$ys+$ydispbase)}]
	}
	# Include corner point, if needed
	if {$xlo>0} {
	    if {$yhi<0} {
		lappend dsplst \
			[expr {round(($xmin-$xdatabase)*$xs+$xdispbase)}]
		lappend dsplst \
			[expr {round(($ymax-$ydatabase)*$ys+$ydispbase)}]
	    } elseif {$ylo>0} {
		lappend dsplst \
			[expr {round(($xmin-$xdatabase)*$xs+$xdispbase)}]
		lappend dsplst \
			[expr {round(($ymin-$ydatabase)*$ys+$ydispbase)}]
	    }
	} elseif {$xhi<0} {
	    if {$yhi<0} {
		lappend dsplst \
			[expr {round(($xmax-$xdatabase)*$xs+$xdispbase)}]
		lappend dsplst \
			[expr {round(($ymax-$ydatabase)*$ys+$ydispbase)}]
	    } elseif {$ylo>0} {
		lappend dsplst \
			[expr {round(($xmax-$xdatabase)*$xs+$xdispbase)}]
		lappend dsplst \
			[expr {round(($ymin-$ydatabase)*$ys+$ydispbase)}]
	    }
	}
    }
    return $curinc
}



# Debugging proc.  If 'expr $test' is false, then raises
# error with message "$errmsg".
;proc OWgwAssert { test errmsg } {
    if {![uplevel 1 [list expr $test]]} {
        # Pause handling of incoming network traffic
        if {[string length [package provide Net]]} {
            foreach link [Net_Link Instances] {
                $link Pause
            }
        }
        global errorInfo
        set errorInfo [Oc_StackTrace]
        Oc_Log Log \
           "Assertion failed: $test\n[uplevel 1 [list subst $errmsg]]" \
           error
        if {[string length [package provide Net]]} {
            foreach link [Net_Link Instances] {
                $link Resume
            }
        }
    }
}
