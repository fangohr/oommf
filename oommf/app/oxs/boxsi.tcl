# FILE: boxsi.tcl
#
# The Batch-mode OOMMF eXtensible Solver Interface

# Support libraries
package require Oc 1.1
package require Oxs 1.2
package require Net 1.2.0.3

# Make background errors fatal.  Otherwise, since boxsi is assumed
# to run non-interactively, a background error can cause the boxsi
# to hang, if, for example, the error occurs in proc Initialize
# and as a result initialization is not completed.
rename bgerror orig_boxsi_bgerror
proc bgerror { msg } {
   orig_boxsi_bgerror $msg
   puts stderr "\nFATAL BACKGROUND ERROR: $msg"
   exit 13
}

Oc_IgnoreTermLoss  ;# Try to keep going, even if controlling terminal
## goes down.

# Application description boilerplate
Oc_Main SetAppName Boxsi
Oc_Main SetVersion 1.2.0.5
regexp \\\044Date:(.*)\\\044 {$Date: 2012-09-25 17:11:58 $} _ date
Oc_Main SetDate [string trim $date]
Oc_Main SetAuthor [Oc_Person Lookup dgp]
Oc_Main SetHelpURL [Oc_Url FromFilename [file join [file dirname \
        [file dirname [file dirname [Oc_DirectPathname [info \
        script]]]]] doc userguide userguide\
        OOMMF_eXtensible_Solver_Int.html]]

# Command line options
Oc_CommandLine ActivateOptionSet Net
Oc_CommandLine Option restart {
    {flag {expr {$flag==0 || $flag==1 || $flag==2}} \
        {= 0 (default=no), 1 (force) or 2 (try)}}
} {
    global restart_flag; set restart_flag $flag
} {1 => use <basename>.restart file to restart simulation}
set restart_flag 0
Oxs_SetRestartFlag $restart_flag
#trace variable restart_flag w {Oxs_SetRestartFlag $restart_flag ;# }

