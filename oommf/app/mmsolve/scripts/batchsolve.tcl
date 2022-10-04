# FILE: batchsolve.tcl
#
# This file must be interpreted by the mmsolve shell.
#
# Command line options:
#
package require Oc 2
package require Mms 2

Oc_ForceStderrDefaultMessage	;# use stderr, not dialog for messages

Oc_Main SetAppName batchsolve
Oc_Main SetVersion 2.0b0
Oc_Main SetDataRole producer

Oc_CommandLine ActivateOptionSet Net

Oc_CommandLine Option interface {
        {flag {expr {![catch {expr {$flag && $flag}}]}} {= 0 or 1}}
    } {
	global interface; set interface $flag
} {Whether to export an interface (default=1)}
set interface 1	;# Default: export interface

Oc_CommandLine Option restart {
        {flag {expr {![catch {expr {$flag && $flag}}]}} {= 0 or 1}}
    } {
	global restart; set restart $flag
} {1 => use <basename>.log file to restart simulation}
set restart 0	;# Default: start solving at beginning

Oc_CommandLine Option end_exit {
        {flag {expr {![catch {expr {$flag && $flag}}]}} {= 0 or 1}}
    } {
	global end_exit; set end_exit $flag
} {Whether to explicitly exit after solving (default=1)}
# Set default based on whether we're sourced into another script
if {[string compare $argv0 [info script]] == 0} {
    # Started as the main application
    set end_exit 1	;# Default: exit when solving completed
} else {
    # Sourced into another application
    set end_exit 0	;# Default: Don't exit our caller app!
}

Oc_CommandLine Option end_paused {} {
	global end_paused; set end_paused 1
} {Enter event loop after solving problem}
set end_paused 0  ;# Default is to not pause

Oc_CommandLine Option start_paused {} {
	global start_paused; set start_paused 1
} {Pause solver after loading problem}
set start_paused 0  ;# Default is to not pause

Oc_CommandLine Option [Oc_CommandLine Switch] {
	{{file optional} {} {Problem description file\
                                (MIF 1.x format) to load and run}}
    } {
	global miffile; set miffile $file
} {End of options; next argument is file}
set miffile ""

# More details on the options:
#
#   -interface <0|1>       Whether to export an interactive interface (cf.
#                          InteractiveServerInit).  Currently this is the
#                          only network-active code inside this file.  Note,
#                          however, that this file may be sourced from inside
#                          another script (e.g., batchslave.tcl) that can
#                          provide network access to the code in this file.
#  -start_paused         Pause solver after loading problem.
#  -end_paused           Pause solver and enter event loop at bottom of
#                          script rather than just falling off the end.
#  -end_exit <0|1>       If 1, explicitly call exit at bottom of script.
#                          If mif file has been specified on the command line,
#                          then the default value is 1.  If a mif file has
#                          not been specified, then the default value is 0.
#  -restart <0|1>        If 1, then uses <basename>.log log file to restart
#                          run from last checkpoint.  If this is not possible,
#                          then a warning is issued and the problem runs from
#                          the beginning.  This property holds for *all*
#                          problems fed to the solver (not just the first).
#  file			  Immediately load and run the specified mif file.

Oc_CommandLine Parse $argv

# Accessible global variables:
#
#   mif  --- Handle to a mms_mif object
# solver --- Handle to a mms_solver object
# search_path --- Directories to look in for *.mif files.

# For documentation on the global variables inputs, outputs, events,
# and commands, see mmsolve2d.tcl.

Oc_IgnoreTermLoss  ;# Try to keep going, even if controlling terminal
## goes down.

# Set up to write log messages to oommf/batchsolve.errors.
# Note that the solver class will later re-direct this to its own log
# file.
FileLogger SetFile \
	[file join [Oc_Main GetOOMMFRootDir] batchsolve.errors]
