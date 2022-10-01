# FILE: boxsi.tcl
#
# The Batch-mode OOMMF eXtensible Solver Interface

# Support libraries
package require Oc 2
package require Oxs 2
package require Net 2

# Make background errors fatal.  Otherwise, since boxsi is assumed
# to run non-interactively, a background error can cause the boxsi
# to hang, if, for example, the error occurs in proc Initialize
# and as a result initialization is not completed.
rename bgerror orig_boxsi_bgerror
proc bgerror { msg } {
   Oc_Log Log "FATAL BACKGROUND ERROR: $msg" error
   orig_boxsi_bgerror $msg
   puts stderr "\nFATAL BACKGROUND ERROR: $msg"
   catch {exit 13}
}

Oc_IgnoreTermLoss  ;# Try to keep going, even if controlling terminal
## goes down.

# Application description boilerplate
Oc_Main SetAppName Boxsi
Oc_Main SetVersion 2.0b0
regexp \\\044Date:(.*)\\\044 {$Date: 2016/01/31 02:14:37 $} _ date
Oc_Main SetDate [string trim $date]
Oc_Main SetAuthor [Oc_Person Lookup dgp]
Oc_Main SetHelpURL [Oc_Url FromFilename [file join [file dirname \
        [file dirname [file dirname [Oc_DirectPathname [info \
        script]]]]] doc userguide userguide\
        OOMMF_eXtensible_Solver_Int.html]]
Oc_Main SetDataRole producer

# Command line options
Oc_CommandLine ActivateOptionSet Net

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

if {![Oc_Option Get OxsLogs directory _]} {
   set logfile [file join $_ boxsi.errors]
} else {
   set logfile [file join [Oc_Main GetOOMMFRootDir] boxsi.errors]
}
Oc_CommandLine Option logfile {
   {name {expr {![string match {} $name]}}}
} {
   global logfile;  set logfile $name
} [subst {Name of log file (default is $logfile)}]

Oc_CommandLine Option loglevel {
   {level {expr {[regexp {^[0-9]+$} $level]}}}
} {
   global loglevel;  set loglevel $level
} {Level of log detail to boxsi.errors (default is 1)}
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
   set thread_limit [Oc_GetThreadLimit]
   if {[string match {} $thread_limit]} {
      set maxnote {}
   } else {
      set maxnote "; max is $thread_limit"
   }
   set threadcount_request [Oc_EnforceThreadLimit [Oc_GetMaxThreadCount]]
   Oc_CommandLine Option threads {
      {count {expr {[regexp {^[0-9]+$} $count] && $count>0}}}
   } {
      global threadcount_request
      set threadcount_request [Oc_EnforceThreadLimit $count]
   } [subst {Number of concurrent threads\
                (default is $threadcount_request$maxnote)}]
} else {
   set threadcount_request 1  ;# Safety
}

# NUMA (non-uniform memory access) support
set numanodes none
if {[Oc_NumaAvailable]} {
   set numanodes [Oc_GetDefaultNumaNodes]
   Oc_CommandLine Option numanodes {
      {nodes {regexp {^([0-9 ,]*|auto|none)$} $nodes}}
   } {
      global numanodes cmdline_numanodes
      set cmdline_numanodes [set numanodes [string trim $nodes]]
      # The cmdline_numanodes variable is used to detect the situation
      # where numanodes are set on the command line by an abbreviated
      # form of the option string, such as "-numa" instead of
      # "-numanodes", to prevent the log report from showing two
      # numanodes entries. (See proc ProbOptions for implementation
      # details.)
   } [subst {NUMA memory and run nodes (or "auto" or "none")\
                (default is "$numanodes")}]
}

# Default output directory; if {}, then use directory containing MIF file.
set output_directory {}
if {[info exists env(OOMMF_OUTDIR)]} {
   set output_directory $env(OOMMF_OUTDIR)
}
if {[string match {} $output_directory]} {
   set default_output_dirstr "MIF file directory"
} else {
   set default_output_dirstr $output_directory
}
Oc_CommandLine Option outdir {
   {dir {expr {![string match {} $dir]}}}
} {
   global output_directory
   set output_directory [Oc_DirectPathname $dir]
} "Directory for output (default is $default_output_dirstr)"