Oc_CommandLine Option nocrccheck {
	{flag {expr {$flag==0 || $flag==1}} {= 0 (default) or 1}}
    } {
	global nocrccheck_flag; set nocrccheck_flag $flag
} {1 => Disable CRC check on simulation restarts}
set nocrccheck_flag 0
Oxs_SetRestartCrcCheck [expr {!$nocrccheck_flag}]
trace variable nocrccheck_flag w \
	{Oxs_SetRestartCrcCheck [expr {!$nocrccheck_flag}] ;# }

Oc_CommandLine Option exitondone {
	{flag {expr {![catch {expr {$flag && $flag}}]}} {= 0 or 1 (default)}}
    } {
	global exitondone; set exitondone $flag
} {1 => Exit when problem solved}
set exitondone 1

Oc_CommandLine Option pause {
	{flag {expr {![catch {expr {$flag && $flag}}]}} {= 0 (default) or 1}}
    } {
	global autorun_pause; set autorun_pause $flag
} {1 => Pause after problem initialization}
set autorun_pause 0

Oc_CommandLine Option nice {
	{flag {expr {![catch {expr {$flag && $flag}}]}} {= 0 (default) or 1}}
    } {
	global nice; set nice $flag
} {1 => Drop priority after starting}
set nice 0

Oc_CommandLine Option loglevel {
      {level {expr {[regexp {^[0-9]+$} $level]}}}
   } {
      global loglevel;  set loglevel $level
} {Level of log detail to oommf/boxsi.errors (default is 1)}
set loglevel 1

Oc_CommandLine Option regression_test {
	{ flag {expr {![catch {expr {$flag && $flag}}]}} \
             {= 0 (default) or positive integer}}
    } {
	global regression_test; set regression_test $flag
} {non-zero => Run regression test}
set regression_test 0

Oc_CommandLine Option regression_testname {
   { basename {expr {![string match {} $basename]}} { is test output basename}}
    } {
	global regression_testname; set regression_testname $basename
} {Set basename for regression_test output}
set regression_testname "regression-test-output"

Oc_CommandLine Option parameters \
{{params {expr {([llength $params]%2)==0}} { is a list of name+value pairs}}} \
{global MIF_params; set MIF_params $params} \
{Set MIF file parameters}
set MIF_params {}

set killtags {}
Oc_CommandLine Option kill {
    {tags {} {is keyword "all" or tag list}}
} {
      global killtags; set killtags $tags
} {Applications to kill on program exit }

# Multi-thread support
if {[Oc_HaveThreads]} {
   set threadcount [Oc_GetMaxThreadCount]
   Oc_CommandLine Option threads {
      {number {expr {[regexp {^[0-9]+$} $number] && $number>0}}}
   } {
      global threadcount;  set threadcount $number
   } [subst {Number of concurrent threads (default is $threadcount)}]
} else {
   set threadcount 1  ;# Safety
}

# NUMA (non-uniform memory access) support
set numanodes none
if {[Oc_NumaAvailable]} {
   if {[info exists env(OOMMF_NUMANODES)]} {
      set nodes $env(OOMMF_NUMANODES)
      if {![regexp {^([0-9 ,]*|auto|none)$} $nodes]} {
         puts stderr "\n************************************************"
         puts stderr "ERROR: Bad environment variable setting:\
                   OOMMF_NUMANODES=$nodes"
         puts stderr "   Overriding to \"$numanodes\""
         puts stderr "************************************************"
      } else {
         set numanodes $nodes
      }
   }
   Oc_CommandLine Option numanodes {
      {nodes {regexp {^([0-9 ,]*|auto|none)$} $nodes}}
   } {
      global numanodes
      set numanodes $nodes
   } [subst {NUMA memory and run nodes (or "auto" or "none")\
                (default is "$numanodes")}]
}

Oc_CommandLine Option restart {
    {flag {expr {$flag==0 || $flag==1 || $flag==2}} \
        {= 0 (default=no), 1 (force) or 2 (try)}}
} {
    global restart_flag; set restart_flag $flag
} {1 => use <basename>.restart file to restart simulation}
set restart_flag 0
Oxs_SetRestartFlag $restart_flag


Oc_CommandLine Option [Oc_CommandLine Switch] {
    {filename {} {Input MIF 2.1 problem file}}
} {
    global problem; set problem $filename
} {End of options; next argument is filename}

##########################################################################
# Parse commandline and initialize threading
##########################################################################
Oc_CommandLine Parse $argv
if {[catch {Oxs_SetRestartFlag $restart_flag} msg]} {
   Oc_Log Log $msg error
   exit 1
}

if {[Oc_HaveThreads]} {
   Oc_SetMaxThreadCount $threadcount
   set aboutinfo "Number of threads: $threadcount"
} else {
   set aboutinfo "Single threaded build"
}

if {[Oc_NumaAvailable]} {
   if {[string match auto $numanodes]} {
      set nodes {}
   } elseif {[string match none $numanodes]} {
      set nodes none
   } else {
      set nodes [split $numanodes " ,"]
   }
   if {![string match "none" $nodes]} {
      Oc_NumaInit $threadcount $nodes
   }
   append aboutinfo "\nNUMA: $numanodes"
}
Oc_Main SetExtraInfo $aboutinfo
set update_extra_info $aboutinfo
unset aboutinfo

##########################################################################
# Define the GUI of this app to be displayed remotely by clients of the
# Net_GeneralInterface protocol.  Return $gui in response to the
# GetGui message.
##########################################################################
set gui {
    package require Oc 1.1
    package require Tk
    package require Ow
    wm withdraw .
}
append gui "[list Oc_Main SetAppName [Oc_Main GetAppName]]\n"
append gui "[list Oc_Main SetVersion [Oc_Main GetVersion]]\n"
append gui "[list Oc_Main SetExtraInfo [Oc_Main GetExtraInfo]]\n"
append gui {
share update_extra_info
trace variable update_extra_info w { Oc_Main SetExtraInfo $update_extra_info ;# }
}
append gui "[list Oc_Main SetPid [pid]]\n"
append gui {

regexp \\\044Date:(.*)\\\044 {$Date: 2012-09-25 17:11:58 $} _ date
Oc_Main SetDate [string trim $date]

# This won't cross different OOMMF installations nicely
Oc_Main SetAuthor [Oc_Person Lookup donahue]

}

# This might not cross nicely either:
#   Originally the filename of the Oxs help HTML documentation section
# was constructed on the solver side and passed to the interface side in
# the gui string.  However, if the solver and interface are running on
# two different machines, then there are potentially two problems with
# that setup.  The Help menu item launches a documentation viewer on the
# interface side.  If the solver and interface machines don't share a
# common filesystem, then it is likely that the Help filename passed
# across will be wrong; in this case mmHelp will display a File not
# found Retrieval Error.  Even worse, the 'Oc_Url FromFilename' call
# raises an error if the filename passed to it does not look like an an
# absolute pathname.  This will happen if, for example, one of the
# machines is running Windows and the other is Unix, since absolute
# paths on Windows look like 'C:/foo/bar.html' but Unix wants
# '/foo/bar.html'.  If this error is not caught, then the interface
# won't even come up.  Definitely not very user friendly.
#   To bypass these problems, the code below uses 'Oc_Main
# GetOOMMFRootDir' to construct the Help filename on the interface side.
# Only down side to this that I see is if the interface is running out
# of a different OOMMF version than the solver; but in that case it
# would not be clear which documentation to display anyway, since we
# don't know if the user is asking for help on the interface or on the
# solver internals.  Regardless, we don't currently have a general way
# to make the solver-side documentation available, so we might as well
# serve up the interface-side docs.
#   A catch is put around this code is just to make doubly sure that any
# problems with setting up the Help URL don't prevent the user
# interfacing from being displayed.  If the Help URL is not explicitly
# set, then as a fallback Oc_Main returns a pointer to the documentation
# front page, built by indexing off the directory holding the
# interface-side Oc_Main script.  Note that this is the same method used
# by 'Oc_Main GetOOMMFRootDir'.  -mjd, 29-July-2002
append gui {
catch {
   set boxsi_doc_section [list [file join \
                          [Oc_Main GetOOMMFRootDir] doc userguide userguide \
                          OOMMF_eXtensible_Solver_Bat.html]]
   Oc_Main SetHelpURL [Oc_Url FromFilename $boxsi_doc_section]
   proc SetOidCallback {code result args} {
      if {$code == 0} {Oc_Main SetOid $result}
      rename SetOidCallback {}
   }
   remote -callback SetOidCallback serveroid
   if {[Ow_IsAqua]} {
      # Add some padding to allow space for Aqua window resize
      # control in lower righthand corner
      . configure -relief flat -borderwidth 15
   }
}
}

append gui {
proc ConsoleInfo {m t s} {
    puts stderr $m
}
Oc_Log SetLogHandler [list ConsoleInfo] info
Oc_Log AddType infolog ;# Record in log file only

set menubar .mb
foreach {fmenu hmenu} [Ow_MakeMenubar . $menubar File Help] {}
$fmenu add command -label "Show Console" -command { console show } -underline 0
$fmenu add command -label "Close Interface" -command closeGui -underline 0
$fmenu add separator
$fmenu add command -label "Clear Schedule" -command ClearSchedule
$fmenu add separator
$fmenu add command -label "Exit [Oc_Main GetAppName]" \
	-command exit -underline 1
Ow_StdHelpMenu $hmenu

set menuwidth [Ow_GuessMenuWidth $menubar]
set brace [canvas .brace -width $menuwidth -height 0 -borderwidth 0 \
        -highlightthickness 0]
pack $brace -side top

if {[package vcompare [package provide Tk] 8] >= 0 \
        && [string match windows $tcl_platform(platform)]} {
    # Windows doesn't size Tcl 8.0 menubar cleanly
    Oc_DisableAutoSize .
    wm geometry . "${menuwidth}x0"
    update
    wm geometry . {}
    Oc_EnableAutoSize .
    update
}

share problem

set control_interface_state disabled
share control_interface_state

set bf [frame .controlbuttons]
pack [button $bf.run -text Run -command {set status Run} \
	-state $control_interface_state] -fill x -side left
pack [button $bf.relax -text Relax -command {set status Relax} \
	-state $control_interface_state] -fill x -side left
pack [button $bf.step -text Step -command {set status Step} \
	-state $control_interface_state] -fill x -side left
pack [button $bf.pause -text Pause -command {set status Pause} \
	-state $control_interface_state] -fill x -side left

pack $bf -side top -fill x

set stateframe [frame .state]

set framerow 0
grid [label $stateframe.problemlabel -text Problem:] \
	-column 0 -row $framerow -sticky e
grid [label $stateframe.problemvalue -textvariable problem] \
	-column 1 -row $framerow -sticky w

share status
incr framerow
grid [label $stateframe.statuslabel -text Status:] \
	-column 0 -row $framerow -sticky e
grid [label $stateframe.statusvalue -textvariable status] \
	-column 1 -row $framerow -sticky w

share stagerequest
share number_of_stages
proc SetupStageShow {args} {
    global stagerequest number_of_stages stageshow
    if {[catch {format %2d $stagerequest} astr]} {
	set astr "-"
    }
    if {[catch {format %d [expr {$number_of_stages-1}]} bstr]} {
	set bstr "-"
    }
    set stageshow "$astr / $bstr"
}
SetupStageShow
trace variable stagerequest w SetupStageShow
trace variable number_of_stages w SetupStageShow
incr framerow
grid [label $stateframe.stagelabel -text Stage:] \
	-column 0 -row $framerow -sticky e
grid [label $stateframe.stagevalue -textvariable stageshow] \
	-column 1 -row $framerow -sticky w

unset framerow
grid columnconfigure $stateframe 0 -weight 0
grid columnconfigure $stateframe 1 -weight 1

pack $stateframe -side top -fill x

trace variable control_interface_state w {
    foreach _ [winfo children .controlbuttons] {
	$_ configure -state [uplevel #0 set control_interface_state];
    } ;# }

# Widget lists; these are tied to output_interface_state
# for state (normal/disable) toggling.
set output_tkbox_list {}
set output_owbox_list {}
set output_interface_state disabled
share output_interface_state

set oframe [frame .sopanel]

set opframe [frame $oframe.outputs]
pack [label $opframe.l -text " Output " -relief groove] -side top -fill x
Ow_ListBox New oplb $opframe.lb -height 4 -variable oplbsel
# Note: At this time (Feb-2006), tk list boxes on Mac OS X/Darwin don't
# scroll properly if the height is smaller than 4.
lappend output_owbox_list $oplb
pack [$oplb Cget -winpath] -side top -fill both -expand 1
share opAssocList
trace variable opAssocList w { uplevel #0 {
    catch {unset opArray}
    array set opArray $opAssocList
    set opList [lsort [array names opArray]]
    UpdateSchedule } ;# }
trace variable opList w {
    $oplb Remove 0 end
    eval [list $oplb Insert 0] $opList ;# }
trace variable oplbsel w {uplevel #0 {
    trace vdelete destList w {SetDestinationList; UpdateSchedule ;#}
    if {[llength $oplbsel]} {
	upvar 0 $opArray([lindex $oplbsel 0])List destList
	SetDestinationList
    }
    trace variable destList w {SetDestinationList; UpdateSchedule ;#}
} ;# }
pack $opframe -side left -fill both -expand 1

# The destination lists (Find a way to construct dynamically, yet
# efficiently):
share vectorFieldList
share scalarFieldList
share DataTableList
set vectorFieldList [list]
set scalarFieldList [list]
set DataTableList [list]

set dframe [frame $oframe.destinations]
pack [label $dframe.l -text " Destination " -relief groove] -side top -fill x
Ow_ListBox New dlb $dframe.lb -height 4 -variable dlbsel
# Note: At this time (Feb-2006), tk list boxes on Mac OS X/Darwin don't
# scroll properly if the height is smaller than 4.
lappend output_owbox_list $dlb
pack [$dlb Cget -winpath] -side top -fill both -expand 1
proc SetDestinationList {} {
    global dlb destList
    $dlb Remove 0 end
    eval [list $dlb Insert 0] $destList
}
trace variable destList w {SetDestinationList; UpdateSchedule ;#}
pack $dframe -side left -fill both -expand 1
	
set sframe [frame $oframe.schedule]
grid [label $sframe.l -text Schedule -relief groove] - -sticky new
grid columnconfigure $sframe 0 -pad 15
grid columnconfigure $sframe 1 -weight 1
grid [button $sframe.send -command \
	{remote Oxs_Output Send [lindex $oplbsel 0] [lindex $dlbsel 0]} \
	-text Send] - -sticky new
lappend output_tkbox_list $sframe.send

#
# FOR NOW JUST HACK IN THE EVENTS WE SUPPORT
#	eventually this may be driver-dependent
set events [list Step Stage]
set schedwidgets [list $sframe.send]
foreach event $events {
    set active [checkbutton $sframe.a$event -text $event -anchor w \
	    -variable Schedule---activeA($event)]
    $active configure -command [subst -nocommands \
		{Schedule Active [$active cget -variable] $event}]
    lappend output_tkbox_list $active
    Ow_EntryBox New frequency $sframe.f$event -label every \
	    -autoundo 0 -valuewidth 4 \
	    -variable Schedule---frequencyA($event) \
	    -callback [list EntryCallback $event] \
	    -foreground Black -disabledforeground #a3a3a3 \
	    -valuetype posint -coloredits 1 -writethrough 0 \
	    -outer_frame_options "-bd 0"
    lappend output_owbox_list $frequency
    grid $active [$frequency Cget -winpath] -sticky nw
    lappend schedwidgets $active $frequency
}

grid rowconfigure $sframe [expr {[lindex [grid size $sframe] 1] - 1}] -weight 1
pack $sframe -side left -fill y

proc EntryCallback {e w args} {
    upvar #0 [$w Cget -variable] var
    set var [$w Cget -value]
    Schedule Frequency [$w Cget -variable] $e
}
proc Schedule {x v e} {
    global oplbsel dlbsel
    upvar #0 $v value
    remote Oxs_Schedule Set [lindex $oplbsel 0] [lindex $dlbsel 0] $x $e $value
}

proc ToggleOutputState {} {
   global output_interface_state
   global output_tkbox_list
   global output_owbox_list
   foreach w $output_tkbox_list {
      $w configure -state $output_interface_state
   }
   foreach w $output_owbox_list {
      $w Configure -state $output_interface_state
   }
   UpdateSchedule
}
trace variable output_interface_state w "ToggleOutputState ;#"

trace variable oplbsel w "UpdateSchedule ;#"
trace variable dlbsel w "UpdateSchedule ;#"
Oc_EventHandler Bindtags UpdateSchedule UpdateSchedule
proc UpdateSchedule {} {
    global oplbsel dlbsel schedwidgets opArray
    Oc_EventHandler Generate UpdateSchedule Reset
    set os [lindex $oplbsel 0]
    set ds [lindex $dlbsel 0]
    set state disabled
    if {[info exists opArray($os)]} {
	upvar #0 $opArray($os)List destList
	if {[lsearch -exact $destList $ds] >= 0} {
	    set state normal
	}
    }
    foreach _ $schedwidgets {
	if {[string match *EntryBox* $_]} {
	    $_ Configure -state $state
	} else {
	    $_ configure -state $state
	}
    }
    if {[string compare normal $state]} {return}
    regsub -all {:+} $os : os
    regsub -all "\[ \r\t\n]+" $os _ os
    regsub -all {:+} $ds : ds
    regsub -all "\[ \r\t\n]+" $ds _ ds
    global Schedule-$os-$ds-active
    global Schedule-$os-$ds-frequency
    global Schedule---frequencyA

    if {![info exists Schedule-$os-$ds-active]} {
	share Schedule-$os-$ds-active
	trace variable Schedule-$os-$ds-active w [subst { uplevel #0 {
	    catch {[list unset Schedule-$os-$ds-activeA]}
	    [list array set Schedule-$os-$ds-activeA] \[[list set \
	    Schedule-$os-$ds-active]] } ;# }]
    }

    if {![info exists Schedule-$os-$ds-frequency]} {
	share Schedule-$os-$ds-frequency
    } else {
	array set Schedule---frequencyA [set Schedule-$os-$ds-frequency]
    }

    # Reconfigure Schedule widgets
    foreach _ $schedwidgets {
	if {[regexp {.+[.]a([^.]+)$} $_ -> e]} {
	    $_ configure -variable Schedule-$os-$ds-activeA($e)
	}
    }

    # When entry boxes commit - write change to shared variable.
    trace variable Schedule---frequencyA w [subst { uplevel #0 {
	    [list set Schedule-$os-$ds-frequency] \
	    \[[list array get Schedule---frequencyA]]} ;# }]

    Oc_EventHandler New _ UpdateSchedule Reset \
	    [subst {trace vdelete Schedule---frequencyA w { uplevel #0 {
	    [list set Schedule-$os-$ds-frequency] \
	    \[[list array get Schedule---frequencyA]]} ;# }}] \
	    -oneshot 1

    # When shared variable changes - write change to entry box
    trace variable Schedule-$os-$ds-frequency w [subst { uplevel #0 {
	    [list array set Schedule---frequencyA] \[[list set \
	    Schedule-$os-$ds-frequency]] } ;# }]

    Oc_EventHandler New _ UpdateSchedule Reset [subst \
	    {[list trace vdelete Schedule-$os-$ds-frequency w] { uplevel #0 {
	    [list array set Schedule---frequencyA] \[[list set \
	    Schedule-$os-$ds-frequency]] } ;# }}] \
	    -oneshot 1
}
proc ClearSchedule {} {
    global opList opArray

    # Loop over all the outputs and destinations
    foreach o $opList {
	upvar #0 $opArray($o)List destList
	foreach d $destList {
	    remote Oxs_Schedule Set $o $d Active Stage 0
	    remote Oxs_Schedule Set $o $d Active Step 0
	}
    }
}

pack $oframe -side top -fill both -expand 1

update idletasks ;# Help interface display at natural size

}
##########################################################################
# End of GUI script.
##########################################################################

##########################################################################
# Handle Tk
##########################################################################
if {[Oc_Main HasTk]} {
    wm withdraw .        ;# "." is just an empty window.
    package require Ow
    Ow_SetIcon .

    # Evaluate $gui?  (in a slave interp?) so that when this app
    # is run with Tk it presents locally the same interface it
    # otherwise exports to be displayed remotely?

}

# Set up to write log messages to oommf/boxsi.errors.
FileLogger SetFile [file join [Oc_Main GetOOMMFRootDir] boxsi.errors]
Oc_Log SetLogHandler [list FileLogger Log] panic Oc_Log
Oc_Log SetLogHandler [list FileLogger Log] error Oc_Log
Oc_Log SetLogHandler [list FileLogger Log] warning Oc_Log
Oc_Log SetLogHandler [list FileLogger Log] info Oc_Log
Oc_Log SetLogHandler [list FileLogger Log] panic
Oc_Log SetLogHandler [list FileLogger Log] error
Oc_Log SetLogHandler [list FileLogger Log] warning
Oc_Log SetLogHandler [list FileLogger Log] info

Oc_Log AddType infolog ;# Record in log file only
if {$loglevel>0} {
   Oc_Log SetLogHandler [list FileLogger Log] infolog
}

# Create a new Oxs_Destination for each Net_Thread that becomes Ready
Oc_EventHandler New _ Net_Thread Ready [list Oxs_Destination New _ %object]

##########################################################################
# Here's the guts of OXS -- a switchboard between interactive GUI events
# and the set of Tcl commands provided by OXS
##########################################################################
# OXS provides the following classes:
#	FileLogger
#	Oxs_Mif
# and the following operations:
#	Oxs_ProbInit $file $params	: Calls Oxs_ProbRelease.  Reads
#					: $file into an Oxs_Mif object
#                                       : with $params parameter list.
#					: Creates Ext and Output objects
#					: Resets objects (driver)
#	Oxs_ProbReset			: Resets objects (driver)
#       Oxs_SetRestartFlag $flag        : Determines whether the next
#                                       : call to Oxs_ProbInit generates
#                                       : a fresh run, or if instead a
#                                       : checkpoint file is used to
#                                       : restart a previously suspended
#                                       : run.
#	Oxs_Run				: driver takes steps, may generate
#					: Step, Stage, Done events
#	Oxs_ProbRelease			: Destroys Ext and Output objects
#	Oxs_ListRegisteredExtClasses	: returns names
#	Oxs_ListExtObjects		: returns names
#	Oxs_ListEnergyObjects		: returns names
#	Oxs_ListOutputObjects		: returns output tokens
#	Oxs_OutputGet $outputToken $args	: return or write output value
#	Oxs_OutputNames $outputToken	: returns name/type/units of output
#
# Some of these commands only make sense when the solver is in a
# particular state.  For example, [Oxs_Run] can only succeed if a
# problem is loaded.  So, this script has to keep track of the
# state of the solver.
##########################################################################

# Any changes to status get channeled through [ChangeStatus]
set status UNINITIALIZED
trace variable status w [list ChangeStatus $status]
proc ChangeStatus {old args} {
    global status problem
    global control_interface_state
    global output_interface_state
    if {[string match $old $status]} {return}
    if {[string match Done $old]} {
       # Limit stage changes available from Done state
       if {![string match "Loading..." $status] \
           && ![string match "Initializing..." $status]} {
          set status Done
          return
       }
    }
    trace vdelete status w [list ChangeStatus $old]
    trace variable status w [list ChangeStatus $status]
    Oc_EventHandler DeleteGroup ChangeStatus
    switch -exact -- $status {
	"" {
	    # The initial state -- no problem loaded.
	    # Also the state after any problem load fails, or
	    # a problem is released.
	    set control_interface_state disabled
	    set output_interface_state disabled
	}
	Loading... {
	    set control_interface_state disabled
	    set output_interface_state disabled
	    # Let interface get updated with above changes, then
	    # call ProblemLoad
	    after idle LoadProblem problem
	}
	Initializing... {
	    set control_interface_state disabled
	    set output_interface_state disabled
	    after idle Reset
	}
	Pause {
	    # A problem is loaded, but not running.
	    set control_interface_state normal
	    set output_interface_state normal
	}
	Run {
	    after idle Loop Run
	}
	Relax {
	    Oc_EventHandler New _ Oxs Stage [list set status Pause] \
		    -oneshot 1 -groups [list ChangeStatus]
	    after idle Loop Relax
	}
        Step {
            Oc_EventHandler New _ Oxs Step [list set status Pause] \
                    -oneshot 1 -groups [list ChangeStatus]
            after idle Loop Step
        }
	Error {
	    set control_interface_state disabled
	    set output_interface_state disabled
	    global exitondone
	    if {$exitondone} {
		exit 1  ;# will trigger ReleaseProblem, etc.
	    } else {
	        # do nothing ?
	    }
	}
	Done {
	    set control_interface_state disabled
	    set output_interface_state normal
	    global exitondone
	    if {$exitondone} {
		exit	;# will trigger ReleaseProblem, etc.
	    } else {
	        # do nothing ?
               global problem
               if {[info exists problem]} {
                  Oc_Log Log "Done \"[file tail $problem]\"" infolog
               }

	    }
	}
	default {error "Status: $status not yet implemented"}
    }
}
# Initialize status to "" -- no problem loaded.
set status ""

# Routine to flush pending log messages.  Used for cleanup
proc FlushLog {} {
    foreach id [after info] {
	foreach {script type} [after info $id] break
	if {[string match idle $type] && [string match *Oc_Log* $script]} {
	    uplevel #0 $script
	}
    }
}

# Be sure any loaded problem gets release on application exit
Oc_EventHandler New _ Oc_Main Shutdown ReleaseProblem -oneshot 1
Oc_EventHandler New _ Oc_Main Shutdown FlushLog
proc ReleaseProblem {} {
    global status
    # We're about to release any loaded problem.  Spread the word.
    Oc_EventHandler Generate Oxs Release
    if {[catch {
	Oxs_ProbRelease
    } msg]} {
	# This is really bad.  Kill the solver.
	#
	# ...but first flush any pending log messages to the
	# error log file.
	FlushLog
	Oc_Log Log "Oxs_ProbRelease FAILED:\n\t$msg" panic
	exit
    }
    global stagerequest number_of_stages problem
    set stagerequest {}
    set number_of_stages {}
    if {[info exists problem]} {
       Oc_Log Log "End \"[file tail $problem]\"" infolog
    }
    set status ""
}

proc ProbInfo {} {
   set txt "[Oc_Main GetAppName] version [Oc_Main GetVersion]\n"
   global tcl_platform
   append txt "Running on: [info hostname]\n"
   append txt   "OS/machine: $tcl_platform(os)/$tcl_platform(machine)\n"
   if {[info exists tcl_platform(user)]} {
      append txt "User: $tcl_platform(user)\t"
   }
   append txt "PID: [Oc_Main GetPid]"
   if {[string length [Oc_Main GetExtraInfo]]} {
      append txt "\n[Oc_Main GetExtraInfo]"
   }
   return $txt
}

proc MeshGeometry {} {
   # Returns a tweaked version of the mesh geometry string
   set geostr [Oxs_MeshGeometryString]
   if {[regexp {([0-9]+) cells$} $geostr dummy cellcount]} {
      while {[regsub {([0-9])([0-9][0-9][0-9])( |$)} \
                 $cellcount {\1 \2} cellcount]} {}
      regsub {([0-9]+) cells$} $geostr "$cellcount cells" geostr
   }
   return $geostr
}

set workdir [Oc_DirectPathname "."]  ;# Initial working directory
Oc_Log AddSource LoadProblem
proc LoadProblem {fname} {
   # Side effects: If problem $f is successfully loaded, then the
   # working directory is changed to the directory holding $f.
   # Also, f is changed to a direct (i.e., absolute) pathname.
   # This way, the file can be loaded again later without worrying
   # about changes to the working directory.

   global status step autorun_pause workdir
   global stage stagerequest number_of_stages
   global killtags
   upvar 1 $fname f

   # We're about to release any loaded problem.  Spread the word.
   Oc_EventHandler Generate Oxs Release

   set f [Oc_DirectPathname $f]
   set newdir [file dirname $f]
   set origdir [Oc_DirectPathname "."]

   set msg "not readable"
   if {![file readable $f] || [catch {
      cd $newdir
      set workdir $newdir
   } msg] || [catch {
      global MIF_params
      if {[llength $MIF_params]} {
         set ps "Params: $MIF_params"
      } else {
         set ps "no params"
      }
      Oc_Log Log "Start \"$f\", $ps" infolog
      Oxs_ProbInit $f $MIF_params ;# includes a call to Oxs_ProbReset
      global update_extra_info
      append update_extra_info "\nMesh geometry: [MeshGeometry]"
      Oc_Main SetExtraInfo $update_extra_info
      set pi [ProbInfo]
      Oc_Log Log $pi infolog
      flush stderr ; puts "Start  [file tail $f], $ps\n$pi" ; flush stdout
   } msg] || [catch {
      global regression_test regression_testname
      if {$regression_test} {
         # This is not a standard run, but part of a regression test.
         # Make some standard modifications to the effective contents
         # of the import MIF file to make it more amenable to such
         # testing.  See the LoadTestSetup method in base/mif.tcl for
         # details.
         #   Note: Do this setup before creating Oxs_Output objects,
         # or else output objects get the basename as specified in the
         # MIF file, not the special basename use for regression
         # tests.
         [Oxs_GetMif] RegressionTestSetup \
            $regression_test $regression_testname

         # If regression_test == 1, then this is a "load" test run on
         # a mif file out of the oxs/examples directory.  In that
         # case, set stage and total iteration limits to small values
         # so the test runs quickly.  See the LoadTestSetup method in
         # base/driver.h for details.
         if {$regression_test==1} {
            # Load test; set stage and total iteration limits to small
            # values so test runs quickly.
            Oxs_DriverLoadTestSetup
         }
      }
      foreach o [Oxs_ListOutputObjects] {
         Oxs_Output New _ $o
      }
   } msg]} {
      # Error; the problem has been released
      global errorInfo errorCode
      after idle [subst {[list set errorInfo $errorInfo]; \
                            [list set errorCode $errorCode]; [list \
           Oc_Log Log "Error loading $f:\n\t$msg" error LoadProblem]}]
      set status Error
      cd $origdir  ;# Make certain cwd is same as on entry
      set workdir $origdir
   } else {
      set mif [Oxs_GetMif]
      foreach {crc fsize} [$mif GetCrc] break
      Oc_Log Log "Loaded \"[file tail $f]\", CRC: $crc, $fsize bytes " infolog
      foreach {step stage number_of_stages} [Oxs_GetRunStateCounts] break
      set stagerequest $stage
      set script {set status Pause}
      if {!$autorun_pause} {
         # Run
         append script {; after 1 {set status Run}}
         ## The 'after 1' is to allow the solver console
         ## (as opposed to the interface console), if any,
         ## an opportunity to initialize and display before
         ## the solver begins chewing CPU cycles.  Otherwise,
         ## the console never(?) displays and the interface
         ## can't be brought up either.
      }
      after idle [list SetupInitialSchedule $script]
      Oc_EventHandler New _ Oxs Release \
         [list $mif KillApps $killtags] -oneshot 1
   }
}

proc SetupInitialSchedule {script} {
    # Need account server; HACK(?) - we know it's in [global a]
    # need MIF object; HACK(?) - retrieve it from OXS
    upvar #0 a acct
    if {![$acct Ready]} {
	Oc_EventHandler New _ $acct Ready [info level 0] -oneshot 1
	return
    }
    set mif [Oxs_GetMif]
    Oc_EventHandler New _ $mif ScheduleReady $script -oneshot 1 \
            -groups [list $mif]
    $mif SetupSchedule $acct
}

Oc_Log AddSource Reset
proc Reset {} {
    global status stage step stagerequest
    set status Pause
    Oc_EventHandler Generate Oxs Cleanup
    if {[catch {
	Oxs_ProbReset
    } msg]} {
	global errorInfo errorCode
	after idle [subst {[list set errorInfo $errorInfo]; \
		[list set errorCode $errorCode]; [list \
		Oc_Log Log "Reset error:\n\t$msg" error Reset]}]
	ReleaseProblem
	set status Error
    } else {
	set stage 0
	set stagerequest 0
	set step 0
	set status Pause
        # Should the schedule be reset too?
        # No, we decided that a Reset during interactive operations should
        # keep whatever schedule has been set up interactively.  For batch
        # use, resets will be rare, and if it turns out that in that case
        # we should reset the schedule, we'll add it when that becomes clear.
    }
}

# Keep taking steps as long as the status is unchanged,
# remaining one of Run, Relax.
Oc_Log AddSource Loop
proc Loop {type} {
    global status
    if {![string match $type $status]} {return}
    if {[catch {Oxs_Run} msg]} {
	global errorInfo errorCode
	after idle [subst {[list set errorInfo $errorInfo]; \
		[list set errorCode $errorCode]; \
		[list Oc_Log Log $msg error Loop]}]
	ReleaseProblem
	set status Error
    } else {
	;# Fire event handlers
	foreach ev $msg {
	    # $ev is a 4 item list: <event_type state_id stage step>
	    set event [lindex $ev 0]
	    switch -exact -- $event {
		STEP {
		    Oc_EventHandler Generate Oxs Step \
			    -stage [lindex $ev 2] \
			    -step [lindex $ev 3]
		}
		STAGE_DONE {
		    Oc_EventHandler Generate Oxs Stage
		}
		RUN_DONE {
		    Oc_EventHandler Generate Oxs Done
		}
		default {
		    after idle [list Oc_Log Log \
			    "Unrecognized event: $event" error Loop]
		    ReleaseProblem
	    	    set status Error
		}
	    }
	}
    }
    after idle [info level 0]
}

# When the Load... menu item chooses a problem,
# $problem is the file to load.
trace variable problem w {uplevel #0 set status Loading... ;#}

# Update Stage and Step counts for Oxs_Schedule
Oc_EventHandler New _ Oxs Step [list set step %step]
Oc_EventHandler New _ Oxs Step [list set stage %stage]
Oc_EventHandler New _ Oxs Step {
    if {$stagerequest != %stage} {
		trace vdelete stagerequest w {
		    if {[info exists stage] && $stage != $stagerequest} {
			Oxs_SetStage $stagerequest
			set stage [Oxs_GetStage]
			if {$stage != $stagerequest} {
			    after idle [list set stagerequest $stage]
			}
		    } ;#}
	set stagerequest %stage
		trace variable stagerequest w {
		    if {[info exists stage] && $stage != $stagerequest} {
			Oxs_SetStage $stagerequest
			set stage [Oxs_GetStage]
			if {$stage != $stagerequest} {
			    after idle [list set stagerequest $stage]
			}
		    } ;#}
    }
}
		trace variable stagerequest w {
		    if {[info exists stage] && $stage != $stagerequest} {
			Oxs_SetStage $stagerequest
			set stage [Oxs_GetStage]
			if {$stage != $stagerequest} {
			    after idle [list set stagerequest $stage]
			}
		    } ;#}

# Terminate Loop when solver is Done
Oc_EventHandler New _ Oxs Done [list set status Done]

# All problem releases should request cleanup first
Oc_EventHandler New _ Oxs Release [list Oc_EventHandler Generate Oxs Cleanup]

##########################################################################
# Our server offers GeneralInterface protocol services -- start it
##########################################################################
Net_Server New server -alias [Oc_Main GetAppName] \
	-protocol [Net_GeneralInterfaceProtocol $gui {
		Oxs_Output Oxs_Schedule
	}]
$server Start 0

##########################################################################
# Track the threads known to the account server
#	code mostly cribbed from mmLaunch
#	good candidate for library code?
##########################################################################
# Get Thread info from account server:
proc Initialize {acct} {
   # Now that connections to servers are established, it's safe
   # to finish initialization and possibly start computing.
   global problem
   set problem $problem  ;# Fire trace

   global nice
   if {$nice} {
      Oc_MakeNice
   }

   AccountReady $acct
}
proc AccountReady {acct} {
    set qid [$acct Send threads]
    Oc_EventHandler New _ $acct Reply$qid [list GetThreadsReply $acct] \
        -groups [list $acct]
    Oc_EventHandler New _ $acct Ready [list AccountReady $acct] -oneshot 1
}
proc GetThreadsReply { acct } {
    # Set up to receive NewThread messages, but only one handler per account
    Oc_EventHandler DeleteGroup GetThreadsReply-$acct
    Oc_EventHandler New _ $acct Readable [list HandleAccountTell $acct] \
            -groups [list $acct GetThreadsReply-$acct]
    set threads [$acct Get]
    Oc_Log Log "Received thread list: $threads" status
    # Create a Net_Thread instance for each element of the returned
    # thread list
    if {![lindex $threads 0]} {
        foreach triple [lrange $threads 1 end] {
	    NewThread $acct [lindex $triple 1]
        }
    }
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

    # Do not keep any connections to yourself!
    if {[string match [$acct OID]:* $id]} {return}

    Net_Thread New _ -hostname [$acct Cget -hostname] \
            -accountname [$acct Cget -accountname] -pid $id
}
# Delay "nice" of this process until any children are spawned.
Net_Account New a
if {[$a Ready]} {
    Initialize $a
} else {
    Oc_EventHandler New _ $a Ready [list Initialize $a] -oneshot 1
}

vwait forever

