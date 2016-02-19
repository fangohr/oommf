Oc_Application Define {
    -name		mmArchive
    -version		1.2.0.6
    -machine		omfsh
    -file		mmarchive.tcl
    -options		{-tk 0}
}

Oc_Application Define {
    -name		cmdserver
    -version		1.2.0.6
    -machine		filtersh
    -file		cmdserver.tcl
    -options		{-tk 0}
    -mode		fg
}