# Checkpoint (restart) file options
Oc_CommandLine Option restart {
    {flag {expr {$flag==0 || $flag==1 || $flag==2}} \
        {= 0 (default=no), 1 (force) or 2 (try)}}
} {
    global restart_flag; set restart_flag $flag
} {1 => use <basename>.restart file to restart simulation}
set restart_flag 0
Oxs_SetRestartFlag $restart_flag

# Note: At present, the restart file dir is set from the command line
# at problem start and never changed.  Trying to change this once a
# problem is loaded is problematic, because we want any droppings in
# the old directory cleaned-up before using the new directory.
set restart_file_directory {}
if {[info exists env(OOMMF_RESTARTFILEDIR)]} {
   set restart_file_directory $env(OOMMF_RESTARTFILEDIR)
} else {
   set restart_file_directory $output_directory
}
if {[string match {} $restart_file_directory]} {
   set default_restart_dirstr "MIF file directory"
} else {
   set default_restart_dirstr $restart_file_directory
}
Oc_CommandLine Option restartfiledir {
   {dir {expr {![string match {} $dir]}}}
} {
   global restart_file_directory;  set restart_file_directory $dir
} "Directory for restart files (default is $default_restart_dirstr)"


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
if {[catch {Oxs_SetRestartFileDir $restart_file_directory} msg]} {
   Oc_Log Log $msg error
   exit 1
}

if {[Oc_HaveThreads]} {
   if {$threadcount_request<1} {set threadcount_request 1}
   set threadcount [Oc_EnforceThreadLimit $threadcount_request]
   Oc_SetMaxThreadCount $threadcount
   set aboutinfo "Number of threads: $threadcount"
   set thread_limit [Oc_GetThreadLimit]
   if {![string match {} $thread_limit]} {
      append aboutinfo " (limit is $thread_limit)"
   }
} else {
   set aboutinfo "Single threaded build"
}

