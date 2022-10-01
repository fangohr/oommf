# FILE: mmlaunch.tcl
#
# An application which presents an interface for launching and 
# interconnecting other OOMMF applications.
#
# Last modified on: $Date: 2015/03/25 16:43:38 $
# Last modified by: $Author: dgp $

# Support libraries
package require Ow
package require Net
package require Oc 2
if {[catch {package require Tk 8}]} {
    package require Tk 4.1
}
wm withdraw .

Oc_Main SetAppName mmLaunch
Oc_Main SetVersion 2.0b0
regexp \\\044Date:(.*)\\\044 {$Date: 2015/03/25 16:43:38 $} _ date
Oc_Main SetDate [string trim $date]
# regexp \\\044Author:(.*)\\\044 {$Author: dgp $} _ author
# Oc_Main SetAuthor [Oc_Person Lookup [string trim $author]]
Oc_Main SetAuthor [Oc_Person Lookup dgp]
Oc_Main SetHelpURL [Oc_Url FromFilename [file join [file dirname \
        [file dirname [file dirname [Oc_DirectPathname [info \
        script]]]]] doc userguide userguide \
	OOMMF_Launcher_Control_Inte.html]]

Oc_CommandLine ActivateOptionSet Net
Oc_CommandLine Parse $argv

set menubar .mb
foreach {fmenu hmenu} [Ow_MakeMenubar . $menubar File Help] {}
$fmenu add command -label "Exit All OOMMF" -command {ConfirmExitAll}
$fmenu add command -label "Exit" -underline 1 -command exit
proc Die { win } {
    if {![string match . $win]} {return}
    exit
}
bind $menubar <Destroy> {+Die %W}
wm protocol . WM_DELETE_WINDOW { exit }
Oc_EventHandler New _ Oc_Main Exit {proc Die win {}}
Ow_StdHelpMenu $hmenu

# Host checkbutton frame
set hframe [frame .omfHosts -bd 4 -relief ridge]
set hpanel [frame .omfPanel -relief groove]

set menuwidth [Ow_GuessMenuWidth $menubar]
set brace [canvas .brace -width $menuwidth -height 0 -borderwidth 0 \
        -highlightthickness 0]
pack $brace -side top

pack $hframe -side left -fill both -expand 1
pack $hpanel -side left -fill both -expand 1

wm title . [Oc_Main GetInstanceName]
wm iconname . [wm title .]
Ow_SetIcon .
wm deiconify .

if {[package vcompare [package provide Tk] 8] >=0 \
	&& [string match windows $tcl_platform(platform)]} {
    # Windows doesn't size Tcl 8.0+ menubar cleanly
    Oc_DisableAutoSize .
    wm geometry . "${menuwidth}x0"
    update
    wm geometry . {}
    Oc_EnableAutoSize .
    update
}

proc KillAll { acct } {
   $acct Send kill *
   $acct Delete
}

proc CheckLastAccount {} {
   if {[llength [Net_Account Instances]] == 1} {
      Oc_Main AllowShutdown
   }
}

proc ConfirmExitAll {} {
    set response [Ow_Dialog 1 "Exit all OOMMF processes?" \
	    warning "Are you *sure* you want to exit all running OOMMF\
	    programs (including all solvers)?" {} 1 Yes No]
    if {$response == 0} {
       foreach acct [Net_Account Instances] {
          if {[$acct Ready]} {
             KillAll $acct
          }
       }
       Oc_EventHandler New _ Net_Account Ready [list KillAll %object]
       # Keep event loop running while account servers go away.
       if {[llength [Net_Account Instances]]} {
          Oc_EventHandler New _ Net_Account Delete CheckLastAccount
          Oc_Main BlockShutdown
       }
       exit
    }
}

proc CheckLastThread {} {
    if {[llength [Net_Thread Instances]] == 1} {
	Oc_Main AllowShutdown
    }
}

