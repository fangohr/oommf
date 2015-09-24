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
	pack [$_ Cget -winpath] -fill x -expand 1 -padx 10 -pady 10

	# Frame for interior grid
	set gf [frame $winpath.gf]
	
	# Labels
	set dep(autolabel) {}
        checkbutton $gf.autolabel -text "Auto Label" \
                -variable ${gpsc}(autolabel) \
                -onvalue 1 -offvalue 0 \
		-command "$this UpdateState autolabel"
	grid $gf.autolabel -columnspan 6 -sticky sw

	label $gf.xlabel -text "X Label:"
        Ow_EntryBox New _ $gf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 10 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(xlabel)
	lappend dep(autolabel) $gf.xlabel $_
	grid x $gf.xlabel [$_ Cget -winpath] - - -
	grid $gf.xlabel -sticky e
	grid [$_ Cget -winpath] -sticky ew

	label $gf.ylabel -text "Y1 Label:"
        Ow_EntryBox New _ $gf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 10 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(ylabel)
	lappend dep(autolabel) $gf.ylabel $_
	grid x $gf.ylabel [$_ Cget -winpath] - - -
	grid $gf.ylabel -sticky w
	grid [$_ Cget -winpath] -sticky ew
        $this UpdateState autolabel

	label $gf.y2label -text "Y2 Label:"
        Ow_EntryBox New _ $gf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 10 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(y2label)
	lappend dep(autolabel) $gf.y2label $_
	grid x $gf.y2label [$_ Cget -winpath] - - -
	grid $gf.y2label -sticky w
	grid [$_ Cget -winpath] -sticky ew
        $this UpdateState autolabel

	# Spacer
	set rowcnt [lindex [grid size $gf] 1]
	grid rowconfigure $gf $rowcnt -minsize 10
	incr rowcnt

	# Scaling
	set dep(autolimits) {}
        checkbutton $gf.autolimits -text "Auto Scale" \
                -variable ${gpsc}(autolimits) \
                -onvalue 1 -offvalue 0 \
		-command "$this UpdateState autolimits"
	grid $gf.autolimits -columnspan 6 -sticky sw -row $rowcnt

	label $gf.xmin -text "X Min:"
        Ow_EntryBox New aval $gf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 6 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(xmin) \
		-valuetype float -valuejustify right
	lappend dep(autolimits) $gf.xmin $aval
	label $gf.xmax -text "X Max:"
        Ow_EntryBox New bval $gf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 6 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(xmax) \
		-valuetype float -valuejustify right
	lappend dep(autolimits) $gf.xmax $bval
	grid    x $gf.xmin [$aval Cget -winpath] \
		x $gf.xmax [$bval Cget -winpath]
	grid $gf.xmin $gf.xmax -sticky e
	grid [$aval Cget -winpath] [$bval Cget -winpath] -sticky ew

	label $gf.ymin -text "Y1 Min:"
        Ow_EntryBox New aval $gf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 6 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(ymin) \
		-valuetype float -valuejustify right
	lappend dep(autolimits) $gf.ymin $aval

	label $gf.ymax -text "Y1 Max:"
        Ow_EntryBox New bval $gf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 6 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(ymax) \
		-valuetype float -valuejustify right
	lappend dep(autolimits) $gf.ymax $bval
	grid    x $gf.ymin [$aval Cget -winpath] \
		x $gf.ymax [$bval Cget -winpath]
	grid $gf.ymin $gf.ymax -sticky e
	grid [$aval Cget -winpath] [$bval Cget -winpath] -sticky ew

	label $gf.y2min -text "Y2 Min:"
        Ow_EntryBox New aval $gf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 6 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(y2min) \
		-valuetype float -valuejustify right
	lappend dep(autolimits) $gf.y2min $aval

	label $gf.y2max -text "Y2 Max:"
        Ow_EntryBox New bval $gf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 6 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(y2max) \
		-valuetype float -valuejustify right
	lappend dep(autolimits) $gf.y2max $bval
	grid    x $gf.y2min [$aval Cget -winpath] \
		x $gf.y2max [$bval Cget -winpath]
	grid $gf.y2min $gf.y2max -sticky e
	grid [$aval Cget -winpath] [$bval Cget -winpath] -sticky ew

	$this UpdateState autolimits

	# Spacer
	set rowcnt [lindex [grid size $gf] 1]
	grid rowconfigure $gf $rowcnt -minsize 10
	incr rowcnt

        # Auto offsets
        checkbutton $gf.auto_offset_y -text "Auto Offset Y1" \
                -variable ${gpsc}(auto_offset_y) \
                -onvalue 1 -offvalue 0
        checkbutton $gf.auto_offset_y2 -text "Auto Offset Y2" \
                -variable ${gpsc}(auto_offset_y2) \
                -onvalue 1 -offvalue 0
        grid $gf.auto_offset_y - - $gf.auto_offset_y2 - - \
           -sticky w -row $rowcnt

	# Spacer
	set rowcnt [lindex [grid size $gf] 1]
	grid rowconfigure $gf $rowcnt -minsize 10
	incr rowcnt

	# Margins
	label $gf.marginrequests -text "Margin Requests"
	grid $gf.marginrequests -sticky w -columnspan 6 -row $rowcnt
	label $gf.left -text "Left:"
	Ow_EntryBox New lval $gf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 5 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(lmargin_min) \
		-valuetype posint -valuejustify right
	label $gf.right -text "Right:"
	Ow_EntryBox New rval $gf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 5 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(rmargin_min) \
		-valuetype posint -valuejustify right
	grid    x $gf.left [$lval Cget -winpath] \
		x $gf.right [$rval Cget -winpath]
	grid $gf.left $gf.right -sticky e
	grid [$lval Cget -winpath] [$rval Cget -winpath] -sticky w
	label $gf.top -text "Top:"
	Ow_EntryBox New tval $gf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 5 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(tmargin_min) \
		-valuetype posint -valuejustify right
	label $gf.bottom -text "Bottom:"
	Ow_EntryBox New bval $gf \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -valuewidth 5 \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(bmargin_min) \
		-valuetype posint -valuejustify right
	grid    x $gf.top [$tval Cget -winpath] \
		x $gf.bottom [$bval Cget -winpath]
	grid $gf.top $gf.bottom -sticky e
	grid [$tval Cget -winpath] [$bval Cget -winpath] -sticky w

	# Pack gf frame
	grid columnconfigure $gf 0 -minsize 10
	grid columnconfigure $gf 2 -weight 1
	grid columnconfigure $gf 3 -minsize 15
	grid columnconfigure $gf 5 -weight 1
	pack $gf -fill x -expand 1 -padx 10

        # Color selections
        pack [frame $winpath.cfspacer -height 10] -side top
        set cf [frame $winpath.cf]

        # Canvas color
        label $cf.cclab -text "Canvas color:"
        radiobutton $cf.ccb1 -text "white" \
           -value "white" -variable ${gpsc}(canvas_color)
        radiobutton $cf.ccb2 -text "dark green" \
           -value "#042" -variable ${gpsc}(canvas_color)
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

	pack $cf -fill x -expand 1 -padx 10 -pady 2

        # Curve width and point buffer size (miscellaneous frame)
        set mf [frame $winpath.mf]
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
        pack [$pbs Cget -winpath] \
           -fill none -expand 0 -anchor e -padx 10 -pady 8 -side bottom
        pack [$ss Cget -winpath] \
           -fill none -expand 0 -anchor e -padx 10 -side bottom
        pack [$sf Cget -winpath] \
           -fill none -expand 0 -anchor e -padx 10 -side bottom
        pack [$cw Cget -winpath] \
           -fill none -expand 0 -anchor e -padx 10 -side bottom
        pack $mf -anchor w -ipady 5

        # Control buttons
	set ctrlframe [frame $winpath.ctrlframe]

        set close [button $ctrlframe.close -text "Close" \
                -command "$this Action close" ]
        set apply [button $ctrlframe.apply -text "Apply" \
                -command "$this Action apply" ]
        set ok [button $ctrlframe.ok -text "OK" \
                -command "$this Action ok" ]
        pack $close $apply $ok -side left -expand 1 -padx 20

	pack $ctrlframe -fill x -expand 1 -padx 10 -pady 10

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
