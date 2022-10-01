
Oc_Application Define {
    -name		Oxsii
    -version		2.0b0
    -machine		oxs
    -file		oxsii.tcl
}

Oc_Application Define {
    -name		Boxsi
    -version		2.0b0
    -machine		oxs
    -file		boxsi.tcl
    -mode		fg
}

Oc_Application Define {
    -name		demagtensor
    -version		2.0b0
    -machine		demagtensor
    -mode		fg
}

Oc_Application Define {
    -name		MIFConvert
    -version		2.0b0
    -machine		tclsh
    -file		mifconvert.tcl
    -mode		fg
}

Oc_Application Define {
    -name		lastjob
    -version		2.0b0
    -machine		tclsh
    -file		lastjob.tcl
    -mode		fg
}

Oc_Application Define {
    -name		oxsregression
    -version		2.0b0
    -machine		tclsh
    -file		regression_tests/oxsregression.tcl
    -mode		fg
}

Oc_Application Define {
    -name		oxspkg
    -version		2.0b0
    -machine		tclsh
    -file		contrib/oxspkg.tcl
    -mode		fg
}