if {[Oc_NumaAvailable]} {
   set numanodes [string trim $numanodes]
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

# Regression test setup.
if {$regression_test>0} {
   # This is not a standard run, but part of a regression test.  Make
   # some standard modifications to the effective contents of the import
   # MIF file to make it more amenable to such testing.  See the
   # SetRegressionTestLevel proc and RegressionTestSetup method in
   # base/mif.tcl for details. NB: In LoadProblem below there is a call
   # to Oxs_DriverLoadTestSetup for adjusting stage and iteration
   # limits.
   Oxs_Mif SetRegressionTestLevel $regression_test $regression_testname
}

##########################################################################
# Checkpoint control
##########################################################################
proc UpdateCheckpointControl { name1 name2 op } {
   global checkpoint_control checkpoint_control_list
   foreach {name value} $checkpoint_control_list {
      if {[string compare $value $checkpoint_control($name)]==0} {
         continue   ;# No change in setting
      }
      switch -exact $name {
         disposal {
            Oxs_SetCheckpointDisposal $value
            set checkpoint_control(disposal) $value
         }
         filename {
            error "Checkpoint filename not allowed to change\
                   during simulation run."
         }
         interval {
            Oxs_SetCheckpointInterval $value
            set checkpoint_control(interval) $value
         }
         timestamp {
            error "Checkpoint timestamp can't be varied by user"
         }
         default {
            error "Unrecognized checkpoint control array item: \"$name\""
         }
      }
   }
}

Oc_EventHandler New _ Oxs Checkpoint {
   set checkpoint_control(timestamp) [Oxs_CheckpointTimestamp]
   trace vdelete checkpoint_control_list w UpdateCheckpointControl
   set checkpoint_control_list [array get checkpoint_control]
   trace variable checkpoint_control_list w UpdateCheckpointControl
}

proc InitializeCheckpointControl {} {
   global checkpoint_control checkpoint_control_list
   global checkpoint_timestamp
   if {![Oxs_IsProblemLoaded]} {
      # Default settings
      set checkpoint_control(disposal) standard
      set checkpoint_control(filename) ""
      set checkpoint_control(interval) 15
   } else {
      set checkpoint_control(disposal) [Oxs_GetCheckpointDisposal]
      set checkpoint_control(filename) [Oxs_GetCheckpointFilename]
      set checkpoint_control(interval) [Oxs_GetCheckpointInterval]
   }
   set checkpoint_control(timestamp) "never"
   ## No checkpoint file (yet) from this run

   trace vdelete checkpoint_control_list w UpdateCheckpointControl
   set checkpoint_control_list [array get checkpoint_control]
   trace variable checkpoint_control_list w UpdateCheckpointControl
}

InitializeCheckpointControl

##########################################################################
# Define the GUI of this app to be displayed remotely by clients of the
# Net_GeneralInterface protocol.  Return $gui in response to the
# GetGui message.
##########################################################################
set gui {
    package require Oc 2
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

regexp \\\044Date:(.*)\\\044 {$Date: 2016/01/31 02:14:37 $} _ date
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
if {![Oc_Option Get Menu show_console_option _] && $_} {
   $fmenu add command -label "Show Console" -command { console show } \
       -underline 5
}
$fmenu add command -label "Close Interface" -command closeGui -underline 6
$fmenu add command -label "Clear Schedule" -command ClearSchedule -underline 6

$fmenu add command -label "Checkpoint Control..." -underline 5 \
      -command [list CheckpointControlButton $fmenu "Checkpoint Control..."] \
      -state normal
$fmenu add separator
$fmenu add command -label "Exit [Oc_Main GetAppName]" \
	-command exit -underline 1
share checkpoint_control_list
share checkpoint_timestamp

########################################################################
# Checkpoint control dialog.  Note that this is running on the server
# (mmLaunch) side, so we need to locate checkpoint.tcl relative to OOMMF
# root.

source [file join [Oc_Main GetOOMMFRootDir] app oxs checkpoint.tcl]

########################################################################

Ow_StdHelpMenu $hmenu

set menuwidth [Ow_GuessMenuWidth $menubar]
set bracewidth [Ow_GetWindowTitleSize [wm title .]]
if {$bracewidth<$menuwidth} {
   set bracewidth $menuwidth
}
set brace [canvas .brace -width $bracewidth -height 0 -borderwidth 0 \
        -highlightthickness 0]
pack $brace -side top
## Note: OID is assigned prior to this code being run, so we don't
##       have to handle window title changing from OID assignment.

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
	{remote Oxs_Output Send [lindex $oplbsel 0] [lindex $dlbsel 0] 1} \
	-text Send] - -sticky new
lappend output_tkbox_list $sframe.send

#
# FOR NOW JUST HACK IN THE EVENTS WE SUPPORT
#	eventually this may be driver-dependent
set events [list Step Stage Done]
set schedwidgets [list $sframe.send]
foreach event $events {
    set active [checkbutton $sframe.a$event -text $event -anchor w \
	    -variable Schedule---activeA($event)]
    $active configure -command [subst -nocommands \
		{Schedule Active [$active cget -variable] $event}]
    lappend output_tkbox_list $active
   if {[string compare Done $event]==0} {
      grid $active -sticky nw
      lappend schedwidgets $active
   } else {
    Ow_EntryBox New frequency $sframe.f$event -label every \
	    -autoundo 0 -valuewidth 4 \
	    -variable Schedule---frequencyA($event) \
	    -callback [list EntryCallback $event] \
	    -valuetype posint -coloredits 1 -writethrough 0 \
	    -outer_frame_options "-bd 0"
    lappend output_owbox_list $frequency
    grid $active [$frequency Cget -winpath] -sticky nw
    lappend schedwidgets $active $frequency
   }
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
    Ow_PropagateGeometry .
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
	    remote Oxs_Schedule Set $o $d Active Done 0
	}
    }
}

# Override default window delete handling
proc DeleteVerify {} {
   set response [Ow_Dialog 1 "Exit [Oc_Main GetInstanceName]?" \
                    warning "Are you *sure* you want to exit \
			[Oc_Main GetInstanceName]?" {} 1 Exit Cancel]
   if {$response == 0} {
      after 10 exit
   }
}
wm protocol . WM_DELETE_WINDOW DeleteVerify

pack $oframe -side top -fill both -expand 1

update idletasks ;# Help interface display at natural size
Ow_PropagateGeometry .

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

# Set up to write log messages to oommf/boxsi.errors (or alternative).
Oc_FileLogger StderrEcho 1  ;# Echo log messages to stderr
Oc_FileLogger SetFile $logfile
Oc_Log SetLogHandler [list Oc_FileLogger Log] panic Oc_Log
Oc_Log SetLogHandler [list Oc_FileLogger Log] error Oc_Log
Oc_Log SetLogHandler [list Oc_FileLogger Log] warning Oc_Log
Oc_Log SetLogHandler [list Oc_FileLogger Log] info Oc_Log
Oc_Log SetLogHandler [list Oc_FileLogger Log] panic
Oc_Log SetLogHandler [list Oc_FileLogger Log] error
Oc_Log SetLogHandler [list Oc_FileLogger Log] warning
Oc_Log SetLogHandler [list Oc_FileLogger Log] info

