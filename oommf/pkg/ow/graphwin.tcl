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
# (x,y)->(cx,cy) is given by
#
#             cx = (x-xgraphmin)*xscale + lmargin
#    with xscale = plotwidth/(xgraphmax-xgraphmin)
# and
#            cy  = (y-ygraphmax)*yscale + tmargin
#         yscale = -plotheight/(ygraphmax-ygraphmin)
#
# Although it is possible to remove one addition by rewriting the cx formula
# as
#             cx = x*xscale + xoff
#
#  with  xoff = lmargin - xgraphmin*xscale precomputed, the numerics
# should be better with the original form.
#
# For logarithmic axes the transformations are
#             cx = (log(x/xgraphmin))*xscale + lmargin
#    with xscale = plotwidth/(log(xgraphmax/xgraphmin))
# and
#             cy = (log(y/ygraphmax))*yscale + tmargin
#    with yscale = -plotheight/(log(ygraphmax/ygraphmin))
#
# The inverse transforms are
#
#              x = (cx-lmargin)/xscale + xgraphmin
#              y = (cy-tmargin)/yscale + ygraphmax
#
# in the linear case and
#
#              x = xgraphmin*exp((cx-lmargin)/xscale)
#              y = ygraphmax*exp((cy-tmargin)/yscale)
#
# in the logarithmic case.
#
# Oc_Class Ow_GraphWin methods:
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
#    private method GetCanvasCoords { x y yaxis {round 1}}
#       Converts from user to canvas coordinates
#    private proc DisplayFormat {val digits}
#       For formatting axis limits
#    private proc RoundGently { min max logscale {pts {}} {tolerance 0.01}}
#       Tweaks axis limits
#    callback method ShowPosition { cx cy button state convert tags }
#    callback method UnshowPosition { state }
#       Routines for displaying position information.
#    callback method ZoomBoxInit { x y button shiftmod }
#    callback method ZoomBoxDraw { x2 y2 }
#    callback method ZoomBoxDo { x2 y2 }
#       Routines for mouse-based graph scaling
#    proc SameStates { a b }
#    method ResetStoreStack {}
#    method GetStoreStackSize {}
#    method StoreState {}
#    method GetStateIndex { {offset 0} }
#    method GetState { {offset 0} }
#    method RecoverState { {offset 0} }
#       Routines for handling display state stack
#    method ResetPanZoom {}
#    method UpdatePanZoomDisplay {}
#    public method Zoom { factor }
#       Rescales about center of display.
#    method Pan { xpan ypan y2pan }
#       Key based pan
#    private method DrawRules { x y }
#       Draws vertical rule at x=$x, and a horizontal rule at y=$y
#    callback method ZoomBlock { script }
#    callback method BindWrap { script }
#       Bindings to allow the dragging of the key and rules.
#    callback method DragInit { obj x y button state }
#    callback method Drag { obj x y button state }
#    method SetMargins { left right top bottom }
#       Sets minimum margin values, in pixels.
#    method SetLogAxes { xlog ylog y2log }
#       Turns logscaling on and off; updates display if limits change
#    method DrawAxes {}
#       Clears canvas, then renders title, axes labels, axes,
#       horizontal and vertical rules, and key.  Also sets
#       {t,b,l,r}margin, plotwidth/plotheight, and {x,y}scale,
#       {x,y}off values.
#    method NewCurve { curvelabel yaxis}
#    private method GetCurveId { curvename } { id }
#       Curve creation, deletion and identification routines.
#       Adds new points to data list, and marks display list
#       as out-of-date.  "newpts" is a list of the form
#       "x1 y1 x2 y2 x3 y3 ...".  Set x_i == {} to request
#       a break in the curve between points i-1 and i+1.
#       The return value is a bounding box for new data,
#       in graph coords, unless all x_i in newpts are {}, in
#       which case the return value is {}.
#    private method DeleteCurveByName { curvename }
#       Private DeleteCurve interface that uses "curvename"
#       directly, as opposed to the public DeleteCurve method
#       that constructs "curvename" from "curvelabel" and "yaxis".
#    method DeleteAllCurves {}
#    method DeleteCurve { curvelabel yaxis }
#    private method SetCurveExtrema { curveid }
#    private method CutCurve { curveid }
#    method AddDataPoints { curvelabel yaxis newpts }
#       The above 3 methods are the only public methods that
#       access individual curves. In the internal data structure
#       "curve", each curve is uniquely identified by the string
#       "$curvelabel,$yaxis".
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
#    private method SymbolSquare {clrindex tags index pts}
#    private method SymbolDiamond {clrindex tags index pts}
#    private method SymbolCircle12 {clrindex tags index pts}
#    private method SymbolTriangle {clrindex tags index pts}
#    private method SymbolInvertTriangle {clrindex tags index pts}
#    private method SymbolPlus {clrindex tags index pts}
#    private method SymbolX {clrindex tags index pts}
#    private method PlotSymbols {symbol color tags under_id offset pts}
#       Symbol drawing routines.
#    private method PlotCurve { curveid { startindex 0 }}
#       Draws specified portion (startindex to 'end') of specified
#       curve onto the canvas.
#    method AxisUse {}
#       Returns number of curves plotted against each y axis.
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
#    private proc RoundRange {min max logscale}
#       Graph limits automatic rounding; more aggressive than RoundGently
#    private method SpecifyGraphExtents {xmin xmax ymin ymax y2min y2max}
#       Internal method used by routines needing to set graph limits
#       without rounding.
#    method SetGraphLimits {{xmin {}} {xmax {}} {ymin {}} {ymax {}}
#                           {y2min {}} {y2max {}}}
#       Allows client to set graph extents.  Empty-string valued imports
#       are filled with data from GetDataExtents, rounded outward using
#       RoundRange.  The return value is 1 if the graph extents are
#       changed, 0 if the new values are the same as the old values.
#    method GetGraphLimits {}
#       Returns [list $xgraphmin $xgraphmax
#                     $ygraphmin $ygraphmax $y2graphmin $y2graphmax]
#    method GetGraphLimitsReduced {} {
#       Same as GetGraphLimits, but any axis without curves plotted
#       against it has limits set to empty strings.  This is used in
#       the display state stack to implement transparent limits.
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
# Non-class procs (used to cut down on call overhead):
#   proc OWgwPutInBox { x y xmin ymin xmax ymax } {
#     Returns a two element list [list $xnew $ynew], which shifts
#     x and y as necessary to put it into the specified box.
#
#   proc OWgwMakeLogLinDispPt {
#            dsplstname xlogscale ylogscale
#            xdatabase ydatabase xs ys
#            xdispbase ydispbase
#            xmin ymin xmax ymax
#            displeft disptop dispright dispbottom
#            prevdata x y}
#     Converts (xprev,yprev) (x,y) from data to display coords in
#     a manner suitable for appending to a canvas create line call.
#     This is a service proc for the AddDisplayPoints method.
#

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
    const public variable axescolor = black ;# axescolor is set inside
    ## method RefreshDisplay to contrast with canvas background.
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

    # Canvas background color. The mmGraph configuration dialog supports
    # two colors, white and #004020 (which is a dark green), and the
    # auto color select mode in the Ow_GraphWin constructor below
    # chooses one of those two colors. But in principle any valid Tk
    # color can be requested for default_canvas_color or canvas_color.
    # The private method DefaultCanvasColor is called to determine
    # the default color if default_canvas_color is empty or "auto".
    # Note that axescolor is set inside method RefreshDisplay to
    #  contrast with canvas_color.
    const private common light_canvas_color #FFFFFF
    const private common dark_canvas_color  #004020
    public common default_canvas_color
    public variable canvas_color {
       if {[string match {} $canvas_color] \
              || [string match auto $canvas_color]} {
          set canvas_color [Ow_GraphWin GetDefaultCanvasColor]
       }
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
    private variable key_background = white
    private variable key_boxcolor   = black

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
    private variable xscale  =  1.
    private variable yscale  = -1.
    private variable y2scale = -1.
    # For linear case, xscale = (xgraphmax-xgraphmin)/plotwidth
    # For log scaling, xscale = (log(xgraphmax)-log(xgraphmin))/plotwidth
    private variable xgraphmin  = 0. ;# Graph limits, in data coords.
    private variable xgraphmax  = 1. ;# Default *min values set at 0.1 rather
    private variable ygraphmin  = 0. ;# than 0 if *logscale is true.
    private variable ygraphmax  = 1.
    private variable y2graphmin = 0.
    private variable y2graphmax = 1.
    private variable xlogscale  = 0
    private variable ylogscale  = 0
    private variable y2logscale = 0

    # Display state stack.  Each display state is a six-tuple, xmin xmax
    # ymin ymax y2min y2max.  The stack gets reset any time
    # SetGraphLimits is called with all imports set for autoscale.
    private variable statestack = {}
    private variable stateindex = -1

    # Pan/Zoom settings
    private variable pz_base = {}
    private variable pz_panx = 0.
    private variable pz_pany = 0.  ;# y and y2 axis share pan count
    private variable pz_zoom = 1.0

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

    #private array variable color     ;# Color array
    #private variable symbol_types = 7 ;# Number of different types of symbols

    # Curve colors.  The first color in each pair is the lead
    # color, which is used to color lines and unfilled symbols.  The
    # secondary color provides good contrast to the lead color and is
    # used for coloring filled symbols.
    # 
    # The colors used with light colored background are stored in
    # colorset_light:
    #  Index  Lead color              Secondary color
    #    0    #FF0000 = red           #00FFFF = cyan
    #    1    #0000FF = blue          #FFFF00 = yellow
    #    2    #00FF00 = green         #FF00FF = magenta
    #    3    #FF00FF = magenta       #00FF00 = green
    #    4    #00868B = turquoise4    #8B0599 = purple
    #    5    #FF8C00 = darkorange    #0073FF = azure radiance
    #    6    #000000 = black         #FF69B4 = hotpink
    #    7    #00FFFF = cyan          #FF0000 = red
    #    8    #A52A2A = brown         #2AA5A5 = jungle green
    #    9    #A9A9A9 = darkgray      #907373 = opium
    #   10    #7D26CD = purple3       #77CD26 = atlantis
    private variable colorset_light = \
       {"#FF0000 #00FFFF"
        "#0000FF #FFFF00"
        "#00FF00 #FF00FF"
        "#FF00FF #00FF00"
        "#00868B #8B0599"
        "#FF8C00 #0073FF"
        "#000000 #FF69B4"
        "#00FFFF #FF0000"
        "#A52A2A #2AA5A5"
        "#A9A9A9 #907373"
        "#7D26CD #77CD26"}
    # Note: The combination of curly brackets and # characters plays
    # havoc with emacs Tcl-mode indentation.  The above combination of
    # one set of curly braces with double quotes works OK, however.
    # 
    # The colors used with dark colored background are stored in
    # colorset_dark:
    #  Index  Lead color              Secondary color
    #    0    #FF0000 = red           #00FFFF = cyan
    #    1    #0000FF = blue          #FFFF00 = yellow
    #    2    #00FF00 = green         #FF00FF = magenta
    #    3    #FF00FF = magenta       #00FF00 = green
    #    4    #FFFF00 = yellow        #8B0599 = purple
    #    5    #FF8C00 = darkorange    #0073FF = azure radiance
    #    6    #FFFFFF = white         #FF69B4 = hotpink
    #    7    #00FFFF = cyan          #FF0000 = red
    #    8    #A52A2A = brown         #2AA5A5 = jungle green
    #    9    #A9A9A9 = darkgray      #907373 = opium
    #   10    #7D26CD = purple3       #77CD26 = atlantis
    # These are mostly the same as colorset_light; tweak as desired.
    private variable colorset_dark = \
       {"#FF0000 #00FFFF"
        "#0000FF #FFFF00"
        "#00FF00 #FF00FF"
        "#FF00FF #00FF00"
        "#FFFF00 #8B0599"
        "#FF8C00 #0073FF"
        "#FFFFFF #FF69B4"
        "#00FFFF #FF0000"
        "#A52A2A #2AA5A5"
        "#A9A9A9 #907373"
        "#7D26CD #77CD26"}
    #
    # Variable colorset holds a copy of either colorset_light or
    # colorset_dark, depending on chosen background.
    private variable colorset

    # Ordered symbols list.  Each name references an instance
    # method with name Symbol${name}
    private variable symbols = {Square Diamond \
                                   Triangle InvertTriangle \
                                   Plus X Circle12}

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
    ##       curve($curveid,color)    = Drawing color index, when using
    ##                                  curve color selection.
    ##       curve($curveid,segcolor)  = Drawing color index, when using
    ##                                   segment color selection.
    ##       curve($curveid,symbol)    = Drawing symbol index for curve
    ##                                   selection.
    ##       curve($curveid,segsymbol) = Drawing symbol index for segment
    ##                                   selection.
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
    ##       curve($curveid,xabsmin)  = Minimum value of |x|>0 among data points
    ##       curve($curveid,xabsmax)  = Maximum value of |x|>0 among data points
    ##       curve($curveid,ymin)     = Minimum value of y among data points
    ##       curve($curveid,ymax)     = Maximum value of y among data points
    ##       curve($curveid,yabsmin)  = Minimum value of |y|>0 among data points
    ##       curve($curveid,yabsmax)  = Maximum value of |y|>0 among data points
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
           if {![string match {} $default_canvas_color] \
                  && ![string match auto $default_canvas_color] } {
              # User specified color
              set canvas_color $default_canvas_color
           } else {
              set canvas_color [Ow_GraphWin GetDefaultCanvasColor]
           }
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
	$graph bind showpos <<Ow_LeftButton>>  \
           "$this ShowPosition %x %y 1 %s 1 pos"
	$graph bind showpos <<Ow_RightButton>> \
           "$this ShowPosition %x %y 3 %s 1 pos"
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
        $this ResetStoreStack

        # Re-enable callbacks
        set callback $callback_keep
    }

    proc GetDefaultCanvasColor {} {
       if {![string match {} $default_canvas_color] \
              && ![string match auto $default_canvas_color] } {
          return $default_canvas_color
       }
       set background [option get . background *]
       if {[llength $background]>0} {
          # --- Auto canvas background color selection ---
          # If background is set in the Tk option database, use that to
          # pick canvas_color.
          # Note: Ow_ChangeColorScheme calls tk_setPalette, and any call
          #  to tk_setPalette sets the background entry to the Tk option
          #  database.
          if {[Ow_GetShade $background]<128} {
             set canvas_color $dark_canvas_color
          } else {
             set canvas_color $light_canvas_color
          }
       } else {
          set canvas_color $light_canvas_color ;# default
          # If on macOS/aqua and dark mode is specified, use dark canvas
          if {[string match aqua [tk windowingsystem]]} {
             update idletasks ;# In auto appearance mode, the command
             ##   ::tk::unsupported::MacWindowStyle isdark
             ## needs 'update idletasks' to work properly (Tk 8.6.10).
             if {![catch {::tk::unsupported::MacWindowStyle isdark .} _] \
                    && $_} {
                set canvas_color $dark_canvas_color
             }
          }
       }
       return $canvas_color
    }

    method GetCanvasColor {} {
       return [$graph cget -background]
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
    private method RenderKeyText { x y left_justify ylist } {
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
                     -fill [lindex $colorset $curve($curveid,color) 0] \
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
                set color_id [lindex $curve($curveid,segcolor) 0]
                if {[string match {} $color_id]} {
                   set color_id [expr {$segcount % [llength $colorset]}]
                   lappend curve($curveid,segcolor) $color_id
                   lappend curve($curveid,segsymbol) \
                      [expr {$segcount % [llength $symbols]}]
                   incr segcount
                }
                set color [lindex $colorset $color_id 0]
                set tempid [$graph create text $x $y \
                               -anchor nw \
                               -fill $color \
                               -justify left \
                               -tags {key key_text} \
                               -font $key_font \
                               -text $text]
                set bbox [$graph bbox $tempid]
                set ynew [lindex $bbox 3]
                set xtemp [lindex $bbox 2]
                for {set i 1} {$i<$segno} {incr i} {
                   set colorid [lindex $curve($curveid,segcolor) $i]
                   if {[string match {} $color_id]} {
                      set color_id [expr {$segcount % [llength $colorset]}]
                      lappend curve($curveid,segcolor) $color_id
                      lappend curve($curveid,segsymbol) \
                         [expr {$segcount % [llength $symbols]}]
                      incr segcount
                   }
                   set color [lindex $colorset $color_id 0]
                   set tempid [$graph create text $xtemp $y \
                                  -anchor nw \
                                  -fill $color \
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
                         set color_id [expr {$segcount % [llength $colorset]}]
                   lappend curve($curveid,segcolor) $color_id
                   lappend curve($curveid,segsymbol) \
                            [expr {$segcount % [llength $symbols]}]
                   incr segcount
                }
                # Write out text string, from right to left
                set xtemp $x
                for {set i [expr {$number_of_segments-1}]} \
                      {$i>0} {incr i -1} {
                   set color_id [lindex $curve($curveid,segcolor) $i]
                   set color [lindex $colorset $color_id 0]
                   set tempid [$graph create text $xtemp $y \
                                  -anchor ne \
                                  -fill $color \
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
                set color_id [lindex $curve($curveid,segcolor) 0]
                set color [lindex $colorset $color_id 0]
                set tempid [$graph create text $xtemp $y \
                               -anchor ne \
                               -fill $color \
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
                    -tags {key key_text} -width 1 -fill $key_boxcolor]
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
            $graph create rectangle $x1 $y1 $x2 $y2 -tag {key key_box} \
                    -outline {}
        } else {
            # Non-empty keybox
            $graph create rectangle $x1 $y1 $x2 $y2 -tag {key key_box} \
                    -fill $key_background -outline $key_boxcolor \
                    -width $keywidth
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
                    -tags {key} -width 1 -fill $key_boxcolor]
            set bbox [$graph bbox $tempid]
            set y [lindex $bbox 3]
        }
        $this RenderKeyText $x $y $left_justify $y2list
    }
    private method GetUserCoords { cx cy yaxis } {
          xscale yscale y2scale
          xgraphmin ygraphmax y2graphmax
          lmargin tmargin
          xlogscale ylogscale y2logscale} {
       # Given canvas coordinates cx and cy,
       # returns corresponding user coordinates ux and uy.
       if {$xscale==0.} {
          set ux $cx ;# Safety
       } elseif {!$xlogscale} {
          set ux [expr {($cx-$lmargin)/double($xscale)+$xgraphmin}]
       } else { ;# Logscale
          set ux [expr {$xgraphmin*exp(($cx-$lmargin)/double($xscale))}]
       }
       if {$yaxis!=2} {
          # Measure off y1 axis
          if {$yscale==0.} {
             set uy $cy   ;# Safety
          } elseif {!$ylogscale} {
             set uy [expr {($cy-$tmargin)/double($yscale)+$ygraphmax}]
          } else { ;# Logscale
             set uy [expr {$ygraphmax*exp(($cy-$tmargin)/double($yscale))}]
          }
       } else {
          # Measure off y2 axis
          if {$y2scale==0.} {
             set uy $cy   ;# Safety
          } elseif {!$y2logscale} {
             set uy [expr {($cy-$tmargin)/double($y2scale)+$y2graphmax}]
          } else { ;# Logscale
             set uy [expr {$y2graphmax*exp(($cy-$tmargin)/double($y2scale))}]
          }
        }
        return [list $ux $uy]
    }

    private method GetCanvasCoords { x y yaxis {round 1}} {
          xscale yscale y2scale
          xgraphmin ygraphmax y2graphmax
          lmargin tmargin
          xlogscale ylogscale y2logscale} {
       # Given user coordinates x and y, returns corresponding canvas
       # coordinates cx and cy.  If x or y is an empty string then the
       # corresponding return canvas value is set empty.
       #
       # The code here is inlined in method AddDisplayPoints (q.v.).
       if {[string match {} $x]} {
          set cx {}
       } else {
          if {!$xlogscale} {
             set cx [expr {($x-$xgraphmin)*$xscale + $lmargin}]
          } else { ;# Logscale
             set cx [expr {log($x/double($xgraphmin))*$xscale + $lmargin}]
          }
          if {$round} { set cx [expr {floor(0.5+$cx)}] }
       }
       if {[string match {} $y]} {
          set cy {}
       } else {
          if {$yaxis!=2} {
             # Measure off y1 axis
             if {!$ylogscale} {
                set cy [expr {($y-$ygraphmax)*$yscale + $tmargin}]
             } else { ;# Logscale
                set cy [expr {log($y/double($ygraphmax))*$yscale + $tmargin}]
             }
          } else {
             # Measure off y2 axis
             if {!$y2logscale} {
                set cy [expr {($y-$y2graphmax)*$y2scale + $tmargin}]
             } else { ;# Logscale
                set cy [expr {log($y/double($y2graphmax))*$y2scale + $tmargin}]
             }
          }
          if {$round} { set cy [expr {floor(0.5+$cy)}] }
       }
       return [list $cx $cy]
    }

    private proc DisplayFormat {val digits} {
       set precision [expr {$digits-1}]
       if {$val == 0.0} { # Zero special case
          set dval 0
       } elseif {abs($val)<1e-5 || 100000<=abs($val)} { # Use exponential format
          set dval [format "%.${precision}e" $val]
          # Strip trailing zeros and decimal point in mantissa
          if {$precision>0} { ;# Require decimal point
             set dval [regsub {0*e} $dval e]
             set dval [regsub {\.e} $dval e]
          }
       } else { # Use %f formatting
          set logval [expr {int(floor(log10(abs($val))))}]
          set decdig [expr {$precision-$logval}]
          if {$decdig>=0} { # Digits to right of decimal point
             set dval [format "%.${decdig}f" $val]
          } else { # All digits to left of decimal point; drop extra precision
             set dval [format %.0f [format "%.${precision}e" $val]]
          }
          # Strip trailing zeros and decimal point
          if {$decdig>0} {  ;# Require decimal point
             set dval [string trimright $dval "0"]
             set dval [string trimright $dval .]
          }
       }
       return $dval
    }

    private proc RoundGently { min max logscale {pts {}} {tolerance 0.01}} {
       # If pts is empty, then return is a two item list containing
       # rounded values for min and max.  Otherwise the return is a list
       # with the same length of pts containing rounded values for each
       # element of pts.
       #
       # Default tolerance 0.01 == 1%, meaning +/-0.5% for
       # linear scales, tol_adj = 2 times that for log scales.
       if {[llength $pts]==0} {
          set pts [list $min $max]
       }
       if {!$logscale} {
          set delta [expr {0.5*($max-$min)*$tolerance}]
          if {$delta <= 0.0} {
             return $pts  ;# Safety
          }
          set slack [expr {pow(10,floor(log10($delta)))}]
          set limits {}
          foreach val $pts {
             set tmp [expr {abs($val/$slack)}]
             if {$tmp < 1.0} {
                set digs 1  ;# Safety
             } else {
                set digs [expr {1+int(floor(log10($tmp)))}]
             }
             lappend limits [Ow_GraphWin DisplayFormat $val $digs]
          }
       } else { ;# logscale.  Work with ratios rather than differences
          if {$min<=0} { return $pts } ;# Safety
          set tol_adj 2.0 ;# Tolerance adjustment for log scale (empirical)
          set wrktol [expr {0.5*$tol_adj*$tolerance}]
          set delta [expr {pow($max/double($min),$wrktol)-1}]
          if {$delta<=0.0} { return [list $min $max] } ;# Safety
          set limits {}
          foreach val $pts {
             set slack [expr {pow(10,floor(log10($val*$delta)))}]
             set tmp [expr {abs($val/$slack)}]
             if {$tmp < 1.0} {
                set digs 1  ;# Safety
             } else {
                set digs [expr {1+int(floor(log10($tmp)))}]
             }
             lappend limits [Ow_GraphWin DisplayFormat $val $digs]
          }
       }
       return $limits
    }

    callback method ShowPosition { cx cy button state convert tags } { \
	    graph canvas_color axescolor \
            xgraphmin xgraphmax ygraphmin ygraphmax \
            y2graphmin y2graphmax xlogscale ylogscale y2logscale \
            lmargin plotwidth bindblock ShiftMask } {

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
        # of point.  Use canvas coords so both linear and
        # logarithmic scales can be treated uniformly.
        set cxmid [expr {$lmargin + 0.5*$plotwidth}]
        if {$cx>=$cxmid} {
            set textanchor se
            set cxoff -3
        } else {
            set textanchor sw
            set cxoff 3
        }
        set cyoff -1
        # Format text for display
        set fx [Ow_GraphWin RoundGently $xgraphmin $xgraphmax $xlogscale $ux]
        if {$yaxis == 1} {
           set fy \
              [Ow_GraphWin RoundGently $ygraphmin $ygraphmax $ylogscale $uy]
        } else {
           set fy \
              [Ow_GraphWin RoundGently $y2graphmin $y2graphmax $y2logscale $uy]
        }
        # Do display
        set textid [$graph create text \
                [expr {$cx+$cxoff}] [expr {$cy+$cyoff}] \
                -text "($fx,$fy)" -fill $axescolor \
                -anchor $textanchor -tags $tags]
        set bbox [$graph bbox $textid]
        eval "$graph create rectangle $bbox -fill $canvas_color -outline {} \
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

    callback method ZoomBoxDo { sx2 sy2 } {
       # Imports are screen coordinates
       if {$zoombox_yaxis==0} return ;# No zoom in progress
       $graph delete zoombox

       # Convert from screen coords to canvas coords
       set xa [lindex $zoombox_basept 0]
       set ya [lindex $zoombox_basept 1]
       set xb [$graph canvasx $sx2]
       set yb [$graph canvasy $sy2]
       set zw [expr {abs($xa-$xb)}]
       set zh [expr {abs($ya-$yb)}]
       if {$zw<$zoombox_minsize || $zh<$zoombox_minsize} {
          set zoombox_yaxis 0   ;# Don't zoom
          return
       }

       # Convert from canvas to user coordinates, and round
       # to nice numbers.
       foreach {uxa uya} [$this GetUserCoords $xa $ya $zoombox_yaxis] { break }
       foreach {uxb uyb} [$this GetUserCoords $xb $yb $zoombox_yaxis] { break }

       if {$uxa > $uxb} {
          set t $uxa ; set uxa $uxb ; set uxb $t
       }
       if {$uya > $uyb} {
          set t $uya ; set uya $uyb ; set uyb $t
       }

       foreach {rxa rxb} \
          [Ow_GraphWin RoundGently $uxa $uxb $xlogscale] { break }
       if {$zoombox_yaxis == 1} {
          foreach {ry1a ry1b} \
             [Ow_GraphWin RoundGently $uya $uyb $ylogscale] { break }
          set ry2a $y2graphmin
          set ry2b $y2graphmax
       } else {
          foreach {ry2a ry2b} \
             [Ow_GraphWin RoundGently $uya $uyb $y2logscale] { break }
          set ry1a $ygraphmin
          set ry1b $ygraphmax
       }

       # Update display
       $this StoreState
       if {[$this SpecifyGraphExtents $rxa $rxb \
               $ry1a $ry1b $ry2a $ry2b]} {
          $this RefreshDisplay
          $this StoreState
       }

       # Turn off zoombox-in-progress indicator
       set zoombox_yaxis 0
    }

    #########################################################
    # Display state (graph limit) store stack ###############

    # Proc for comparing two display states, where a display state is a
    # 6-tuple of xmin, xmax, ymin, ymax, y2min, y2max.  Returns 1 if
    # states are the same, otherwise 0.  Empty strings are treated as
    # wildcards that match anything.
    proc SameStates { a b } {
       if {[llength $a]==0 || [llength $b]==0} {
          return 1  ;# One state or other is fully empty.
       }
       foreach x $a y $b {
          if {![string match {} $x] && ![string match {} $y] \
                 && $x != $y} {
             return 0 ;# States differ
          }
       }
       return 1
    }
    method ResetStoreStack {} {
       set stateindex 0
       set statestack {}
       lappend statestack [$this GetGraphLimitsReduced]
    }
    method GetStoreStackSize {} {
       set size [llength $statestack]
       if {$size == 1 && [string match {} [lindex $statestack 0]]} {
          set size 0  ;# Dummy first element
       }
       return $size
    }
    method StoreState {} {
       # If current display state is different from state at current
       # stateindex, then increment stateindex, store the current
       # display state at the new stateindex, and truncate any states on
       # statestack beyond the new stateindex.
       #  Do nothing if current display state matches the state at the
       # current statindex.
       #  If an axis has no curves currently plotted against it, then
       # store empty strings as the limits for that axis.  In the
       # GetState method empty strings are replaced with the then
       # current axis limits.
       set displaystate [$this GetGraphLimitsReduced]
       set refstate [lindex $statestack $stateindex]
       if {[llength $refstate]==0} {
          # Empty state on stack; replace it with current state but
          # don't increment stateindex.
       } else {
          if {[Ow_GraphWin SameStates $displaystate $refstate]} {
             return  ;# Do nothing
          }
          incr stateindex
       }
       if {[llength $statestack]<=$stateindex} {
          # Note: lreplace (other branch) requires $stateindex be a
          # valid index.
          set stateindex [llength $statestack]
          lappend statestack $displaystate
       } else {
          set statestack [lreplace $statestack $stateindex end $displaystate]
       }
       $this ResetPanZoom
    }
    method GetStateIndex { {offset 0} } {
       # If offset is zero then returns stateindex.
       #
       # If offset is positive the return stateindex+offset.
       #
       # If offset is negative and display matches the state at
       # stateindex, then return stateindex+offset.  If offset is
       # negative and the display does not match the state at
       # stateindex, then return stateindex+offset+1.  In particular, if
       # the display state does not match the stateindex state then
       # offset=0 and offset=-1 both return stateindex.
       set index $stateindex
       if {$offset < 0} {
          set displaystate [$this GetGraphLimits]
          set refstate [lindex $statestack $stateindex]
          if {![Ow_GraphWin SameStates $displaystate $refstate]} {
             incr index  ;# States differ
          }
       }
       incr index $offset
       if {$index<0} {set index 0}
       if {$index >= [llength $statestack]} {
          set index [expr {[llength $statestack]-1}]
       }
       return $index
    }
    method GetState { {offset 0} } {
       # Returns the state at requested relative offset in the display
       # state stack.  Empty (transparent) limits are replaced with the
       # current display limits.
       set stateindex [$this GetStateIndex $offset]
       set refstate [lindex $statestack $stateindex]
       set displaystate [$this GetGraphLimits]
       set rtnstate {}
       foreach a $refstate b $displaystate {
          if {![string match {} $a]} {
             lappend rtnstate $a
          } else {
             lappend rtnstate $b
          }
       }
       return $rtnstate
    }
    method RecoverState { {offset 0} } {
       $this SpecifyGraphExtents {*}[$this GetState $offset]
       $this RefreshDisplay
       $this ResetPanZoom
    }
    ############### Display state (graph limit) store stack #
    #########################################################

    # Makes current display base configuration for PanZoom control
    method ResetPanZoom {} {
       set pz_panx 0.
       set pz_pany 0.
       set pz_zoom 1.0
    }

    # Realize current PanZoom settings on the display
    method UpdatePanZoomDisplay {} {
       set pz_base [$this GetState 0]
       foreach {xmin xmax ymin ymax y2min y2max} $pz_base { break }
       if {!$xlogscale} {
          set xwidth [expr {$xmax-$xmin}]
          set xmid [expr {0.5*($xmin+$xmax)}]
          set nxmid [expr {$xmid+$xwidth*$pz_panx}]
          set nxhalfwidth [expr {0.5*$xwidth/$pz_zoom}]
          set nxmin [expr {$nxmid - $nxhalfwidth}]
          set nxmax [expr {$nxmid + $nxhalfwidth}]
       } else {
          set xrat [expr {double($xmax)/$xmin}]
          set xmid [expr {sqrt($xmin*$xmax)}]
          set nxmid [expr {$xmid*pow($xrat,$pz_panx)}]
          set nxhalfrat [expr {pow($xrat,0.5/$pz_zoom)}]
          set nxmin [expr {$nxmid/$nxhalfrat}]
          set nxmax [expr {$nxmid*$nxhalfrat}]
       }
       if {!$ylogscale} {
          set ywidth [expr {$ymax-$ymin}]
          set ymid [expr {0.5*($ymin+$ymax)}]
          set nymid [expr {$ymid+$ywidth*$pz_pany}]
          set nyhalfwidth [expr {0.5*$ywidth/$pz_zoom}]
          set nymin [expr {$nymid - $nyhalfwidth}]
          set nymax [expr {$nymid + $nyhalfwidth}]
       } else {
          set yrat [expr {double($ymax)/$ymin}]
          set ymid [expr {sqrt($ymin*$ymax)}]
          set nymid [expr {$ymid*pow($yrat,$pz_pany)}]
          set nyhalfrat [expr {pow($yrat,0.5/$pz_zoom)}]
          set nymin [expr {$nymid/$nyhalfrat}]
          set nymax [expr {$nymid*$nyhalfrat}]
       }
       if {!$y2logscale} {
          set y2width [expr {$y2max-$y2min}]
          set y2mid [expr {0.5*($y2min+$y2max)}]
          set ny2mid [expr {$y2mid+$y2width*$pz_pany}]
          set ny2halfwidth [expr {0.5*$y2width/$pz_zoom}]
          set ny2min [expr {$ny2mid - $ny2halfwidth}]
          set ny2max [expr {$ny2mid + $ny2halfwidth}]
       } else {
          set y2rat [expr {double($y2max)/$y2min}]
          set y2mid [expr {sqrt($y2min*$y2max)}]
          set ny2mid [expr {$y2mid*pow($y2rat,$pz_pany)}]
          set ny2halfrat [expr {pow($y2rat,0.5/$pz_zoom)}]
          set ny2min [expr {$ny2mid/$ny2halfrat}]
          set ny2max [expr {$ny2mid*$ny2halfrat}]
       }

       # Round limits
       foreach {nxmin nxmax} \
          [Ow_GraphWin RoundGently $nxmin $nxmax $xlogscale] { break }
       foreach {nymin nymax} \
          [Ow_GraphWin RoundGently $nymin $nymax $ylogscale] { break }
       foreach {ny2min ny2max} \
          [Ow_GraphWin RoundGently $ny2min $ny2max $y2logscale] { break }

       # Update display
       if {[$this SpecifyGraphExtents $nxmin $nxmax \
               $nymin $nymax $ny2min $ny2max]} {
          $this RefreshDisplay
       }
    }

    method Zoom { factor } {
       set pz_zoom [expr {$pz_zoom*$factor}]
       $this UpdatePanZoomDisplay
    }

    method Pan { xpan ypan y2pan } {
       # Independent y2 pan not supported.
       set pz_panx [expr {$pz_panx+$xpan/$pz_zoom}]
       set pz_pany [expr {$pz_pany+$ypan/$pz_zoom}]
       $this UpdatePanZoomDisplay
    }

    private method DrawRules { x y } {
        # Note: Imports x and y are in graph (not canvas) coordinates.
        #  If $x=={}, then the vertical rule is drawn on the leftmost
        # edge of the graph.  If $y=={}, then the horizontal rule is
        # drawn on the bottom-most edge of the graph.
        set vrulepos $x
        set hrulepos $y
        foreach {cxmin cymin cxmax cymax} \
                [list $lmargin $tmargin \
                  [expr {$lmargin+$plotwidth}] \
                  [expr {$tmargin+$plotheight}]] {}
        foreach {cx cy} [$this GetCanvasCoords $x $y 1] break
        if {[string match {} $x] || $cx<$cxmin} {set cx $cxmin}
        if {$cx>$cxmax} {set cx $cxmax}
        if {[string match {} $y] || $cy>$cymax} {set cy $cymax}
        if {$cy<$cymin} {set cy $cymin}
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
    method SetLogAxes { xlog ylog y2log } {
       set xlogscale  $xlog
       set ylogscale  $ylog
       set y2logscale $y2log
       # Check that graph limits are still valid and adjust as
       # necessary.
       if {[$this SpecifyGraphExtents {*}[$this GetGraphLimits]]} {
          $this RefreshDisplay
       }
    }
    method DrawAxes {} {
        # Clears canvas, then renders axes, title, axes labels, tics,
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
            $graph create text 0 0 -text $title \
               -font $title_font -fill $axescolor -tags title_test
            set bbox [$graph bbox title_test]
            set title_height [expr {1.5*([lindex $bbox 3]-[lindex $bbox 1])}]
            $graph delete title_test
        } else {
            set title_height 0
        }

        # Calculate lower bound for margin size from axes coordinate text
        $graph delete margin_test   ;# Safety
        $graph create text 0 0 -text $xgraphmin \
                -font $label_font -fill $axescolor \
                -anchor sw -tags margin_test
        $graph create text 0 0 -text $xgraphmax \
                -font $label_font -fill $axescolor \
                -anchor sw -tags margin_test
        set bbox [$graph bbox margin_test]
        set xtext_height [expr {([lindex $bbox 3]-[lindex $bbox 1])*1.2+1}]
        set xtext_halfwidth [expr {([lindex $bbox 2]-[lindex $bbox 0])*0.6+1}]
        ## Include a little bit extra
        $graph delete margin_test

        if {$y1ccnt>0} {
            $graph create text 0 0 -text $ygraphmin    \
                    -font $label_font -fill $axescolor \
                    -anchor sw -tags margin_test
            $graph create text 0 0 -text $ygraphmax    \
                    -font $label_font -fill $axescolor \
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
            $graph create text 0 0 -text $y2graphmin   \
                    -font $label_font -fill $axescolor \
                    -anchor sw -tags margin_test
            $graph create text 0 0 -text $y2graphmax   \
                    -font $label_font -fill $axescolor \
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
        if {!$xlogscale} {
           set xrange [expr {double($xgraphmax-$xgraphmin)}]   ;# Temporary
        } else {
           set xrange [expr {double(log($xgraphmax)-log($xgraphmin))}]
        }
        if {$xrange==0.} {set xrange  1.}
        set xscale [expr {double($plotwidth)/$xrange}]
        if {$xscale==0.} {set xscale 1.}

        if {!$ylogscale} {
           set yrange [expr {double($ygraphmax-$ygraphmin)}]   ;# Temporary
        } else {
           set yrange [expr {double(log($ygraphmax)-log($ygraphmin))}]
        }
        if {$yrange==0.} {set yrange  1.}
        set yscale [expr {double(-$plotheight)/$yrange}]
        if {$yscale==0.} {set yscale -1.}

        if {!$y2logscale} {
           set y2range [expr {double($y2graphmax-$y2graphmin)}] ;# Temporary
        } else {
           set y2range [expr {double(log($y2graphmax)-log($y2graphmin))}]
        }
        if {$y2range==0.} {set y2range  1.}
        set y2scale [expr {double(-$plotheight)/$y2range}]
        if {$y2scale==0.} {set y2scale -1.}

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
        set matte_color [$graph cget -background]
        set matteid [$graph create rectangle 0 0 \
                     [expr {2*$canwidth}] [expr {$ydispmin-$axeswidth/2.}] \
                     -outline {} -fill $matte_color -tags axes]
        $graph create rectangle 0 0 \
                [expr {$xdispmin-$axeswidth/2.}] $canheight \
                -outline {} -fill $matte_color -tags axes
        $graph create rectangle [expr {$xdispmax+$axeswidth/2.}] 0 \
                [expr {2*$canwidth}] $canheight \
                -outline {} -fill $matte_color -tags axes
        $graph create rectangle 0 [expr {$ydispmax+$axeswidth/2.}] \
                [expr {2*$canwidth}] [expr {2*$canheight}] \
                -outline {} -fill $matte_color -tags axes

        $graph create text $xdispmin [expr {$ydispmax+$bmargin*0.1}] \
                -font $label_font -fill $axescolor \
                -text $xgraphmin -anchor n -tags axes
        $graph create text $xdispmax [expr {$ydispmax+$bmargin*0.1}] \
                -font $label_font -fill $axescolor \
                -text $xgraphmax -anchor n -tags axes
        if {$y1ccnt>0} {
            $graph create text [expr {$xdispmin-$lmargin*0.1}] $ydispmax \
                    -font $label_font -fill $axescolor \
                    -text $ygraphmin -anchor se -tags axes
            $graph create text [expr {$xdispmin-$lmargin*0.1}] $ydispmin \
                    -font $label_font -fill $axescolor \
                    -text $ygraphmax -anchor ne -tags axes
        }
        if {$y2ccnt>0} {
            $graph create text [expr {$xdispmax+$rmargin*0.1}] $ydispmax \
                    -font $label_font -fill $axescolor \
                    -text $y2graphmin -anchor sw -tags axes
            $graph create text [expr {$xdispmax+$rmargin*0.1}] $ydispmin \
                    -font $label_font -fill $axescolor \
                    -text $y2graphmax -anchor nw -tags axes
        }

        # Render title
        if {![string match {} $title]} {
            $graph create text \
                    [expr {($xdispmin+$xdispmax)/2.}] \
                    [expr {$tmargin/2.}] \
                    -text $title -font $title_font -fill $axescolor \
                    -justify center -tags axes
            ## Use [expr {$canwidth/2.}] for x to get centering
            ## with respect to page instead of wrt graph.
        }

        # Place labels
        $graph create text \
                [expr {($xdispmin+$xdispmax)/2.}] \
                [expr {$canheight-$bmargin*0.5}]  \
                -font $label_font -fill $axescolor -justify center \
                -text $xlabel -tags axes
        if {$y1ccnt>0} {
            $graph create text \
                    [expr {$lmargin*0.5}]             \
                    [expr {($ydispmin+$ydispmax)/2.}] \
                    -font $label_font -fill $axescolor -justify center \
                    -text $ylabel -tags axes
        }
        if {$y2ccnt>0} {
            $graph create text \
                    [expr {$canwidth - $rmargin*0.5}] \
                    [expr {($ydispmin+$ydispmax)/2.}] \
                    -font $label_font -fill $axescolor -justify center \
                    -text $y2label -tags axes
        }

        # Tics
        set ticlength {}
        foreach len [list 1.4 2.0 1.4] {
           lappend ticlength [expr {$len*$axeswidth}]
        }
        set linoff {}
        set logoff {}
        foreach off [list 0.25 0.50 0.75] {
           lappend linoff $off
           lappend logoff [expr {log10(10*$off)}]
        }

        # X-axes tics
        if {!$xlogscale} {
           set offsets $linoff
        } else {
           set offsets $logoff
        }
        foreach off $offsets tlen $ticlength {
           set xt [expr {$xdispmin + $off*($xdispmax-$xdispmin)}]
           $graph create line $xt $ydispmax $xt [expr {$ydispmax - $tlen}] \
              -fill $axescolor -width $axeswidth -tags {axes showpos}
           $graph create line $xt $ydispmin $xt [expr {$ydispmin + $tlen}] \
              -fill $axescolor -width $axeswidth -tags {axes showpos}
        }

        # Y1-axis tics
        if {$y1ccnt>0} {
           if {!$ylogscale} {
              set offsets $linoff
           } else {
              set offsets $logoff
           }
           foreach off $offsets tlen $ticlength {
              set yt [expr {$ydispmax - $off*($ydispmax-$ydispmin)}]
              $graph create line \
                 $xdispmin $yt [expr {$xdispmin + $tlen}] $yt \
                 -fill $axescolor -width $axeswidth -tags {axes showpos}
              if {$y2ccnt==0} {
                 $graph create line \
                    $xdispmax $yt [expr {$xdispmax - $tlen}] $yt \
                    -fill $axescolor -width $axeswidth -tags {axes showpos}
              }
           }
        }

        # Y2-axis tics
        if {$y2ccnt>0} {
           if {!$y2logscale} {
              set offsets $linoff
           } else {
              set offsets $logoff
           }
           foreach off $offsets tlen $ticlength {
              set yt [expr {$ydispmax - $off*($ydispmax-$ydispmin)}]
              if {$y1ccnt==0} {
                 $graph create line \
                    $xdispmin $yt [expr {$xdispmin + $tlen}] $yt \
                    -fill $axescolor -width $axeswidth -tags {axes showpos}
              }
              $graph create line \
                 $xdispmax $yt [expr {$xdispmax - $tlen}] $yt \
                 -fill $axescolor -width $axeswidth -tags {axes showpos}
           }
        }

        # Draw rules
        $this DrawRules $vrulepos $hrulepos

        # Draw key
        $this DrawKey $key_pos
    }

    method NewCurve { curvelabel yaxis } {
        set curvename "$curvelabel,$yaxis"
        if {[info exists id($curvename)]} {
            error "Curve $curvename already exists"
        }

        lappend idlist $curvecount
        set curveid $curvecount
        set colorid  [expr {$curvecount % [llength $colorset]}]
        set symbolid [expr {$curvecount % [llength $symbols]}]
        incr curvecount
        set segcolor {} ;# Segcolors are assigned in lazy fashion,
                          ## at time of initial rendering.
        set segsymbol {} ;# Ditto

        set id($curvename) $curveid
        array set curve [list              \
                $curveid,name  $curvename  \
                $curveid,label $curvelabel \
                $curveid,color $colorid \
                $curveid,segcolor  {} \
                $curveid,symbol $symbolid \
                $curveid,segsymbol  {} \
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
                $curveid,xmax        {}      \
                $curveid,xabsmin     {}      \
                $curveid,xabsmax     {}      \
                $curveid,ymin        {}      \
                $curveid,ymax        {}      \
                $curveid,yabsmin     {}      \
                $curveid,yabsmax     {}       ]

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
    private method SetCurveExtrema { curveid } { curve } {
       set xmin [set ymin "z"]
       set xmax [set ymax {}]
       set xabsmin [set yabsmin "z"]
       foreach { x y } $curve($curveid,pts) {
          if {$x<$xmin} {set xmin $x}
          if {$x>$xmax} {set xmax $x}
          if {abs($x)<$xabsmin && $x != 0.0} {
             set xabsmin [expr {abs($x)}]
          }
          if {$y<$ymin} {set ymin $y}
          if {$y>$ymax} {set ymax $y}
          if {abs($y)<$yabsmin && $y != 0.0} {
             set yabsmin [expr {abs($y)}]
          }
       }
       if {[string match {} $xmax]} {
          # No non-empty points entered
          set xmin [set ymin {}]
          set xabsmin [set xabsmax {}]
          set yabsmin [set yabsmax {}]
       } else { # Figure out x/yabsmax
          set xabsmax [expr {abs($xmax)}]
          if {$xabsmax<abs($xmin)} { set xabsmax [expr {abs($xmin)}] }
          if {$xabsmax == 0} { set xabsmin [set xabsmax {}] }
          set yabsmax [expr {abs($ymax)}]
          if {$yabsmax<abs($ymin)} { set yabsmax [expr {abs($ymin)}] }
          if {$yabsmax == 0} { set yabsmin [set yabsmax {}] }
       }
       set curve($curveid,xmin) $xmin
       set curve($curveid,xmax) $xmax
       set curve($curveid,xabsmin) $xabsmin
       set curve($curveid,xabsmax) $xabsmax
       set curve($curveid,ymin) $ymin
       set curve($curveid,ymax) $ymax
       set curve($curveid,yabsmin) $yabsmin
       set curve($curveid,yabsmax) $yabsmax
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

       $this SetCurveExtrema $curveid

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
    method AddDataPoints { curvelabel yaxis newpts } {
       id curve ptlimit xlogscale ylogscale y2logscale } {
        # Appends points to specified curve.  Returns 1 if x or y axis
        # data range is extended.  Note that appending may be restricted
        # if ptlimit is reached.
        # Also sets curve($curveid,dispcurrent) to 0 (false).
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
        set xabsmin [set yabsmin "z"]
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
                if {abs($x)<$xabsmin && $x != 0.0} {
                   set xabsmin [expr {abs($x)}]
                }
                if {$y<$ymin} {set ymin $y}
                if {$y>$ymax} {set ymax $y}
                if {abs($y)<$yabsmin && $y != 0.0} {
                   set yabsmin [expr {abs($y)}]
                }
                lappend curve($curveid,pts) $x $y
                incr lastindex 2
            }
        }

        # Finish up {x,y}abs{min,max} processing
        if {[string match {} $xmax]} {
            # No non-empty points entered
           set xabsmin [set xabsmax {}]
           set yabsmin [set yabsmax {}]
        } else { # Figure out x/yabsmax
           set xabsmax [expr {abs($xmax)}]
           if {$xabsmax<abs($xmin)} { set xabsmax [expr {abs($xmin)}] }
           if {$xabsmax == 0} { set xabsmin [set xabsmax {}] }
           set yabsmax [expr {abs($ymax)}]
           if {$yabsmax<abs($ymin)} { set yabsmax [expr {abs($ymin)}] }
           if {$yabsmax == 0} { set yabsmin [set yabsmax {}] }
        }

        # Adjust range extents
        set xrangechange 0
        set yrangechange 0
        set xabsrangechange 0
        set yabsrangechange 0
        set curve($curveid,lastpt) [list $lastindex $x $y]
        if {[string match {} $xmax]} {
            # No non-empty points entered
            set xmin [set ymin {}]
        } elseif {[string match {} $curve($curveid,xmin)]} {
            set curve($curveid,xmin) $xmin
            set curve($curveid,xmax) $xmax
            set curve($curveid,xabsmin) $xabsmin
            set curve($curveid,xabsmax) $xabsmax
            set curve($curveid,ymin) $ymin
            set curve($curveid,ymax) $ymax
            set curve($curveid,yabsmin) $yabsmin
            set curve($curveid,yabsmax) $yabsmax
            set xrangechange 1
            set yrangechange 1
            set xabsrangechange 1
            set yabsrangechange 1
        } else {
            if {$xmin<$curve($curveid,xmin)} {
                set curve($curveid,xmin) $xmin
                set xrangechange 1
            }
            if {$xmax>$curve($curveid,xmax)} {
                set curve($curveid,xmax) $xmax
                set xrangechange 1
            }
            if {$ymin<$curve($curveid,ymin)} {
                set curve($curveid,ymin) $ymin
                set yrangechange 1
            }
            if {$ymax>$curve($curveid,ymax)} {
                set curve($curveid,ymax) $ymax
                set yrangechange 1
            }
        }
        if {![string match {} $xabsmax]} { # Non-zero values entered
           if {[string match {} $curve($curveid,xabsmax)]} {
              set curve($curveid,xabsmin) $xabsmin
              set curve($curveid,xabsmax) $xabsmax
              set xabsrangechange 1
           } else {
              if {$xabsmin<$curve($curveid,xabsmin)} {
                 set curve($curveid,xabsmin) $xabsmin
                 set xabsrangechange 1
              }
              if {$xabsmax>$curve($curveid,xabsmax)} {
                 set curve($curveid,xabsmax) $xabsmax
                 set xabsrangechange 1
              }
           }
        }
        if {![string match {} $yabsmax]} { # Non-zero values entered
           if {[string match {} $curve($curveid,yabsmax)]} {
              set curve($curveid,yabsmin) $yabsmin
              set curve($curveid,yabsmax) $yabsmax
              set yabsrangechange 1
           } else {
              if {$yabsmin<$curve($curveid,yabsmin)} {
                 set curve($curveid,yabsmin) $yabsmin
                 set yabsrangechange 1
              }
              if {$yabsmax>$curve($curveid,yabsmax)} {
                 set curve($curveid,yabsmax) $yabsmax
                 set yabsrangechange 1
              }
           }
        }
        set rangechange 0
        if {(!$xlogscale && $xrangechange) \
               || ($xlogscale && $xabsrangechange)} {
           set rangechange 1
        }
        if {$yaxis != 2} {
           if {(!$ylogscale && $yrangechange) \
                  || ($ylogscale && $yabsrangechange)} {
              set rangechange 1
           }
        } else {
           if {(!$y2logscale && $yrangechange) \
                  || ($y2logscale && $yabsrangechange)} {
              set rangechange 1
           }
        }

        set curve($curveid,dispcurrent) 0
        if {$ptlimit>0 && $lastindex+2>[expr {2*$ptlimit}]} {
            $this CutCurve $curveid
        }

        return $rangechange
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
                    $curveid,xmax     {}         \
                    $curveid,xabsmin  {}         \
                    $curveid,xabsmax  {}         \
                    $curveid,ymin     {}         \
                    $curveid,ymax     {}         \
                    $curveid,yabsmin  {}         \
                    $curveid,yabsmax  {}          ]
            $graph delete tag$curveid
        }
        $this DrawKey
    }
    private method AddDisplayPoints { cid datapts } {
       boundary_clip_size curve xscale yscale y2scale
       xlogscale ylogscale y2logscale lmargin tmargin
       plotwidth plotheight xgraphmin xgraphmax
       ygraphmin ygraphmax y2graphmin y2graphmax} {
       # Converts datapts to display coordinates, and appends to
       # end of last display segment.
       #   To start a new segment, calling routines should lappend an empty
       # list to $curve($cid,ptsdisp) before calling this routine.
       #   The return value is a value suitable for passing to PlotCurve
       # for drawing only new points.  Special values: -2 => no
       # new points were added, -3 => last list elt of $curve($cid,ptsdisp)
       # removed.
       #
       # NOTE: Although the canvas widget uses floating point coordinates,
       #  the canvas here is setup with pixels corresponding to integral
       #  coordinates.  So we round to nearest integer as part of this
       #  processing.

       # Determine y-axis scaling
       if { $curve($cid,yaxis) != 2 } {
          set ys $yscale
          set yls $ylogscale
          set ygmin $ygraphmin
          set ygmax $ygraphmax
       } else {
          set ys $y2scale
          set yls $y2logscale
          set ygmin $y2graphmin
          set ygmax $y2graphmax
       }

       # The draw bounding box is canvas graph extents expanded by
       # boundary_clip_size.  Invert the data-to-canvas transform to
       # find the matching values in data coords. Add half a pixel fudge
       # to allow for floating point to integer rounding.
       set displeft [expr {$lmargin-$boundary_clip_size}]
       set dispright [expr {$lmargin+$plotwidth+$boundary_clip_size}]
       set dispbottom [expr {$tmargin+$plotheight+$boundary_clip_size}]
       set disptop [expr {$tmargin-$boundary_clip_size}]
       foreach {xminbound yminbound} \
          [$this GetUserCoords [expr {$displeft-0.5}] [expr {$dispbottom+0.5}] \
              $curve($cid,yaxis)] { break }
       foreach {xmaxbound ymaxbound} \
          [$this GetUserCoords [expr {$dispright+0.5}] [expr {$disptop-0.5}] \
              $curve($cid,yaxis)] { break }
       set displist [lindex $curve($cid,ptsdisp) end]
       set startindex [llength $displist]
       set datapts_start 0
       if {[llength $curve($cid,lastdpt)]==3} {
          foreach { xprev yprev previnc } $curve($cid,lastdpt) { break }
          set cprev [$this GetCanvasCoords $xprev $yprev $curve($cid,yaxis)]
          if {$startindex>0} {
             if {$previnc} {incr startindex -2}
          } else {
             # No points drawn yet
             if {$xminbound<$xprev && $xprev<$xmaxbound \
                    && $yminbound<$yprev && $yprev<$ymaxbound} {
                lappend displist {*}$cprev
                set previnc 1
             } else {
                set previnc 0
             }
          }
       } else {
          # Fresh segment
          set xprev [lindex $datapts 0]
          if {$xlogscale} { set xprev [expr {abs($xprev)}] }
          set yprev [lindex $datapts 1]
          if {$yls} { set yprev [expr {abs($yprev)}] }
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
             set datapts_start 2
             if {$xminbound<$xprev && $xprev<$xmaxbound \
                    && $yminbound<$yprev && $yprev<$ymaxbound} {
                set cprev [$this GetCanvasCoords $xprev $yprev \
                              $curve($cid,yaxis)]
                lappend displist {*}$cprev
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

       # Add all points that will display at distinct screen locations.
       # For linear axes we just check that the difference between
       # successive points is bigger than 1/axis_scaling.  For log
       # scaling the corresponding check is that the ratio of successive
       # points lies between 10^(-1/axis_scaling) and
       # 10^(1/axis_scaling).
       set xdiff 1.  ; set ydiff 1.   ;# Safety init
       set xrat 1.   ; set yrat  1.
       if {$xscale!=0.} {
          if {!$xlogscale} {
             set xdiff [expr {1./abs($xscale)}]
          } else { ;# Log scale
             set xrat [expr {exp(1./abs($xscale))}]
          }
       }
       if {$ys!=0.} {
          if {!$yls} {
             set ydiff [expr {1./abs($ys)}]
          } else { ;# Log scale
             set yrat [expr {exp(1./abs($ys))}]
          }
       }
       set prevdata [list $previnc $xprev $yprev]
       if {$previnc} {
          lappend prevdata {*}$cprev
       }
       foreach { x y } [lrange $datapts $datapts_start end] {
          set tooclose 0
          if {!$xlogscale} {
             if {abs($x-$xprev)<$xdiff} {
                incr tooclose
             }
          } else {
             # Note: If xprev is 0 then it shouldn't be on the display list!
             if {$x == 0.0} { continue }
             set x [expr {abs($x)}]
             if {$x<$xrat*$xprev && $xprev<$xrat*$x} {
                incr tooclose
             }
          }
          if {!$yls} {
             if {abs($y-$yprev)<$ydiff} {
                incr tooclose
             }
          } else {
             # Note: If yprev is 0 then it shouldn't be on the display list!
             if {$y == 0.0} { continue }
             set y [expr {abs($y)}]
             if {$y<$yrat*$yprev && $yprev<$yrat*$y} {
                incr tooclose
             }
          }
          if {$tooclose == 2} {
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
             set prevdata [list 0 $x $y]
             continue
          }
          set prevdata [OWgwMakeLogLinDispPt displist $xlogscale $yls \
                           $xgraphmin $ygmax $xscale $ys \
                           $lmargin $tmargin \
                           $xminbound $yminbound $xmaxbound $ymaxbound \
                           $displeft $disptop $dispright $dispbottom \
                           $prevdata $x $y]
          foreach {previnc xprev yprev} $prevdata { break }
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

    # Each Symbol proc draws the specified symbol onto the
    # instance $graph canvas at locations specified by pts.
    # Points are skipped according to symbol_freq, starting
    # at offset offset_index.  Import clrindex is the index
    # to color for the symbol in the list colorset.  Return
    # is the canvas ids for the rendered symbols.
    private method SymbolSquare { clrindex tags index pts} {
       set color [lindex [lindex $colorset $clrindex] 0]
       set gids {}
       foreach {x y} $pts {
          incr index
          if { $index % $symbol_freq != 0 } { continue }
          set x1 [expr {$x-0.5*$symbol_size}]
          set x2 [expr {$x1 + $symbol_size}]
          set y1 [expr {$y-0.5*$symbol_size}]
          set y2 [expr {$y1 + $symbol_size}]
          lappend gids [$graph create line \
                           $x1 $y1 $x2 $y1 $x2 $y2 $x1 $y2 $x1 $y1 \
                           -fill $color -width $curve_width -tags $tags]
       }
       return $gids
    }
    private method SymbolDiamond { clrindex tags index pts} {
       set color [lindex [lindex $colorset $clrindex] 0]
       set gids {}
       foreach {x y} $pts {
          incr index
          if { $index % $symbol_freq != 0 } { continue }
          set x1 [expr {$x-0.5*$symbol_size}]
          set x2 [expr {$x1 + $symbol_size}]
          set y1 [expr {$y-0.5*$symbol_size}]
          set y2 [expr {$y1 + $symbol_size}]
          lappend gids [$graph create line \
                           $x1 $y $x $y1 $x2 $y $x $y2 $x1 $y \
                           -fill $color -width $curve_width -tags $tags]
       }
       return $gids
    }
    private method SymbolCircle12 { clrindex tags index pts} {
       # Circle with vertical radius mark at clock position 12 o'clock
       set fill [lindex [lindex $colorset $clrindex] 0]
       set outline [lindex [lindex $colorset $clrindex] 1]
       set gids {}
       foreach {x y} $pts {
          incr index
          if { $index % $symbol_freq != 0 } { continue }
          set x1 [expr {$x-0.5*$symbol_size}]
          set x2 [expr {$x1 + $symbol_size}]
          set y1 [expr {$y-0.5*$symbol_size}]
          set y2 [expr {$y1 + $symbol_size}]
          lappend gids [$graph create arc \
            $x1 $y1 $x2 $y2 -start 90 -extent 359 \
            -fill $fill -outline $outline -width $curve_width -tags $tags]
       }
       return $gids
    }
    private method SymbolTriangle { clrindex tags index pts} {
       set color [lindex [lindex $colorset $clrindex] 0]
       set gids {}
       foreach {x y} $pts {
          incr index
          if { $index % $symbol_freq != 0 } { continue }
          set x1 [expr {$x-0.5*$symbol_size}]
          set x2 [expr {$x1 + $symbol_size}]
          set y1 [expr {$y-0.5*$symbol_size}]
          set y2 [expr {$y1 + $symbol_size}]
          lappend gids [$graph create line \
                           $x1 $y1 $x2 $y1 $x $y2 $x1 $y1 \
                           -fill $color -width $curve_width -tags $tags]
       }
       return $gids
    }
    private method SymbolInvertTriangle { clrindex tags index pts} {
       # Inverted triangle symbol
       set color [lindex [lindex $colorset $clrindex] 0]
       set gids {}
       foreach {x y} $pts {
          incr index
          if { $index % $symbol_freq != 0 } { continue }
          set x1 [expr {$x-0.5*$symbol_size}]
          set x2 [expr {$x1 + $symbol_size}]
          set y1 [expr {$y-0.5*$symbol_size}]
          set y2 [expr {$y1 + $symbol_size}]
          lappend gids [$graph create line \
                           $x1 $y2 $x $y1 $x2 $y2 $x1 $y2 \
                           -fill $color -width $curve_width -tags $tags]
       }
       return $gids
    }
    private method SymbolPlus { clrindex tags index pts} {
       # "+" symbol
       set color [lindex [lindex $colorset $clrindex] 0]
       set gids {}
       foreach {x y} $pts {
          incr index
          if { $index % $symbol_freq != 0 } { continue }
          set x1 [expr {$x-0.5*$symbol_size}]
          set x2 [expr {$x1 + $symbol_size}]
          set y1 [expr {$y-0.5*$symbol_size}]
          set y2 [expr {$y1 + $symbol_size}]
          lappend gids [$graph create line $x1 $y $x2 $y \
                           -fill $color -width $curve_width -tags $tags]
          lappend gids [$graph create line $x $y1 $x $y2 \
                           -fill $color -width $curve_width -tags $tags]
       }
       return $gids
    }
    private method SymbolX { clrindex tags index pts} {
       # "X" symbol
       set color [lindex [lindex $colorset $clrindex] 0]
       set gids {}
       foreach {x y} $pts {
          incr index
          if { $index % $symbol_freq != 0 } { continue }
          set x1 [expr {$x-0.5*$symbol_size}]
          set x2 [expr {$x1 + $symbol_size}]
          set y1 [expr {$y-0.5*$symbol_size}]
          set y2 [expr {$y1 + $symbol_size}]
          lappend gids [$graph create line $x1 $y1 $x2 $y2 \
                           -fill $color -width $curve_width -tags $tags]
          lappend gids [$graph create line $x2 $y1 $x1 $y2 \
                           -fill $color -width $curve_width -tags $tags]
       }
       return $gids
    }

    private method PlotSymbols { symbol_id color_id tags \
                                    under_id offset_index pts } {
       # Draws symbols at a subset of pts, determined by freq.
       if {$symbol_freq<1} { return {} } ;# freq == 0 ==> draw no symbols

       # Call symbol method
       set index [expr {$offset_index/2 - 1}]
       set symname [lindex $symbols $symbol_id]
       set gids [$this Symbol${symname} $color_id $tags $index $pts]

       # Finish up
       foreach g $gids { $graph lower $g $under_id }
       return $gids
    }

    private method PlotCurve { curveid update_only} {
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
               set color_id $curve($curveid,color)
               set symbol_id $curve($curveid,symbol)
            } else {
               while {[string match {} \
                [set color_id [lindex $curve($curveid,segcolor) $segindex]]]} {
                  # Segment colors and symbols are assigned in lazy fashion
                  lappend curve($curveid,segcolor) \
                     [expr {$segcount % [llength $colorset]}]
                  lappend curve($curveid,segsymbol) \
                     [expr {$segcount % [llength $symbol]}]
                  incr segcount
               }
               set symbol_id [lindex $curve($curveid,segsymbol) $segindex]
            }
            set color [lindex $colorset $color_id 0]
            if {[llength $ptlist]>=4} {
               set gid [eval $graph create line $ptlist \
                           -fill $color \
                           -width $curve_width \
                           -smooth $smoothcurves \
                           -tags {[list tag$curveid showpos]}]
               $graph lower $gid $matteid
            }
            $this PlotSymbols $symbol_id $color_id \
               [list tag$curveid showpos] $matteid $startindex $ptlist
         } else {
            # Redraw all segments
            set curve($curveid,segcount) 1
            $graph delete tag$curveid
            if {[string match "segment" $color_selection]} {
               set color_by_segment 1
            } else {
               set color_by_segment 0
               set color_id $curve($curveid,color)
               set symbol_id $curve($curveid,symbol)
            }
            set segindex 0
            foreach ptlist $curve($curveid,ptsdisp) {
               if {$color_by_segment} {
                  while {[string match {} [set color_id \
                          [lindex $curve($curveid,segcolor) $segindex]]]} {
                     # Segment colors and symbols are assigned in lazy fashion
                     lappend curve($curveid,segcolor) \
                        [expr {$segcount % [llength $colorset]}]
                     lappend curve($curveid,segsymbol) \
                        [expr {$segcount % [llength $symbols]}]
                     incr segcount
                  }
                  set symbol_id [lindex $curve($curveid,segsymbol) $segindex]
                  incr segindex
               }
               set totalsize [llength $ptlist]
               if {$totalsize==0} { continue }
               OWgwAssert {$totalsize%2==0} \
                  {PC-b Bad ptlist: $ptlist}                     ;###
               set color [lindex $colorset $color_id 0]
               if {!$usedrawpiece} {
                  if {$totalsize>=4} {
                     set cmd [linsert $ptlist 0 $graph create line]
                     lappend cmd -fill $color \
                        -width $curve_width \
                        -smooth $smoothcurves \
                        -tags [list tag$curveid showpos]
                     set gid [eval $cmd]
                     $graph lower $gid $matteid
                  }
                  $this PlotSymbols $symbol_id $color_id \
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
                     lappend cmd -fill $color \
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
                     lappend cmd -fill $color \
                        -width $curve_width \
                        -smooth $smoothcurves \
                        -tags [list tag$curveid showpos]
                     set gid [eval $cmd]
                     $graph lower $gid $matteid
                  }
                  $this PlotSymbols $symbol_id $color_id \
                     [list tag$curveid showpos] $matteid 0 $ptlist
               }
            }
         }
        if {$entry_segcount != $segcount} {
           # New segement(s) rendered; update key
           $this DrawKey
        }
    }
    method AxisUse {} {
       # Returns a two item list specifying the number of
       # active (non-hidden) curves plotted against the y
       # and y2 axes.
       set y1count 0
       set y2count 0
       foreach curveid $idlist {
          if {! $curve($curveid,hidden)} {
             if {$curve($curveid,yaxis) == 2} {
                incr y2count
             } else {
                incr y1count
             }
          }
       }
       return [list $y1count $y2count]
    }
    method GetCurveList { yaxis } {
       # Returns a list of all non-hidden curves on the specified
       # axis, identified by name.  (This is the same name that
       # appears in the graph key.)
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
        if {[llength $y1list]==0 && [llength $y2list]==0} {
           # No curves selected; reset display state stack
           $this ResetStoreStack
        } elseif {[$this GetStoreStackSize]==0} {
           $this StoreState
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
       set xabsmin [set yabsmin [set y2absmin "z"]]
       set xabsmax [set yabsmax [set y2absmax {}]]
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
          set test $curve($cid,xabsmin)
          if {![string match {} $test] && $test<$xabsmin} {
             set xabsmin $test
          }
          set test $curve($cid,xabsmax)
          if {$test>$xabsmax} {set xabsmax $test}
          if {$curve($cid,yaxis)!=2} {
             set test $curve($cid,ymin)
             if {$test<$ymin} {set ymin $test}
             set test $curve($cid,ymax)
             if {$test>$ymax} {set ymax $test}
             set test $curve($cid,yabsmin)
             if {![string match {} $test] && $test<$yabsmin} {
                set yabsmin $test
             }
             set test $curve($cid,yabsmax)
             if {$test>$yabsmax} {set yabsmax $test}
          } else {
             set test $curve($cid,ymin)
             if {$test<$y2min} {set y2min $test}
             set test $curve($cid,ymax)
             if {$test>$y2max} {set y2max $test}
             set test $curve($cid,yabsmin)
             if {![string match {} $test] && $test<$y2absmin} {
                set y2absmin $test
             }
             set test $curve($cid,yabsmax)
             if {$test>$y2absmax} {set y2absmax $test}
          }
          set cidptcount [expr [llength $curve($cid,pts)]/2 - 1]
          if {$cidptcount>0} { incr ptcount $cidptcount }
       }
       # Check: If {x,y,y2}absmin == "z", then there should be no
       # non-zero values for any curve on that axis, and therefore the
       # corresponding absmax value should be empty.
       OWgwAssert \
          {![string match z $xabsmin] || [string match {} $xabsmax]} \
          "xabsmin/max logic error in Ow_GraphWin GetDataExtents"
       OWgwAssert \
          {![string match z $yabsmin] || [string match {} $yabsmax]} \
          "xabsmin/max logic error in Ow_GraphWin GetDataExtents"
       OWgwAssert \
          {![string match z $y2absmin] || [string match {} $y2absmax]} \
          "xabsmin/max logic error in Ow_GraphWin GetDataExtents"

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
       if {[string match {} $xabsmax]} {
          set xabsmin 0.1 ; set xabsmax 1.0
       }
       if {[string match {} $yabsmax]} {
          set yabsmin 0.1 ; set yabsmax 1.0
       }
       if {[string match {} $y2absmax]} {
          set y2absmin 0.1 ; set y2absmax 1.0
       }
       if {$xlogscale} {
          set xmin $xabsmin ; set xmax $xabsmax
       }
       if {$ylogscale} {
          set ymin $yabsmin ; set ymax $yabsmax
       }
       if {$y2logscale} {
          set y2min $y2absmin ; set y2max $y2absmax
       }

       if {$ptcount == 0} {
          # No curve has more than 1 point in it, so expand
          # empty ranges by 10% so auto-limit calculations
          # don't try to display out to 17 digits.
          if { $xmin == $xmax } {
             if {$xmin<0.0} {
                set xmin [expr {$xmin * 1.05}]
                set xmax [expr {$xmax * 0.95}]
             } else {                        
                set xmin [expr {$xmin * 0.95}]
                set xmax [expr {$xmax * 1.05}]
             }
          }
          if { $ymin == $ymax } {
             if {$ymin<0.0} {
                set ymin [expr {$ymin * 1.05}]
                set ymax [expr {$ymax * 0.95}]
             } else {                        
                set ymin [expr {$ymin * 0.95}]
                set ymax [expr {$ymax * 1.05}]
             }
          }
          if { $y2min == $y2max } {
             if {$y2min<0.0} {
                set y2min [expr {$y2min * 1.05}]
                set y2max [expr {$y2max * 0.95}]
             } else {
                set y2min [expr {$y2min * 0.95}]
                set y2max [expr {$y2max * 1.05}]
             }
          }
       }
       return [list $xmin $xmax $ymin $ymax $y2min $y2max]
    }
    private proc RoundRange {min max logscale} {
       # Assumes min<=max, and if logscale is true then also 0<min
       OWgwAssert {$min<=$max && (!$logscale || 0<$min)} \
          "Bad imports to RoundRange: ${min}, ${max}, ${logscale}"
       if {$min<0 && $max>0} {
          # Min and max have opposite signs; keep one digit
          # (Note: logscale must be false)
          set scale [expr {pow(10,floor(log10($max)))}]
          set tmax [expr {ceil($max/$scale)*$scale}]
          set scale [expr {pow(10,floor(log10(-$min)))}]
          set tmin [expr {floor($min/$scale)*$scale}]
          set minsigfig [set maxsigfig 1]
       } else {
          # Min and max have same sign
          set absmin [expr {abs($min)}]
          set absmax [expr {abs($max)}]
          if {$min<0.0} {
             set t $absmin ; set absmin $absmax ; set absmax $t
          }
          if {$absmax>=10*$absmin} {
             # Ratio bigger than 10; keep one digit on larger
             # value.  Round smaller value to zero for linear
             # scale axes, or one digit on log scale axes.
             if {$absmax == 0.0} {
                set tmax 1
             } else {
                set scale [expr {pow(10,floor(log10($absmax)))}]
                set tmax [expr {ceil($absmax/$scale)*$scale}]
             }
             if {!$logscale} {
                set tmin 0
             } else {
                set scale [expr {pow(10,floor(log10($absmin)))}]
                set tmin [expr {floor($absmin/$scale)*$scale}]
                OWgwAssert {0<$tmin} "Coding error; tmin = $tmin <= 0"
             }
             set minsigfig [set maxsigfig 1]
          } else {
             # Ratio smaller than 10; compute number of digits
             # necessary to distinguish min from max
             set diff [expr {$absmax-$absmin}]
             if {$diff <= 0.0} {
                # min == max.  Inflate +/- 6.25%.
                set absmax [expr {$absmax*1.0625}]
                set absmin [expr {$absmin/1.0625}]
                set diff [expr {$absmax-$absmin}]
             }
             set scale [expr {pow(10,floor(log10($diff)))}]
             set tmax [expr {ceil($absmax/$scale)}]
             set maxsigfig [expr {1+int(floor(log10($tmax)))}]
             set tmax [expr {$tmax*$scale}]
             set tmin [expr {floor($absmin/$scale)}]
             if {$logscale && $tmin<=0.0} {
                set tmin 0.1 ;# Safety; sample case: min=9, max=89
                set minsigfig 1
             } else {
                set minsigfig \
                   [expr {$tmin==0.0 ? 1 : 1+int(floor(log10($tmin)))}]
             }
             set tmin [expr {$tmin*$scale}]
          }
          if {$min<0.0} {
             # Change sign and swap order
             set t $tmin
             set tmin [expr {-1*$tmax}]
             set tmax [expr {-1*$t}]
          }
       }
       set tmin [Ow_GraphWin DisplayFormat $tmin $minsigfig]
       set tmax [Ow_GraphWin DisplayFormat $tmax $maxsigfig]
       return [list $tmin $tmax]
    }

    private method SpecifyGraphExtents {xmin xmax ymin ymax y2min y2max} {
       # Performs some validity checks, and if satisfied sets
       # {x,y,y2}graph{min,max} to given imports as strings without
       # reformatting.  This routine is intended for internal use by
       # Ow_GraphWin only; external code should use the SetGraphLimits
       # interface.  SetGraphLimits rounds the imports to "nice" values,
       # formats the values, calls this routine, and then updates the
       # pan/zoom base state.  SetGraphLimits can optionally select
       # graph extents automatically based on curve data.
       #
       # Returns 1 if the graph limits were changed (as strings).

       # Sanity checks
       if {$xmin>$xmax} {
          set t $xmin ; set xmin $xmax ; set xmax $xmin
       }
       if {$ymin>$ymax} {
          set t $ymin ; set ymin $ymax ; set ymax $ymin
       }
       if {$y2min>$y2max} {
          set t $y2min ; set y2min $y2max ; set y2max $y2min
       }

       # Log scale safety
       set deflogrange 1e-6
       set digs 6
       if {$xlogscale && $xmin <= 0.0} {
          if {$xmax <= 0.0} { # Both limits non-positive
             if {$xmin == 0.0} {
                set xmin $deflogrange ; set xmax 1.0
             } elseif {$xmax == 0.0} {
                set xmax [expr {abs($xmin)}]
                set xmin [expr {$deflogrange*$xmax}]
             } else {
                set t [expr {abs($xmax)}]
                set xmax [expr {abs($xmin)}]
                set xmin $t
             }
             set xmax [Ow_GraphWin DisplayFormat $xmax $digs]
             set xmin [Ow_GraphWin DisplayFormat $xmin $digs]
          } else { # xmin non-positive, xmax positive
             set xmin [expr {abs($xmin)}]
             if {$xmin>$xmax} {
                set xmax [Ow_GraphWin DisplayFormat $xmin $digs]
             }
             set xmin [expr {$deflogrange*$xmax}]
             set xmin [Ow_GraphWin DisplayFormat $xmin $digs]
          }
       }
       if {$ylogscale && $ymin <= 0.0} {
          if {$ymax <= 0.0} { # Both limits non-positive
             if {$ymin == 0.0} {
                set ymin $deflogrange ; set ymax 1.0
             } elseif {$ymax == 0.0} {
                set ymax [expr {abs($ymin)}]
                set ymin [expr {$deflogrange*$ymax}]
             } else {
                set t [expr {abs($ymax)}]
                set ymax [expr {abs($ymin)}]
                set ymin $t
             }
             set ymax [Ow_GraphWin DisplayFormat $ymax $digs]
             set ymin [Ow_GraphWin DisplayFormat $ymin $digs]
          } else { # ymin non-positive, ymax positive
             set ymin [expr {abs($ymin)}]
             if {$ymin>$ymax} {
                set ymax [Ow_GraphWin DisplayFormat $ymin $digs]
             }
             set ymin [expr {$deflogrange*$ymax}]
             set ymin [Ow_GraphWin DisplayFormat $ymin $digs]
          }
       }
       if {$y2logscale && $y2min <= 0.0} {
          if {$y2max <= 0.0} { # Both limits non-positive
             if {$y2min == 0.0} {
                set y2min $deflogrange ; set y2max 1.0
             } elseif {$y2max == 0.0} {
                set y2max [expr {abs($y2min)}]
                set y2min [expr {$deflogrange*$y2max}]
             } else {
                set t [expr {abs($y2max)}]
                set y2max [expr {abs($y2min)}]
                set y2min $t
             }
             set y2max [Ow_GraphWin DisplayFormat $y2max $digs]
             set y2min [Ow_GraphWin DisplayFormat $y2min $digs]
          } else { # y2min non-positive, y2max positive
             set y2min [expr {abs($y2min)}]
             if {$y2min>$y2max} {
                set y2max [Ow_GraphWin DisplayFormat $y2min $digs]
             }
             set y2min [expr {$deflogrange*$y2max}]
             set y2min [Ow_GraphWin DisplayFormat $y2min $digs]
          }
       }

       # Safety adjustment
       set eps 1e-12
       set digs 14
       if {$xmax<=$xmin} {
          set xmax [expr {$xmin+$eps*abs($xmin)}]
          set xmin [expr {$xmin-$eps*abs($xmin)}]
          set xmin [Ow_GraphWin DisplayFormat $xmin $digs]
          set xmax [Ow_GraphWin DisplayFormat $xmax $digs]
       }
       if {$ymax<=$ymin} {
          set ymax [expr {$ymin+$eps*abs($ymin)}]
          set ymin [expr {$ymin-$eps*abs($ymin)}]
          set ymin [Ow_GraphWin DisplayFormat $ymin $digs]
          set ymax [Ow_GraphWin DisplayFormat $ymax $digs]
       }
       if {$y2max<=$y2min} {
          set y2max [expr {$y2min+$eps*abs($y2min)}]
          set y2min [expr {$y2min-$eps*abs($y2min)}]
          set y2min [Ow_GraphWin DisplayFormat $y2min $digs]
          set y2max [Ow_GraphWin DisplayFormat $y2max $digs]
       }
       if {[string compare $xmin $xgraphmin]!=0 || \
           [string compare $xmax $xgraphmax]!=0 || \
           [string compare $ymin $ygraphmin]!=0 || \
           [string compare $ymax $ygraphmax]!=0 || \
           [string compare $y2min $y2graphmin]!=0 || \
           [string compare $y2max $y2graphmax]!=0} {
          # Change limits
          set xgraphmin $xmin
          set xgraphmax $xmax
          set ygraphmin $ymin
          set ygraphmax $ymax
          set y2graphmin $y2min
          set y2graphmax $y2max
          # Mark all curve displays invalid
          $this InvalidateDisplay $idlist
          return 1
       }
       return 0 ;# No change
    }

    method SetGraphLimits {{xmin {}} {xmax {}} \
                           {ymin {}} {ymax {}} {y2min {}} {y2max {}} } {
       # Sets the graph numerical limits.  Any blank values are filled
       # in using data values (autoscale).  If all values are set to
       # autoscale then the display state stack is reset (i.e., cleared).
       # This routine does not update the display, but does add the
       # new state onto the display state stack.
       set autocount 0
       foreach chk [list $xmin $xmax $ymin $ymax $y2min $y2max] {
          if {[string match {} $chk]} { incr autocount }
       }
       if {$autocount>0} {
          # Autoscale one or more limit values
          foreach {txmin txmax tymin tymax ty2min ty2max} \
             [$this GetDataExtents] { break }
          foreach elt {txmin txmax tymin tymax ty2min ty2max} {
             # Round to 15 digits to allow for rounding errors.  It may
             # be better to scale based on range, e.g., txmax-txmin.
             set $elt [format %.14e [set $elt]]
          }

          # Make "nice" limits
          foreach {txmin txmax} \
             [Ow_GraphWin RoundRange $txmin $txmax $xlogscale] { break }
          foreach {tymin tymax} \
             [Ow_GraphWin RoundRange $tymin $tymax $ylogscale] { break }
          foreach {ty2min ty2max} \
             [Ow_GraphWin RoundRange $ty2min $ty2max $y2logscale] { break }

          if {[string match {} $xmin]} { set xmin $txmin }
          if {[string match {} $xmax]} { set xmax $txmax }
          if {[string match {} $ymin]} { set ymin $tymin }
          if {[string match {} $ymax]} { set ymax $tymax }
          if {[string match {} $y2min]} { set y2min $ty2min }
          if {[string match {} $y2max]} { set y2max $ty2max }
       }
       set change [$this SpecifyGraphExtents \
                      $xmin $xmax $ymin $ymax $y2min $y2max]
       if {$autocount == 6} {
          $this ResetStoreStack  ;# Dump old stack states
       } else {
          $this StoreState ;# Add new state to stack
       }
       return $change
    }

    method GetGraphLimits {} { \
            xgraphmin xgraphmax \
            ygraphmin ygraphmax y2graphmin y2graphmax} {
        return [list $xgraphmin $xgraphmax \
                $ygraphmin $ygraphmax $y2graphmin $y2graphmax]
    }
    method GetGraphLimitsReduced {} {
       # Same as GetGraphLimits, but any axis without curves plotted
       # against it has limits set to empty strings.  This is used in
       # the display state stack to implement transparent limits.
       #   As a special case, if no curves are plotted then the return
       # is an empty string.
       foreach {c1cnt c2cnt} [$this AxisUse] { break }
       if {$c1cnt + $c2cnt == 0} {
          # No curves set
          return {}
       }
       set displaystate [$this GetGraphLimits]
       if {$c1cnt == 0} {
          # Disable limit settings for y1 axis
          set displaystate [lreplace $displaystate 2 3 {} {}]
       }
       if {$c2cnt == 0} {
          # Disable limit settings for y2 axis
          set displaystate [lreplace $displaystate 4 5 {} {}]
       }
       return $displaystate
    }
    method RefreshDisplay {} {
       set canvasshade [Ow_GetShade $canvas_color]
       if {$canvasshade<128} {  ;# Dark background
          set axescolor white
          set colorset $colorset_dark
       } else {                 ;# Light background
          set axescolor black
          set colorset $colorset_light
       }
       set key_background $canvas_color
       set key_boxcolor $axescolor
       $this DrawAxes
       $this DrawCurves 0
       $this DrawKey
    }
    method Reset {} {
        # Re-initializes most of the graph properties to initial defaults
        $this DeleteAllCurves
        $this SetGraphLimits
        set hrule {} ; set vrule {}
        set title  {}
        set xlabel {}
        set ylabel {}
        $this RefreshDisplay
        $this ResetStoreStack
    }
    private method PrintData { curvepat } {
       # For debugging
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

# OWgwMakeLogLinDispPt : Service proc of AddDisplayPoints method.
#
# Given two points P0=(u0,v0) and P1=(u1,v1) in linear (e.g., display)
# coordinates, the line through those points is P(t) = (1-t)*P0 + t*P1 =
# [(1-t)*u0+t*u1, (1-t)*v0+t*v1].  So P(0) = P0, P(1) = P1, and
# {P(t):0<t<1} is the line segment between P0 and P1.  The intersection
# between that line and the vertical line x=a is
#
#     a = (1-t)*u0 + t*u1 = u0 + t*(u1-u0) => t = (a-u0)/(u1-u0)
#  => v = (1-t)*v0 + t*v1 = v0 + (v1-v0)*t = v0 + (v1-v0)*(a-u0)/(u1-u0)
#
# The point (u(t),v(t)) = (a,v0 + (v1-v0)*(a-u0)/(u1-u0)) lies between
# P0 and P1 iff 0<t<1 <=> 0 < (a-u0)/(u1-u0) < 1
#                     <=> 0 < (a-u0)*(u1-u0) && |a-u0| < |u1-u0|
#
# The computation is analogous for horizontal lines, say y=b
#
#     b = (1-s)*v0 + s*v1 = v0 + s*(v1-v0) => s = (b-v0)/(v1-v0)
#  => u = (1-s)*u0 + s*u1 = u0 + (u1-u0)*s = u0 + (u1-u0)*(b-v0)/(v1-v0)
#
# The point (u(s),v(s)) = (u0 + (u1-u0)*(b-v0)/(v1-v0)) lies between
# P0 and P1 iff 0<s<1 <=> 0 < (b-v0)/(v1-v0) < 1
#                     <=> 0 < (b-v0)*(v1-v0) && |b-v0| < |v1-v0|
#
#
# Imports ---
#  dsplstname : Name of display list in caller (canvas coords)
#  {x,y}logscale : 0 => linear axis, 1 => log scale axis
#  {x,y}database : data coords corresponding to canvas origin,
#                  namely (xgraphmin,ygmax)
#  xs, ys : scaling, canvas coords/data coords.  For log scaled
#           axes this is the scaling ratio after taking log of
#           the data.
#  {x,y}dispbase : canvas origin, (lmargin,tmargin)
#  {x,y}{min,max} : data coords corresponding to fudged viewing window
#  disp{left,top,right,bottom} : canvas coords of viewing window
#                                +/- fudge
#  prevdata : list {previnc, xprev, yprev, xprevdisp, yprevdisp}
#             where previnc is true iff previous point was inside
#             fudged viewing window, (xprev,yprev) are data coords
#             of preceding point, and (xprevdisp,yprevdisp) are
#             canvas coords of preceding point.  The last two elements
#             are optional, and are included only if the coords were
#             computed by previous necessity.
#  x, y : Current point, data coords
#
# Export ---
#  List {curinc, x, y, xdisp, ydisp} corresponding to prevdata import
#  for new data.  Here (xdisp, ydisp) are included iff needed during
#  processing.  This list can be used directly as prevdata for the
#  next call to this routine.
#
# If curinc is true then {xdisp ydisp} required, if inc is false then
# {xdisp ydisp} are put on list if they were computed for other reasons.
# The lastdsp (last display) point pair is checked in the case where
# previnc was false but computations indicate that a new segment is to
# be added to dsplst because either the current point is inside the
# display region or else the segment between the previous point and the
# current point crosses the display region.  The issue is that if the
# last point on dsplst is on the other side of the display region then a
# false segment can be introduced.  For example:
#
#             b      ------ q1
#         b' /------/        |
#     +-----*-----------+    |
#     |    / \          |    |
#     |   a   \     g   |    |
#     |        \   /    |    |
#     |         \ /     |    |
#     +----------#------+    |
#               / f'-\       |
#              /      -- \   |
#            f            -- q2
#
#
# The scenario is that points a and b appeared earlier, with a inside
# the display region and b not.  So points a and intermediary b' are
# placed onto dsplst.  Next several additional points, terminating with
# point f, are processed, but they are all outside the display region
# and have no effect on dsplst.  Then point g is added.  It is inside
# the display region, so we want to produce a segment from f' to g.
# However, if we simply add f' to dsplst, then we'll get an anomalous
# segment running from b' to f' (since b' immediately precedes f' on
# dsplst).  The only provision in the drawing routines for adding a
# break to a curve creates separate segments that may be colored
# differently, which is not what we want.  So instead this code needs
# to detect this occurrence and introduce one or two corner posts,
# here labeled q1 and q2, such that line segments from b' to q1 to q2 to f'
# all lie outside the display region and so do not affect the display.
#
# NOTE: All canvas coords should be integer-valued doubles,
#       e.g., 7.0, -3.0, 0.0.  This includes in particular
#       the disp{left,top,right,bottom} values.
#
;proc OWgwMakeLogLinDispPt { dsplstname xlogscale ylogscale
                             xdatabase ydatabase xs ys
                             xdispbase ydispbase
                             xmin ymin xmax ymax
                             displeft disptop dispright dispbottom
                             prevdata x y} {
   # Extract values from prevdata. Note that last two elements
   # (xprevdisp and yprevdisp) are optional.

   # NOTE: This routine applies log to x and y as needed, but assumes
   # all data imports are positive.  The caller should weed out zero
   # values and either pitch negative values or apply abs() before
   # calling this routine.

# DATAUSE:
#  x, xmin, xmax, y, ymin, ymax  # data coords, used to find points
#                                # that may cross display
#  previnc, xlogscale, ylogscale
#  xdatabase, ydatabase, xs, ys, displeft, disptop   <-- for transform
#  displeft, disptop, dispright, dispbottom <- canvas coords corresponding
#                                              to x/y-min/max; for corners
#  dsplst                        # Append to, also find corner needs
#  xprev, yprev or u0, v0 (data / canvas coords) # x/yprev needed for coarse
#                             # winnowing, u/v0 needed for inbound crossings

# Next rewrite: Might want to not bother with computing display border
# crossing points, but instead just give canvas computed display
# coordinates and let the canvas figure the boundary crossing.  Would
# probably be faster in the case when the curve goes in and out of the
# display window a lot.  OTOH, corner insertions would still be needed,
# and that code would change a bit; in particular, corner repeats might
# be harder to determine.  OTOOH, we could change the display list to be
# a list of lists, where each list is a polyline, with breaks in
# polylines when a curve exits the canvas.  This would require reworking
# the curve rendering code.

   upvar $dsplstname dsplst
   foreach {previnc xprev yprev u0 v0} $prevdata { break }

   if {$xmin<=$x && $x<=$xmax && $ymin<=$y && $y<=$ymax} {
      set curinc 1
   } else {
      set curinc 0
   }

   if {$curinc && $previnc} {
      # The canonical case: (xprev,yprev) already in display
      # list, and (x,y) inside display box.
      if {$xlogscale} {
         set xdisp [expr {floor(log($x/$xdatabase)*$xs+$xdispbase+0.5)}]
      } else {
         set xdisp [expr {floor(($x-$xdatabase)*$xs+$xdispbase+0.5)}]
      }
      if {$ylogscale} {
         set ydisp [expr {floor(log($y/$ydatabase)*$ys+$ydispbase+0.5)}]
      } else {
         set ydisp [expr {floor(($y-$ydatabase)*$ys+$ydispbase+0.5)}]
      }
      lappend dsplst $xdisp $ydisp
      return [list 1 $x $y $xdisp $ydisp]
   }
   # Otherwise, either the previous or the current point
   # is not inside the display box

   # Dispose of simple no display effect case.  This is currently
   # commented out because this case is weeded out by calling routine.
   # if {!$curinc && !$previnc} {
   #    # If display box and prev+cur box don't overlap,
   #    # then cur has no effect on display.
   #    if {($x<$xmin && $xprev<$xmin) || ($xmax<$x && $xmax<$xprev) || \
   #        ($y<$ymin && $yprev<$ymin) || ($ymax<$y && $ymax<$yprev)} {
   #       return [list 0 $x $y]
   #    }
   # }

   # It can still happen that current point does not change dsplst,
   # but to figure this out we might as well convert x and y to display
   # coordinates, since this is needed for the log axis case and is not
   # terribly expensive in the linear case.
   if {$xlogscale} {
      set u1 [expr {log($x/$xdatabase)*$xs+$xdispbase}]
   } else {
      set u1 [expr {($x-$xdatabase)*$xs+$xdispbase}]
   }
   set ru1 [expr {floor($u1+0.5)}]
   if {$ylogscale} {
      set v1 [expr {log($y/$ydatabase)*$ys+$ydispbase}]
   } else {
      set v1 [expr {($y-$ydatabase)*$ys+$ydispbase}]
   }
   set rv1 [expr {floor($v1+0.5)}]
   if {[llength $prevdata]<5} {
      # Display coordinates xprevdisp and yprevdisp (u0, v0) not
      # computed previously; do so now.  Round to match behavior
      # in case u0, v0 are computed in previous pass.
      if {$xlogscale} {
         set u0 [expr {floor(log($xprev/$xdatabase)*$xs+$xdispbase+0.5)}]
      } else {
         set u0 [expr {floor(($xprev-$xdatabase)*$xs+$xdispbase+0.5)}]
      }
      if {$ylogscale} {
         set v0 [expr {floor(log($yprev/$ydatabase)*$ys+$ydispbase+0.5)}]
      } else {
         set v0 [expr {floor(($yprev-$ydatabase)*$ys+$ydispbase+0.5)}]
      }
   }

   # Find display boundary crossings, and put in false points to mimic
   # display of fully plotted curve.  This doesn't work exactly right
   # with "smooth" curves --- do we care?
   set crossings {}
   set ulo $displeft  ;  set uhi $dispright
   set vlo $disptop   ;  set vhi $dispbottom
   set udiff [expr {$u1-$u0}] ; set absudiff [expr {abs($udiff)}]
   set vdiff [expr {$v1-$v0}] ; set absvdiff [expr {abs($vdiff)}]

   # Check left and right vertical crossings, u=ulo and u=uhi.
   # Round computed y coords to integer.  (We assume ulo and uhi
   # are integers.)  No crossing is added if udiff==0, ediff==0,
   # or ediff==udiff.  AFAICT this is OK.  In the first case
   # the line segment from (u0,v0) to (u1,v1) runs parallel to
   # the left and right vertical boundaries, and so can't "cross"
   # them.  In the latter two cases one point or the other lies
   # on the boundary.  There might be a small hole for rounding
   # error in the latter cases, if a point on a boundary is
   # somehow classified as being outside the boundary, so watch
   # for that.
   foreach ubdry [list $ulo $uhi] {
      set ediff [expr {$ubdry-$u0}]
      if {$udiff*$ediff>0 && abs($ediff)<$absudiff} {
         # 0 < t = ediff/udiff < 1.  Check crossing is between top and
         # bottom edges
         set tst [expr {abs($ediff)*$vdiff}]
         if {$absudiff*($vlo-$v0)<= $tst && $tst <= $absudiff*($vhi-$v0)} {
            # Tests passed
            set t [expr {$ediff/$udiff}]  ;#  t should be in (0,1) (i.e.,
            set vt [expr {floor($v0+$vdiff*$t+0.5)}] ;## no overflow)
            lappend crossings [list $t $ubdry $vt]
         }
      }
   }

   # Check top and bottom horizontal crossings, v=vlo and v=vhi.
   # Round computed x coords to integer.  (We assume vlo and vhi
   # are integers.)
   foreach vbdry [list $vlo $vhi] {
      set ediff [expr {$vbdry-$v0}]
      if {$vdiff*$ediff>0 && abs($ediff)<$absvdiff} {
         # 0 < t = ediff/vdiff < 1.  Check crossing is between left and
         # right edges
         set tst [expr {abs($ediff)*$udiff}]
         if {$absvdiff*($ulo-$u0)<= $tst && $tst <= $absvdiff*($uhi-$u0)} {
            # Tests passed
            set t [expr {$ediff/$vdiff}]  ;#  t should be in (0,1) (i.e.,
            set ut [expr {floor($u0+$udiff*$t+0.5)}] ;## no overflow)
            lappend crossings [list $t $ut $vbdry]
         }
      }
   }

   # In principle there shouldn't be more than two crossings, but protect
   # against rounding errors by coalescing close points.
   if {[llength $crossings]>1} {
      set crossings [lsort -real -index 0 $crossings]
      set newcrossings [list [set keeppt [lindex $crossings 0]]]
      foreach tst [lrange $crossings 1 end] {
         if {abs([lindex $keeppt 1]-[lindex $tst 1])>=1 || \
                abs([lindex $keeppt 2]-[lindex $tst 2])>=1} {
            # Points don't overlap.  Keep new point
            lappend newcrossings [set keeppt $tst]
         }
      }
      set crossings $newcrossings
   }

   # Construct display list additions from crossings list and, if the
   # current point is inside the display window, the current point.
   set dspadditions {}
   foreach elt $crossings {
      lappend dspadditions [lindex $elt 1] [lindex $elt 2]
   }
   if {$curinc} {
      lappend dspadditions $ru1 $rv1
   }

   # It remains to check if corner points are needed to wrap
   # curve trace around the display window.
   if {!$previnc && [llength $crossings]>0 && [llength $dsplst]>0} {
      # Since previnc is false, the first point on dspadditions is a
      # crossing into the display region, which must lie on one of the
      # display edges.  Likewise, (xlastdsp,ylastdsp) is the point where
      # the curve previously left the display, so it must also lie on
      # the border.  In principle a border crossing might be at a
      # corner, in which case it actually lies on two edges, but that
      # occurrence should be rare, so we'll arbitrarily assign corner
      # points to one edge.  At worse this will generated a corner
      # repeat, which we'll check for at the end.
      #   Enumerate the edges like so
      #     C0          1          C1
      #       +------------------+  <- vlo
      #       |                  |
      #       |                  |
      #    0  |                  | 2
      #       |                  |
      #       |                  |
      #       +------------------+  <- vhi
      #    C3 ^        3         ^ C2
      #       |                  |
      #      ulo                uhi
      #
      # Each corner is assigned the same number as the adjacent edge on
      # the counterclockwise (e.g., the corner between edges 1 and 2 is
      # corner C1, the corner between edges 3 and 0 is corner C3).
      #
      set xlastdsp [lindex $dsplst end-1]
      set ylastdsp [lindex $dsplst end]
      if {$xlastdsp == $ulo} {
         set lastedge 0
      } elseif {$ylastdsp == $vlo} {
         set lastedge 1
      } elseif {$xlastdsp == $uhi} {
         set lastedge 2
      } elseif {$ylastdsp == $vhi} {
         set lastedge 3
      } else {
         error "ERROR: lastdsp ($xlastdsp,$ylastdsp)\
          not on display boundary ($ulo,$vlo,$uhi,$vhi)"
      }
      if {[lindex $dspadditions 0] == $ulo} {
         set newedge 0
      } elseif {[lindex $dspadditions 1] == $vlo} {
         set newedge 1
      } elseif {[lindex $dspadditions 0] == $uhi} {
         set newedge 2
      } elseif {[lindex $dspadditions 1] == $vhi} {
         set newedge 3
      } else {
         set details "Corners: $ulo $vlo $uhi $vhi"
         append details ", new: $dspadditions"
         error "ERROR: Curve entry not on display boundary\n$details"
      }
      set edgediff [expr {$newedge - $lastedge}]
      if {$edgediff!=0} {
         # Exit and entry points on different edges.  Add corner points.
         if {$edgediff<0} { incr edgediff 4 }
         if {$edgediff == 3} {
            set cornerlist $newedge
         } else {
            set cornerlist $lastedge
            if {$edgediff == 2} {
               lappend cornerlist [expr {$lastedge<3 ? $lastedge+1 : 0}]
            }
         }
         # Convert corner ids to display coordinates
         set cornerpts {}
         foreach corner $cornerlist {
            switch $corner {
               0 { lappend cornerpts $ulo $vlo }
               1 { lappend cornerpts $uhi $vlo }
               2 { lappend cornerpts $uhi $vhi }
               3 { lappend cornerpts $ulo $vhi }
            }
         }
         # Pitch the first corner if it matches lastdsp, or the last
         # corner if it matches the first point in dspadditions.
         if {[lindex $cornerpts 0] == $xlastdsp && \
                [lindex $cornerpts 1] == $ylastdsp} {
            set cornerpts [lreplace $cornerpts 0 1]
         }
         if {[lindex $cornerpts end-1] == [lindex $dspadditions 0] && \
                [lindex $cornerpts end] == [lindex $dspadditions 1]} {
            set cornerpts [lreplace $cornerpts end-1 end]
         }

         set dspadditions [concat $cornerpts $dspadditions]
      }
   } ;# End corner addition block

   lappend dsplst {*}$dspadditions  ;# Much faster than concat

   return [list $curinc $x $y $ru1 $rv1]
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
