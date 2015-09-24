# FILE: data.tcl
# Data client program
#
# This script is used with the scripts server.tcl and ctrl.tcl to test
# proper functioning of Tcl's socket fileevents.  See server.tcl.
#
set s [socket localhost 7000]
fconfigure $s -blocking 0 -buffering line -translation {auto crlf}
fileevent $s readable [list Read $s]
proc Read {s} {
    set ::value [gets $s]
    if {![eof $s]} {
	puts $s "Got: $::value"
    }
}
label .l -textvariable value
button .quit -text Quit -command exit
pack .l .quit
# END FILE: data.tcl
