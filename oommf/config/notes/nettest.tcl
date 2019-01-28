# FILE: nettest.tcl
#
# This script is used to debug OOMMF's Net package. It must be evaluated
# with OOMMF's filtersh shell.  In OOMMF's root directory, type:
#
#	tclsh oommf.tcl filtersh config/notes/nettest.tcl
#
# A long stream of messages will be written to stderr logging the
# operations of the Net package as it starts up the host directory
# and account directory servers.  Capture that log information in
# a file, and include it with your message to the OOMMF developers
# when you ask for their help debugging netowrking problems with
# your OOMMF installation.

package require Oc

# Report all status messages to stdout
proc WriteToStdout {msg type src} {
    puts "$src $type: $msg"
}
Oc_Log SetLogHandler WriteToStdout status

package require Net 2

proc Die {} {
    global server
    $server Stop
    after 2000 exit 0
}

Net_Protocol New protocol -name "OOMMF NetworkTest protocol 0.1"
$protocol AddMessage start die {} {
    after 2000 Die
    return [list close [list 0 Bye!]]
}
Net_Server New server -protocol $protocol -alias nettest
$server Start 0

# Need to get the OOMMF ID (OID) of the server we just started.

while {[catch {$server SID}]} {after 100; update}

set cmd [list Net_Thread New thread -hostname localhost -pid [$server SID] \
	-accountname [Net_Account DefaultAccountName] -alias nettest]

if {[package vcompare [package provide Oc] 1.1] < 0} {
    lappend cmd -log [em_extension ApplicationLog]
}
eval $cmd
Oc_EventHandler New _ $thread Ready [list $thread Send die]

vwait forever
