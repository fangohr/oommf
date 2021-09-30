# FILE: printdialog.tcl
#
# Tcl/Tk script for print dialog box
#
# Last modified on: $Date: 2010/06/25 00:04:47 $
# Last modified by: $Author: donahue $
#

############################## Print Dialog ############################
# Local configuration results are stored in array prn
Oc_Class Ow_PrintDlg {
    const public variable dialog_title = "Print"
    const public variable winpath

    # Import trace and initialization array.  Must be global.
    const public variable import_arrname = {}

    # Routine in mainline to call on an Apply or OK event.
    public variable apply_callback = Oc_Nop 

    # If menu_data is a two item list, consisting of menuname and
    # itemlabel, then this dialog box will automatically take care
    # of disabling itemlabel at construction, and re-enable itemlabel
    # on destruction.  If smart_menu is true, then rather than
    # disabling itemlabel, it gets recolored and redefined to raise
    # the existing dialog box.  On destruction, itemlabel is reset
    # to its previous definition.
    const public variable smart_menu = 1
    const public variable smart_menu_color = red
    const public variable menu_data = {}
    const private variable menu_cleanup = Oc_Nop

    # Internal state array
    private array variable prn

    # Crop and print ratio options
    const public variable crop_option = 1

    # Variable to keep track of current units, for update purposes
    private variable current_units

    public proc Print { userwidget canvas parr } {
        upvar $parr popt
        if {![info exists popt]} {
            error "Import config array $parr does not exist"
        }
        set errcode 0

        # File overwrite check
        set outname [string trim $popt(outdev)]
        if { "|" != [string index $outname 0] \
                && [file exists $outname] } {
            # File is not a pipe, and already exists.
            # Check with user before overwriting.
            set response [Ow_Dialog 1 "[Oc_Main GetAppName]: Print Warning" \
                    warning "Overwrite existing file $outname?" \
                    {} "1" "Yes" "No"]
            if { $response != 0 } {
                # First (0'th) button (which is "Yes") _not_ selected
                set errcode 1
                return $errcode
            }
        }

        # Calculate positioning (in cm)
        switch -exact -- [string tolower $popt(paper)] {
            # Set paper dimensions
            letter { set xmax 21.59; set ymax 27.94 }
            legal  { set xmax 21.59; set ymax 35.56 }
            a4     { set xmax 21.;   set ymax 29.7 }
            a3     { set xmax 29.7;  set ymax 42.  }
            default { Ow_FatalError "Illegal papertype: $popt(paper)" \
                    "$class Error" }
        }
        set xmargin $popt(lmargin)
        set ymargin $popt(tmargin)
        if {[string match "in" $popt(units)]} {
            set xmargin [expr $xmargin*2.54]
            set ymargin [expr $ymargin*2.54]
            set psize "-pagewidth $popt(pwidth)i -pageheight $popt(pheight)i"
        } else {
            set psize "-pagewidth $popt(pwidth)c -pageheight $popt(pheight)c"
        }

        if {[string compare "landscape" [string tolower $popt(orient)]]==0} {
            # Rotation notes: If $rotate==1, then the 'page' coordinate system
            # origin is moved to the upper lefthand corner of the page, with
            # +x running down the page and +y running to the right.  If you
            # adopt this reference frame, then you can ignore the rotation
            # and just pretend you are working from the start on a landscape'd
            # page.  The man page and online documentation
            # http://www.sunlabs.com
	    #        /tcl/development/man/TkCmd/canvas.n.html#M55
            # says "in rotated output the x-axis runs along the long dimension
            # of the page (``landscape'' orientation)."  This is true for
            # the 'canvas' coordinate system, but not true for the 'page'
            # coordinate system (adjusted with the -pagex & -pagey options).
            set rotate 1
            set temp $xmargin ; set xmargin $ymargin; set ymargin $temp
        } else { set rotate 0 }
        # The following page offsets are used in conjuction
        # with the '$canvas postscript -anchor' option
        set anchor {}
        if {[string match "top" $popt(vpos)]} {
            # Position at top of page, minus margin
            if {!$rotate} {
                set yoff [expr $ymax-$ymargin]
            } else {
                set xoff $xmargin
            }
            set anchor "n"
        } elseif {[string match "bottom" $popt(vpos)]} {
            # Position along page bottom margin
            if {!$rotate} {
                set yoff $ymargin
            } else {
                set xoff [expr $xmax-$xmargin]
            }
            set anchor "s"
        } else {
            # Top/bottom center
            if {!$rotate} {
                set yoff [expr $ymax/2.]
            } else {
                set xoff [expr $xmax/2.]
            }
        }
        if {[string match "right" $popt(hpos)]} {
            # Position page right, minus margin
            if {!$rotate} {
                set xoff [expr $xmax-$xmargin]
            } else {
                set yoff [expr $ymax-$ymargin]
            }
            append anchor "e"
        } elseif {[string match "left" $popt(hpos)]} {
            # Position page left, plus margin
            if {!$rotate} {
                set xoff $xmargin
            } else {
                set yoff $ymargin
            }
            append anchor "w"
        } else {
            # Left/right center
            if {!$rotate} {
                set xoff [expr $xmax/2.]
            } else {
                set yoff [expr $ymax/2.]
            }
        }
        if {[string match {} $anchor]} {
            set anchor "c"
        }
        # Setup crop options
        set cropoptions {}
        set sr [$canvas cget -scrollregion]
        if {!$popt(croptoview) && ![string match {} $sr]} {
            # Don't crop; print whole canvas
	    set x [lindex $sr 0]  ; set y [lindex $sr 1]
            set width [expr [lindex $sr 2] - $x]
            set height [expr [lindex $sr 3] - $y]
            set cropoptions "-x $x -y $y -width $width -height $height"
        }

        # Print
        set output {}
        set errmsg {}
        if { [catch {
            set output [open $outname w]
            puts -nonewline $output \
                    [eval $canvas postscript -colormode color \
                    $psize -pageanchor $anchor \
                    -pagex ${xoff}c -pagey ${yoff}c -rotate $rotate \
                    $cropoptions ]
        } errmsg] } {
            Ow_NonFatalError "Error printing to $outname: $errmsg" \
                    "[Oc_Main GetAppName]: Print Error"
            set errcode 2
        }
        catch {
            if { ![string match {} $output] } { close $output }
        }
        return $errcode
    }
    Constructor {args} {
        # Create a (presumably) unique name for the toplevel
        # window based on the instance name.
        set winpath ".[$this WinBasename]"

	# Process user preferences
	eval $this Configure $args

        # Create dialog window
        toplevel $winpath
        if {[Ow_IsAqua]} {
           # Add some padding to allow space for Aqua window resize
           # control in lower righthand corner
           $winpath configure -bd 4 -relief flat -padx 5 -pady 5
        }
        wm group $winpath .
        set dialogwidth [Ow_SetWindowTitle $winpath $dialog_title]
        set brace [canvas ${winpath}.brace -width $dialogwidth \
                    -height 0 -borderwidth 0 -highlightthickness 0]
        pack $brace -side top
        Ow_PositionChild $winpath ;# Position at +.25+/25 over '.'
        focus $winpath

        # Setup destroy binding.
	bind $winpath <Destroy> "$this WinDestroy %W"

	# Bind Ctrl-. shortcut to raise and transfer focus to '.'
	bind $winpath <Control-Key-period> {Ow_RaiseWindow .}

	# If requested, reconfigure menu
	if {[llength $menu_data]==2} {
	    set name [lindex $menu_data 0]
	    set item [lindex $menu_data 1]
	    if {$smart_menu} {
		set origcmd [$name entrycget $item -command]
		set menuorigfgcolor [$name entrycget $item -foreground]
		set menuorigafgcolor [$name entrycget $item -activeforeground]
		set menu_cleanup \
			[list $name entryconfigure $item \
			  -command $origcmd \
			  -foreground $menuorigfgcolor\
			  -activeforeground $menuorigafgcolor]
		$name entryconfigure $item \
			-command "Ow_RaiseWindow $winpath" \
			-foreground $smart_menu_color \
			-activeforeground $smart_menu_color
	    } else {
		$name entryconfigure $item -state disabled
		set menu_cleanup \
			[list $name entryconfigure $item -state normal]
	    }
	}

        # Setup internal state array default values
        array set prn {
            outdev   "| lpr"
            orient   "landscape"
            paper    "letter"
            hpos     "center"
            vpos     "center"
            units    "in"
            tmargin  "1.0"
            lmargin  "1.0"
            pwidth   "6.0"
            pheight  "6.0"
            croptoview "1"
            aspect_crop {}
            aspect_nocrop {}
        }
	global tcl_platform
	if {[string match windows $tcl_platform(platform)]} {
	    # This is a temporary hack to get around the fact
	    # that Windows doesn't support pipelining to lpr.
	    set prn(outdev) "print.ps"
	}

        set gprn [$this GlobalName prn]
        ## Global name of prn array.  This is needed to tie
        ## elements of the array to Tk buttons.

        # Allow override from user initialization array
        if {![string match {} $import_arrname]} {
            upvar #0 $import_arrname userinit
            if {[info exists userinit]} {
                foreach elt [array names userinit] {
                    if {[info exists prn($elt)]} {
                        set prn($elt) $userinit($elt)
                    }
                }
            }
            trace variable userinit w "$this ExternalUpdate $import_arrname"
        }

        set current_units $prn(units)
        trace variable prn(units) w "$this ChangeUnits"
	trace variable prn(croptoview) w "$this UpdateAspect pwidth"

        # Left frame
        set lframe [frame $winpath.lframe -relief ridge -bd 4]
        pack $lframe -side left -anchor nw \
                -fill x -expand 1 -padx 5 -pady 5

        # Output device
        Ow_EntryBox New outdev $lframe.outdev \
                -label "Print to:" -variable ${gprn}(outdev)
        pack [$outdev Cget -winpath] -side top -fill x -expand 1

        # Orientation
        set oframe [frame $lframe.oframe -relief ridge -bd 2]
        pack $oframe -side top -fill x -expand 1
        set olabel [label $oframe.label -text "Orientation:"]
        set op [radiobutton $oframe.port -text Portrait \
                -value portrait -variable ${gprn}(orient)]
        set ol [radiobutton $oframe.land -text Landscape \
                -value landscape -variable ${gprn}(orient)]
        pack $olabel $op $ol -side left

        # Paper type
        set ptframe [frame $lframe.ptframe -relief ridge -bd 2]
        pack $ptframe -side top -fill x -expand 1
        set ptlabel [label $ptframe.label -text "Paper Type"]
        set ptlet [radiobutton $ptframe.let  -text Letter \
                -value letter -variable ${gprn}(paper)]
        set ptleg [radiobutton $ptframe.leg -text Legal \
                -value legal -variable ${gprn}(paper)]
        set pta4 [radiobutton $ptframe.a4 -text A4 \
                -value a4 -variable ${gprn}(paper)]
        set pta3 [radiobutton $ptframe.a3 -text A3 \
                -value a3 -variable ${gprn}(paper)]
        pack $ptlabel -side top
        pack $ptlet $ptleg $pta4 $pta3 -side left -fill x -expand 1

        # Horizontal position
        set hpframe [frame $lframe.hpframe -relief ridge -bd 2]
        pack $hpframe -side top -fill x -expand 1
        set hplabel [label $hpframe.label -text "Horizontal Position"]
        pack $hplabel -side top
        set hpbtnframe [frame $hpframe.btnframe -relief flat -bd 0]
        pack $hpbtnframe -side top -fill none -expand 0 -padx 4
        set hpleft [radiobutton $hpframe.left -width 7 -anchor w \
                -text Left -value left -variable ${gprn}(hpos)]
        set hpcenter [radiobutton $hpframe.center -width 7 -anchor w \
                -text Center -value center -variable ${gprn}(hpos)]
        set hpright [radiobutton $hpframe.right -width 7 -anchor w \
                -text Right -value right -variable ${gprn}(hpos)]
        pack $hpleft $hpcenter $hpright -side left -in $hpbtnframe

        # Vertical position
        set vpframe [frame $lframe.vpframe -relief ridge -bd 2]
        pack $vpframe -side top -fill x -expand 1
        set vplabel [label $vpframe.label -text "Vertical Position"]
        pack $vplabel -side top
        set vpbtnframe [frame $vpframe.btnframe -relief flat -bd 0]
        pack $vpbtnframe -side top -fill none -expand 0 -padx 4
        set vpleft [radiobutton $vpframe.left -width 7 -anchor w \
                -text Top -value top -variable ${gprn}(vpos)]
        set vpcenter [radiobutton $vpframe.center -width 7 -anchor w \
                -text Center -value center -variable ${gprn}(vpos)]
        set vpright [radiobutton $vpframe.right -width 7 -anchor w \
                -text Bottom -value bottom  -variable ${gprn}(vpos)]
        pack $vpleft $vpcenter $vpright -side left -in $vpbtnframe

        # Upper right frame
        set rframe [frame $winpath.rframe -relief ridge -bd 4]
        pack $rframe -side top -anchor ne \
                -fill x -expand 0 -padx 5 -pady 5

        # Unit selection
        set uframe [frame $rframe.uframe -relief ridge -bd 2]
        pack $uframe -side top -fill x -expand 1
        set ulabel [label $uframe.label -text "Units:"]
        set uin [radiobutton $uframe.uin -text "in" \
                -value "in" -variable ${gprn}(units)]
        set ucm [radiobutton $uframe.ucm -text "cm" \
                -value "cm" -variable ${gprn}(units)]
        pack $ulabel $uin $ucm -side left -expand 1

        # Margin sizes
        set mframe [frame $rframe.mframe -relief ridge -bd 2]
        pack $mframe -side top -fill x -expand 1
        Ow_EntryBox New mtop $mframe.mtop \
                -outer_frame_options "-bd 0 -relief flat" \
                -labelwidth 12 -valuewidth 6 -valuetype fixed \
                -label "Margin Top:" -variable ${gprn}(tmargin)
        Ow_EntryBox New mleft $mframe.mleft \
                -outer_frame_options "-bd 0 -relief flat" \
                -labelwidth 7 -valuewidth 6 -valuetype fixed \
                -label "Left:" -variable ${gprn}(lmargin)
        pack [$mtop Cget -winpath] [$mleft Cget -winpath] \
                -side left -expand 1

        # Print size
        set sframe [frame $rframe.sframe -relief ridge -bd 2]
        pack $sframe -side top -fill x -expand 1
        Ow_EntryBox New pwidth $sframe.pwidth \
                -outer_frame_options "-bd 0 -relief flat" \
                -labelwidth 12 -valuewidth 6 \
                -label "Print Width:" -variable ${gprn}(pwidth) \
		-valuetype posfixed
        Ow_EntryBox New pheight $sframe.pheight \
                -outer_frame_options "-bd 0 -relief flat" \
                -labelwidth 7 -valuewidth 6  \
                -label "Height:" -variable ${gprn}(pheight) \
		-valuetype posfixed
	$this AspectTrace 1 ;# Enable tracing between pwidth and pheight
        pack [$pwidth Cget -winpath] [$pheight Cget -winpath] \
                -side left -expand 1

        # Crop selection
        if {$crop_option} {
            set cframe [frame $rframe.cframe -relief ridge -bd 2]
            pack $cframe -side top -fill x -expand 1
            set crop [checkbutton $cframe.crop -text "Crop to view" \
                    -variable ${gprn}(croptoview)]
            pack $crop -side top -expand 1
        }

	# Enforce initial print aspect ratio
	$this UpdateAspect pwidth

        # Control buttons
        set ctrlframe [frame $winpath.ctrlframe -relief ridge -bd 4]
        pack $ctrlframe -side bottom -anchor se \
                -fill x -expand 0 -padx 5 -pady 5
        set close [button $ctrlframe.close -text "Close" -width 8 \
                -command "$this Action close"]
        set apply [button $ctrlframe.apply -text "Apply" -width 8 \
                -command "$this Action apply"]
        set ok [button $ctrlframe.ok -text "OK" -width 8 \
                -command "$this Action ok"]
        pack $close $apply $ok -side left -fill both -expand 1 \
                -padx 2 -pady 1
        wm protocol $winpath WM_DELETE_WINDOW "$close invoke"
    }

    # Routine to turn aspect tracing on and off
    private method AspectTrace { tracestate } { prn } {
	if {$tracestate} {
	    trace variable prn(pwidth)  w "$this UpdateAspect pwidth"
	    trace variable prn(pheight) w "$this UpdateAspect pheight"
	} else {
	    trace vdelete prn(pwidth)  w "$this UpdateAspect pwidth"
	    trace vdelete prn(pheight) w "$this UpdateAspect pheight"
	}
    }

    # Print width/height callback
    callback method UpdateAspect { elt args } {
	set value $prn($elt)
	if {[Ow_IsPosFixedPoint $value]==0} {
	    # New value is a complete number
	    if {$prn(croptoview)} { set aspect $prn(aspect_crop) } \
		    else          { set aspect $prn(aspect_nocrop) }
	    if {![string match {} $aspect] && $aspect>0} {
		$this AspectTrace 0
		set aspect [expr double($aspect)]
		switch -- $elt {
		    pwidth {
			# pwidth just set; adjust pheight
			set newvalue \
				[expr round(1000.*$value/$aspect)/1000.]
			if {$newvalue!=$prn(pheight)} {
			    set prn(pheight) $newvalue
			}
		    }
		    pheight {
			# pheight just set; adjust pwidth
			set newvalue \
				[expr round(1000.*$value*$aspect)/1000.]
			if {$newvalue!=$prn(pwidth)} {
			    set prn(pwidth) $newvalue
			}
		    }
		}
		$this AspectTrace 1
	    }
	}
    }

    # Unit change event
    callback method ChangeUnits { args } {
        set mult 1.0
        switch $current_units {
            in {
                if { [string match cm $prn(units)] } {
                    set mult 2.54
                }
            }
            cm {
                if { [string match in $prn(units)] } {
                    set mult [expr 1./2.54]
                }
            }
        }
        set current_units $prn(units)

	$this AspectTrace 0
        foreach elt {tmargin lmargin pwidth pheight} {
            catch {set prn($elt) \
		    [expr round(1000.*$prn($elt)*$mult)/1000.]}
            ## The catch protects against incomplete or ill formed
            ## numeric fields in prn()
        }
	$this AspectTrace 1
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
        if {[info exists prn($elt)] && \
                [string compare $prn($elt) $source($elt)]!=0} {
            set prn($elt) $source($elt)
	    switch -- $elt {
		aspect_crop {
		    if {$prn(croptoview)} {
			$this UpdateAspect pwidth
		    }
		}
		aspect_nocrop {
		    if {!$prn(croptoview)} {
			$this UpdateAspect pwidth
		    }
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
                eval $apply_callback $this prn
            }
            ok {
                eval $apply_callback $this prn
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
        # Re-enable launching menu
	if {[llength $menu_data]==2} {
	    set menuwin [lindex $menu_data 0]
	    if {[winfo exists $menuwin]} {
		# If main window is going down, menu might already
		# be destroyed.
		if {[catch { eval $menu_cleanup } errmsg]} {
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
