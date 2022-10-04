# FILE: plotconfig.tcl
#
# Tcl/Tk script for display configuration
#

###################### Plot Select and Configuration #####################
# 4-June-1998, mjd
#
# Local configuration results are stored in array psc.
Oc_Class Mmd_PlotSelectConfigure {
    const public variable winpath
    const public variable title

    # Import trace and initialization array.  Must be global.
    const public variable import_arrname = {}

    # Routine in mainline to call on an Apply or OK event.
    public variable apply_callback = Oc_Nop 

    # Menu and index (as a 2-item list) in the launching window
    # to toggle off in the constructor and on in the destructor.
    const public variable menu = {}
    const public variable menuraise = 0
    const public variable menucolor = red
    const private variable menudeletecleanup = Oc_Nop

    # Internal state array
    private array variable psc

    # State (normal/disabled) dependency lists
    private array variable state_dep

    # Listbox update lists array
    private array variable listbox_dep

    # Label update lists array;  This is a list of pairs,
    # {widget script}, where "eval $script" should evaluate
    # to the new label value.  The local variable "$_" is
    # set before running $script to the value $psc($elt),
    # where "$elt" is the label_dep index.
    private array variable label_dep

    # Variable tie update lists array;  This is a list of pairs,
    # {widget script}, where "eval $script" should evalute
    # to the new variable that widget should tie to.  The local
    # variable "$_" is set before running $script to the value
    # $psc($elt), where "$elt" is the var_dep index.
    private array variable var_dep

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

	# Bind Ctrl-. shortcut to raise and transfer focus to '.'
	bind $winpath <Control-Key-period> {Ow_RaiseWindow .}

	# Setup handling of associated menu item
	if {[llength $menu]==2} {
	    set menuwin [lindex $menu 0]
	    set menuitem [lindex $menu 1]
	    if {[catch {
		if {$menuraise} {
		    set origcmd \
			    [$menuwin entrycget $menuitem -command]
		    set origfgcolor \
			    [$menuwin entrycget $menuitem -foreground]
		    set origafgcolor \
			    [$menuwin entrycget $menuitem -activeforeground]
		    set menudeletecleanup \
			    [list $menuwin entryconfigure $menuitem \
			    -command $origcmd \
			    -foreground $origfgcolor\
			    -activeforeground $origafgcolor]
		    $menuwin entryconfigure $menuitem \
			    -command "Ow_RaiseWindow $winpath" \
			    -foreground $menucolor \
			    -activeforeground $menucolor
		} else {
		    $menuwin entryconfigure \
			    $menuitem -state disabled
		    set menudeletecleanup [list $menuwin \
			    entryconfigure $menuitem -state normal]
		}
	    } errmsg]} {
		Oc_Log Log $errmsg error
	    }
	}

        # Setup internal state array default values
        array set psc {
	    quantitylist       {}
            arrow,status       1
            arrow,colormap     Red-Black-Blue
            arrow,colorcount   256
            arrow,quantity     z
	    arrow,colorphase   0.
	    arrow,colorreverse 0
            arrow,autosample   1
            arrow,subsample    0
            arrow,viewscale     1
            arrow,size         1
            pixel,status       0
            pixel,colormap     Red-Black-Blue
            pixel,colorcount   256
	    pixel,opaque       1
            pixel,quantity     x
	    pixel,colorphase   0.
	    pixel,colorreverse 0
            pixel,autosample   1
            pixel,subsample    0
            pixel,size         1
            misc,drawboundary  1
            misc,boundarywidth 1
            misc,boundarycolor black
            misc,background    white
            misc,margin        10
	    misc,confine       1
	    misc,zoom          1
	    misc,datascale     1
	    misc,default_datascale 1
	    misc,dataunits     {}
	    misc,spaceunits    {}
	    viewaxis             {+z}
	    viewaxis,center        0
	    viewaxis,xarrowspan    {}
	    viewaxis,xpixelspan    {}
	    viewaxis,yarrowspan    {}
	    viewaxis,ypixelspan    {}
	    viewaxis,zarrowspan    {}
	    viewaxis,zpixelspan    {}
        }
        set gpsc [$this GlobalName psc]
        ## Global name of psc array.  This is needed to tie
        ## elements of this array to Tk buttons.

	set listbox_dep(colormaps) {}
	set listbox_dep(quantitylist) {}

        # Allow override from user initialization array
        if {![string match {} $import_arrname]} {
            upvar #0 $import_arrname userinit
            if {[info exists userinit]} {
                foreach elt [array names userinit] {
		    set psc($elt) $userinit($elt)
                }
            }
            trace variable userinit w "$this ExternalUpdate $import_arrname"
        }

	# To simplify the plot configure interface, the single entry
	# "Boundary width" is used to control both "misc,drawboundary"
	# and "misc,boundarywidth".  On entry, if "misc,drawboundary"
	# is false, then set boundarywidth to 0.  On "Apply" or "OK"
	# actions, "misc,drawboundary" is set false iff
	# "misc,boundarywidth" is <=0.
	if {!$psc(misc,drawboundary)} {
	    set psc(misc,boundarywidth) 0
	}

        ########### Create dialog box
        # Grid Cells
        for {set i 0} {$i<3} {incr i} {
            for {set j 0} {$j<6} {incr j} {
                set f [frame $winpath.a$i$j -bd 2 -relief ridge]
                set a$i$j $f
                grid $f -row $i -column $j -sticky news
            }
        }

        # Column labels
        set clbl0 [label $a00.label -text "Plot Type"]
        set clbl1 [label $a01.label -text "Colormap"]
        set clbl2 [label $a02.label -text "# of Colors"]
        set clbl3 [label $a03.label -text "Color Quantity"]
        set clbl4 [label $a04.label -text "Subsample"]
        set clbl5 [label $a05.label -text "Size"]

        pack $clbl0 $clbl1 $clbl2 $clbl3 $clbl4 $clbl5 -fill both

        # Arrow plot configuration
	set state_dep(arrow) {}
        set arrstat [checkbutton $a10.stat -text "Arrow" \
                -variable ${gpsc}(arrow,status) \
                -onvalue 1 -offvalue 0]
        $arrstat configure \
		-command "$this UpdatePlotTypeState arrow"
        pack $arrstat -anchor nw

        set disabledcolor [$arrstat cget -disabledforeground] ;# Used
        ## below in Ow_EntryBox configuration

        Ow_ListBox New arrcm $a11.cm -height 4 \
                -variable ${gpsc}(arrow,colormap) \
                -outer_frame_options "-bd 0" \
		-listbox_options "-bd 2 -relief flat -width 0"
        # Note: At this time (Feb-2006), tk list boxes on
        # Mac OS X/Darwin don't scroll properly if the height
        # is smaller than 4.
        eval $arrcm Insert end [GetDefaultColorMapList]
        pack $a11.cm -anchor ne -fill both -expand 1
        lappend state_dep(arrow) $arrcm
	lappend listbox_dep(colormaps) $arrcm

	Ow_EntryBox New arrcc $a12.cc -valuetype posint \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {} -labelwidth -1 \
		-valuewidth 5 -valuejustify right \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(arrow,colorcount)
        lappend state_dep(arrow) $arrcc

        set arrinvert [checkbutton $a12.aci -text "Reverse" \
		-borderwidth 2 -pady 0 \
		-variable ${gpsc}(arrow,colorreverse) \
		-onvalue 1 -offvalue 0]
        lappend state_dep(arrow) $arrinvert

        pack [$arrcc Cget -winpath] -anchor n -fill x -padx 5
        pack  $arrinvert -anchor n -fill both -expand 1 -padx 5

        Ow_ListBox New arrqt $a13.qt -height 4 \
                -variable ${gpsc}(arrow,quantity) \
                -outer_frame_options "-bd 0" \
		-listbox_options "-bd 2 -relief flat -width 0"
        # Note: At this time (Feb-2006), tk list boxes on
        # Mac OS X/Darwin don't scroll properly if the height
        # is smaller than 4.
        eval $arrqt Insert end $psc(quantitylist)
        pack $a13.qt -anchor ne -fill both -expand 1
        lappend state_dep(arrow) $arrqt
	lappend listbox_dep(quantitylist) $arrqt

        Ow_EntryBox New arrsr $a14.sr -valuetype fixed \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {}  -labelwidth -1 \
		-valuewidth 5 -valuejustify right \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(arrow,subsample)
	trace variable psc(arrow,status) w \
		"$this UpdateSubsampleState arrow $arrsr"
	trace variable psc(arrow,autosample) w \
		"$this UpdateSubsampleState arrow $arrsr"

        set arrautosample [checkbutton $a14.as -text "Auto" \
		-borderwidth 2 -pady 0 \
		-variable ${gpsc}(arrow,autosample) \
		-onvalue 1 -offvalue 0]
        lappend state_dep(arrow) $arrautosample

        pack [$arrsr Cget -winpath] -anchor n -fill x -padx 5 -pady 5
        pack  $arrautosample -anchor n -fill both -expand 1 -padx 5

        Ow_EntryBox New arrsize $a15.size -valuetype fixed \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {}  -labelwidth -1 \
		-valuewidth 5 -valuejustify right \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(arrow,size)
        lappend state_dep(arrow) $arrsize

        set arrviewscale [checkbutton $a15.as -text "View\nscale" \
                -borderwidth 2 -pady 0 \
                -variable ${gpsc}(arrow,viewscale) \
                -onvalue 1 -offvalue 0]
        lappend state_dep(arrow) $arrviewscale

        pack [$arrsize Cget -winpath] -anchor n -fill x -padx 5 -pady 5
        pack  $arrviewscale -anchor n -fill both -expand 1 -padx 5

        # Initialize arrow type display state
        $this UpdatePlotTypeState arrow
	$this UpdateSubsampleState arrow $arrsr dumA dumB dumC

        # Pixel plot configuration
	set state_dep(pixel) {}
        set pixstat [checkbutton $a20.stat -text "Pixel" \
                -variable ${gpsc}(pixel,status) -onvalue 1 -offvalue 0]
        $pixstat configure \
		-command "$this UpdatePlotTypeState pixel"
        pack $pixstat -anchor nw

        Ow_ListBox New pixcm $a21.cm -height 4 \
                -variable ${gpsc}(pixel,colormap) \
                -outer_frame_options "-bd 0" \
		-listbox_options "-bd 2 -relief flat -width 0"
        eval $pixcm Insert end [GetDefaultColorMapList]
        pack $a21.cm -anchor ne -fill both -expand 1
        lappend state_dep(pixel) $pixcm
	lappend listbox_dep(colormaps) $pixcm

	Ow_EntryBox New pixcc $a22.cc -valuetype posint \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {}  -labelwidth -1 \
		-valuewidth 5 -valuejustify right \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(pixel,colorcount)
        lappend state_dep(pixel) $pixcc

        set pixinvert [checkbutton $a22.pci -text "Reverse" \
		-borderwidth 2 -pady 0 \
		-variable ${gpsc}(pixel,colorreverse) \
		-onvalue 1 -offvalue 0]
        lappend state_dep(pixel) $pixinvert

        set pixopaque [checkbutton $a22.po -text "Opaque" \
		-borderwidth 2 -pady 0 \
                -variable ${gpsc}(pixel,opaque) -onvalue 1 -offvalue 0]
        lappend state_dep(pixel) $pixopaque

        pack [$pixcc Cget -winpath] -anchor n -fill x -padx 5
        pack $pixinvert $pixopaque -anchor n \
		-fill both -expand 1 -padx 5

        Ow_ListBox New pixqt $a23.qt -height 4 \
                -variable ${gpsc}(pixel,quantity) \
                -outer_frame_options "-bd 0" \
		-listbox_options "-bd 2 -relief flat -width 0"
        eval $pixqt Insert end $psc(quantitylist)
        pack $a23.qt -anchor ne -fill both -expand 1
        lappend state_dep(pixel) $pixqt
	lappend listbox_dep(quantitylist) $pixqt

        Ow_EntryBox New pixsr $a24.sr -valuetype fixed \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {}  -labelwidth -1 \
		-valuewidth 5 -valuejustify right \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(pixel,subsample)
	trace variable psc(pixel,status) w \
		"$this UpdateSubsampleState pixel $pixsr"
	trace variable psc(pixel,autosample) w \
		"$this UpdateSubsampleState pixel $pixsr"

        set pixautosample [checkbutton $a24.as -text "Auto" \
		-borderwidth 2 -pady 0 \
		-variable ${gpsc}(pixel,autosample) \
		-onvalue 1 -offvalue 0]
        lappend state_dep(pixel) $pixautosample

        pack [$pixsr Cget -winpath] -anchor n -fill x -padx 5 -pady 5
        pack  $pixautosample -anchor n -fill both -expand 1 -padx 5

        Ow_EntryBox New pixsize $a25.size -valuetype fixed \
                -outer_frame_options "-bd 0 -relief flat" \
                -label {}  -labelwidth -1 \
		-valuewidth 5 -valuejustify right \
                -disabledforeground $disabledcolor \
                -variable ${gpsc}(pixel,size)
        pack [$pixsize Cget -winpath] -anchor n -fill x -padx 5 -pady 5
        lappend state_dep(pixel) $pixsize
    
        # Initialize pixel type display state
        $this UpdatePlotTypeState pixel
	$this UpdateSubsampleState pixel $pixsr dumA dumB dumC

        # Miscellaneous configuration
        set a30 [frame $winpath.a30 -bd 2 -relief ridge]
        grid $a30 -row 3 -column 0 -columnspan 6 -sticky news
        set f30 [frame $a30.f30 -bd 0]
        pack $f30 -side top -fill x -expand 1

	
	set tlabel "Data Scale"
	set tunits $psc(misc,dataunits);
	if {![string match {} $tunits]} {
	    append tlabel " ($tunits)"
	}
	append tlabel ":"
        Ow_EntryBox New datascalebox $f30.datascale -valuetype posfloat \
                -outer_frame_options "-bd 0 -relief flat" \
                -label $tlabel -valuewidth 8 -valuejustify right \
                -variable ${gpsc}(misc,datascale)
	set label_dep(misc,dataunits) [list $datascalebox \
		{if {[string match {} $_]} {format "Data Scale:"} \
		else {format "Data Scale ($_):"}}]
	## Note: This is the only dependency tied to misc,dataunits

        Ow_EntryBox New zoombox $f30.zoom -valuetype posfloat \
                -outer_frame_options "-bd 0 -relief flat" \
                -label "Zoom:" -valuewidth 8 -valuejustify right \
                -variable ${gpsc}(misc,zoom)

        Ow_EntryBox New marginbox $f30.margin -valuetype fixed \
                -outer_frame_options "-bd 0 -relief flat" \
                -label "Margin:" -valuewidth 8 -valuejustify right \
                -variable ${gpsc}(misc,margin)

	set axis [string index $psc(viewaxis) 1]
	set tlabel [string toupper $axis]
	append tlabel "-slice center"
	set tunits $psc(misc,spaceunits);
	if {![string match {} $tunits]} {
	    append tlabel " ($tunits)"
	}
	append tlabel ":"
        Ow_EntryBox New slicecenterbox $f30.zcenter -valuetype float \
                -outer_frame_options "-bd 0 -relief flat" \
                -label $tlabel -valuewidth 8 -valuejustify right \
                -variable ${gpsc}(viewaxis,center)

	set label_dep(viewaxis) [list \
		$slicecenterbox	{
	 	  set axis [string toupper [string index $_ 1]]
	          set units $psc(misc,spaceunits)
	          if {[string match {} $units]} {
		      format "${axis}-slice center:"
		  } else {
		      format "${axis}-slice center ($units):"
		  }
	      }]

	set tlabel "Arrow span"
	if {![string match {} $tunits]} {
	    append tlabel " ($tunits)"
	}
	append tlabel ":"
        Ow_EntryBox New arrowspanbox $f30.arrowspan -valuetype float \
                -outer_frame_options "-bd 0 -relief flat" \
                -label $tlabel -valuewidth 8 -valuejustify right \
                -variable ${gpsc}(viewaxis,${axis}arrowspan)

	set depscript [subst -nocommands {
	    set axis [string index \$_ 1]
            format "${gpsc}(viewaxis,\${axis}arrowspan)"}]
	set var_dep(viewaxis) [list $arrowspanbox $depscript]

	set tlabel "Pixel span"
	if {![string match {} $tunits]} {
	    append tlabel " ($tunits)"
	}
	append tlabel ":"
        Ow_EntryBox New pixelspanbox $f30.pixelspan -valuetype float \
                -outer_frame_options "-bd 0 -relief flat" \
                -label $tlabel -valuewidth 8 -valuejustify right \
                -variable ${gpsc}(viewaxis,${axis}pixelspan)

	set depscript [subst -nocommands {
	    set axis [string index \$_ 1]
            format "${gpsc}(viewaxis,\${axis}pixelspan)"}]
	lappend var_dep(viewaxis) $pixelspanbox $depscript

	set label_dep(misc,spaceunits) [list \
		$slicecenterbox {
	 	  set axis [string toupper [string index $psc(viewaxis) 1]]
	          if {[string match {} $_]} {
		      format "${axis}-slice center:"
		  } else {
		      format "${axis}-slice center ($_):"
		  }
	        } \
		$arrowspanbox \
		{if {[string match {} $_]} {format "Arrow span:"} \
		else {format "Arrow span ($_):"}} \
		$pixelspanbox \
		{if {[string match {} $_]} {format "Pixel span:"} \
		else {format "Pixel span ($_):"}}]

        Ow_EntryBox New bdrywidth $f30.bdrywidth -valuetype posint \
                -outer_frame_options "-bd 0 -relief flat" \
                -label "Boundary width:" -valuewidth 8 \
		-valuejustify right -variable ${gpsc}(misc,boundarywidth)

        Ow_EntryBox New bdrycolorbox $f30.bdrycolor -valuetype text \
                -outer_frame_options "-bd 0 -relief flat" \
                -label "Boundary color:" -valuewidth 8 \
		-valuejustify left -variable ${gpsc}(misc,boundarycolor)

        Ow_EntryBox New bgcolorbox $f30.bgcolor -valuetype text \
                -outer_frame_options "-bd 0 -relief flat" \
                -label "Background color:" -valuewidth 8 \
		-valuejustify left -variable ${gpsc}(misc,background)

	grid [$datascalebox Cget -winpath]    x \
		[$zoombox Cget -winpath]      x \
		[$marginbox Cget -winpath]
	grid [$slicecenterbox Cget -winpath]  x \
		[$arrowspanbox Cget -winpath] x \
		[$pixelspanbox Cget -winpath]
	grid [$bdrywidth Cget -winpath]       x \
		[$bdrycolorbox Cget -winpath] x \
		[$bgcolorbox Cget -winpath]

	grid configure \
		[$datascalebox Cget -winpath]   \
		[$zoombox Cget -winpath]        \
		[$marginbox Cget -winpath]      \
		[$slicecenterbox Cget -winpath] \
		[$arrowspanbox Cget -winpath]   \
		[$pixelspanbox Cget -winpath]   \
		[$bdrywidth Cget -winpath]      \
		[$bdrycolorbox Cget -winpath]   \
		[$bgcolorbox Cget -winpath]     \
		-sticky e

	grid columnconfigure $f30 0 -weight 1
	grid columnconfigure $f30 1 -weight 2
	grid columnconfigure $f30 2 -weight 1
	grid columnconfigure $f30 3 -weight 2
	grid columnconfigure $f30 4 -weight 1

        grid rowconfigure $winpath 3 -pad 10

        # Control buttons
        set a40 [frame $winpath.a40 -bd 0]
        grid $a40 -row 4 -column 0 -columnspan 6 -sticky news
        grid rowconfigure $winpath 4 -pad 10
        set close [button $a40.close -text "Close" \
                -command "$this Action close" ]
        set apply [button $a40.apply -text "Apply" \
                -command "$this Action apply" ]
        set ok [button $a40.ok -text "OK" \
                -command "$this Action ok" ]
        pack $close $apply $ok -side left -expand 1
	grid rowconfigure $winpath {1 2} -weight 1
	grid columnconfigure $winpath {1 3} -weight 1
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
	    # Reset listboxes as necessary
	    if {[info exists listbox_dep($elt)]} {
		foreach lb $listbox_dep($elt) {
		    $lb Remove 0 end
		    eval {$lb Insert end} $psc($elt)		    
		}
	    }
	    # Reset labels as necessary
	    if {[info exists label_dep($elt)]} {
		foreach {widget script} $label_dep($elt) {
		    set _ $psc($elt)
		    $widget Configure -label [eval $script]
		}
	    }
	    # Reset variable ties as necessary
	    if {[info exists var_dep($elt)]} {
		foreach {widget script} $var_dep($elt) {
		    set _ $psc($elt)
		    $widget Configure -variable [eval $script]
		}
	    }
        }
    }

    # Plot type status change helper functions
    callback method UpdatePlotTypeState { plottype } {
        if {$psc($plottype,status)} {
            set mystate normal
        } else {
            set mystate disabled
        }
        foreach win $state_dep($plottype) {
            $win configure -state $mystate
        }
    }

    callback method UpdateSubsampleState { plottype win name elt ops } {
       # plottype should be either "arrow" or "pixel".
       if {$psc($plottype,status) && !$psc($plottype,autosample)} {
	    $win configure -state normal
	} else {
	    $win configure -state disabled
	}
    }

    # Helper function for action routine
    private method CheckPscArray {} {
	# Conversion for simplified boundary interface
	if {$psc(misc,boundarywidth)<=0.0} {
	    set psc(misc,drawboundary) 0
	} else {
	    set psc(misc,drawboundary) 1
	}
	# Valid color checks
	if {[catch {Nb_GetColor $psc(misc,boundarycolor)}]} {
	    return -code error \
		    "Invalid boundary color: \"$psc(misc,boundarycolor)\""
	}
	if {[catch {Nb_GetColor $psc(misc,background)}]} {
	    return -code error \
		    "Invalid background color: \"$psc(misc,background)\""
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
		$this CheckPscArray
                eval $apply_callback psc
            }
            ok {
		$this CheckPscArray
                eval $apply_callback psc
                $this Delete
                return
            }
            default {
                return -code error "Illegal action code: $cmd"
            }
        }
    }

    # Destructor methods
    method WinDestroy { w } { winpath } {
	if {[string compare $winpath $w]!=0} { return } ;# Child destroy event
	$this Delete
    }
    Destructor {

        # Re-enable launching menu
        if {![string match {} $menu]} {
	    set menuwin [lindex $menu 0]
	    if {[winfo exists $menuwin]} {
		# If main window is going down, menu might already
		# be destroyed.
		if {[catch { eval $menudeletecleanup } errmsg]} {
		    Oc_Log Log $errmsg error
		}
	    }
        }

        # Remove external update trace, if any
        if {[catch {
            if {![string match {} $import_arrname]} {
                upvar #0 $import_arrname userinit
                trace vdelete userinit w "$this ExternalUpdate $import_arrname"
            }
        } errmsg]} {
            Oc_Log Log $errmsg error
        }

	# Return focus to main window.  (This will probably
	# fail if $winpath doesn't currently have the focus.)
	Ow_MoveFocus .

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
