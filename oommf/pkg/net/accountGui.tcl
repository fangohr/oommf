# FILE: accountGui.tcl
#
# Code providing Tk interface to client side of account server.
# Used in mmLaunch.
#
# Last modified on: $Date: 2012-06-26 01:37:52 $
# Last modified by: $Author: donahue $
#

Oc_Class Net_AccountGui {
    const public variable winpath
    const public variable outer_frame_options = "-bd 4 -relief ridge"
    const public variable account
    private variable accountlabel
    private variable programs
    private variable threads
    private variable plabel
    private variable tlabel
    private variable threadbuttons = {}
    private variable myProgramButtons = {}
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
        set accountname [$account Cget -accountname]
        set accountlabel [label $winpath.label -text $accountname \
                -relief groove]
        pack $accountlabel -side top -fill x
        set programs [frame $winpath.programs -relief flat]
	set ptspacer [frame $winpath.ptspacer -relief flat -width 10]
        set threads [frame $winpath.threads -relief flat]
        set plabel [label $programs.label -text Programs -relief flat]
        set tlabel [label $threads.label -text Threads -relief flat]
        pack $plabel -side top -in $programs -fill x
        grid $tlabel - -in $threads -sticky n
        pack $programs -side left -fill both  -expand 1
	pack $ptspacer -side left -anchor s
        pack $threads -side left -fill x -expand 1 -anchor n
        $this DisplayThreadButtons
        Oc_EventHandler New _ Net_Thread Delete [list $this \
                DisplayThreadButtons] -groups [list $this]
        Oc_EventHandler New _ Net_Thread Ready [list $this \
                DisplayThreadButtons] -groups [list $this]
	# Widget bindings
	bind $winpath <Destroy> "+$this WinDestroy %W"
	if {[$account Ready]} {
	    $this Initialize
	} else {
	    Oc_EventHandler New _ $account Ready [list $this Initialize] \
		    -oneshot 1 -groups [list $this]
	}
    }
    method Initialize {} {
        set qid [$account Send programs]
        Oc_EventHandler New _ $account Reply$qid [list $this \
                ProgramsReply $qid] -oneshot 1 -groups [list $this $this-$qid]
        Oc_EventHandler New _ $account Timeout$qid \
		[list $this ProgramsTimeout $qid]  \
		-oneshot 1 -groups [list $this $this-$qid]
    }
    method DisplayThreadButtons {} {
        foreach w $threadbuttons {
            destroy $w
        }
	array set threadArray {}
        foreach thread [Net_Thread Instances] {
            if {[string match [$account Cget -accountname] \
                    [$thread Cget -accountname]] && [string match \
                    [$account Cget -hostname] [$thread Cget -hostname]]} {
                if {[$thread Ready]} {
		    set id [$thread Cget -pid]
		    regexp {[0-9]+} $id id
		    if {![info exists threadArray($id)] || [$thread HasGui]} {
			set threadArray($id) $thread
		    }
                }
            }
        }
        set threadbuttons {}
	foreach id [lsort -integer [array names threadArray]] {
	    set thread $threadArray($id)
            if {[$thread HasGui]} {
		set btn [checkbutton $threads.c$thread \
			-padx 0 -pady 0 \
			-variable [$thread GlobalName btnvar] \
			-command [list $thread ToggleGui]]
		lappend threadbuttons $btn
	    } else {
		set btn x ;# Empty column
	    }
	    set lbl [label $threads.l$thread \
		    -text "[$thread Cget -alias] <$id>"]
	    lappend threadbuttons $lbl
	    grid $lbl $btn -in $threads -sticky e
	}
    }
    method ProgramsReply {id} {
        Oc_EventHandler DeleteGroup $this-$id
        set result [$account Get]
        if {![lindex $result 0]} {
            foreach pgm [lrange $result 1 end] {
                set btn [button $programs.w$pgm -text $pgm]
                $btn configure -command [list $this LaunchProgram $pgm $btn]
                lappend myProgramButtons $btn
                pack $btn -side top -fill x 
            }
        } else {
            set msg "Error report from account service directory\
                    [$account Cget -hostname]:[$account Cget -accountname]:\
                    \n\t[lrange $result 1 end]\n\nIn reply to\
                    query:\n\tprograms"
            error $msg $msg
        }
    }
    method LaunchProgram { pgm btn } {
	global env
        $btn configure -state disabled
	if {![$account Ready]} {
	    Oc_EventHandler New _ $account Ready \
		    [list $this LaunchProgram $pgm $btn] -oneshot 1 \
		    -groups [list $this $account]
	    return
	}
	if {[info exists env(DISPLAY)]} {
	    set qid [$account Send launch $pgm DISPLAY $env(DISPLAY)]
	} else {
	    set qid [$account Send launch $pgm]
	}
        Oc_EventHandler New _ $account Reply$qid [list $this \
                LaunchReply $btn $qid $pgm] -oneshot 1 \
                -groups [list $this $this-$qid]
        Oc_EventHandler New _ $account Timeout$qid [list $this \
                LaunchTimeout $btn $qid] -oneshot 1 \
                -groups [list $this $this-$qid]
    }
    method LaunchReply { btn id pgm } {
        Oc_EventHandler DeleteGroup $this-$id
        if {[winfo exists $btn]} {
            $btn configure -state normal
        }
        set result [$account Get]
        if {[lindex $result 0]} {
            set msg "Error report from account service directory\
                    [$account Cget -hostname]:[$account Cget -accountname]:\
                    \n\t[lrange $result 1 end]\n\nIn reply to\
                    query:\n\tlaunch $pgm"
            error $msg $msg
        }
    }
    method LaunchTimeout { btn id } {
        Oc_EventHandler DeleteGroup $this-$id
        if {[winfo exists $btn]} {
            $btn configure -state normal
        }
        set msg "Timeout waiting for account service directory\
                [$account Cget -hostname]:[$account Cget -accountname]\
                \nto reply to query:\n\tlaunch $pgm"
        error $msg $msg
    }
    method ProgramsTimeout {id} {
        Oc_EventHandler DeleteGroup $this-$id
        set msg "Timeout waiting for account service directory\
                [$account Cget -hostname]:[$account Cget -accountname]\
                \nto reply to query:\n\tprograms"
        error $msg $msg
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
	# Delete everything else
	if {[info exists winpath] && [winfo exists $winpath]} {
	    # Destroy instance frame, all children, and bindings
	    destroy $winpath
	}
    }
}
