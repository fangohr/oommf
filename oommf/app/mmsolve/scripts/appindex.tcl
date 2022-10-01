Oc_Application Define {
    -name		batchmaster
    -version		2.0b0
    -machine		filtersh
    -file		batchmaster.tcl
    -options		{-tk 0}
    -mode		fg
}

Oc_Application Define {
    -name		batchslave
    -version		2.0b0
    -machine		mmsolve
    -file		batchslave.tcl
    -options		{-tk 0}
}

Oc_Application Define {
    -name		batchsolve
    -version		2.0b0
    -machine		mmsolve
    -file		batchsolve.tcl
    -options		{-tk 0}
    -mode		fg
}

Oc_Application Define {
    -name		mag2hfield
    -version		2.0b0
    -machine		mmsolve
    -file		mag2hfield.tcl
    -options		{-tk 0}
    -mode		fg
}

