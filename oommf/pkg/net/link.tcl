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

    # Constructor is a non-blocking operation.  If it doesn't return
    # an error, it will generate a Ready event if and when it has established
    # a socket connection.  Then, it will generate Readable events whenever
    # it has a line to be read (using the Get method).  It will generate a
    # Delete event whenever it is destroyed.
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
	    # Tcl for Windows until release 8.0.5.  Use -async
	    # connection if it is available.
	    global tcl_platform
            if {[string match windows $tcl_platform(platform)]
                    && [package vcompare [package provide Tcl] 8.0] <= 0} {

               # On Tcl for Alpha Windows NT platforms, [socket -async]
               # appears to still be broken in release 8.0.5.  Rather
               # than get in the business of tracking machine
               # dependencies, we'll assume use of [socket -async]
               # needs to wait until at least release 8.1 of Tcl.
               # global tcl_patchLevel
               # if {[string compare 8.0.5 $tcl_patchLevel]} {
                  # socket -async is broken.  Do blocking open
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
               # }
            }
	    if {[catch {socket -async $hostname $port} msg]} {
		set msg "Can't connect to $hostname:$port:\n\t$msg"
		error $msg $msg
	    }
	    set socket $msg
	    fileevent $socket writable [list $this VerifyOpen]
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

    # An asynchoronous client socket connection reports that it
    # is open.  Verify that, perform user id check if requested,
    # and configure the socket.
    method VerifyOpen {} {
	global tcl_platform
	fileevent $socket writable {}
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
        fconfigure $socket -blocking 0 -buffering line -translation {auto crlf}
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
            $this SocketError 1 0 $readCount
            return
        }
        if {$readCount < 0} {
            if {[fblocked $socket]} {
                # 'gets' call is blocked until a whole line is available
                return
            }
            if {[eof $socket]} {
                $this SocketEOF
                return
            }
        }
        set lineFromSocket [Oc_Url Unescape $line]
        Oc_EventHandler Generate $this Readable
    }

    private method SocketError { eread ewrite msg } {
       if {![info exists socket]} { return }
       fileevent $socket readable {}
       fileevent $socket writable {}
       if {$ewrite} {
          set write_error 1
          set write_disabled 1
          Oc_Log Log "$this: socket write error in connection to\
		$hostname:$port:\n\t$msg" status $class
       }
       if {$eread} {
          set read_error 1
          Oc_Log Log "$this: socket read error in connection to\
		$hostname:$port:\n\t$msg" status $class
       } else {
          # Error occured during write --- other end of socket probably
          # closed.  But there may be valid data waiting in input
          # buffer, so drain it:
          while {1} {
             if {[catch {gets $socket line} readCount]} {
                set read_error 1
                break
             }
             if {$readCount < 0} {
                if {[fblocked $socket]} {
                   # 'gets' call is blocked until a whole line is available
                   after 500
                   continue
                }
                # Otherwise eof
                Oc_Log Log \
                   "$this: detected socket close by $hostname:$port" \
                   status $class
                break
             }
             set lineFromSocket [Oc_Url Unescape $line]
             Oc_EventHandler Generate $this Readable
          }
       }
       $this Delete
       return
    }

    private method SocketEOF {} {
        fileevent $socket readable {}
	catch { puts $socket "" } ;# For some reason, this seems to
	## help close the network connection on the other end.
	## -mjd, 1998-10-09
        Oc_Log Log "$this: socket closed by $hostname:$port" status $class
        $this Delete
        return
    }

    method Get {} {lineFromSocket} {
        return $lineFromSocket
    }

    method Put { msg } {write_disabled socket} {
        if {$write_disabled} { return }

        # Escape the newlines in the string -- a mini version of
        # 'Oc_Url Escape'
        regsub -all % $msg %25 msg
        regsub -all "\n" $msg %0a msg

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
        fileevent $socket readable {}
        set write_disabled 1
        Oc_EventHandler Generate $this Delete
        Oc_EventHandler DeleteGroup $this
        if {[info exists socket]} {
	    $this Close
	}
    }

 }
