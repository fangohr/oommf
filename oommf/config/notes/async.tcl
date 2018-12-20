#!/bin/sh
# FILE: async.tcl
# Usage: async.tcl <host> <port>
#
# The file pkg/net/link.tcl tries to use [socket -async] to make
# network connections.  Unfortunately, [socket -async] is broken
# in some releases of Tcl on some platforms.  This script tests
# for a broken [socket -async].  To use it, run it twice.  First
# provide a host and port that allow a connection.  Then provide
# a host and port that refused a connection.  If either run results
# in a "Tcl is broken" message, then pkg/net/link.tcl needs to be
# written so that [socket -async] is avoided for that combination
# of platform and Tcl release.
#
# See also
# http://sourceforge.net/bugs/?group_id=10894&func=detailbug&bug_id=118945
#
# \
exec tclsh "$0" ${1+"$@"}

proc AsyncConnect {host port} {
    puts "creating socket..."
    set s [socket -async $host $port]
    puts "socket created"
    fileevent $s writable [list VerifyConnection $s $host $port]
}

proc VerifyConnection {s host port} {
    global connected
    fileevent $s writable {}

    # If there's an error condition on the socket (failure to connect),
    # then [fconfigure -error] should provide an error message
    #
    set msg [fconfigure $s -error]
    if {![string match {} $msg]} {
        puts "Connection to $host:$port failed: $msg"
        set connected 0
        catch {close $s}
        return
    }

    # Connection reportedly successful.  At this point we normally
    # configure the socket for whatever communications we want and
    # set up fileevent handlers to manage protocols over the socket.
    #
    # Here, as a simple check, print out the peername information
    # as a check of a good connection.  If we really are connected,
    # we print information about the far end of the connection.  If
    # not, print an error message indicating that Tcl's fileevent,
    # fconfigure, and socket are not working as advertised.
    #
    if {[catch {fconfigure $s -peername} peerInfo]} {
	puts "Tcl is broken: $peerInfo"
    } else {
	puts "Connected: $peerInfo"
    }
    set connected 1
    close $s
}

proc tick {} {
    # Demonstrate event loop is active, and provide eventual timeout
    global tick connected
    incr tick
    puts "tick $tick"
    if {$tick >= 6000} {set connected 0; return}
    after 10 tick
}

set connected 0
set tick 0
tick
eval AsyncConnect $argv
vwait connected
# END FILE: async