Oc_Log SetLogHandler [list FileLogger Log] panic Oc_Log
Oc_Log SetLogHandler [list FileLogger Log] error Oc_Log
Oc_Log SetLogHandler [list FileLogger Log] warning Oc_Log
Oc_Log SetLogHandler [list FileLogger Log] info Oc_Log
Oc_Log SetLogHandler [list FileLogger Log] panic
Oc_Log SetLogHandler [list FileLogger Log] error
Oc_Log SetLogHandler [list FileLogger Log] warning
Oc_Log SetLogHandler [list FileLogger Log] info

global mif solver interface start_paused end_paused restart

set default_script_dir [file dirname [info script]]
set root_oommf [file join $default_script_dir .. .. ..]
if {![info exists search_path]} {set search_path {}}
set search_path [concat $search_path [list           \
	. ./data ./scripts                           \
	.. ../data ../scripts                        \
	$default_script_dir                          \
	[file join $default_script_dir data]         \
	[file join $default_script_dir scripts]      \
	[file join $root_oommf app mmpe]             \
	[file join $root_oommf app mmpe data]        \
	[file join $root_oommf app mmpe scripts]     \
	[file join $root_oommf app mmsolve]          \
	[file join $root_oommf app mmsolve data]     \
	[file join $root_oommf app mmsolve scripts]]]

proc FindFile { name } {
    global search_path
    set load_name {}
    foreach dir $search_path {
	set attempt [file join $dir $name]
	if {[file readable $attempt]} {
	    set load_name $attempt
	    break
	}
    }
    if {[string match {} $load_name]} {
	Oc_Log Log "Unable to locate file $name.  Search path: $search_path" \
		warning
    }
    return $load_name
}

