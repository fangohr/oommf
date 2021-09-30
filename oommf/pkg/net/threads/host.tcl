# FILE: host.tcl
#
# The master server for an OOMMF host.
#
# It listens on the port name by its first command line argument, and 
# directs communications among other OOMMF threads on the localhost.
#
# If a second command line argument is given, it must be the port
# on which host.tcl contacts its parent to signal that its server is
# ready.  This also suppresses error messages.
#
# Debugging notes:
# 1) You can launch a standalone host server via
#   app/omfsh/<platform>/omfsh pkg/net/threads/host.tcl -tk 0 15136
#
# 2) If you connect to the host server with telnet, use "query"
#    messages (e.g., "query 1 messages") to interact with the
#    host server. When finished, use
#      query N bye
#    to begin the connection shutdown. You should receive back
#      tell netconn serverclose
#    to which you should reply
#      tell netconn ackserverclose
#    and the connection should close. If not, type Ctrl-] to enter
#    telnet command mode an enter "close".
# 3) You can also try
#      close stderr
#      set chanstderr [open host-[pid].log a]
#      fconfigure $chanstderr -buffering none
#      close stdout
#      set chanstdout [open host-[pid].log a]
#      fconfigure $chanstdout -buffering none
#      Oc_ForceStderrDefaultMessage
#      Oc_Log SetLogHandler Oc_DefaultLogMessage status
#      Oc_Log SetLogHandler Oc_DefaultLogMessage debug
#    When a channel is opened, if any compatible standard channel is not
#    open then it will be attached to the new channel. So the close/open
#    calls connect stderr and stdout to the host*log file.  The
#    following commands direct comprehensive logging output to the
#    host*log file.

if {([llength $argv] > 2) || ([llength $argv] == 0)} {
    error "usage: host.tcl <service_port> ?<creator_port>?"
}

global master
set master(version) 0.1

# Set up for auto-loading
set master(relhdir) [string trimright [file dirname [info script]] ./]
set master(abshdir) [file join [pwd] $master(relhdir)]
set master(pdir) [file dirname $master(abshdir)]
set master(libdir) [file dirname $master(pdir)]

if {[lsearch $auto_path $master(libdir)] < 0} {
    lappend auto_path $master(libdir)
}

# Support libraries
package require Oc 
package require Net

# Ignore Ctrl-C's, Ctrl-Z's
Oc_IgnoreInteractiveSignals
# set up to reap zombie children
proc Reap {} {
    catch {exec {}}
    after 300000 [info level 0]
}
Reap

# Try to keep going, even if controlling terminal goes down.
Oc_IgnoreTermLoss

proc Die {} {
    global master
    $master(server) Delete
    $master(protocol) Delete
    Oc_Log Log "OOMMF host server died" status
    exit
}

# Define protocol
Net_Protocol New master(protocol) \
        -name [list OOMMF host protocol $master(version)]
# Replace default "exit" message with a no-op
$master(protocol) AddMessage start exit {} {
    return [list start [list 0 ""]]
}
$master(protocol) AddMessage start lookup {acct} {
    global portmap
    if {[info exists portmap($acct)]} {
        return [list start [list 0 $portmap($acct)]]
    } else {
        return [list start [list 1 $acct not registered]]
    }
}
$master(protocol) AddMessage start register {acct port} {
    global portmap
    if {[info exists portmap($acct)]} {
        return [list start \
                [list 1 $acct already registered at port $portmap($acct)]]
    }
    set ret [list start [list 0 [array set portmap [list $acct $port]]]]

    # If the connection is lost, de-register the account server.
    Oc_EventHandler New _ $connection Delete [list unset portmap($acct)] \
	-groups [list register-$acct]
    return $ret
}
$master(protocol) AddMessage start deregister { acct port } {
    # Ought to call back for security -- and that will require
    # event-driven query-handling.
    #
    # If this request comes in from a remote machine, it should not
    # be trusted without checking.
    global portmap
    if {![info exists portmap($acct)]} {
        return [list start [list 1 $acct not registered]]
    }
    if {![string match $port $portmap($acct)]} {
        return [list start [list 1 permission denied]]
    }
    Oc_EventHandler DeleteGroup register-$acct
    set ret [list start [list 0 [unset portmap($acct)]]]
    return $ret
}

# Define server and start it.  Turn off client identity checks
# to allow multiple users to use one host server.
Net_Server New master(server) -protocol $master(protocol) \
   -register 0 -user_id_check 0
set serviceport [lindex $argv 0]
set startError [catch {$master(server) Start $serviceport} master(msg)]

if {$startError} {
    puts stderr "HOST SERVER ERROR:\
                 Can't start OOMMF host server on port $serviceport:\n  \
                 $master(msg)."
    after 2000
}

if {[llength $argv] == 2} {
    # Ping creator.  If server is running then ping back with
    # OOMMF_HOSTPORT, port number and two newlines.  If server open
    # fails (probably because some other process grabbed the port
    # first) then ping back with single newline.
    set port [lindex $argv 1]
    if {[catch {socket localhost $port} s]} {
        $master(server) Delete
        $master(protocol) Delete
        Oc_Log Log "Unable to call back $port: $s" status
        error "Unable to call back $port: $s"
    }
    Oc_Log Log "Called back $port" status
    if {!$startError} {
       catch { puts $s [list OOMMF_HOSTPORT [$master(server) Port]] }
    }
    catch { puts $s ""} ;# Seems to help close port
                        ## on other side. -mjd 981009
    catch {close $s} msg
    if {[string length $msg]} {
	return -code error "close error: $msg"
    }
} elseif {$startError} {
    Oc_Log Log "Can't start OOMMF host server on port\
	    $serviceport:\n\t$master(msg)." status
    error "Can't start OOMMF host server on port $serviceport:
	$master(msg)."
}

# Once a connection becomes ready, set up handler to catch
# connection destructions.  On last one, exit.
Oc_EventHandler New _ Net_Connection Ready \
   [list Oc_EventHandler New _ Net_Connection Delete \
       [list CheckConnect] -groups checkconnect] \
    -oneshot 1
proc CheckConnect {} {
    # A Net_Connection is being destroyed.  If it's the last one,
    # schedule our suicide
    if {[llength [Net_Connection Instances]] == 1} {
       # Stop accepting new connections immediately
       global master
       $master(server) ForbidServiceStart
       if {[$master(server) IsListening]} {
          $master(server) Stop
       }
       # Shut down
       after idle Die
       Oc_EventHandler DeleteGroup checkconnect
       ## In principle if in this branch then no more Net_Connection
       ## Delete events can occur, so this DeleteGroup probably isn't
       ## necessary. But it also doesn't hurt.
    }
}

if {!$startError} {
    vwait master(forever)
}
