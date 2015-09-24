# FILE: omfopts.tcl
#
# Save parameter "OMF" option box for mmDisp File|Save dialog
#
# NOTE: This subwidget does not make callbacks.

Oc_Class OmfOpts {
    const public variable winpath
    const public variable outer_frame_options = "-bd 4 -relief ridge"
    public variable datatype = binary4 ;# One of text binary4 binary8
    private variable btn_rectangular_grid = Oc_Nop;
    public variable gridtype = rectangular
    public variable allowed_gridcode = 1 { 
	$this GridsAllowedUpdate $allowed_gridcode
    } ;# 0 => Irreg grids only, 1 => Irreg & rectangular grids
    public variable callback = Oc_Nop
    Constructor {basewinpath args} {
	if {![string match {.} \
		[string index $basewinpath \
		[expr {[string length $basewinpath]-1}]]]} {
	    append basewinpath {.}
	}
	eval $this Configure -winpath ${basewinpath}w$this $args

	# Temporarily disable callbacks until construction is complete
	set callback_keep $callback  ;  set callback Oc_Nop

	# Pack entire subwidget into a subframe.
	eval frame $winpath $outer_frame_options
	set innerframe [frame $winpath.innerframe -relief raised -bd 2]
	set btnframeA [frame $winpath.btnframeA -relief sunken -bd 2]
	set btnframeB [frame $winpath.btnframeB -relief sunken -bd 2]

	set outerlabel [label $winpath.label -text "OVF File Options" \
		-relief flat -padx 4 -pady 4]

	set btn1 [radiobutton $winpath.btn1 -text "Text" \
		-relief flat -anchor w -value text \
		-variable [$this GlobalName datatype]]
	set btn2 [radiobutton $winpath.btn2 -text "Binary-4" \
		-relief flat -anchor w -value binary4 \
		-variable [$this GlobalName datatype]]
	set btn3 [radiobutton $winpath.btn3 -text "Binary-8" \
		-relief flat -anchor w -value binary8 \
		-variable [$this GlobalName datatype]]
	set btn4 [radiobutton $winpath.btn4 -text "Rectangular grid" \
		-relief flat -anchor w -value rectangular \
		-variable [$this GlobalName gridtype]]
	set btn5 [radiobutton $winpath.btn5 -text "Irregular grid" \
		-relief flat -anchor w -value irregular \
		-variable [$this GlobalName gridtype]]

	set btn_rectangular_grid $btn4
	$this GridsAllowedUpdate $allowed_gridcode

	pack $innerframe
	pack $outerlabel -in $innerframe \
		-side top -expand 1 -fill both
	pack $btnframeA $btnframeB -in $innerframe \
		-side left -expand 1 -fill both
	pack $btn1 $btn2 $btn3 -in $btnframeA \
		-side top -expand 1 -fill both
	pack $btn4 $btn5 -in $btnframeB \
		-side top -expand 1 -fill both

	# Widget bindings
	bind $winpath <Destroy> "$this WinDestroy %W"

	# Re-enable callbacks
	set callback $callback_keep
    }
    method GridsAllowedUpdate { gridcode } \
	    { allowed_gridcode btn_rectangular_grid gridtype } {
	set allowed_gridcode $gridcode
	if { $allowed_gridcode != 0 } {
	    if {[string compare "disabled" \
		    [$btn_rectangular_grid cget -state]]==0} {
		$btn_rectangular_grid configure -state normal
		set gridtype "rectangular"
	    }
	} else {
	    $btn_rectangular_grid configure -state disabled
	    set gridtype "irregular"
	}
    }
    method GetState {} { datatype gridtype } {
	set statelist {}
	foreach i { datatype gridtype } {
	    lappend statelist -$i [set $i]
	}
	return $statelist
    }
    method WinDestroy { w } { winpath } {
	if {[info exists winpath] && [string compare $winpath $w]!=0} {
	    return    ;# Child destroy event
	}
	$this Delete
    }
    Destructor {
	if {[info exists winpath] && [winfo exists $winpath]} {
            # Remove <Destroy> binding
	    bind $winpath <Destroy> {}
	    # Destroy instance frame, all children, and bindings
	    destroy $winpath
	}
    }
}
