
Oc_Application Define {
    -name		mmLaunch
    -version		1.2.1.0
    -machine		omfsh
    -file		mmlaunch.tcl
}

Oc_Application Define {
    -name               pidinfo
    -version            1.2.1.0
    -machine            filtersh
    -file               pidinfo.tcl
    -mode               fg
    -options            {-tk 0}
}

Oc_Application Define {
    -name               nickname
    -version            1.2.1.0
    -machine            filtersh
    -file               nickname.tcl
    -mode               fg
    -options            {-tk 0}
}

Oc_Application Define {
    -name               killoommf
    -version            1.2.1.0
    -machine            filtersh
    -file               killoommf.tcl
    -mode               fg
    -options            {-tk 0}
}

Oc_Application Define {
    -name               launchhost
    -version            1.2.1.0
    -machine            filtersh
    -file               launchhost.tcl
    -mode               fg
    -options            {-tk 0}
}

