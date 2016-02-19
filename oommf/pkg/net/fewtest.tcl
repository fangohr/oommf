# fewtest.tcl
#
# Last modified on: $Date: 2007/03/21 01:17:04 $
# Last modified by: $Author: donahue $
# 
# This file contains a Tk script which demonstrates a bug in the Tcl
# command [fileevent <socket> writable] on Windows 95/NT platforms
# for pre-8.0 versions of Tcl/Tk.
#
# Usage: wish fewtest.tcl ?port?
#
# When this script is evaluated by wish, it starts a server listening on
# the specified port (or port 25000 by default).  That server will 
# accept exactly one connection.  
#
# Connect to this server with some client (I use telnet).  When the
# connection is made, the client receives the message "Welcome message".
# After that, the server should provide a simple echo service.  At the 
# client end of the connection, the session should look something like:
#
# This is a test
# You said: This is a test
# testing 1 2 3
# You said: testing 1 2 3
#
# When the client closes the connection, this script will exit.
#
# While the server is running, its window displays two labels.  The first
# displays the queue of message to send to the client.  The second displays 
# the 'fileevent writable' event handler.  On a properly working system, 
# these labels just briefly flash their values between the time data is 
# placed on the queue and the time the writable file event causes the data 
# to be written and these values cleared.
#
# On Unix platforms, this script operates as expected.  However on 
# MS Windows 95/NT using wish42.exe, the [file writable] events 
# apparently don't get added to the event queue properly.  The Send
# proc is never invoked.  You can watch the putQueue grow and grow.
#
# Now the funny thing:  Click the mouse on the server window, and two
# messages in the putQueue will be serviced by the Send proc.  It's
# as if 1 mouse click event = 2 file writable events.  This behavior
# only occurs when the [grab .] near the bottom of this example script
# has been evaluated.

;proc Start {port} {
    global server
    set server(socket) [socket -server Connection -myaddr localhost $port]
    set server(port) [lindex [fconfigure $server(socket) -sockname] 2]
}

;proc Connection {socket args} {
    global server link
    set link(socket) $socket
    set link(putQueue) {}
    .lp configure -text "putQueue: $link(putQueue)"
    after 20 SocketOpen
    close $server(socket)
}

;proc SocketOpen {} {
    global link
    set peerinfo [fconfigure $link(socket) -peername]
    fconfigure $link(socket) -blocking 0 -buffering line \
            -translation {auto crlf}
    Put "Welcome message"
    fileevent $link(socket) readable Receive
}

;proc Put {msg} {
    global link
    lappend link(putQueue) $msg
    .lp configure -text "putQueue: $link(putQueue)"
    fileevent $link(socket) writable Send
    .lw configure -text "fileevent writable handler: Send"
}

;proc Send {} {
    global link
    puts $link(socket) [lindex $link(putQueue) 0]
    set link(putQueue) [lrange $link(putQueue) 1 end]
    .lp configure -text "putQueue: $link(putQueue)"
    if {![llength $link(putQueue)]} {
        fileevent $link(socket) writable {}
        .lw configure -text "fileevent writable handler:"
    }
}

;proc Receive {} {
    global link
    set readCount [gets $link(socket) msg]
    if {$readCount < 0} {
        if {[fblocked $link(socket)]} return
        if {[eof $link(socket)]} {
            fileevent $link(socket) readable {}
            close $link(socket)
            exit
        }
    }
    Put "You said: $msg"
}

pack [label .lp]
pack [label .lw]
grab .
lappend argv 25000
Start [lindex $argv 0]
