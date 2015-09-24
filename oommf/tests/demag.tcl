# FILE: demag.tcl
#
# Batch task file for demag test problem.  Run via the command
#
#        oommf.tcl -fg batchmaster -tk 0 demag.tcl
#
# MAKING MODIFICATIONS: It is recommended you copy this sample file
#   and make modifications to the copy (which is then specified on
#   the batchmaster.tcl command line in place of demag.tcl).  You
#   will want to change
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
set BASEMIF demag     ;# Base mif name
set run_list { \
	{ConstMag 1000}
	{ConstMag 500}
	{ConstMag 250}
	{ConstMag 200}
	{ConstMag 125}
	{ConstMag 100}
	{ConstMag 62.5}
	{ConstMag 50}
	{ConstMag 40}
	{ConstMag 31.25}
	{ConstMag 25}
	{ConstMag 20}
	{ConstMag 15.625}
	{ConstMag 10}
	{ConstMag 8}
	{3dCharge 1000}
	{3dCharge 500}
	{3dCharge 250}
	{3dCharge 200}
	{3dCharge 125}
	{3dCharge 100}
	{3dCharge 62.5}
	{3dCharge 50}
	{3dCharge 40}
	{3dCharge 31.25}
	{3dCharge 25}
	{3dCharge 20}
	{3dCharge 15.625}
	{3dCharge 10}
	{3dCharge 8}
    }

# Slave initialization script (with batchsolve.tcl proc redefinitions)
set init_script {
    # Initialize solver. This is run at global scope
    set basename __BASEMIF__      ;# Base mif filename (global)
    mms_mif New mif
    $mif Read [FindFile ${basename}.mif]
    # Redefine TaskInit and GetTextData proc's
    proc SolverTaskInit { args } {
	global mif outtextfile basename cellsize
	set kernel [lindex $args 0]
	set cellsize [lindex $args 1]
	set outbasename "$basename-$kernel-cellsize$cellsize"
	# Modify mif info
	$mif SetCellSize [expr $cellsize*1e-9]  ;# Convert to meters
	$mif SetDemagType $kernel
	$mif SetOutBaseName $outbasename
	$mif SetUserComment "Demag test, kernel: $kernel,\
		cellsize=$cellsize nm"
	set outtextfile [open "$basename.odt" "a+"]
	puts $outtextfile [GetTextData header \
	  "mmSolve run on $basename.mif, with kernel=$kernel,\
	  cellsize=$cellsize nm"]
	flush $outtextfile
    }
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
# Units :  \\\n#     nm       {}       {}       {}     degrees       J/m^3
# Columns: \\\n#  Cellsize   Mx/Ms    My/Ms    Mz/Ms   Max_Ang   Demag_Energy"
	    }
	    data {
		# Dump data
		global solver
		set cellsize [expr [$solver GetCellSize] * 1e9] ;# Convert to nm
		set mag [$solver GetAveMag]
		set mx [lindex $mag 0]
		set my [lindex $mag 1]
		set mz [lindex $mag 2]
		set energies [$solver GetEnergyDensities]
		set edemag [lindex [lindex $energies 3] 1]
		set maxang  [$solver GetMaxAngle]
		set line [format \
			" %-10.5g %8.5f %8.5f %8.5f    %-6g  %-#21.15g" \
			$cellsize $mx $my $mz $maxang $edemag]
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

# Substitute $BASEMIF in for __BASEMIF__ in above script
regsub -all -- __BASEMIF__ $init_script $BASEMIF init_script

# Set init script into TaskInfo
$TaskInfo SetSlaveInitScript $init_script

# Create task list
foreach data $run_list {
    $TaskInfo AppendTask $data [concat BatchTaskRun $data]
}
