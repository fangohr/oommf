# FILE: multitask.tcl
#
# This is a sample batch task file.  Usage example:
#
#   tclsh oommf.tcl batchmaster [-tk 0] multitask.tcl hostname [port]
#
# Function: The TCL script batchmaster.tcl sources this script to get
#    values for the launch list, the slave init script, and the task
#    list.  The first specifies the number of slaves to launch, and
#    how (and where) to launch them.  (Typically the script
#    batchslave.tcl will be used for slave control.)  The second is a
#    one-time slave initialization script, and the third specifies the
#    list of tasks to be sent to the slaves.  Batchmaster.tcl then
#    launches all the specified slaves, initializes them with the
#    slave init script, and then feeds tasks sequentally from the task
#    list to the slaves.  When a slave completes a task it is given
#    the next unclaimed task from task_list.  If there are no more
#    tasks, the slave is shut down.  When all the tasks are complete,
#    batchmaster.tcl prints a summary of the tasks and exits.
#
#
# LOCAL MODIFICATIONS: It is recommended you copy this sample file and
#   make modifications to the copy (which is then specified on the
#   batchmaster.tcl command line in place of multitask.tcl).  You will
#   want to change
#
#  RMT_TCLSH --- Tclsh to use on remote hosts
#  RMT_OOMMF --- Full path to oommf.tcl on remote hosts
#  RMT_WORK_DIR   --- Working directory on remote hosts
#  BASEMIF   --- ${BASEMIF}.mif should be the name of a mif file that is
#                close to the desired problem specification.  Adjustments
#                can be made in the solver init and task scripts.
# $TaskInfo AppendSlave --- Controls location and number of slave
#                processes started by batchmaster.  Hint: you probably
#                don't want more than one slave process per processor
#                per machine.  You can use rsh to launch slaves on
#                other machines.
# $TaskInfo ModifyHostList --- This is a list of machines that are
#                expected (allowed) to set up slave connections.  For
#                local processing use 'localhost', which is included
#                by default.  To enable remote processing, you will
#                have to specify 'hostname' on the batchmaster.tcl
#                commandline.  (You can specify port=0 to get an
#                arbitrary free port.)
# $TaskInfo SetSlaveInitScript --- This script is sent to each slave
#                when it is first brought up.  Do any one-time
#                initialization here, but also define the procs
#                SolverTaskInit, SolverTaskCleanup and optionally
#                BatchTaskRelaxCallback.  Use SolverTaskInit to
#                perform task-specific initialization.
# $TaskInfo AppendTask --- List of tasks.  Each entry is a pair
#                consisting of a task label (id), and a Tcl script to
#                eval.

# Task script configuration
set RMT_MACHINE    foo
set RMT_TCLSH      tclsh
set RMT_OOMMF      "/path/to/oommf/oommf.tcl"
set RMT_WORK_DIR   "/path/to/oommf/app/mmsolve/data"
set LCL_WORK_DIR   "/path/to/oommf/app/mmsolve/data"
set BASEMIF taskA 
set A_list { 10e-13 10e-14 10e-15 10e-16 10e-17 10e-18 }

# Slave launch commands
$TaskInfo ModifyHostList +$RMT_MACHINE
$TaskInfo AppendSlave 1 "cd $LCL_WORK_DIR \;\
	Oc_Application Exec batchslave -tk 0 %connect_info batchsolve.tcl"
$TaskInfo AppendSlave 1 "exec rsh $RMT_MACHINE \
	cd $RMT_WORK_DIR \\\;\
	$RMT_TCLSH $RMT_OOMMF batchslave -tk 0 %connect_info batchsolve.tcl"

# Slave initialization script (with batchsolve.tcl proc redefinitions)
set init_script {
    # Initialize solver. This is run at global scope
    set basename __BASEMIF__      ;# Base mif filename (global)
    mms_mif New mif
    $mif Read [FindFile ${basename}.mif]
    # Redefine TaskInit and TaskCleanup proc's
    proc SolverTaskInit { args } {
	global mif outtextfile basename
	set A [lindex $args 0]
	set outbasename "$basename-A$A"
	$mif SetA $A
	$mif SetOutBaseName $outbasename
	set outtextfile [open "$outbasename.odt" "a+"]
	puts $outtextfile [GetTextData header \
		"mmSolve run on $basename.mif, with A=[$mif GetA]"]
	flush $outtextfile
    }
}
# Substitute $BASEMIF in for __BASEMIF__ in above script
regsub -all -- __BASEMIF__ $init_script $BASEMIF init_script
$TaskInfo SetSlaveInitScript $init_script

# Create task list
foreach A $A_list {
    $TaskInfo AppendTask "A=$A" "BatchTaskRun $A"
}
