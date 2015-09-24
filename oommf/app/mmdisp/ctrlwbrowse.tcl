# FILE: ctrlwbrowse.tcl
#
# Control box sub-widget with browse option
#
# Callback ACTIONID's: RESCAN, SELECT, CLOSE
#   Note: It is the resposibility of the client to check
# the value of -browse_state on receipt of a SELECT callback,
# and perform a widget delete operation as necessary.

Oc_Class CtrlWBrowse {
    const public variable winpath
    const public variable outer_frame_options = "-bd 4 -relief ridge"
    public variable browse_state = 0 { $this BrowseStateTrace }
    public variable callback = Oc_Nop
    Constructor {basewinpath args} {
	if {![string match {.} \
		[string index $basewinpath \
		[expr [string length $basewinpath]-1]]]} {
	    append basewinpath {.}
	}
	eval $this Configure -winpath ${basewinpath}w$this $args

	# Temporarily disable callbacks until construction is complete
	set callback_keep $callback ;	set callback Oc_Nop

	# Pack entire subwidget into a subframe.
	eval frame $winpath $outer_frame_options

	# Sub-frames for alignment
	for {set j 0} {$j<2} {incr j} {
	    set tempcol [frame $winpath.fcol${j} -relief flat -bd 0]
	    for {set i 0} {$i<2} {incr i} {
		set temp $winpath.f${i}${j}
		frame $temp -relief flat -bd 0
		pack $temp -side top -expand 0 -fill both \
			-in $tempcol
	    }
	    pack $tempcol -side left -expand 1 -fill both \
		    -in $winpath
	}

	# Buttons
	checkbutton $winpath.b00 -text "Browse " \
		-relief raised -anchor w \
		-variable [$this GlobalName browse_state] \
		-command "$this BrowseStateTrace"
	pack configure $winpath.f00 -expand 1
	button $winpath.b10 -text "Rescan" \
		-command "$this UserRequest RESCAN"
	button $winpath.b01 -text "OK" \
		-command "$this UserRequest OK"
	button $winpath.b11 -text "Close" \
		-command "$this UserRequest CLOSE"

	# Setup .b11's text to depend on browse state
	$winpath.b11 configure -width 7
	$this BrowseStateTrace

	# Pack buttons into frames
	for {set i 0} {$i<2} {incr i} {
	    for {set j 0} {$j<2} {incr j} {
		pack $winpath.b${i}${j} -expand 1 -fill both \
			-in $winpath.f${i}${j}
	    }
	}

	# Widget bindings
	bind $winpath <Destroy> "$this WinDestroy %W"

	# Re-enable callbacks
	set callback $callback_keep
    }
    method UserRequest { request } { callback browse_state } {
	switch -exact -- $request {
	    RESCAN { eval $callback $this RESCAN }
	    OK     { eval $callback $this SELECT }
	    CLOSE { eval $callback $this CLOSE }
	    default {}
	}
    }
    method WinDestroy { w } { winpath } {
	if {[info exists winpath] && [string compare $winpath $w]!=0} {
	    return    ;# Child destroy event
	}
	$this Delete
    }
    method BrowseStateTrace {} { browse_state winpath } {
	if {![info exists winpath]              \
		|| ![winfo exists $winpath.b01] } {
	    return
	}
	if {$browse_state} {
	    $winpath.b01 configure -text Apply
	} else {
	    $winpath.b01 configure -text OK
	}
    }
    Destructor {
	if {[info exists browse_state]} {
	    trace vdelete [$this GlobalName browse_state] w \
		    "$this BrowseStateTrace"
	}
	if {[info exists winpath] && [winfo exists $winpath]} {
            # Remove <Destroy> binding
	    bind $winpath <Destroy> {}
	    # Destroy instance frame, all children, and bindings
	    destroy $winpath
	}
    }
}
