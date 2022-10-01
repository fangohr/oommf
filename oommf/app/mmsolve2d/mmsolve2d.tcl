# FILE: mmSolve2D.tcl
#
# This file must be interpreted by the mmsolve shell.

# Support libraries
package require Oc 2
package require Mms 2
package require Net 2

Oc_IgnoreTermLoss  ;# Try to keep going, even if controlling terminal
## goes down.

Oc_Main SetAppName mmSolve2D
Oc_Main SetVersion 2.0b0
regexp \\\044Date:(.*)\\\044 {$Date: 2015/10/09 05:50:35 $} _ date
Oc_Main SetDate [string trim $date]
Oc_Main SetAuthor [Oc_Person Lookup dgp]
Oc_Main SetHelpURL [Oc_Url FromFilename [file join [file dirname \
        [file dirname [file dirname [Oc_DirectPathname [info \
        script]]]]] doc userguide userguide \
	2D_Micromagnetic_Solver_mmS.html]]
Oc_Main SetDataRole producer

Oc_CommandLine ActivateOptionSet Net

Oc_CommandLine Option restart {
        {flag {expr {![catch {expr {$flag && $flag}}]}} {= 0 or 1}}
    } {
        global restart_option; set restart_option $flag
} {1 => use <basename>.log file to restart simulation}
set restart_option 0

Oc_CommandLine Parse $argv

if {[Oc_Main HasTk]} {
    wm withdraw .        ;# "." is just an empty window.
    package require Ow
    Ow_SetIcon .
} else {
    # Set up to write log messages to oommf/mmsolve2d.errors.
    # Note that the solver class will later re-direct this to its own log
    # file.
    FileLogger SetFile \
	    [file join [Oc_Main GetOOMMFRootDir] mmsolve2d.errors]
    Oc_Log SetLogHandler [list FileLogger Log] panic Oc_Log
    Oc_Log SetLogHandler [list FileLogger Log] error Oc_Log
    Oc_Log SetLogHandler [list FileLogger Log] warning Oc_Log
    Oc_Log SetLogHandler [list FileLogger Log] info Oc_Log
    Oc_Log SetLogHandler [list FileLogger Log] panic
    Oc_Log SetLogHandler [list FileLogger Log] error
    Oc_Log SetLogHandler [list FileLogger Log] warning
    Oc_Log SetLogHandler [list FileLogger Log] info
}

# Set the values of some global variables which control the
# interaction with the ThreadGui module in mmLaunch.
#
# $commands is a list which tells the interface what commands
# to display in the middle panel of "Runtime Controls".  The
# even elements of this list are labels for a row of buttons.
# Each corresponding odd element of this list is a list of
# button descriptions.  Each button description is also a list.
# The first element of a button description is the text label
# that should be displayed on the button.  The remaining elements
# are the command to be sent back to this app when the button 
# is invoked.
#
# In the following, there is one row of buttons, labeled "Solver".
# There is one button in the row, labeled "LoadProblem" and when
# that button is invoked, the command "RemoteRelayInput ProblemDescription"
# will be sent by the interface back to this app.
# 
set commands {
    Solver	{{LoadProblem RemoteRelayInput ProblemDescription}}
}

# $inputs is a list which tells the interface what data types this
# app takes as inputs.  The even elements of this list are labels
# identifying the data types.  The corresponding odd elements are
# a list which describes how to manage data of that type.  The
# first element is the name of a protocol which delivers data
# of that type.  This information is used by the interface to
# filter which threads can be selected to supply input data.
# The second element is the message within that protocol which
# requests a value of that data type.  The third element is
# the command in this app which will be completed with the
# data value returned from the source thread, and then evaluated.
# 
# For example, the following declares a single input data type,
# "ProblemDescription", for this app.  Data of that type can be
# retrieved from servers which speak the "ProbEd" protocol. 
# To get a data value, send the message "GetProbDescFile" to
# one of those servers.  Then when the server returns a value,
# evaluate the command "LoadProbDescFile $value".
#
array set inputs {
    ProblemDescription	{ProbEd
			 GetProbDescFile
			 LoadProbDescFile
                        }
}

