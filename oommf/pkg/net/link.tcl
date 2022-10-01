# FILE: link.tcl
#
# An object which implements an event-driven, asynchronous, line-based
# network connection.  It generates an event Readable, when a line
# is available for reading, and queues lines for eventual writing.
# This is the object which actually owns and manages a Tcl socket.
#
# The Tcl event loop must be running for this event-driven object to function.
#
# Last modified on: $Date: 2015/09/30 07:41:35 $
# Last modified by: $Author: donahue $
#

Oc_Class Net_Link {

    # The socket through which we communicate to the host.
    const public variable socket

    # The name of the remote host
    public variable hostname

    # The remote port
    public variable port

    # line read from socket
    private variable lineFromSocket

    # Has there been an error on the socket?
    private variable read_error = 0
    private variable write_error = 0

    # Disable writes.  Reading can be disabled by deleting the readable
    # event handler.
    private variable write_disabled = 0

    # Identity checks.

    # The value checkUserIdentities should be either 0, 1, or 2.  It is
    # active # only when a connection is made originating from the local
    # # machine.  If 0, then no identity check is performed---the #
    # connection is allowed.  If 2, then an identity check is #
    # performed, and the connection will be refused unless the same #
    # user is on both sides of the connection.  If 1, then a check is #
    # made, but if the connection is refused only if the client user #
    # is positively different than the server user.  (On some systems, #
    # it may not be possible to uncover the identity of the client #
    # side user.)
    #
    # For example, to turn on strong local machine identity
    # verification, put this in the oommf/config/options.tcl file:
    #
    #    Oc_Option Add * Net_Link checkUserIdentities 2
    #
    # NOTE: The checks in this class apply only to client side sockets.
    # Server side sockets (aka "accepted sockets") should be checked at
    # the socket level (i.e., where the socket connection is made).
    const public common checkUserIdentities 1
    const public variable user_id_check

    # List of after delete command id's. These are recorded so they
    # can be canceled in the destructor. (BTW, the 'after cancel
    # script' command cancels at most one instance of a matching after
    # "script" command. For example
    #   after 20000 puts Hi
    #   after 30000 puts Hi
    #   after cancel puts Hi
    # will print "Hi" one time after 20 seconds.
    private variable after_delete_list = {}

    # Constructor is a non-blocking operation.  If it doesn't return
    # an error, it will generate a Ready event if and when it has established
    # a socket connection.  Then, it will generate Readable events whenever
    # it has a line to be read (using the Get method).  It will generate a
    # Delete event whenever it is destroyed.
    private variable checkconnecterror_eventid = {}
    Constructor { args } {
        set user_id_check $checkUserIdentities
        eval $this Configure $args
        # Try to open connection to host
        if {![info exists socket]} {
            # Client socket
            if {![info exists hostname] || ![info exists port]} {
                error "Missing required options: -hostname and -port"
            }
            #
            # Asynchronous connection of client sockets was broken in
            # Tcl for Windows prior to release 8.0.5, was broken in
            # 8.0.5 for Alpha Windows NT, and also broken in various
            # ways in Tcl 8.5.15 and 8.6.1-8.6.3.  Refer to
            #   http://wiki.tcl.tk/39687
            # Given this history, retain the following synchronous
            # connection code as an option that can be selected
            # from the platform config file.
            if {![catch {[Oc_Config RunPlatform] \
                             GetValue socket_noasync} val] \
                    && $val} {
               # Synchronous connect
               if {[catch {socket $hostname $port} testsocket]} {
                  set msg "Can't connect to $hostname:$port:\n\t$testsocket"
                  error $msg $msg
               }
               if {![$this UserIdentityCheck $testsocket]} {
                  error "User identity check failed on $hostname:$port"
               }
               set socket $testsocket
               set event [after 0 $this ConfigureSocket]
               Oc_EventHandler New _ $this Delete \
                   [list after cancel $event] -oneshot 1 \
                   -groups [list $this-ConfigureSocket $this]
               return
            } else {
               # Asynchronous connect
               if {[catch {socket -async $hostname $port} msg]} {
                  set msg "Can't connect to $hostname:$port:\n\t$msg"
                  error $msg $msg
               }
               set socket $msg
               if {![catch {fconfigure $socket -error} msg]} {
                  # -error option introduced in Tcl 8.2.
                  # Set up polling for socket errors.  Errors are
                  # suppose raise a writable event on the socket, so
                  # this polling should not be necessary, but this
                  # polling acts as fallback for buggy socket
                  # libraries.
                  set checkconnecterror_eventid \
                      [after 250 [list $this CheckConnectError]]
               }
               fileevent $socket writable [list $this VerifyOpen]
            }
        } else {
           # Accepted socket (server side).  Note: User id checks are
           # not performed at this level for server side sockets.
           # Server side user id checks should take place at the
           # server level, where the socket connection is made.
           foreach {_ hostname port} [fconfigure $socket -peername] {break}
           set event [after 0 $this ConfigureSocket]
           Oc_EventHandler New _ $this Delete \
              [list after cancel $event] -oneshot 1 \
              -groups [list $this-ConfigureSocket $this]
        }
    }

    method CheckConnectError {} {
       if {![string match {} [fconfigure $socket -error]]} {
          # Error raised; connection initialization complete
          $this VerifyOpen
       } else {
          # No result; reschedule check
          after cancel $checkconnecterror_eventid ;# Safety
          set checkconnecterror_eventid \
              [after 250 [list $this CheckConnectError]]
       }
    }

    # An asynchoronous client socket connection reports that it
    # is open.  Verify that, perform user id check if requested,
    # and configure the socket.
    method VerifyOpen {} {
        global tcl_platform
        fileevent $socket writable {}
        after cancel $checkconnecterror_eventid
        if {![catch {fconfigure $socket -error} msg]} {
            # -error recognized
            if {![string match {} $msg]} {
                set msg "$this: Can't connect to $hostname:$port:\n\t$msg"
                Oc_Log Log $msg status $class
                $this Delete
                return
            }
        }
        # Also check that we can actually write...
        if {[catch {puts -nonewline $socket ""} msg]} {
            # Write failed
            set msg "$this: Can't write to $hostname:$port:\n\t$msg"
            Oc_Log Log $msg status $class
            $this Delete
            return
        }

        # Experience shows the only reliable way to be sure we have
        # a connected socket is to try to retrieve information about
        # the peer.  That blocks (boo!) but we need to do it anyway.
        if {[catch {
                foreach {_ hostname port} [fconfigure $socket -peername] {break}
                } msg]} {
            set msg "$this: Not connected to $hostname:$port\n\t:$msg"
            Oc_Log Log $msg status $class
            $this Delete
            return
        }

        # Identity check.  This also involves blocking calls to fconfigure.
        if {![$this UserIdentityCheck $socket]} {
           unset socket
           set msg "$this: User identity check failed on $hostname:$port"
           Oc_Log Log $msg status $class
           $this Delete
           return
        }

        $this ConfigureSocket
    }

    private method UserIdentityCheck { clientsocket } {
       # Returns 1 if socket passes user id check, 0 if it fails.
       # NOTE: If the check fails, then a bye message is sent across
       # client socket and the socket is closed.
       if {$user_id_check} {
          set clearance [Net_CheckSocketAccess $clientsocket]
          if {$clearance < $user_id_check} {
             catch {puts $clientsocket "query 0 bye"}
             catch {close $clientsocket}
             return 0
          }
       }
       return 1
    }

    # configure socket for line-oriented traffic, then report the link
    # is ready
    method ConfigureSocket {} {
        Oc_EventHandler DeleteGroup $this-ConfigureSocket
        fconfigure $socket -blocking 0 -buffering line \
                           -translation {auto crlf} -encoding utf-8
        fileevent $socket readable [list $this Receive]
        Oc_Log Log "$this: Connected to $hostname:$port" status $class
        Oc_EventHandler Generate $this Ready
    }

    method Pause {} {socket} {
        fileevent $socket readable {}
    }

    method Resume {} {socket} {
        fileevent $socket readable [list $this Receive]
    }

    # handler called when there is data available from the host
    # It invokes other events describing what it receives.
    method Receive {} {socket lineFromSocket} {
        if {[catch {gets $socket line} readCount]} {
           Oc_EventHandler Generate $this SocketError -msg $readCount
           $this SocketError 1 0 $readCount
           return
        }
        if {$readCount < 0} {
            if {[fblocked $socket]} {
                Oc_EventHandler Generate $this SocketBlocked \
                   -readCount $readCount
                # 'gets' call is blocked until a whole line is available
                return
            }
            if {[eof $socket]} {
                $this SocketEOF
                return
            }
        }
        set lineFromSocket [Oc_Url Unescape $line]
        Oc_EventHandler Generate $this Receive -line $lineFromSocket
        Oc_EventHandler Generate $this Readable
    }

    private method SocketError { eread ewrite msg } {
       # Disables I/O and constructs error message
       if {![info exists socket]} {
          set errmsg "SocketError: $msg"
       } else {
          fileevent $socket readable {}
          fileevent $socket writable {}
          if {$ewrite} {
             set write_error 1
             set write_disabled 1
             set errmsg "$this: socket write error in connection to\
                $hostname:$port:\n\t$msg"
          }
          if {$eread} {
             set read_error 1
             set errmsg "$this: socket read error in connection to\
                $hostname:$port:\n\t$msg"
          }
       }
       Oc_Log Log $errmsg status $class
       lappend after_delete_list [after 0 [list $this Delete]]
       ## Allow call stack to unwind before triggering $this Delete
       ## event handlers.
       return
    }

    private method SocketEOF {} {
        Oc_EventHandler Generate $this SocketEOF
        fileevent $socket readable {}
        catch { puts $socket "" } ;# For some reason, this seems to
        ## help close the network connection on the other end.
        ## -mjd, 1998-10-09
        Oc_Log Log "$this: socket closed by $hostname:$port" status $class
        lappend after_delete_list [after 0 [list $this Delete]]
        ## Allow call stack to unwind before triggering $this Delete
        ## event handlers.
        return
    }

    method Get {} {lineFromSocket} {
        return $lineFromSocket
    }

    method Put { msg } {write_disabled socket} {
        if {$write_disabled} { return }

        # The socket message read routine is line buffered, and expects
        # each line to be a complete message. Newlines and carriage
        # returns in the string interfere with this, so escape them.
        # This is a a mini version of 'Oc_Url Escape'.
        regsub -all % $msg %25 msg
        regsub -all "\r" $msg %0d msg
        regsub -all "\n" $msg %0a msg

        Oc_EventHandler Generate $this Write -line $msg
        if {[catch {puts $socket $msg} errmsg]} {
           $this SocketError 0 1 $errmsg
           return
        }
    }

    method Flush {} {socket} {
        if {[info exists socket] && !$write_error} {
            # Non-blocking sockets on Windows don't seem to flush
            # properly (Tcl Bug 1329754).  As a workaround force
            # the socket to blocking configuration before flushing
            fconfigure $socket -blocking 1
            flush $socket
            fconfigure $socket -blocking 0
        }
    }

    method Close {} {socket} {
        if {[info exists socket]} {
            Oc_Log Log "$this: Closing socket $socket" status $class
            # The [catch] here should not be needed, but we've received
            # several reports of the following [close] throwing an error
            # which we have not been able to duplicate.  A [catch] will
            # mask that problem and should be harmless.
            fconfigure $socket -blocking 1
            catch {close $socket}
            unset socket
        }
    }

    Destructor {
        # Disable I/O.  Necessary lest event handlers be naughty.
        after cancel $checkconnecterror_eventid
        foreach id $after_delete_list {
           after cancel $id
        }
        fileevent $socket readable {}
        set write_disabled 1
        Oc_EventHandler Generate $this Delete
        Oc_EventHandler DeleteGroup $this
        if {[info exists socket]} {
            $this Close
        }
    }

 }
