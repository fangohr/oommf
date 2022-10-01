# Note: pimake should be run using the tclsh the user selected
# to launch the build (aka "user_tclsh").
Oc_Application Define {
    -name		pimake
    -version		2.0b0
    -machine		user_tclsh
    -file		pimake.tcl
    -mode		fg
}