Oc_Log AddType infolog ;# Record in log file only
if {$loglevel>0} {
   Oc_Log SetLogHandler [list Oc_FileLogger Log] infolog
}
if {$loglevel>1} {
   # Dump status messages too
   Oc_Log SetLogHandler [list Oc_FileLogger Log] status
}

if {[catch {Oc_AddCLogFile $logfile} errmsg]} { ;# For C++-level logging
   Oc_Log Log $errmsg panic
   exit
}

##########################################################################
# Here's the guts of OXS -- a switchboard between interactive GUI events
# and the set of Tcl commands provided by OXS
##########################################################################
# OXS provides the following classes:
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
               after idle exit  ;# will trigger ReleaseProblem, etc.
               # Put this on after idle list so that any pending events
               # have an opportunity to process.
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

proc KillApps { oidlist } {
   global kill_apps_wait account_server
   if {[llength $oidlist]==0} {
      set kill_apps_wait 0
      return 
   }
   set kill_apps_wait 1
   set qid [$account_server Send kill $oidlist]
   Oc_EventHandler New _ $account_server Reply$qid KillAppsReply
   # What happens if account server crashes?
}

proc KillAppsReply {} {
   global kill_apps_wait
   set kill_apps_wait 0
}


proc ReleaseProblem { {errcode 0} } {
   if {![Oxs_IsProblemLoaded]} { return }
   # Get list of OIDs to be killed as per command line -kill option.
   # Note that Oxs_ProbRelease destroys the MIF object, so we have
   # to get the list first.

   # Kill any outstanding "after idle Loop" commands
   foreach ai [after info] {
      foreach {script type} [after info $ai] {
         if {[string match Loop* $script]} {
            after cancel $ai
         }
      }
   }

   global killtags
   set mif [Oxs_GetMif]
   # If error occurs during problem load, then mif object might not be
   # set.
   if {[llength [info commands $mif]] == 1} {
      set killoids [$mif KillOids $killtags]
   } else {
      set killoids {}
   }

   if {[catch {
      Oxs_ProbRelease $errcode
   } msg]} {
      # This is really bad.  Kill the solver.
      #
      # ...but first flush any pending log messages to the
      # error log file.
      Oxs_FlushLog
      Oc_Log Log "Oxs_ProbRelease FAILED:\n\t$msg" panic
      exit
   }

   if {[catch {Oc_EventHandler Generate Oxs Release} msg]} {
      Oc_Log Log "Oxs Release handler FAILURE:\n\t$msg" panic
      exit
   }

   KillApps $killoids
   global kill_apps_wait
   while {$kill_apps_wait} {
      vwait kill_apps_wait
   }

   # Note: There are traces on the following variables.
   global status stagerequest number_of_stages problem
   set stagerequest $stagerequest
   set number_of_stages $number_of_stages
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

proc ProbOptions {} {
   # Returns a list of options in effect for currently loaded problem
   global argv MIF_params
   # Arguments from the command line
   for {set i 0} {$i<[llength $argv]-1} {incr i} {
      if {[regexp {^-+(.*)} [lindex $argv $i] dummy elt]} {
         if {[string match {} $elt]} { break }
         set elt [string trimleft $elt "-"]
         set opts($elt) [lindex $argv [incr i]]
      }
   }
   # Current MIF parameters; for boxsi these should be set on the
   # command line, but it shouldn't hurt to reset them from
   # MIF_params.
   if {[llength $MIF_params]} {
      set opts(parameters) $MIF_params
   }
   # Current thread and numanodes settings may be filled in
   # from environment settings rather than the command line;
   # either way (re)set them here.
   if {[Oc_HaveThreads]} {
      global threadcount
      set opts(threads) $threadcount
      if {[Oc_NumaAvailable]} {
         global numanodes cmdline_numanodes
         if {![info exists cmdline_numanodes] ||
             [string compare $numanodes $cmdline_numanodes]!=0} {
            set opts(numanodes) $numanodes
         }
      }
   }
   set optlist {}
   foreach elt [lsort [array names opts]] {
      lappend optlist "-$elt" $opts($elt)
   }
   return $optlist
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
   global checkpoint_timestamp
   upvar 1 $fname f

   set f [Oc_DirectPathname $f]
   set newdir [file dirname $f]
   set origdir [Oc_DirectPathname "."]

   set msg "not readable"
   if {![file readable $f] || [catch {
      cd $newdir
      set workdir $newdir
   } msg] || [catch {
      ReleaseProblem   ;# Should be a NOP for boxsi
      set opts [ProbOptions]
      Oc_Log Log "Start: \"$f\"\nOptions: $opts" infolog
      global MIF_params update_extra_info
      Oxs_ProbInit $f $MIF_params ;# includes a call to Oxs_ProbReset
      append update_extra_info "\nMesh geometry: [MeshGeometry]"
      set cpf [Oxs_GetCheckpointFilename]
      if {[string match {} $cpf]} {
         append update_extra_info "\nCheckpointing disabled"
      } else {
         append update_extra_info "\nCheckpoint file: $cpf"
      }
      Oc_Main SetExtraInfo $update_extra_info
      set pi [ProbInfo]
      Oc_Log Log $pi infolog
      flush stderr
      puts "Start: \"$f\"\nOptions: $opts\n$pi" ; flush stdout
   } msg] || [catch {
      global regression_test
      if {$regression_test == 1} {
         # regression_test == 1 indicates that this is a "load" test run
         # on a mif file out of the oxs/examples directory.  In that
         # case, set stage and total iteration limits to small values so
         # the test runs quickly.  See the LoadTestSetup method in
         # base/driver.h for details.
         Oxs_DriverLoadTestSetup
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
      InitializeCheckpointControl
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
      if {[Oxs_IsRunDone]==1} {
         after idle [list Oc_EventHandler Generate Oxs Done]
      }
   }
}

proc SetupInitialSchedule {script} {
    # Need account server; HACK(?) - we know it's in
    # [global account_server]
    # need MIF object; HACK(?) - retrieve it from OXS
    upvar #0 account_server acct
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
	ReleaseProblem 1
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

proc GenerateRunEvents { events } {
   # Fire event handlers
   global status
   foreach ev $events {
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
         CHECKPOINT {
            Oc_EventHandler Generate Oxs Checkpoint
         }
         default {
            after idle [list Oc_Log Log \
                           "Unrecognized event: $event" error Loop]
            ReleaseProblem 1
            set status Error
         }
      }
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
	ReleaseProblem 1
	set status Error
    } else {
       # Fire event handlers
       GenerateRunEvents $msg
    }
    after idle [info level 0]
}

# When the Load... menu item chooses a problem,
# $problem is the file to load.
trace variable problem w {uplevel #0 set status Loading... ;#}

# Update Stage and Step counts for Oxs_Schedule
proc ChangeStageRequestTraceCmd { args } {
   global stage stagerequest
   if {[info exists stage] && $stage != $stagerequest} {
      set results [Oxs_SetStage $stagerequest]
      set stage [lindex $results 0]
      GenerateRunEvents [lindex $results 1]
   }
}
Oc_EventHandler New _ Oxs Step [list set step %step]
Oc_EventHandler New _ Oxs Step [list set stage %stage]
Oc_EventHandler New _ Oxs Step {
   if {$stagerequest != %stage} {
      trace remove variable stagerequest write ChangeStageRequestTraceCmd
      set stagerequest %stage
      trace add variable stagerequest write ChangeStageRequestTraceCmd
   }
}
trace add variable stagerequest write ChangeStageRequestTraceCmd

# Terminate Loop when solver is Done. Put this on the after idle list so
# that all other Done event handlers have an opportunity to fire first.
Oc_EventHandler New _ Oxs Done [list after idle set status Done]


# All problem releases should request cleanup first, which includes
# writing a end record to DataTable output.  Next, Oxs_FlushData is
# called to wait for all sent data to be acknowledged by receiver.
# Application termination by KillApps should be held back until after
# Oxs_FlushData completes.
Oc_EventHandler New _ Oxs Release [list Oc_EventHandler Generate Oxs Cleanup]
Oc_EventHandler New _ Oxs Release Oxs_FlushData

# Be sure any loaded problem gets release on application exit.
Oc_EventHandler New _ Oc_Main Shutdown ReleaseProblem -oneshot 1
Oc_EventHandler New _ Oc_Main Shutdown Oxs_FlushLog
Oc_EventHandler New _ Oc_Main Shutdown [list puts "Boxsi run end."]
## The "Boxsi run end." message can be used by scripts to detect the
## end of a boxsi run even if the output channel from boxsi doesn't
## signal EOF because (for example) some boxsi background child
## process (for example an account server) is still running.

##########################################################################
# Our server offers GeneralInterface protocol services -- start it
##########################################################################
Net_Server New server -alias [Oc_Main GetAppName] \
	-protocol [Net_GeneralInterfaceProtocol $gui {
		Oxs_Output Oxs_Schedule
	}]
$server Start 0
Oc_EventHandler New register_failure $server RegisterFail \
    [list error "Failure to register with account server"]] \
    -oneshot 1 -groups [list $server]
Oc_EventHandler New _ $server RegisterSuccess \
    [list $register_failure Delete] -oneshot 1 -groups [list $server]
# One observed failure mode: Boxsi starts, launches an account server,
# registers with it and gets back OID 0.  Then boxsi launches an
# mmArchive instance.  mmArchive comes up but something crashes the
# account server before mmAchive can register with it.  mmArchive is
# unaware of this situation, however, and starts up an account server
# in the normal fashion.  At this time boxsi realizes that the account
# server is down and starts another one.  Suppose now that the account
# server started by mmArchive wins the race to be the one and only
# account server, and suppose further that mmArchive registers with it
# before boxsi finds it.  Not knowing any better, this new account
# server gives out OID 0 to mmArchive.  What happens when boxsi hooks
# up with the new account server and tells the account server that it,
# boxsi, have already been assigned OID 0???

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
# Supported export protocols. (Note: White space is significant.)
global export_protocols
set export_protocols {
   {OOMMF DataTable protocol}
   {OOMMF vectorField protocol}
   {OOMMF scalarField protocol}
}

proc AccountReady {acct} {
    global export_protocols
    set qid [$acct Send services $export_protocols]
    Oc_EventHandler New _ $acct Reply$qid [list GetServicesReply $acct] \
        -groups [list $acct]
    Oc_EventHandler New _ $acct Ready [list AccountReady $acct] -oneshot 1
}
proc GetServicesReply { acct } {
    # Set up to receive NewService messages, but only one handler per account
    Oc_EventHandler DeleteGroup GetServicesReply-$acct
    Oc_EventHandler New _ $acct Readable [list HandleAccountTell $acct] \
            -groups [list $acct GetServicesReply-$acct]
    set services [$acct Get]
    Oc_Log Log "Received service list: $services" status
    # Create a Net_Thread instance for each element of the returned
    # service list. The first element of $services is, as usual, a
    # result code. Following that is a list of quartets, with each
    # quartet of the form: advertisedname sid port protocolname.
    if {![lindex $services 0]} {
        foreach quartet [lrange $services 1 end] {
	    NewService $acct $quartet
        }
    }
}
# Detect and handle newservice messages from account server
proc HandleAccountTell { acct } {
    set message [$acct Get]
    switch -exact -- [lindex $message 0] {
        newservice {
            NewService $acct [lrange $message 1 end]
        }
        deleteservice {
           Oxs_Destination DeleteService [lindex $message 1]
        }
        notify -
        newoid -
        deleteoid {
           # Ignore notifications and OID info
        }
        default {
          Oc_Log Log "Bad message from account $acct:\n\t$message" status
        }
    }
}
# There's a new service; create corresponding local instance
proc NewService { acct quartet } {
   # The quartet import is a four item list of the form
   #   advertisedname  sid  port  fullprotocol
   # which matches the reply to the "threads" request from the account
   # server.
   lassign $quartet appname sid port fullprotocol

   # Safety: Don't connect to yourself
   if {[string match [$acct OID]:* $sid]} {return}

   # Otherwise, setup an Oxs_Destination with lazy Net_Thread construction.
   Oxs_Destination New _ \
      -hostname [$acct Cget -hostname] \
      -accountname [$acct Cget -accountname] \
      -name "${appname}<${sid}>" \
      -fullprotocol $fullprotocol \
      -serverport $port
}

# Delay "nice" of this process until any children are spawned.
Net_Account New account_server
if {[$account_server Ready]} {
    Initialize $account_server
} else {
    Oc_EventHandler New _ $account_server Ready \
        [list Initialize $account_server] -oneshot 1
}

vwait forever

