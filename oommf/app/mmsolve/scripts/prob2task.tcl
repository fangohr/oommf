# FILE: prob2task.tcl
#
# This is a sample batch task file.  Usage example:
#
#        oommf.tcl -fg batchmaster -tk 0 prob2task.tcl
#
# LOCAL MODIFICATIONS: It is recommended you copy this sample file and
#   make modifications to the copy (which is then specified on the
#   batchmaster.tcl command line in place of prob2task.tcl).  You will
#   want to change
#
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
set BASEMIF prob2     ;# Base mif name
#set CELLSIZE 20       ;# Simulation cellsize in nm
#set dlex_list { 30 25 20 15 10 5 }
#set CELLSIZE 40       ;# Simulation cellsize in nm
#set dlex_list { 15 10 5 }
set CELLSIZE 250       ;# Simulation cellsize in nm
set dlex_list { 0.25 0.125 }


# Slave initialization script (with batchsolve.tcl proc redefinitions)
set init_script {
    # Initialize solver. This is run at global scope
    set basename __BASEMIF__      ;# Base mif filename (global)
    set cellsize __CELLSIZE__     ;# Simulation discretization size
    mms_mif New mif
    $mif Read [FindFile ${basename}.mif]
    # Redefine TaskInit and TaskCleanup proc's
    proc SolverTaskInit { args } {
	global mif outtextfile basename cellsize
	set dlex [lindex $args 0]
	set outbasename "$basename-dlex$dlex-cellsize$cellsize"
	# Convert d/lex to A
	set tmp [expr [$mif GetPartWidth]*[$mif GetMs]/$dlex]
	set A [expr $tmp*$tmp*6.2831853e-7]
	$mif SetA $A
	$mif SetCellSize [expr $cellsize*1e-9]  ;# Convert to meters
	$mif SetOutBaseName $outbasename
	$mif SetUserComment "muMag Problem 2, d/lex=$dlex,\
		cellsize=$cellsize nm"
	set outtextfile [open "$outbasename.odt" "a+"]
	puts $outtextfile [GetTextData header \
	  "mmSolve run on $basename.mif, for d/lex=$dlex (A=[$mif GetA])"]
	flush $outtextfile
    }
    proc SolverTaskCleanup { args } {
        global outtextfile
        close $outtextfile
    }
}
# Substitute $BASEMIF in for __BASEMIF__ in above script
regsub -all -- __BASEMIF__ $init_script $BASEMIF init_script
# Substitute $CELLSIZE in for __CELLSIZE__ in above script
regsub -all -- __CELLSIZE__ $init_script $CELLSIZE init_script

# Set init script into TaskInfo
$TaskInfo SetSlaveInitScript $init_script

# Create task list
foreach dlex $dlex_list {
    $TaskInfo AppendTask "d/lex=$dlex" "BatchTaskRun $dlex"
}
