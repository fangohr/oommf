# File: simplecontrol.tcl
#
# Sample micromagnetic control script.  Must be run using the
# mmsolve shell.
#
if {$argc>0} { set mif_file [lindex $argv 0]} else {
    puts stderr "Usage: mmsolve simplecontrol.tcl <MIF 1.x file>"
    exit 1
}

# Support libraries
package require Oc
package require Tk
package require Ow

# Text output subroutines
proc PrintState { solver } {
    set stepcount [$solver GetODEStepCount]
    set mxh [$solver GetMaxMxH]
    set maxangle [$solver GetMaxAngle]
    set energies [join [$solver GetEnergyDensities]]
    puts [format "%5d %10.7f  %5.1f  %12.4f %12.4f %12.4f %12.4f %12.4f" \
	    $stepcount $mxh $maxangle \
	    [lindex $energies 1] [lindex $energies 3] \
	    [lindex $energies 5] [lindex $energies 7] \
	    [lindex $energies 9]]
}

proc PrintLabels {} {
puts "  ODE     Max       Max                               ENERGIES"
puts " Step   |m x h|    Angle     Total       Exchange    Anisotropy     Demag        Zeeman"
}

# Graph and terminal update subroutine
proc UpdateDisplay { solver graph } {
    set this_step [$solver GetODEStepCount]
    set mxh [$solver GetMaxMxH]
    set energies [join [$solver GetEnergyDensities]]
    if {$this_step % 10 == 0} {
        puts "Step [format "%4d" $this_step]: \
                Max |m x h| =[format "%7.5f" $mxh]"
    }
    $graph AddDataPoints \
            "Total Energy" $this_step [lindex $energies 1]
    $graph AddDataPoints \
            "Exchange Energy" $this_step [lindex $energies 3]
    $graph AddDataPoints \
            "Anisotropy Energy" $this_step [lindex $energies 5]
    $graph AddDataPoints \
            "Demag Energy" $this_step [lindex $energies 7]
    $graph AddDataPoints \
            "Zeeman Energy" $this_step [lindex $energies 9]
    set limits_change [$graph SetGraphLimits]
    if {$limits_change} {
        $graph RefreshDisplay
    } else {
        $graph DrawCurves
    }
}

# Initialize solver and display graph
mms_mif New mif
$mif Read $mif_file
mms_solver New solver $mif

# Initialize display graph
Ow_GraphWin New graph . -xlabel "Iteration" -ylabel "Energy" \
        -curve_width 3
$graph NewCurve "Total Energy"
$graph NewCurve "Exchange Energy"
$graph NewCurve "Anisotropy Energy"
$graph NewCurve "Demag Energy"
$graph NewCurve "Zeeman Energy"
pack [$graph Cget -winpath] -side right -fill both -expand 1

# Setup display updates
Oc_EventHandler New _ $solver Iteration "UpdateDisplay $solver $graph"

# Build control buttons
set btn_frame [frame .f]
set b0 [button .b0 -text Labels -command PrintLabels]
set b1 [button .b2 -text "Print State" \
	-command "PrintState $solver"]
set b2 [button .b1 -text "Run" \
	-command "$solver ChangeSolverState Run"]
set b3 [button .b3 -text "Pause" \
	-command "$solver ChangeSolverState Pause"]
set b4 [button .b4 -text "Exit" \
	-command "$solver ChangeSolverState Pause ; $solver Delete ; exit"]
pack $b0 $b1 $b2 $b3 $b4 -in $btn_frame -side top -fill both -expand 1
pack $btn_frame -side left -fill y
