# FILE: hostGui.tcl
#
# Last modified on: $Date: 2008/02/14 20:50:03 $
# Last modified by: $Author: donahue $
#

Oc_Class Net_HostGui {
    const public variable winpath
    const public variable outer_frame_options = "-bd 4 -relief ridge"
    public variable host
    private variable acctbuttons = {}
    private variable ctrlbar
    private variable panel
    Constructor {basewinpath args} {
	# Don't add '.' suffix if there already is one.
	# (This happens chiefly when basewinpath is exactly '.')
	if {![string match {.} \
		[string index $basewinpath \
		[expr [string length $basewinpath]-1]]]} {
	    append basewinpath {.}
	}
	eval $this Configure -winpath ${basewinpath}w$this $args

	# Pack entire subwidget into a subframe.  This makes
	# housekeeping easier, since the only bindings on
	# $winpath will be ones added from inside the class,
	# and also all subwindows can be destroyed by simply
	# destroying $winpath.  (The instance destructor should
	# not "destroy" $frame, because $frame is owned external
	# to this class.
	eval frame $winpath $outer_frame_options
        set ctrlbar [frame $winpath.ctrl -relief groove]
        set panel [frame $winpath.panel -relief groove]
        if {0} {
           set hostname [$host Cget -hostname]
        } else {
           # Alternative method.  When is this needed?
           # When you want something other than "localhost"
           # as the hostname.
           set hostname [info hostname]
           regsub {[.].*$} $hostname {} hostname
        }
        set hostlabel [label $ctrlbar.label -text $hostname:]
        pack $hostlabel -side left -in $ctrlbar -fill x -expand 0
        $this DisplayAccountButtons
        Oc_EventHandler New _ Net_Account Delete \
                [list $this DisplayAccountButtons] -groups [list $this]
        Oc_EventHandler New _ Net_Account Ready \
                [list $this DisplayAccountButtons] -groups [list $this]
        pack $ctrlbar -side top -fill x -expand 0
        pack $panel -side top -fill both -expand 1

	# Widget bindings
	bind $winpath <Destroy> "+$this WinDestroy %W"
    }
    method DisplayAccountButtons {} {
        foreach w $acctbuttons {
            destroy $w
        }
        set acctbuttons {}
        foreach acct [Net_Account Instances] {
            if {[string match [$host Cget -hostname] [$acct Cget -hostname]]} {
                if {[$acct Ready]} {
                    set btn [checkbutton $ctrlbar.w$acct \
                            -text [$acct Cget -accountname] \
                            -variable [$acct GlobalName btnvar] \
                            -command [list $acct ToggleGui $panel]]
                    lappend acctbuttons $btn
                    pack $btn -side left -in $ctrlbar -expand 0
                }
            }
        }
    }
    method WinDestroy { w } { winpath } {
	if {[string compare $winpath $w]!=0} { return } ;# Child destroy event
	$this Delete
    }
    private variable delete_in_progress = 0
    Destructor {
	# Protect against re-entrancy
	if { [info exists delete_in_progress] && \
		$delete_in_progress } { return }
	set delete_in_progress 1
        Oc_EventHandler DeleteGroup $this
        foreach w $acctbuttons {
	    if { ![winfo exists $w] } { continue }
            # Get the account instance the button controls
            set a [lindex [$w cget -command] 0]
            # If its GUI is on, turn it off (by invoking button)
            if {[$a Cget -gui]} {
                $w invoke
            }
        }
	# Delete everything else
	if {[info exists winpath] && [winfo exists $winpath]} {
	    # Destroy instance frame, all children, and bindings
	    destroy $winpath
	}
    }
}
