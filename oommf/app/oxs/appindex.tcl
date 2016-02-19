
Oc_Application Define {
    -name		Oxsii
    -version		1.2.0.6
    -machine		oxs
    -file		oxsii.tcl
}

Oc_Application Define {
    -name		Boxsi
    -version		1.2.0.6
    -machine		oxs
    -file		boxsi.tcl
    -mode		fg
}

Oc_Application Define {
    -name		MIFConvert
    -version		1.2.0.6
    -machine		tclsh
    -file		mifconvert.tcl
    -mode		fg
}

Oc_Application Define {
    -name		lastjob
    -version		1.2.0.6
    -machine		tclsh
    -file		lastjob.tcl
    -mode		fg
}

Oc_Application Define {
    -name		oxsregression
    -version		1.2.0.6
    -machine		tclsh
    -file		regression_tests/runtests.tcl
    -mode		fg
}

Oc_Application Define {
    -name		oxspkg
    -version		1.2.0.6
    -machine		tclsh
    -file		contrib/oxspkg.tcl
    -mode		fg
}