proc DisplayHostButtons {} {
    global hframe hpanel
    # Destroy existing buttons
    foreach w [winfo children $hframe] {
        destroy $w
    }
    set displayedhost 0
    set ports {}
    foreach host [Net_Host Instances] {
        if {[$host Ready]} {
           if {0} {
            set btn [checkbutton $hframe.w$host -text [$host Cget -hostname] \
                -anchor w \
                -variable [$host GlobalName btnvar] \
                -command [list $host ToggleGui $hpanel]]
           } else {
            set hname [info hostname]
            regsub {[.].*$} $hname {} hname
            set btn [checkbutton $hframe.w$host -text $hname \
                -anchor w \
                -variable [$host GlobalName btnvar] \
                -command [list $host ToggleGui $hpanel]]
           }
           pack $btn -side top -fill x
           set displayedhost 1
           Ow_PropagateGeometry .
           lappend ports [$host HostPort]
        }
    }
    switch [llength $ports] {
        0 { set aboutinfo {} }
        1 { set aboutinfo "Host port: [lindex $ports 0]" }
        default { set aboutinfo "Host ports: $ports" }
    }
    Oc_Main SetExtraInfo $aboutinfo
}
Oc_EventHandler New _ Net_Host Ready DisplayHostButtons
Oc_EventHandler New _ Net_Host Delete DisplayHostButtons

# When any account becomes ready, get its threads
Oc_EventHandler New _ Net_Account Ready [list GetThreads %object]

proc GetThreads { acct } {
    set qid [$acct Send threads]
    Oc_EventHandler New _ $acct Reply$qid [list GetThreadsReply $acct $qid] \
        -groups [list $acct GetThreads-$qid]
    Oc_EventHandler New _ $acct Timeout$qid \
	[list GetThreadsTimeout $acct $qid] \
        -groups [list $acct GetThreads-$qid]
}
proc GetThreadsReply { acct qid } {
    Oc_EventHandler DeleteGroup GetThreads-$qid
    # Set up to receive NewThread messages, but only one handler per account
    Oc_EventHandler DeleteGroup GetThreadsReply-$acct
    Oc_EventHandler New _ $acct Readable [list HandleAccountTell $acct] \
            -groups [list $acct GetThreadsReply-$acct]
    set threads [$acct Get]
    Oc_Log Log "Received thread list: $threads" status
    # Create a Net_Thread instance for each element of the returned
    # thread list
    if {![lindex $threads 0]} {
        foreach quartet [lrange $threads 1 end] {
	    NewThread $acct [lindex $quartet 1]
        }
    }
}
proc GetThreadsTimeout { acct qid } {
    Oc_EventHandler DeleteGroup GetThreads-$qid
    set msg "Timed out waiting for thread list"
    error $msg $msg
}

# Detect and handle NewThread message from account server
proc HandleAccountTell { acct } {
    set message [$acct Get]
    switch -exact -- [lindex $message 0] {
        newthread {
            NewThread $acct [lindex $message 1]
        }
	deletethread {
	    Net_Thread New t -hostname [$acct Cget -hostname] \
		    -pid [lindex $message 1]
	    if {[$t Ready]} {
		$t Send bye
	    } else {
		Oc_EventHandler New _ $t Ready [list $t Send bye]
	    }
	}
        default {
          Oc_Log Log "Bad message from account $acct:\n\t$message" status
        }
    }
}

# There's a new thread with id $id, create corresponding local instance
proc NewThread { acct id } {
    Net_Thread New _ -hostname [$acct Cget -hostname] \
            -accountname [$acct Cget -accountname] -pid $id
}

# Create initial host list -- try to connect to account servers
#
# By default, connect to and provide an interface for our
# account server on the local host. If the hostList option
# is set, connect to the hosts on that list instead.
set hostList localhost
Oc_Option Get LaunchInterface hostList hostList
foreach host $hostList {
    if {[string match $host* [info hostname]]} {
    #
    # if the $host is a name of the localhost, be
    # sure to connect with the literal name "localhost"
    # (the default) so that servers can be auto-started
    # if needed.
    #
        Net_Host New _
        Net_Account New _
    } else {
    #
    # Attempt to contact remote hosts by name.
    #
        Net_Host New _ -hostname $host
        Net_Account New _ -hostname $host
    }
}
