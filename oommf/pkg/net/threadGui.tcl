# FILE: threadGui.tcl
#
# Last modified on: $Date: 2010/06/25 00:04:46 $
# Last modified by: $Author: donahue $
#
package require Tk
package require Ow

Oc_Class Net_ThreadGui {
    const public variable winpath
    const public variable thread
    private variable inputs
    private variable ctrl 
    private variable cmdbtns
    private variable outputs
    private variable dataselection
    private variable indataselection
    private variable threadselection
    private variable ithreadselection
    private variable dt
    private variable idt
    private variable schedw
    private array variable opdb = {}
    private array variable ipdb = {}
    private array variable evdb = {}
    private array variable button = {}
    private variable status = "Status: "
    Constructor {args} {
	eval $this Configure -winpath .w$this $args
        toplevel $winpath
        wm group $winpath
	regexp {[0-9]+} [$thread Cget -pid] id
        wm title $winpath <$id>[$thread Cget -alias]
	after idle "Ow_SetIcon $winpath"

        # Input control panel
        set inputs [frame $winpath.inputs -bd 4 -relief ridge]
        pack $inputs -side top -fill both -expand 1

        # Runtime control panel with exit button
        set ctrl [frame $winpath.ctrl -bd 4 -relief ridge]
        pack [label $ctrl.label -textvariable [$this GlobalName status] \
		-width 60 -anchor w -relief groove] -side top -in $ctrl -fill x
        pack $ctrl -side top -fill both -expand 1
	set exitCmd [list $thread Send exit]
	if {[string match 0.0 [$thread ProtocolVersion]]} {
	    set exitCmd [list $thread Send delete]
	}
        pack [button $ctrl.exit -text Exit -command $exitCmd] \
		-in $ctrl -side right -fill both

        # Output schedule control panel
        set outputs [frame $winpath.outputs -bd 4 -relief ridge]
        pack $outputs -side top -fill both -expand 1
        bind $winpath <Destroy> "+$this WinDestroy %W"
        $this ResetInterface
	Oc_EventHandler New _ $thread Readable [list $this Receive] \
		-groups [list $this]
    }
    method ResetInterface {} {
        Oc_EventHandler New _ $thread Reply[$thread Send Commands] \
                [list $this ReceiveCommands] -oneshot 1 -groups [list $this]
        Oc_EventHandler New _ $thread Reply[$thread Send Outputs] \
                [list $this ReceiveOutputs] -oneshot 1 -groups [list $this]
        Oc_EventHandler New _ $thread Reply[$thread Send Inputs] \
                [list $this ReceiveInputs] -oneshot 1 -groups [list $this]
        Oc_EventHandler New _ $thread Reply[$thread Send Events] \
                [list $this ReceiveEvents] -oneshot 1 -groups [list $this]
        catch {unset opdb}
        catch {unset ipdb}
        catch {unset evdb}
        array set opdb {}
        array set ipdb {}
        array set evdb {}
    }
    method Receive {} {
        set message [$thread Get]
	switch [lindex $message 0] {
	    InterfaceChange {
		$this ResetInterface
	    }
	    Status {
		set status "Status: [join [lrange $message 1 end]]"
	    }
	}
    }
    method ReceiveCommands {} {
        set cmds [$thread Get]
        if {![lindex $cmds 0]} { ;# Check for error
	set min_label_width 75
        if {[info exists cmdbtns] && [winfo exists $cmdbtns]} {
            destroy $cmdbtns
        }
        set cmdbtns [frame $ctrl.cmdbtns]
        foreach {l group} [lindex $cmds 1] {
            frame $cmdbtns.w$l
	    frame $cmdbtns.w$l.labelframe
	    frame $cmdbtns.w$l.labelspacer -width $min_label_width
	    label $cmdbtns.w$l.label -text "${l}:"
	    pack $cmdbtns.w$l.labelspacer -in $cmdbtns.w$l.labelframe -side top
            pack $cmdbtns.w$l.label -in $cmdbtns.w$l.labelframe -side right \
		    -anchor e
            pack $cmdbtns.w$l.labelframe -in $cmdbtns.w$l -side left -fill both
            foreach c $group {
                set text [lindex $c 0]
                # This might impose restriction that labels contain no spaces
                pack [button $cmdbtns.w$l.w$text -text $text \
                        -command [concat $thread Send [lrange $c 1 end]]] \
                        -in $cmdbtns.w$l -side left -fill both
            }
            pack $cmdbtns.w$l -in $cmdbtns -side top -fill x
        }
        pack $cmdbtns -in $ctrl -side top -fill x
        }
    }
    method ReceiveOutputs {} {
        set op [$thread Get]
        if {![lindex $op 0]} { ;# Check for error
            foreach o [array names opdb] {
                unset opdb($o)
            }
            foreach {o desc} [lindex $op 1] {
                array set opdb [list $o $desc]
            }
            $this DisplayInteractiveOutputButtons
	    $this DisplayOutputPanel
        }
    }
    method ReceiveInputs {} {
        set ip [$thread Get]
        if {![lindex $ip 0]} { ;# Check for error
            foreach o [array names ipdb] {
                unset ipdb($o)
            }
            foreach {o desc} [lindex $ip 1] {
                array set ipdb [list $o $desc]
            }
	    $this DisplayInputPanel
        }
    }
    method DisplayOutputPanel {} {
	destroy $outputs.label
	if {[llength [array names opdb]]} {
	    if {![info exists button(so)]} {
		set button(so) 1
	    }
	    pack [checkbutton $outputs.label -text "Scheduled Outputs" \
		    -variable [$this GlobalName button](so) \
		    -command [list $this DisplayOutputs] \
		    -relief groove] \
		    -side top -in $outputs -fill x
	} else {
	    # This is not right, but it only breaks the case where an
	    # app reports no Outputs, then some Outputs.  No apps do that.
	    set button(so) 0
	}
	$this DisplayOutputs
    }
    method DisplayInteractiveOutputButtons {} {
        if {[winfo exists $ctrl.io]} {
            destroy $ctrl.io
        }
	if {![array size opdb]} {return}
        frame $ctrl.io
	frame $ctrl.io.labelframe
	frame $ctrl.io.labelspacer -width 75
	label $ctrl.io.label -text "Interactive\nOutputs:" -justify right
	pack $ctrl.io.labelspacer -in $ctrl.io.labelframe -side top
	pack $ctrl.io.label -in $ctrl.io.labelframe -side right -anchor e
        pack $ctrl.io.labelframe -in $ctrl.io -side left -fill both
        foreach o [array names opdb] {
            pack [button $ctrl.io.w$o -text $o \
                -command [list $this InteractiveOutput $o]] \
                -in $ctrl.io -side left -fill both
        }
        pack $ctrl.io -side bottom -fill both -expand 1
    }
    method DisplayInputPanel {} {
	destroy $inputs.label
	if {[llength [array names ipdb]]} {
	    if {![info exists button(in)]} {
		set button(in) 1
	    }
	    pack [checkbutton $inputs.label -text "Inputs" \
		    -variable [$this GlobalName button](in) \
		    -command [list $this DisplayInputs] \
		    -relief groove] \
		    -side top -in $inputs -fill x
	} else {
	    # This is not right, but it only breaks the case where an
	    # app reports no Inputs, then some Inputs.  No apps do that.
	    set button(in) 0
	}
	$this DisplayInputs
    }
    method DisplayInputs {} {
        destroy $inputs.data
	if {!$button(in)} {
	    $this DisplayInputThreads ""
	    return
	}
        set data [frame $inputs.data -bd 4 -relief ridge]
        pack [label $data.label -text "Inputs" -relief groove] \
                -side top -in $data -fill x
        foreach o [array names ipdb] {
           set desc $ipdb($o)
           radiobutton $data.w$o -text $o -value $o \
               -variable [$this GlobalName indataselection] \
               -anchor w \
               -command [list $this DisplayInputThreads [lindex $desc 0]]
           catch {$data.w$o configure -tristatevalue "tristatevalue"}
           # Disable Tk 8.5+ tristate mode.
           pack $data.w$o -side top -in $data -fill x
        }
        pack $data -side left -fill y
        if {[array size ipdb]} {
            set indataselection [lindex [array names ipdb] 0]
        } else {
            catch {unset indataselection}
        }
        $this DisplayInputThreads ""
    }
    method DisplayOutputs {} {
        if {[winfo exists $outputs.data]} {
            destroy $outputs.data
        }
        if {!$button(so)} {
            $this DisplayThreads ""
            return
        }
        set data [frame $outputs.data -bd 4 -relief ridge]
        pack [label $data.label -text "Outputs" -relief groove] \
                -side top -in $data -fill x
        foreach o [array names opdb] {
           set desc $opdb($o)
           radiobutton $data.w$o -text $o -value $o \
              -variable [$this GlobalName dataselection] \
              -anchor w \
              -command [list $this DisplayThreads [lindex $desc 0]]
           catch {$data.w$o configure -tristatevalue "tristatevalue"}
           # Disable Tk 8.5+ tristate mode.
           pack $data.w$o -side top -in $data -fill x
        }
        pack $data -side left -fill y
        if {[array size opdb]} {
            set dataselection [lindex [array names opdb] 0]
        }
        $this DisplayThreads ""
    }
    method ReceiveEvents {} {
        set ev [$thread Get]
        if {![lindex $ev 0]} { ;# Check for error
        foreach {e val} [lindex $ev 1] {
            array set evdb [list $e $val]
        }
        }
    }
    method DisplayThreads { type } {
        if {[info exists dt]} {
            destroy $dt
            unset dt
        }
        if {[info exists schedw]} {
            destroy $schedw
            unset schedw
        }
        Oc_EventHandler DeleteGroup $this-DisplayThreads
        # Set up for redraw when thread dies or new thread becomes ready
        Oc_EventHandler New _ Net_Thread Delete \
                [list $this DisplayThreads $type] \
                -groups [list $this $this-DisplayThreads]
        Oc_EventHandler New _ Net_Thread Ready \
                [list $this DisplayThreads $type] \
                -groups [list $this $this-DisplayThreads]
        if {!$button(so)} {
            $this DisplaySchedule
            return
        }
        if {![string length $type] && [info exists dataselection] \
                && [string length $dataselection]} {
            set desc $opdb($dataselection)
            set type [lindex $desc 0]
        }
        set dt [frame $outputs.dt -bd 4 -relief ridge]
        pack [label $dt.label -text "Destination Threads" -relief groove] \
                -side top -in $dt -fill x
        foreach t [Net_Thread Instances] {
            if {[$t Ready]} {
                if {[string match $type [$t Protocol]]} {
		    regexp {[0-9]+} [$t Cget -pid] id
                    set btn [radiobutton $dt.w$t \
                            -text <$id>[$t Cget -alias] \
                            -value $t -anchor w \
                            -command [list $this DisplaySchedule] \
                            -variable [$this GlobalName threadselection]]
                    catch {$btn configure -tristatevalue "tristatevalue"}
                    # Tk 8.5 introduces a tristate mode, which we don't use.
                    # Change default tristatevalue from "" to "tristatevalue"
                    # so that tristate mode is not accidentally selected by
                    # mousing over the radiobutton.
                    pack $btn -side top -in $dt -fill x
                }
            }
        }
        pack $dt -side left -fill both -expand 1
        catch {unset threadselection}
        $this DisplaySchedule
    }
    method DisplayInputThreads { type } {
        if {[info exists idt]} {
            destroy $idt
            unset idt
        }
        Oc_EventHandler DeleteGroup $this-DisplayInputThreads
        set idt [frame $inputs.dt -bd 4 -relief ridge]
        pack [label $idt.label -text "Source Threads" -relief groove] \
                -side top -in $idt -fill x
	if {!$button(in)} {
	    return
	}
        if {![string length $type] && [info exists indataselection] \
                && [string length $indataselection]} {
            set desc $ipdb($indataselection)
            set type [lindex $desc 0]
        }
        foreach t [Net_Thread Instances] {
            if {[$t Ready]} {
                if {[string match $type [$t Protocol]]} {
		    regexp {[0-9]+} [$t Cget -pid] id
                    set btn [radiobutton $idt.w$t \
                            -text [$t Cget -alias]<$id> \
                            -value $t -anchor w \
                            -command [list $this UpdateInput] \
                            -variable [$this GlobalName ithreadselection]]
                    catch {$btn configure -tristatevalue "tristatevalue"}
                    # Disable Tk 8.5+ tristate mode.
                    pack $btn -side top -in $idt -fill x
                }
            } 
        }
        pack $idt -side left -fill both -expand 1
        catch {unset ithreadselection}

        # Set up for redraw when thread dies or new thread becomes ready
        Oc_EventHandler New _ Net_Thread Delete \
                [list $this DisplayInputThreads $type] \
                -groups [list $this $this-DisplayInputThreads]
        Oc_EventHandler New _ Net_Thread Ready \
                [list $this DisplayInputThreads $type] \
                -groups [list $this $this-DisplayInputThreads]
    }
    method DisplaySchedule {} {
        if {[info exists schedw]} {
            destroy $schedw
            unset schedw
        }
        global _cb$thread
        foreach iii [array names _cb$thread *,*,*] {
            regexp {([^,]*),([^,]*),([^,]*)} $iii match d t e
            if {[catch {$t Ready} ready] || !$ready} {
                unset _cb${thread}($iii)
            }
        }
        if {!$button(so)} {
            return
        }
        set schedw [frame $outputs.sched -bd 4 -relief ridge]
	grid [label $schedw.label -text "Schedule" -relief groove] - \
		-sticky new
	grid columnconfigure $schedw 0 -pad 15
	grid columnconfigure $schedw 1 -weight 1
        if {[info exists dataselection] && [string length $dataselection]} {
        set opinfo $opdb($dataselection)
        if {[info exists threadselection] && \
		[string length $threadselection]} {
        foreach ev [lindex $opinfo 1] {
            set btn [checkbutton $schedw.w$ev -text $ev -anchor w \
                -variable _cb${thread}($dataselection,$threadselection,$ev) \
                -command [list $this UpdateSchedule $ev]]
            global _cnt$thread
            if {![info exists \
                    _cnt${thread}($dataselection,$threadselection,$ev)]} {
                set _cnt${thread}($dataselection,$threadselection,$ev) 1
            }
            set evinfo $evdb($ev)
            if {[lindex $evinfo 0]} {
		Ow_EntryBox New _ $schedw.cnt$ev -label "every" \
		 -autoundo 0 -valuewidth 4 \
	         -variable _cnt${thread}($dataselection,$threadselection,$ev) \
		 -callback [list $this UpdateScheduleB $ev] \
		 -valuetype posint -coloredits 1 -writethrough 0 \
		 -outer_frame_options "-bd 0"
		grid $btn [$_ Cget -winpath] -sticky nw
            } else {
		grid $btn -sticky nw
	    }
        }
        set ibtn [checkbutton $schedw.ibtn -text Interactive -anchor w \
		-variable \
		_cb${thread}($dataselection,$threadselection,interactive) \
		-command [list $this UpdateSchedule interactive]]
        set _cnt${thread}($dataselection,$threadselection,interactive) 0
        grid $ibtn -sticky nw
        }
        }
	set rowcount [lindex [grid size $schedw] 1]
	grid rowconfigure $schedw [expr $rowcount-1] -weight 1
        pack $schedw -side right -fill both -expand 1
    }
    method UpdateSchedule { event } {
        global _cb$thread
        global _cnt$thread
        if {[set _cb${thread}($dataselection,$threadselection,$event)]} {
            $thread Send SetHandler $event $dataselection \
                    [$threadselection Cget -hostname] \
                    [$threadselection Cget -accountname] \
                    [$threadselection Cget -pid] \
                    [set _cnt${thread}($dataselection,$threadselection,$event)]
        } else {
            $thread Send UnsetHandler $event $dataselection \
                    [$threadselection Cget -hostname] \
                    [$threadselection Cget -accountname] \
                    [$threadselection Cget -pid]
        }
    }
    method UpdateScheduleB { event widget args } {
	upvar #0 [$widget Cget -variable] var
	set var [$widget Cget -value]
	$this UpdateSchedule $event
    }
    method UpdateInput {} {
        $thread Send SetInput $indataselection \
                [$ithreadselection Cget -hostname] \
                [$ithreadselection Cget -accountname] \
                [$ithreadselection Cget -pid]
    }
    method InteractiveOutput { data } {
	$thread Send InteractiveOutput $data
    }
    private variable delete_in_progress = 0
    method WinDestroy { w } { winpath thread delete_in_progress } {
	if {[string compare $winpath $w]!=0} { return } ;# Child destroy event
	if { [info exists delete_in_progress] && \
		$delete_in_progress } { return }
        upvar #0 [$thread GlobalName btnvar] tmp
        set tmp 0
        $thread ToggleGui
    }
    Destructor {
	# Protect against re-entrancy
	if { [info exists delete_in_progress] && \
		$delete_in_progress } { return }
	set delete_in_progress 1

        Oc_EventHandler DeleteGroup $this
	# Delete everything else
	if {[info exists winpath] && [winfo exists $winpath]} {
            # bind $winpath <Destroy> {}
	    # Destroy instance frame, all children, and bindings
	    # Rely on the delete_in_progress semaphore to protect
	    # against re-entrancy, both here and inside WinDestroy.
	    destroy $winpath
	}
    }
}