# When this app starts up, it does not export any events or outputs.
# It needs to have a problem loaded first.  See below in the 
# Load ProbDecFile proc for info on how these global variables are
# used.
#
array set events {}
array set outputs {}

Oc_EventHandler Bindtags LoadProbDescFile LoadProbDescFile
proc LoadProbDescFile { mif_file threadID} {

    # temporarily reset logging to the default log file (not a
    # problem-specific one).  When the solver is constructed/reset
    # below, logging will get redirected to the file for the
    # problem being solved.
    FileLogger SetFile \
	    [file join [Oc_Main GetOOMMFRootDir] mmsolve2d.errors]

    mms_mif New mif
    if {[catch {$mif Read $mif_file} msg]} {
	Oc_Log Log $msg error
	Oc_EventHandler Generate LoadProbDescFile BadMifFile -thread $threadID
	$mif Delete
	file delete $mif_file
	return
    }

    global commands events inputs outputs
#    catch { unset inputs } ;###
    catch { unset outputs }  ;# "outputs" is _not_ always set!
    catch { unset events }
    catch { unset commands } 
#    array set inputs {}   ;###
# The commands which can be invoked remotely
# See above for description of format.
set commands {
    Solver	{
		 {Reset ResetThread} 
                 {LoadProblem RemoteRelayInput ProblemDescription}
                 {Run ChangeSolverState Run}  
                 {Relax ChangeSolverState Relax}  
                 {Pause ChangeSolverState Pause}  
		 {Field- FieldAdjust -1}
		 {Field+ FieldAdjust 1}
		}
}

# The events generated by this thread.
#
# $events is a list which tells the interface what events are
# generated within this app which the interface may schedule
# outputs for.  The even elements of this list are the names of
# the events.  Each corresponding odd element of this list is a
# list which describes the event.  The first element is a boolean
# which indicates whether the interface should display an "every"
# box for scheduling outputs against this event.  The second element
# is the name of a global variable in this app which holds a handle
# to the instance which is generating the events.  The third element
# is the method to invoke on that instance to get a count on that
# event.  
# 
# Below, two events "Iteration" and "ControlPoint" are described.
# Each supports the "every" box in the interface, and the count
# for each event is returned by [$solver GetODEStepCount] and
# [$solver GetFieldStepCount] respectively.
#
array set events {
    Iteration	 {1 solver GetODEStepCount}
    ControlPoint {1 solver GetFieldStepCount}
}

# The quantities which this thread produces as output
#	The last entry should be moved to be set by controller
#
# $outputs is a list which describes the outputs this app is
# able to send to other apps.  The even elements of this list
# are the names of the outputs.  The corresponding odd elements
# are output description lists.  The first element is the name
# of a protocol which accepts data of this type.  The second 
# element is the list of events for which the interface should
# offer to schedule output of this output type.  The third and
# fourth elements combine to describe how to get an output
# value from the objects and commands in this app.  The fifth
# element is the method within the protocol which is used to
# send the data value to the other app.
#
# For example, the Magnetization output type may be sent to other
# apps which speak the "vectorField" protocol.  The interface uses
# that information to filter which apps may be selected to receive
# Magnetization type output data.  The interface offers to schedule
# output of Magnetization data against both Iteration and ControlPoint
# events.  When it comes time to output a Magnetization data value,
# the command [$solver WriteMagFile] is evaluated and its return
# value is sent to other apps using the "datafile" message of the
# vectorField protocol.
#
array set outputs {
    Magnetization	{vectorField
			 {Iteration ControlPoint}
			 solver
			 WriteMagFile
			 datafile
                        }
    TotalField 		{vectorField
			 {Iteration ControlPoint}
			 solver
			 WriteFieldFile
			 datafile
                        }
    DataTable		{DataTable
			 {Iteration ControlPoint}
			 ""
			 MakeDataTable
			 DataTable
			}
}

    global solver protocol restart_option
    if {![info exists solver]} {
	# First pass---new solver
	mms_solver New solver $mif $restart_option
	Oc_EventHandler Generate LoadProbDescFile InterfaceChange
	# Shutdown handlers:
        Oc_EventHandler New sdh $solver Delete [list exit] \
                -oneshot 1 -groups [list $solver]
	Oc_EventHandler New _ $solver RunDone [list RunEndNotify]
        Oc_EventHandler New _ Oc_Main Shutdown \
               "[list $sdh Delete] ; [list $solver Delete]" \
                -oneshot 1 -groups [list $solver]
        # The solver Destructor raises a solver Delete event.  If the
        # above $solver Delete handler is in place when this Oc_Main
        # Shutdown handler calls $solver Delete, then the solver
        # Destructor causes Oc_Main Exit to be called.  Oc_Main Exit
        # raises an Oc_Main Shutdown event, and around and around we
        # go.
    } else {
	# Reset existing solver
	ResetThread $mif
    }
    $mif Delete  ;# $solver makes a private copy of $mif
    file delete $mif_file
}