proc InteractiveServerInit {} {
    global commands events inputs outputs interface protocol

    if { !$interface } { return }

    # Support libraries
    package require Net 2

    array set inputs {}
    # The commands which can be invoked remotely
    set commands {
	Solver	{
	    {Run SetBatchSolverState Run}  
	    {Pause SetBatchSolverState Pause}  
	    {"End Task" SetBatchSolverState EndTask}
	}
    }

    # The events generated by this thread.
    array set events {
	Iteration	{1 solver GetODEStepCount}
	ControlPoint	{1 solver GetFieldStepCount}
    }

    # The quantities which this thread produces as output
    #	The last entry should be moved to be set by controller
    array set outputs {
	Magnetization	{
	    vectorField
	    {Iteration ControlPoint}
	    solver
	    WriteMagFile
	    datafile
	}
	TotalField 		{
	    vectorField
	    {Iteration ControlPoint}
	    solver
	    WriteFieldFile
	    datafile
	}
	DataTable		{
	    DataTable
	    {Iteration ControlPoint}
	    ""
	    MakeDataTable
	    DataTable
	}
    }

   Oc_EventHandler Bindtags IoServer IoServer

    Net_Protocol New protocol -name "OOMMF batchsolver protocol 0.2"
    $protocol Init {
	Oc_EventHandler New _ mms_solver ChangeState \
	    [list $connection Tell Status %state] \
	    -groups [list $connection]
    }
    $protocol AddMessage start Commands {} {
	global commands
	return [list start [list 0 $commands]]
    }
    $protocol AddMessage start Outputs {} {
	global outputs
	return [list start [list 0 [array get outputs]]]
    }
    $protocol AddMessage start Inputs {} {
	global inputs
	return [list start [list 0 [array get inputs]]]
    }
    $protocol AddMessage start Events {} {
	global events
	return [list start [list 0 [array get events]]]
    }
    $protocol AddMessage start SetHandler {args} {
	return [list start [list 0 [eval SetHandler $args]]]
    }
    $protocol AddMessage start SetInput {args} {
	return [list start [list 0 {}]]
    }
    $protocol AddMessage start UnsetHandler {args} {
	return [list start [list 0 [eval UnsetHandler $args]]]
    }
    # Deprecated
    $protocol AddMessage start RemoteRelayOutput {args} {
	return [list start [list 0 [eval RemoteRelayOutput $args]]]
    }
    $protocol AddMessage start InteractiveOutput {args} {
	return [list start [list 0 [eval InteractiveOutput $args]]]
    }
    $protocol AddMessage start GetSolverState {} {
       global solver
       if {[info exists solver]} {
          return [list start [list 0 [$solver GetSolverState]]]
       }
       return [list start [list 1 {}]]
    }
    $protocol AddMessage start SetBatchSolverState {args} {
	return [list start [list 0 [eval SetBatchSolverState $args]]]
    }
   $protocol AddMessage start TrackIoServers {} {
      return [list start [list 0 [eval TrackIoServers $connection]]]
   }
    Net_Server New server -protocol $protocol -alias batchsolve

    proc SetHandler {event data host acct pid cnt} {
	global events
	Net_Thread New t -hostname $host -accountname $acct -pid $pid
	Oc_EventHandler DeleteGroup $event-$data-$t
	if {[string match interactive $event]} {
            Oc_EventHandler New _ InteractiveOutput $data \
                    [list RelayOutput $data $t] \
                    -groups [list $t $event-$data-$t]
        } else {
            set evinfo $events($event)
            set srcName [lindex $evinfo 1]
            upvar #0 $srcName src
            Oc_EventHandler New _ $src $event \
                    [list GenericHandler $event $data $cnt $t] \
                    -groups [list $t $event-$data-$t]
        }
        global threadrefs
        set threadrefs($event-$data-$t) 1
    }

    proc UnsetHandler {event data host acct pid} {
	Net_Thread New t -hostname $host -accountname $acct -pid $pid
	Oc_EventHandler DeleteGroup $event-$data-$t
        global threadrefs
        catch {unset threadrefs($event-$data-$t)}
        if {![llength [array names threadrefs *-$t]]} {
            $t Delete
        }
    }

    proc GenericHandler {event data cnt thread} {
	global events
	set evinfo $events($event)
	if {[lindex $evinfo 0]} {
	    set srcName [lindex $evinfo 1]
	    upvar #0 $srcName src
	    set evcount [eval $src [lindex $evinfo 2]]
	    if {$evcount % $cnt} {
		return
	    }
	}
	RelayOutput $data $thread
    }
    
    # Deprecated
    proc RemoteRelayOutput { data host acct pid } {
	Net_Thread New t -hostname $host -accountname $acct -pid $pid
	RelayOutput $data $t
    }

    Oc_EventHandler Bindtags InteractiveOutput InteractiveOutput
    proc InteractiveOutput {data} {
	Oc_EventHandler Generate InteractiveOutput $data
    }
    
    # Send the data indicated by 'data' to the process represented by 'thread'
    proc RelayOutput { data thread } {
        # Check to see that 'thread' is ready to send commands to its process
        if {[catch {$thread Ready} ready] || !$ready} {
            # 'thread' isn't ready.  Two possible reasons:
            if {![string match {} [info commands $thread]]} {
                # 1. Thread exists, but just isn't ready yet.
                #    --> Catch its Ready event
                Oc_EventHandler New _ $thread Ready \
                        [list RelayOutput $data $thread] \
                        -oneshot 1 -groups [list $thread]
            }
            # 2. There is no such thread instance --> give up.
        } else {
            # Thread is ready.  Construct and send data.
            global outputs
            set opinfo $outputs($data)
            set srcName [lindex $opinfo 2]
            if {[string length $srcName]} {
                upvar #0 $srcName src
                set datavalue [eval $src [lindex $opinfo 3]]
            } else {
                set datavalue [eval [lindex $opinfo 3]]
            }
            set method [lindex $opinfo 4]
            $thread Send $method $datavalue
        }
    }
    
   ##########################################################################
   # Track the servers known to the account server,
   #	code mostly cribbed from mmsolve2d.tcl
   ##########################################################################
   # Get server info from account server:
   proc Initialize {acct} {
      Oc_MakeNice
      AccountReady $acct
   }

   # Supported export protocols. (Note: White space is significant.)
   global io_protocols
   set io_protocols {
      {OOMMF ProbEd protocol}
      {OOMMF DataTable protocol}
      {OOMMF vectorField protocol}
   }

   proc AccountReady {acct} {
      global io_protocols
      set qid [$acct Send services $io_protocols]
      Oc_EventHandler New _ $acct Reply$qid [list GetServicesReply $acct] \
         -groups [list $acct]
      Oc_EventHandler New _ $acct Ready [list AccountReady $acct] -oneshot 1
   }
   proc GetServicesReply { acct } {
      # Set up to receive NewService messages, but only one handler per account
      Oc_EventHandler DeleteGroup GetServicesReply-$acct
      Oc_EventHandler New _ $acct Readable [list HandleAccountTell $acct] \
         -groups [list $acct GetServicesReply-$acct]
      global io_servers
      array set io_servers {}
      set services [$acct Get]
      Oc_Log Log "Received service list: $services" status
      if {![lindex $services 0]} {
         foreach quartet [lrange $services 1 end] {
	    NewIoServer $acct $quartet
         }
      }
   }
   # Detect and handle newservice messages from account server
   proc HandleAccountTell { acct } {
      set message [$acct Get]
      switch -exact -- [lindex $message 0] {
         newservice {
            NewIoServer $acct [lrange $message 1 end]
         }
         deleteservice {
            DeleteIoServer $acct [lindex $message 1]
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

   # Track I/O servers in global array io_servers, where the index is the sid
   # (service id, of the form oid:servernumber, e.g., 5:0), and the value
   # is the list { advertisedname  port  fullprotocol }
   proc NewIoServer { acct quartet } {
      # The quartet import is a four item list of the form
      #   advertisedname  sid  port  fullprotocol
      # which matches the reply to the "threads" request from the account
      # server.
      lassign $quartet appname sid port fullprotocol
      set hostname [$acct Cget -hostname]
      set accountname [$acct Cget -accountname]

      # Safety: Don't connect to yourself
      if {[string match [$acct OID]:* $sid]} {return}

      global io_servers
      if {![info exists io_servers($sid)]} {
         set details [list $appname $port $fullprotocol $hostname $accountname]
         set io_servers($sid) $details
         Oc_EventHandler Generate IoServer NewIoServer \
            -sid $sid -details $details
      }
   }

   proc DeleteIoServer { acct sid } {
      global io_servers
      if {[info exists io_servers($sid)]} {
         unset io_servers($sid)
         Oc_EventHandler Generate IoServer DeleteIoServer -sid $sid
      }
   }
   proc TrackIoServers { connection } {
      Oc_EventHandler New _ IoServer NewIoServer \
         [list $connection Tell NewIoServer [list %sid %details]]\
         -groups [list $connection]
      Oc_EventHandler New _ IoServer DeleteIoServer \
         [list $connection Tell DeleteIoServer [list %sid]] \
         -groups [list $connection]
      global io_servers
      return [array get io_servers]
   }

   # Delay "nice" of this process until any children are spawned.
   Net_Account New a
   if {[$a Ready]} {
      Initialize $a
   } else {
      Oc_EventHandler New _ $a Ready [list Initialize $a] -oneshot 1
   }

   # Delay "nice" of this process until any children are spawned.
   set starterror [catch {$server Start 0} errmsg]
   Net_Account New a
   if {[$a Ready]} {
      Initialize $a
   } else {
      Oc_EventHandler New _ $a Ready [list Initialize $a] -oneshot 1
   }
   if {$starterror} {
      error $errmsg
   }

   ########################################################################

} ;# end InteractiveServerInit
    
proc MakeDataTable {} {
    global solver
    set Bfield_multiplier 1000  ;# Convert T to mT
    
    set table [list [list Iteration "" [$solver GetODEStepCount]]]
    lappend table [list "Field Updates" {} [$solver GetFieldUpdateCount]]
    lappend table [list "Sim Time" "ns" [expr {[$solver GetSimulationTime]*1e9}]]
    lappend table [list "Time Step" "ns" [expr {[$solver GetTimeStep]*1e9}]]
    lappend table [list "Step Size" "" [$solver GetStepSize]]
    set Bfield [$solver GetNomAppliedField]
    lappend table [list Bx mT [expr {$Bfield_multiplier * [lindex $Bfield 0]}]]
    lappend table [list By mT [expr {$Bfield_multiplier * [lindex $Bfield 1]}]]
    lappend table [list Bz mT [expr {$Bfield_multiplier * [lindex $Bfield 2]}]]
    set Bmag [expr {sqrt([lindex $Bfield 0]*[lindex $Bfield 0] \
	    + [lindex $Bfield 1]*[lindex $Bfield 1] \
	    + [lindex $Bfield 2]*[lindex $Bfield 2])}]
    lappend table [list B mT [expr {$Bfield_multiplier * $Bmag}]]
    lappend table [list "|m x h|" "" [$solver GetMaxMxH]]
    set mag [$solver GetAveMag]
    lappend table [list Mx/Ms "" [lindex $mag 0]]
    lappend table [list My/Ms "" [lindex $mag 1]]
    lappend table [list Mz/Ms "" [lindex $mag 2]]
    set energies [join [$solver GetEnergyDensities]]
    lappend table [list "Total Energy"      "J/m^3" [lindex $energies 1]]
    lappend table [list "Exchange Energy"   "J/m^3" [lindex $energies 3]]
    lappend table [list "Anisotropy Energy" "J/m^3" [lindex $energies 5]]
    lappend table [list "Demag Energy"      "J/m^3" [lindex $energies 7]]
    lappend table [list "Zeeman Energy"     "J/m^3" [lindex $energies 9]]
    lappend table [list "Max Angle" "deg" [$solver GetMaxAngle]]
    lappend table [list \
            "@Filename:[$solver GetOutputBasename]-interactive.odt" {} 0]
    return $table
}

if {[string match {} [info procs GetTextData]]} {
    # GetTextData not defined
    proc GetTextData { type {title {}}} {
	# This is a tri-modal alternate to MakeDataTable
	# "type" should be either "header", "data", or "trailer".
	# The "Title" field is only used if type == header
	switch -exact -- $type {
	    header  {
		set line "# ODT 1.0\n# Table Start"
		if {![string match {} $title]} {
		    append line "\n# Title: $title"
		}
		append line "
# Units :  \\\n#    {}        {}         mT        mT        mT       {}       {}       {}        {J/m^3}
# Columns: \\\n# {Field #} Iteration     Bx        By        Bz      Mx/Ms    My/Ms    Mz/Ms       Energy"
	    }
	    data {
	    # Dump data
		global solver
		set fieldindex [$solver GetFieldStepCount]
		set stepcount [$solver GetODEStepCount]
		set simtime   [expr {[$solver GetSimulationTime]*1e9}]
		set Bfield [$solver GetNomAppliedField]
		set Bx [expr {[lindex $Bfield 0]*1000.}]
		set By [expr {[lindex $Bfield 1]*1000.}]
		set Bz [expr {[lindex $Bfield 2]*1000.}]
		set mxh [$solver GetMaxMxH]
		set mag [$solver GetAveMag]
		set mx [lindex $mag 0]
		set my [lindex $mag 1]
		set mz [lindex $mag 2]
		set energies [join [$solver GetEnergyDensities]]
		set etotal  [lindex $energies 1]
		set eexch   [lindex $energies 3]
		set eanis   [lindex $energies 5]
		set edemag  [lindex $energies 7]
		set ezeeman [lindex $energies 9]
		set maxang  [$solver GetMaxAngle]
		set line [format \
		  "%9d %10d %9.3f %9.3f %9.3f  %8.4f %8.4f %8.4f  %13.4f" \
		  $fieldindex $stepcount $Bx $By $Bz $mx $my $mz $etotal]
	    }
	    trailer {
		set line "# Table End"
	    }
	    default {
		error "Bad type request in GetTextData: $type"
	    }
	}
	return $line
    }
}

proc SetBatchSolverState { newstate } {
    # newstate should be one of Pause, Run, EndTask or Reset.
    # If current state is EndTask, then all calls are ignored
    # until a Reset is sent, which sets batch_solver_state to {}
    # and returns.
    global solver user_args batch_solver_state interface
    if {[string match Reset $newstate]} {
	set batch_solver_state {}
	return
    }
    if {[string match EndTask $batch_solver_state]} {
	return   ;# Do nothing
    }
    if {![info exists solver]} {
	error "solver not defined inside call to SetBatchSolverState"
    }

    switch -exact -- $newstate {
	Pause {
	    $solver ChangeSolverState Pause
	    set batch_solver_state Pause
	}
	Run {
	    $solver ChangeSolverState Run
	    set batch_solver_state Run
	}
	EndTask {
	    $solver ChangeSolverState Pause
	    eval SolverTaskCleanup $user_args
	    set batch_solver_state EndTask
	    if { $interface } {
		# Send end-of-run message to each thread supporting
		# the DataTable protocol
		set eormsg [list [list "@Filename:" {} 0]]
		foreach thread [Net_Thread Instances] {
		    if {[$thread Ready] \
			    && [string match DataTable [$thread Protocol]]} {
			$thread Send DataTable $eormsg
		    }
		}
	    }
	}
	Default {
	    error "Illegal state request in SetBatchSolverState: $newstate"
	}
    }
}

proc BatchTaskLaunch { args } {
    # This routine launches the solver on a problem, and returns
    # without waiting for the solver to complete.  A calling proc
    # may watch the global batch_solver_state to determine when
    # the solver is complete.  The companion routine BatchTaskRun
    # calls this one and waits for problem completion before
    # returning.
    #
    # The import "args" are copied into the global user_args, which
    # are only used by SolverTaskInit and SolverTaskCleanup.

    global mif solver start_paused user_args restart

    set user_args $args

    eval SolverTaskInit $user_args
    if {![info exists solver]} {
	# First time through
	mms_solver New solver $mif $restart
        Oc_EventHandler New _ Oc_Main Exit BatchTaskExit \
                -groups [list $solver]
	InteractiveServerInit     ;# Bring up interactive interface
    } else {
	$solver Reset $mif $restart
    }
    SetBatchSolverState Reset

    # Setup solver state change callbacks
    Oc_EventHandler DeleteGroup ${solver}_callbacks
    Oc_EventHandler New _ \
	    $solver Iteration BatchTaskIterationCallback \
	    -groups [list ${solver}_callbacks]
    Oc_EventHandler New _ \
	    $solver ControlPoint BatchTaskRelaxCallback \
	    -groups [list ${solver}_callbacks]
    Oc_EventHandler New _ \
	    $solver Pause [list SetBatchSolverState Pause] \
	    -groups [list ${solver}_callbacks]
    Oc_EventHandler New _ \
	    $solver FieldDone [list SetBatchSolverState EndTask] \
	    -groups [list ${solver}_callbacks]

    # Start simulation unless delayed start is requested.
    if {$start_paused} {
	SetBatchSolverState Pause
	set start_paused 0     ;# Only pause first time
    } else {
	SetBatchSolverState Run
    }
}

proc BatchTaskRun { args } {
    # This proc starts up the solver and doesn't return until
    # and EndTask event is generated
    global batch_solver_state
    set batch_solver_state {}
    eval BatchTaskLaunch $args
    while {![string match EndTask $batch_solver_state]} {
	vwait batch_solver_state
    }
    # Give other events a chance to clear
    after 100 set lastchance 1 ; vwait lastchance
    return "OK"
}

proc BatchTaskExit {} {
    # Performs cleanup, but does not call 'exit'.  More work is
    # needed on this routine.
    global mif solver
    if {[info exists solver]} {
        SetBatchSolverState EndTask
	Oc_EventHandler DeleteGroup ${solver}_callbacks
	catch { $solver Delete }
    }
    catch { $mif Delete }
}

# The next 3 procs, SolverTaskInit, SolverTaskCleanup and
# BatchTaskRelaxCallback, are default procs used when batchsolve.tcl
# is run with command line input.  When run in slave mode via a task
# list, these procs may be redefined in the task list slave_init_script.
#
if {[string match {} [info procs SolverTaskInit]]} {
    # SolverTaskInit not defined
    proc SolverTaskInit { args } {
	# This is the default SolverTaskInit used when batchsolve.tcl
	# is run with command line input.  When run in slave mode via
	# a task list, this proc may be redefined in the task list
	# slave_init_script.  The import 'args', if non-empty is used
	# as the name of a mif file to load (using the file searching
	# mechanism provided by the FindFile proc).  NB: The 'args'
	# list may be used completely differently by a replacement
	# SolverTaskInit defined in an outer control script.
	global mif outtextfile
	if {![info exists mif]} {
	    mms_mif New mif
	}
	if {[llength $args]>0} {
	    set miffile [FindFile [lindex $args 0]]
	    $mif Read $miffile
	}
	set outbasename [$mif GetOutBaseName]
	set outtextfile [open "$outbasename.odt" "a+"]
	if {[info exists miffile]} {
	    puts $outtextfile [GetTextData header \
		    "mmSolve results from input file $miffile"]
	} else {
	    puts $outtextfile [GetTextData header]
	}
	flush $outtextfile
    }
}
#
if {[string match {} [info procs SolverTaskCleanup]]} {
    # SolverTaskCleanup not defined
    proc SolverTaskCleanup { args } {
	global outtextfile
	puts $outtextfile [GetTextData trailer]
	close $outtextfile
    }
}
#
if {[string match {} [info procs BatchTaskRelaxCallback]]} {
    # BatchTaskRelaxCallback not defined
    proc BatchTaskRelaxCallback {} {
	global outtextfile solver mif
	puts $outtextfile [GetTextData data]
	flush $outtextfile
	set filename [format "%s.field%04d.omf" \
		[$mif GetOutBaseName] [$solver GetFieldStepCount]]
	set filename [Oc_DirectPathname $filename]
	set note "Applied field (T): [$solver GetNomAppliedField]\
		\nIteration: [$solver GetODEStepCount]"
	$solver WriteMagFile $filename $note
    }
}
#
if {[string match {} [info procs BatchTaskIterationCallback]]} {
    # BatchTaskIterationCallback not defined
    proc BatchTaskIterationCallback {} {}  ;# Default is NOP
}

if {![string match {} $miffile]} {
    BatchTaskRun $miffile
}

if {$end_paused} {
    if {[info exists batch_solver_state]} {
	SetBatchSolverState Pause
    }
    vwait forever
}

# Use end_exit variable to direct cleanup
if {$end_exit} {
    catch {$mif Delete}
    catch {$solver Delete}
    exit
}

### Debugging code #######################
#
#global tock
#set tock 0
#proc silly {} {
#    flush stderr ; flush stdout
#
#    global tock
#    incr tock
#    puts stderr "Tick $tock"
#    foreach i [after info] {
#	puts stderr " $i: [after info $i]"
#    }
#    flush stderr
#
#    global _Oc_EventHandler_map
#    if {[array exists _Oc_EventHandler_map]} {
#	parray _Oc_EventHandler_map
#	puts "Event count: [array size _Oc_EventHandler_map]"
##	catch {
##	    set handle $_Oc_EventHandler_map(_Net_Connection1,Reply2)
##	    puts "HANDLE: $handle"
##	    puts "SCRIPT: [$handle Script]"
##          # NB: Script is a private method.  To use this, remove the
##          #  'private' keyword from this method in eventHandler.tcl.
##	}
#	flush stdout
#    }
#
#    after 10000 silly
#}
#
# silly
#
### End debugging code ###################
