set myzeeman_lastfile {}
proc myzeeman { Bx By Bz step } {
    global myzeeman_lastfile

    # Construct new B file name based on field step import
    set newfile "app/mmpe/examples/fileseq$step.obf"

    # Check that file exists and is readable
    if {![file readable $newfile]} {
	# Not readable; reuse last response
	set newfile $myzeeman_lastfile
    }

    # As a development aid, notify the user of the change
    if {![string match {} $newfile]} {
	Oc_Log Log "New B applied field ($Bx $By $Bz; $step)\
		file: $newfile" info
    }

    set myzeeman_lastfile $newfile
    return $newfile
}