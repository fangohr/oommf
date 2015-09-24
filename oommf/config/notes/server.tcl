# FILE: server.tcl
# Computation engine/server program
#
# This script is used with the scripts ctrl.tcl and data.tcl to test
# proper functions of Tcl's socket fileevents.  Running OOMMF on
# Windows 95 and Windows 98 with some releases of Tcl 8.0 and Tcl 8.1,
# we noticed that when we had mmSolve2D sending Data out to two or
# more display applicataions every iteration, mmSolve2D was sluggish
# in responding to control commands from its interface (usually a
# Pause request).  This could mean many iterations would compute after
# the interactive user asked for a Pause in the computation.
#
# These three scripts are pared down versions of OOMMF applications.
# server.tcl represents mmSolve2D.  data.tcl represents a data
# display application, like mmDataTable.  ctrl.tcl represents
# mmSolve2D's interactive interface.  Use wish to start a copy of
# server.tcl, a copy of ctrl.tcl, and two copies of data.tcl.
# Click Start.  You should see the server and data windows display
# incrementing counters that are roughly in sync.  Now click Stop.
# If the counters stop immediately, things are fine.  If they continue
# for awhile before stopping, your Tcl is probably buggy.
#
# We believe the bug these scripts test for was fixed in Tcl 8.2.
# We believe this bug was only present in the version of Tcl for the
# Microsoft Windows OS.
#
########################################################################
# Utility proc for reading from non-blocking socket
proc Receive {datahdlr closehdlr s} {
    if {[catch {gets $s line} readCount]} {
	# socket error
	$closehdlr $s
	close $s
	return
    } 
    if {$readCount < 0} {
	if {[fblocked $s]} {
	    return
	}
	if {[eof $s]} {
	    $closehdlr $s
	    close $s
	    return
	}
    }
    $datahdlr $s $line
}

########################################################################
# Server for Data clients.  Send out iteration counts to them.
# Display their replies in Tk labels.

set datasockets {}

proc DataHdlr {s line} {
    set ::msg($s) $line
}

proc DataCloseHdlr {s} {
    set idx [lsearch -exact $::datasockets $s]
    set ::datasockets [lreplace $::datasockets $idx $idx]
    destroy .l$s
    unset ::msg($s)
}

proc DataChannel {s h p} {
    fconfigure $s -blocking 0 -buffering line -translation {auto crlf}
    puts $s "Connected to Data"
    lappend ::datasockets $s
    pack [label .l$s -textvariable msg($s)]
    fileevent $s readable [list Receive DataHdlr DataCloseHdlr $s]
}

socket -server DataChannel 7000

proc StepReport {ct} {
    foreach s $::datasockets {
	puts $s $ct
    }
}

########################################################################
# Iterative computation engine. [Step] is one iteration.
# [ChangeState] starts and stops the computation.

set afterId {}
set state 0

proc ChangeState {f} {
    if {$::state == $f} {
	return
    }
    after cancel $::afterId
    if {$f} {
	set ::afterId [after idle Step]
    } else {
	set ::afterId {}
    }
    set ::state $f
}

set count 0

proc Step {} {
    after 1000;	# Simulate computation
    StepReport [incr ::count]
    set ::afterId [after 1 {set ::afterId [after idle Step]}]
}

########################################################################
# Server for Control clients.  Recieve "start" and "stop" commands from
# them to start and stop the computation engine.

proc CtrlHdlr {s line} {
    switch -exact $line {
	start	{ChangeState 1}
	stop	{ChangeState 0}
	default	{}
    }
}

proc CtrlCloseHdlr {s} {}

proc CtrlChannel {s h p} {
    fconfigure $s -blocking 0 -buffering line -translation {auto crlf}
    puts $s "Connected to Control"
    fileevent $s readable [list Receive CtrlHdlr CtrlCloseHdlr $s]
}

socket -server CtrlChannel 5000

pack [button .quit -text Quit -command exit]
# END FILE: server.tcl