Oc_EventHandler Bindtags IoServer IoServer

Net_Protocol New protocol -name "OOMMF solver protocol 0.2"
$protocol Init {
    global solver
    if {![info exists solver]} {
	Oc_EventHandler New _ LoadProbDescFile InterfaceChange \
		[list $connection Tell InterfaceChange] -oneshot 1 \
		-groups [list $connection]
    }
    Oc_EventHandler New _ mms_solver ChangeState \
	    [list $connection Tell Status %state] \
	    -groups [list $connection]
    Oc_EventHandler New _ RelayInput ProblemDescription \
	    [list $connection Tell Status "Loading from %threadLabel..."] \
	    -groups [list $connection]
    Oc_EventHandler New _ LoadProbDescFile BadMifFile \
	    [list $connection Tell Status \
	    "Load failed: Invalid Problem Description (MIF) from %thread"] \
	    -groups [list $connection]
    Oc_EventHandler New _ mms_solver BigAngle \
	    [list $connection Tell Status \
	    "WARNING: Spin-spin angle (%angle degrees) exceeds threshold"] \
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
    return [list start [list 0 [eval SetInput $args]]]
}
$protocol AddMessage start UnsetHandler {args} {
    return [list start [list 0 [eval UnsetHandler $args]]]
}
$protocol AddMessage start InteractiveOutput {args} {
    return [list start [list 0 [eval InteractiveOutput $args]]]
}
# Deprecated -- here only to service old clients
$protocol AddMessage start RemoteRelayOutput {args} {
    return [list start [list 0 [eval RemoteRelayOutput $args]]]
}
$protocol AddMessage start RemoteRelayInput {args} {
    return [list start [list 0 [eval RemoteRelayInput $args]]]
}
$protocol AddMessage start FieldAdjust {args} {
    return [list start [list 0 [eval FieldAdjust $args]]]
}
$protocol AddMessage start ResetThread {args} {
    return [list start [list 0 [eval ResetThread $args]]]
}
$protocol AddMessage start GetSolverState {} {
   global solver
   if {[info exists solver]} {
      return [list start [list 0 [$solver GetSolverState]]]
   }
   return [list start [list 1 {}]]
}
$protocol AddMessage start ChangeSolverState {args} {
    return [list start [list 0 [eval ChangeSolverState $args]]]
}
$protocol AddMessage start TrackIoServers {} {
   return [list start [list 0 [eval TrackIoServers $connection]]]
}

Net_Server New server -protocol $protocol -alias mmSolve2D
$server Start 0

proc SetInput {data host acct pid} {
    Oc_EventHandler DeleteGroup SetInput
    Net_Thread New t -hostname $host -accountname $acct -pid $pid
    Oc_EventHandler New _ RemoteRelayInput $data \
	    [list RelayInput $data $t] -groups [list $t SetInput]
}

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

Oc_EventHandler Bindtags InteractiveOutput InteractiveOutput
proc InteractiveOutput { data } {
    Oc_EventHandler Generate InteractiveOutput $data
}

