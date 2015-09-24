# FILE: ctrl.tcl
# Control client program
#
# This script is used with the scripts server.tcl and data.tcl to test
# proper functioning of Tcl's socket fileevents.  See server.tcl.
#
set s [socket localhost 5000]
fconfigure $s -blocking 0 -buffering line -translation {auto crlf}
button .start -text Start -command {puts $s start}
button .stop -text Stop -command {puts $s stop}
button .quit -text Quit -command exit
pack .start .stop .quit
# END FILE: ctrl.tcl
