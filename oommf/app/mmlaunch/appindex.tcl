
Oc_Application Define {
    -name		mmLaunch
    -version		3.0b0
    -machine		omfsh
    -file		mmlaunch.tcl
}

Oc_Application Define {
    -name		mmLaunch
    -aliases		mmLaunch-Classic
    -version		2.0b0
    -machine		omfsh
    -file		mmlaunch-classic.tcl
}

Oc_Application Define {
    -name               pidinfo
    -version            2.0b0
    -machine            filtersh
    -file               pidinfo.tcl
    -mode               fg
    -options            {-tk 0}
}

Oc_Application Define {
    -name               nickname
    -version            2.0b0
    -machine            filtersh
    -file               nickname.tcl
    -mode               fg
    -options            {-tk 0}
}

Oc_Application Define {
    -name               killoommf
    -version            2.0b0
    -machine            filtersh
    -file               killoommf.tcl
    -mode               fg
    -options            {-tk 0}
}

Oc_Application Define {
    -name               launchhost
    -version            2.0b0
    -machine            filtersh
    -file               launchhost.tcl
    -mode               fg
    -options            {-tk 0}
}