# Deprecated -- here only to service old clients
proc RemoteRelayOutput { data host acct pid } {
    Net_Thread New t -hostname $host -accountname $acct -pid $pid
    RelayOutput $data $t
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

Oc_EventHandler Bindtags RemoteRelayInput RemoteRelayInput
proc RemoteRelayInput { data } {
    Oc_EventHandler Generate RemoteRelayInput $data
}

# Send request for 'data' to the process represented by 'thread'
Oc_EventHandler Bindtags RelayInput RelayInput
proc RelayInput { data thread } {
    # Check to see that 'thread' is ready to send commands to its process
    if {[catch {$thread Ready} ready] || !$ready} {
        # 'thread' isn't ready.  Two possible reasons:
        if {![string match {} [info commands $thread]]} {
            # 1. Thread exists, but just isn't ready yet.
            #    --> Catch its Ready event
            Oc_EventHandler New _ $thread Ready \
                    [list RelayInput $data $thread] \
                    -oneshot 1 -groups [list $thread]
        }
        # 2. There is no such thread instance --> give up.
    } else {
        # Thread is ready.  Construct and send request.
	regexp {[0-9]+} [$thread Cget -pid] pid
	Oc_EventHandler Generate RelayInput $data \
		-threadLabel [$thread Cget -alias]<$pid>
        global inputs 
        set ininfo $inputs($data)
        set qid [$thread Send [lindex $ininfo 1]]    
        Oc_EventHandler New _ $thread Reply$qid \
               [list ReceiveRelayInputReply $thread $data] \
               -oneshot 1 -groups [list $thread]
        # Consider handler for timeout event?
    }
}

proc ReceiveRelayInputReply {thread data} {
    global inputs
    set reply [$thread Get]
    if {![lindex $reply 0]} {
        set ininfo $inputs($data)
	regexp {[0-9]+} [$thread Cget -pid] pid
        eval [lindex $ininfo 2] [lrange $reply 1 end] \
		[list [$thread Cget -alias]<$pid>]
    }
}

# Initialize solver
#$solver Configure -default_control_spec [list -torque 1e-4]
set Bfield_multiplier 1000  ;# Convert T to mT

proc MakeDataTable {} {
    global solver Bfield_multiplier
    set table [list [list Iteration "" [$solver GetODEStepCount]]]
    lappend table [list "Field Updates" {} [$solver GetFieldUpdateCount]]
    lappend table [list "Sim Time" "ns" \
	    [expr {[$solver GetSimulationTime]*1e9}]]
    lappend table [list "Time Step" "ns" \
	    [expr {[$solver GetTimeStep]*1e9}]]
    lappend table [list "Step Size" "" [$solver GetStepSize]]
    set Bfield [$solver GetNomAppliedField]
    lappend table [list Bx mT \
	    [expr {$Bfield_multiplier * [lindex $Bfield 0]}]]
    lappend table [list By mT \
	    [expr {$Bfield_multiplier * [lindex $Bfield 1]}]]
    lappend table [list Bz mT \
	    [expr {$Bfield_multiplier * [lindex $Bfield 2]}]]
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
    lappend table [list "@Filename:[$solver GetOutputBasename].odt" {} 0]
    return $table
}

proc FieldAdjust { step } {
    global solver 
    if {$step != 0} {
	$solver IncrementFieldStep $step
    }
}

proc ResetThread { {mif {}} } {
    global solver restart_option
    $solver Reset $mif $restart_option
}

proc ChangeSolverState { newstate } {
    global solver
    $solver ChangeSolverState $newstate
}

proc RunEndNotify {} {
    # Send end-of-run message to each thread supporting
    # the DataTable protocol
    set eormsg [list [list "@Filename:" {} 0]]
    foreach thread [Net_Thread Instances] {
	if {[$thread Ready] && [string match DataTable [$thread Protocol]]} {
	    $thread Send DataTable $eormsg
	}
    }
}

##########################################################################
# Track the servers known to the account server
#	code mostly cribbed from mmLaunch and Oxsii.
#	Good candidate for library code?
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
########################################################################

vwait forever
