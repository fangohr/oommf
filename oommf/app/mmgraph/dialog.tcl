# FILE: dialog.tk
#
# Tcl/Tk scripts for various dialog boxes in mmGraph
#

###################### Plot Select and Configuration #####################
# 25-June-1998, mjd
#
# Local configuration results are stored in array psc.
Oc_Class PlotConfigure {
    const public variable winpath
    const public variable title

    # Import trace and initialization array.  Must be global.
    const public variable import_arrname = {}

    # Routine in mainline to call on an Apply or OK event.
    public variable apply_callback = Oc_Nop 

    # Menu and index (as a 2-item list) in the launching window
    # to toggle off in the constructor and on in the destructor.
    # If menuraise is true, then "SmartDialog" behavior is enabled,
    # where instead of simply disabling the associated menu item,
    # it is instead colored $menucolor and it's behavior is changed
    # to raise the pre-existing dialog box.
    const public variable menu = {}
    const public variable menuraise = 1
    const public variable menucolor = red
    const private variable menudeletecleanup = Oc_Nop

    # Internal state array
    private array variable psc

    # Normal/disabled text color for widgets.  Set usual defaults,
    # but reset inside constructor off of checkbutton colors
    const private variable disabledcolor = #a3a3a3
    const private variable normalcolor = black

    # State (normal/disabled) dependency lists
    private array variable dep

    Constructor {args} {
        # Create a (presumably) unique name for the toplevel
        # window based on the instance name.
        set winpath ".[$this WinBasename]"
	set title "Configure -- [Oc_Main GetTitle]"

	# Process user preferences
	eval $this Configure $args

        # Create dialog window
        toplevel $winpath
        wm group $winpath .
        wm title $winpath $title
        Ow_PositionChild $winpath ;# Position at +.25+/25 over '.'
        focus $winpath

        # Setup destroy binding.
	bind $winpath <Destroy> "$this WinDestroy %W"

	# Setup handling of associated menu item
	if {[llength $menu]==2} {
	    set name [lindex $menu 0]
	    set item [lindex $menu 1]
	    if {$menuraise} {
		set origcmd [$name entrycget $item -command]
		set menuorigfgcolor [$name entrycget $item -foreground]
		set menuorigafgcolor [$name entrycget $item -activeforeground]
		set menudeletecleanup \
			[list $name entryconfigure $item \
			  -command $origcmd \
			  -foreground $menuorigfgcolor\
			  -activeforeground $menuorigafgcolor]
		$name entryconfigure $item \
			-command "Ow_RaiseWindow $winpath" \
			-foreground $menucolor \
			-activeforeground $menucolor
	    } else {
		$name entryconfigure $item -state disabled
		set menudeletecleanup \
			[list $name entryconfigure $item -state normal]
	    }
	}

        # Setup internal state array default values
        array set psc {
           title  {}
           autolabel 1
           xlabel {}
           ylabel {}
           y2label {}
           autolimits 1
           auto_offset_y  0
           auto_offset_y2 0
           xlogscale  0
           ylogscale  0
           y2logscale 0
           xmin   0
           xmax   1
           ymin   0
           ymax   1
           y2min   0
           y2max   1
           lmargin_min 0
           rmargin_min 0
           tmargin_min 0
           bmargin_min 0
           color_selection "curve"
           canvas_color "white"
           curve_width 0
           symbol_freq 0
           symbol_size 10
           ptbufsize  0
        }
        set gpsc [$this GlobalName psc]
        ## Global name of psc array.  This is needed to tie
        ## elements of the array to Tk buttons.

	# Determine "disabled" and "normal" colors
	checkbutton $winpath.cktemp
        set disabledcolor [$winpath.cktemp cget -disabledforeground]
	set normalcolor [$winpath.cktemp cget -foreground]
	destroy $winpath.cktemp

        # Allow override from user initialization array
        if {![string match {} $import_arrname]} {
            upvar #0 $import_arrname userinit
            if {[info exists userinit]} {
                foreach elt [array names userinit] {
                    if {[info exists psc($elt)]} {
                        set psc($elt) $userinit($elt)
                    }
                }
            }
            trace variable userinit w "$this ExternalUpdate $import_arrname"
        }

	# Title
        Ow_EntryBox New _ $winpath \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {Title:} -valuewidth 15 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(title)
	pack [$_ Cget -winpath] -fill x -expand 0 -padx 10 -pady 10

	# Frames for interior grid
	set gf [frame $winpath.gf]
        set lf [frame $winpath.gf.lf]  ;# Left half of dialog box
        set rf [frame $winpath.gf.rf]  ;# Right half of dialog box
        $lf configure -borderwidth 3 -relief ridge
        $rf configure -borderwidth 3 -relief ridge
        pack $lf $rf -side left -expand 1 -fill both -padx 10
        pack $gf -side top -expand 1 -fill both

        ################## Left side ################################
	# Labels
        set spacer_rows {}
	set dep(autolabel) {}
        checkbutton $lf.autolabel -text "Auto Label" \
                -variable ${gpsc}(autolabel) \
                -onvalue 1 -offvalue 0 \
		-command "$this UpdateState autolabel"
	grid $lf.autolabel -columnspan 6 -sticky sw

	label $lf.xlabel -text "X Label:"
        Ow_EntryBox New _ $lf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 10 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(xlabel)
	lappend dep(autolabel) $lf.xlabel $_
	grid x $lf.xlabel [$_ Cget -winpath] - - -
	grid $lf.xlabel -sticky e
	grid [$_ Cget -winpath] -sticky ew

	label $lf.ylabel -text "Y1 Label:"
        Ow_EntryBox New _ $lf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 10 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(ylabel)
	lappend dep(autolabel) $lf.ylabel $_
	grid x $lf.ylabel [$_ Cget -winpath] - - -
	grid $lf.ylabel -sticky w
	grid [$_ Cget -winpath] -sticky ew
        $this UpdateState autolabel

	label $lf.y2label -text "Y2 Label:"
        Ow_EntryBox New _ $lf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 10 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(y2label)
	lappend dep(autolabel) $lf.y2label $_
	grid x $lf.y2label [$_ Cget -winpath] - - -
	grid $lf.y2label -sticky w
	grid [$_ Cget -winpath] -sticky ew
        $this UpdateState autolabel

	# Spacer
	set rowcnt [lindex [grid size $lf] 1]
	grid rowconfigure $lf $rowcnt -minsize 12
        lappend spacer_rows $rowcnt
	incr rowcnt

	# Scaling
	set dep(autolimits) {}
        checkbutton $lf.autolimits -text "Auto Scale" \
                -variable ${gpsc}(autolimits) \
                -onvalue 1 -offvalue 0 \
		-command "$this UpdateState autolimits"
	grid $lf.autolimits -columnspan 6 -sticky sw -row $rowcnt

	label $lf.xmin -text "X Min:"
        Ow_EntryBox New aval $lf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 6 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(xmin) \
		-valuetype float -valuejustify right
	lappend dep(autolimits) $lf.xmin $aval
	label $lf.xmax -text "X Max:"
        Ow_EntryBox New bval $lf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 6 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(xmax) \
		-valuetype float -valuejustify right
	lappend dep(autolimits) $lf.xmax $bval
	grid    x $lf.xmin [$aval Cget -winpath] \
		x $lf.xmax [$bval Cget -winpath]
	grid $lf.xmin $lf.xmax -sticky e
	grid [$aval Cget -winpath] [$bval Cget -winpath] -sticky ew

	label $lf.ymin -text "Y1 Min:"
        Ow_EntryBox New aval $lf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 6 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(ymin) \
		-valuetype float -valuejustify right
	lappend dep(autolimits) $lf.ymin $aval

	label $lf.ymax -text "Y1 Max:"
        Ow_EntryBox New bval $lf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 6 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(ymax) \
		-valuetype float -valuejustify right
	lappend dep(autolimits) $lf.ymax $bval
	grid    x $lf.ymin [$aval Cget -winpath] \
		x $lf.ymax [$bval Cget -winpath]
	grid $lf.ymin $lf.ymax -sticky e
	grid [$aval Cget -winpath] [$bval Cget -winpath] -sticky ew

	label $lf.y2min -text "Y2 Min:"
        Ow_EntryBox New aval $lf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 6 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(y2min) \
		-valuetype float -valuejustify right
	lappend dep(autolimits) $lf.y2min $aval

	label $lf.y2max -text "Y2 Max:"
        Ow_EntryBox New bval $lf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 6 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(y2max) \
		-valuetype float -valuejustify right
	lappend dep(autolimits) $lf.y2max $bval
	grid    x $lf.y2min [$aval Cget -winpath] \
		x $lf.y2max [$bval Cget -winpath]
	grid $lf.y2min $lf.y2max -sticky e
	grid [$aval Cget -winpath] [$bval Cget -winpath] -sticky ew

	$this UpdateState autolimits

	# Spacer
	set rowcnt [lindex [grid size $lf] 1]
	grid rowconfigure $lf $rowcnt -minsize 12
        lappend spacer_rows $rowcnt
	incr rowcnt

        # Auto offsets
        checkbutton $lf.auto_offset_y -text "Auto Offset Y1" \
                -variable ${gpsc}(auto_offset_y) \
                -onvalue 1 -offvalue 0
        checkbutton $lf.auto_offset_y2 -text "Auto Offset Y2" \
                -variable ${gpsc}(auto_offset_y2) \
                -onvalue 1 -offvalue 0
        grid $lf.auto_offset_y - - $lf.auto_offset_y2 - - -row $rowcnt
	incr rowcnt

	# Spacer
	set rowcnt [lindex [grid size $lf] 1]
	grid rowconfigure $lf $rowcnt -minsize 10
        lappend spacer_rows $rowcnt
	incr rowcnt

        # Logarithmic axes
        set logf [frame $lf.logf]
	label $logf.loglab -text "Log scale axes:"
        checkbutton $logf.log_x -text "X" \
                -variable ${gpsc}(xlogscale) \
                -onvalue 1 -offvalue 0
        checkbutton $logf.log_y -text "Y1" \
                -variable ${gpsc}(ylogscale) \
                -onvalue 1 -offvalue 0
        checkbutton $logf.log_y2 -text "Y2" \
                -variable ${gpsc}(y2logscale) \
                -onvalue 1 -offvalue 0
        pack $logf.loglab $logf.log_x $logf.log_y $logf.log_y2 \
           -side left
        grid $logf -columnspan 6 -row $rowcnt
	incr rowcnt

	# Pack lf frame
        grid columnconfigure $lf 0 -weight 0 -minsize 10
        grid columnconfigure $lf 1 -weight 0
        grid columnconfigure $lf 2 -weight 1
        grid columnconfigure $lf 3 -weight 0 -minsize 15
        grid columnconfigure $lf 4 -weight 0
        grid columnconfigure $lf 5 -weight 1
        grid rowconfigure $lf all -weight 0
        grid rowconfigure $lf $spacer_rows -weight 1
        
        ################# Right side ################################
	# Margins
        set spacer_rows {}
	label $rf.marginrequests -text "Margin Requests"
	grid $rf.marginrequests -sticky sw -columnspan 6
	label $rf.left -text "Left:"
	Ow_EntryBox New lval $rf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 5 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(lmargin_min) \
		-valuetype posint -valuejustify right
	label $rf.right -text "Right:"
	Ow_EntryBox New rval $rf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 5 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(rmargin_min) \
		-valuetype posint -valuejustify right
	grid    x $rf.left [$lval Cget -winpath] \
		x $rf.right [$rval Cget -winpath]
	grid $rf.left $rf.right -sticky e
	grid [$lval Cget -winpath] [$rval Cget -winpath] -sticky ew
	label $rf.top -text "Top:"
	Ow_EntryBox New tval $rf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 5 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(tmargin_min) \
		-valuetype posint -valuejustify right
	label $rf.bottom -text "Bottom:"
	Ow_EntryBox New bval $rf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 5 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(bmargin_min) \
		-valuetype posint -valuejustify right
	grid    x $rf.top [$tval Cget -winpath] \
		x $rf.bottom [$bval Cget -winpath]
	grid $rf.top $rf.bottom -sticky e
	grid [$tval Cget -winpath] [$bval Cget -winpath] -sticky ew

	# Spacer
	set rowcnt [lindex [grid size $rf] 1]
	grid rowconfigure $rf $rowcnt -minsize 10
        lappend spacer_rows $rowcnt
	incr rowcnt

        # Color selections
        set cf [frame $rf.cf]

        # Canvas color
        label $cf.cclab -text "Canvas color:"
        if {[Ow_GetShade $psc(canvas_color)]<128} {
           # Dialog only supports two colors, white and dark green.
           # Normalize input color to one or the other.
           set psc(canvas_color) #004020  ;# dark green
        } else {
           set psc(canvas_color) #FFFFFF  ;# white
        }
        radiobutton $cf.ccb1 -text "white" \
           -value "#FFFFFF" -variable ${gpsc}(canvas_color)
        radiobutton $cf.ccb2 -text "dark green" \
           -value "#004020" -variable ${gpsc}(canvas_color)
        grid configure $cf.cclab -row 0 -sticky e
        grid configure x $cf.ccb1 $cf.ccb2 -row 0 -sticky w

        # Curve color selection
        label $cf.cslab -text "Color selection:"
        radiobutton $cf.csb1 -text "curve" \
           -value "curve" -variable ${gpsc}(color_selection)
        radiobutton $cf.csb2 -text "segment" \
           -value "segment" -variable ${gpsc}(color_selection)
        grid configure $cf.cslab -row 1 -sticky e
        grid configure x $cf.csb1 $cf.csb2 -row 1 -sticky w

	grid $cf -columnspan 6 -row $rowcnt

        
	# Spacer
	set rowcnt [lindex [grid size $rf] 1]
	grid rowconfigure $rf $rowcnt -minsize 10
        lappend spacer_rows $rowcnt
	incr rowcnt

        # Curve width and point buffer size (miscellaneous frame)
        set mf [frame $rf.mf]
        Ow_EntryBox New cw $mf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {Curve Width:} -valuewidth 5 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(curve_width) \
                -valuetype posint -valuejustify right
        Ow_EntryBox New sf $mf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {Symbol Freq:} -valuewidth 5 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(symbol_freq) \
                -valuetype posint -valuejustify right
        Ow_EntryBox New ss $mf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {Symbol Size:} -valuewidth 5 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(symbol_size) \
                -valuetype posint -valuejustify right
        Ow_EntryBox New pbs $mf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {Point Buffer Size:} -valuewidth 5 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(ptbufsize) \
                -valuetype posint -valuejustify right
        grid [$cw Cget -winpath]   -sticky e
        grid [$sf Cget -winpath]   -sticky e
        grid [$ss Cget -winpath]   -sticky e
        grid [$pbs Cget -winpath]  -sticky e

	grid $mf -columnspan 6 -row $rowcnt

        grid columnconfigure $rf 0 -weight 0 -minsize 10
        grid columnconfigure $rf 1 -weight 0
        grid columnconfigure $rf 2 -weight 1
        grid columnconfigure $rf 3 -weight 0 -minsize 15
        grid columnconfigure $rf 4 -weight 0
        grid columnconfigure $rf 5 -weight 1
        grid rowconfigure $rf all -weight 0
        grid rowconfigure $rf $spacer_rows -weight 1

        
        # Control buttons
	set ctrlframe [frame $winpath.ctrlframe]

        set close [ttk::button $ctrlframe.close -text "Close" \
                -command "$this Action close" ]
        set apply [ttk::button $ctrlframe.apply -text "Apply" \
                -command "$this Action apply" ]
        set ok [ttk::button $ctrlframe.ok -text "OK" \
                -command "$this Action ok" ]
        pack $close $apply $ok -side left -expand 1 -padx 20

	pack $ctrlframe -fill x -expand 0 -padx 10 -pady 10

        wm protocol $winpath WM_DELETE_WINDOW "$close invoke"
	Ow_SetIcon $winpath
    }

    # Update from external source
    callback method ExternalUpdate { globalarrname name elt ops } {
        ## The trace is setup to pass in the global array name so that
        ## even if the trace is triggered by a call to Tcl_SetVar
        ## in C code called from a proc from which the traced array
        ## is not visible (due to no 'global' statement on that array),
        ## this trace can still function.        
        if {[string match {} $elt]} {return}
        upvar #0 $globalarrname source
        if {[info exists psc($elt)] && \
                ![string match $psc($elt) $source($elt)]} {
            set psc($elt) $source($elt)
        }
    }

    # Plot type status change helper function
    callback method UpdateState { type } {
	# Note: "type" here should be either autolabel
	#  or autolimits.  In either case, if the "auto"
	#  button is on, then the dependency widgets
	#  should be _disabled_.
        if {$psc($type)} {
            set mystate disabled
        } else {
            set mystate normal
        }
        foreach win $dep($type) {
	    if {[catch {$win configure -state $mystate}]} {
		# Label widgets don't support -state option
		# before Tcl/Tk 8.3.2
		if {[string match disabled $mystate]} {
		    $win configure -foreground $disabledcolor
		} else {
		    $win configure -foreground $normalcolor
		}
	    }
        }
    }

    # Dialog action routine
    callback method Action { cmd } {
        switch $cmd {
            close {
                $this Delete
                return
            }
            apply {
                eval $apply_callback psc
            }
            ok {
                eval $apply_callback psc
                $this Delete
                return
            }
            default {
                error "Illegal action code: $cmd"
            }
        }
    }

    # Destructor methods
    method WinDestroy { w } { winpath } {
	if {[string compare $winpath $w]!=0} { return } ;# Child destroy event
	$this Delete
    }
    Destructor {
	# Reset associated menu item.  Don't report error, if any
	# because if this is called from the main app destructor,
	# then it is possible that the menu already been destroyed.
        catch { eval $menudeletecleanup }

        # Remove external update trace, if any
        if {[catch {
            if {![string match {} $import_arrname]} {
                upvar #0 $import_arrname userinit
                trace vdelete userinit w "$this ExternalUpdate $import_arrname"
            }
        } errmsg]} {
            Oc_Log Log $errmsg error
        }
        # Destroy windows
	if {[catch { 
            if {[info exists winpath] && [winfo exists $winpath]} {
                # Remove <Destroy> binding
                bind $winpath <Destroy> {}
                # Destroy toplevel, all children, and all bindings
                destroy $winpath
            }
        } errmsg]} {
            Oc_Log Log $errmsg error
        }
    }
}
