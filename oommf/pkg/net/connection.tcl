# FILE: connection.tcl
#
# An object which manages asynchronous communication over a Net_Link.
#
# Query, expects reply
# Tell, just sends message
#
# The Tcl event loop must be running for this event-driven object to function.
#
# Last modified on: $Date: 2015/09/30 07:41:34 $
# Last modified by: $Author: donahue $
#

Oc_Class Net_Connection {

    # The link which manages lower level of communications.
    private variable link

    # The socket we hand off to our link. Variable socket exists iff
    # this is a server-side connection.
    public variable socket

    # The hostname we hand off to our link
    public variable hostname

    # The port we hand off to our link
    public variable port

    # The protocol used to handle queries received over this connection.
    public variable protocol
    private variable myProtocol

    # Received message
    private variable message
    private variable read_handler

    # query id
    private variable id = 0

    # Ready to send/receive?
    private variable ready = 0
    private variable write_disabled = 0

    # Array of timeout events
    private array variable toe = {}
    public variable timeout = 6000000

    # Interlock for server-side shutdown. Valid only for server-side
    # connections.
    private variable server_close_request = 1

    # A connection closing can be initiated by either the server or
    # client side of a connection. Server-side closings start with the
    # server sending a 'tell netconn serverclose' message across the
    # link. The Net_Connection object on the client side receives this
    # message, sends back a 'tell netconn ackserverclose', and calls its
    # Net_Connection SafeClose method. The server side then receives the
    # ackserverclose message and calls its Net_Connection SafeClose
    # method. The important point here is that when the client receives
    # the serverclose message, it knows that no more messages will be
    # coming from the server, and when it sends the ackserverclose it
    # commits to not sending any more messages from its side. So when an
    # ackserverclose message is handled, on either the sending or
    # receiving end, the Net_Connection object knows that communication
    # is over and it is OK to close the connection.
    #   Client side closing is initiated by the client sending a 'query
    # N bye' or 'query N exit' message (where N is an arbitrary query
    # identifier). This triggers the deletion of the Net_Protocol on the
    # server side of the connection, which calls the CloseServer method
    # on the associated Net_Connection object. CloseServer sends a 'tell
    # netconn serverclose' message back, and the process plays out as
    # detailed in the previous paragraph.
    #   Thus, in all cases if a Net_Connection instance handles an
    # ackserverclose message then it knows communication across the
    # socket is complete and it is safe to shut down. This state is
    # recorded in the variable close_acknowledged.
    private variable close_acknowledged = 0

    # Identity checks
    const public variable user_id_check

    # Constructor is a non-blocking operation.
    Constructor { args } {
        Oc_Log Log "$this Constructor start" status $class
        eval $this Configure $args
        if {[info exists socket]} {
            # Assume a server
            if {![info exists protocol]} {
                error "Server connections need a query-handling protocol"
            }
            if {[info exists user_id_check] && $user_id_check!=0} {
                error "User id checks for server-side sockets must be\
                   done at server level."
            }
            Net_Link New link -socket $socket

            set myProtocol [$protocol Clone $this]
            Oc_EventHandler New _ $myProtocol ClientCloseRequest \
               [list set [$this GlobalName server_close_request] 0] \
               -groups [list $this $myProtocol]

            Oc_EventHandler New _ $link Ready [list $this ServerLinkReady] \
                    -oneshot 1 -groups [list $this]
        } elseif {[info exists hostname] && [info exists port]} {
            # Assume a client
            if {[info exists user_id_check]} {
               Net_Link New link -hostname $hostname -port $port \
                  -user_id_check $user_id_check
            } else {
               Net_Link New link -hostname $hostname -port $port
            }
            Oc_EventHandler New _ $link Ready [list $this LinkReady] \
                    -oneshot 1 -groups [list $this]
        } else {
            error "Missing required options: -hostname and -port or -socket"
        }
        Oc_EventHandler New _ $link Delete [list $this LinkDelete] \
            -oneshot 1 -groups [list $this $this-LinkDelete]
        Oc_Log Log "$this Constructor end" status $class
    }

    method ServerLinkReady {} {
        Oc_Log Log "$this Put [concat tell [$myProtocol Cget -name]]" \
            status $class
        $link Put [concat tell [$myProtocol Cget -name]]
        $this LinkReady
    }

    method LinkReady {} {
        Oc_EventHandler New read_handler $link Readable [list $this Receive] \
                -groups [list $this]
        set hostname [$link Cget -hostname]
        set port [$link Cget -port]
        set ready 1
        Oc_EventHandler Generate $this Ready
    }

    method Ready {} {
       return $ready
    }


    # Every line of data received over the link gets handled by this
    # method.  It has to parse the message and direct it to its
    # destination.
    method Receive {} {
        # NOTE: The function of the Net_Link is to transmit and receive
        # strings.  The Net_Connection wants to interpret these strings
        # as Tcl lists.  Not all string are a valid Tcl list, so we have
        # to be sure the line of input we receive is tested, or somehow
        # interpreted as a valid Tcl list.  I would like to use the
        # split and join commands to translate back and forth between
        # lists and strings but I can't because contrary to their
        # documentation, they are *not* inverses of each other.
        # For example:
        # % split [join {1 {2 3} {4 5 6}}]
        # 1 2 3 4 5 6
        # The list structure can be completely changed. Grrrrrrr..
        # It seems the best we can do is use the Tcl list commands to
        # at least test the the string we receive has a valid interpretation
        # as a Tcl list, and log an error and bail if it does not.

        # NB: See also SafeCloseReceive, which may replace this
        #     handler during shutdown.

        # Get the string
        set rawmessage [$link Get]
        Oc_Log Log "$this Received raw message: $rawmessage" status $class
        # Test for list structure
        if {[catch {set rawmessage [lrange $rawmessage 0 end]} msg]} {
            Oc_Log Log "$this Message not a Tcl list: $rawmessage" \
                    warning $class
            return
        }
        if {![llength $msg]} {
            # Ignore empty messages
            return
        }
        switch -- [lindex $msg 0] {
            reply {
                if {[llength $msg] == 1} {
                    Oc_Log Log "$this Received bad message: $msg" \
                            warning $class
                    return
                }
                set rid [lindex $msg 1]
                if {![catch {set toe($rid)} ev]} {
                    after cancel $ev
                    unset toe($rid)
                }
                set message [lrange $msg 2 end]
                Oc_EventHandler Generate $this Reply$rid
            }
            query {
                if {[llength $msg] < 3} {
                    Oc_Log Log "$this Received bad message: $msg" \
                            warning $class
                    return
                }
                set id [lindex $msg 1]
                set message [lrange $msg 2 end]
                Oc_EventHandler Generate $this Query
		# Save link in case our destructor is called and instance
		# variable is lost.
		set saveLink $link
                Oc_Log Log "$this Handling query: $message" status $class
                # This doesn't allow for event-driven query handling.
                # Should it?  If so, should it timeout?
		set reply [$myProtocol Reply $message]
		# Must check existence in case the call above
		# called $saveLink destructor.
		if {[llength [info commands $saveLink]]} {
                    Oc_Log Log "$this Put [concat reply $id $reply]" \
                        status $class
                    $saveLink Put [concat reply $id $reply]
                    if {[catch {$myProtocol Deleted} _] || $_} {
                       # message was a bye or exit request. Don't allow
                       # any additional writes across connection.
                       catch {set write_disabled 1}
                    }
		}
            }
            tell {
              switch -exact [lindex $msg 1] {
                 netconn {
                    # Control messages directed to Net_Connection
                    switch -exact [lindex $msg 2] {
                       serverclose {
                          # Server on other end of connection is shutting
                          # down. Send acknowledge, but catch in case it
                          # closes first.
                          catch {$link Put [list tell netconn ackserverclose]}
                          set close_acknowledged 1
                          $this SafeClose
                       }
                       ackserverclose {
                          # Client on other end of connection acknowledges
                          # close notice.
                          set close_acknowledged 1
                          $this SafeClose
                       }
                       default {
                          Oc_Log Log "$this Unrecognized Net_Connection\
                           control message: $msg" warning $class
                       }
                    }
                 }
                 default {
                    set message [lrange $msg 1 end]
                    Oc_EventHandler Generate $this Readable
                 }
              }
            }
            default {
                # Log non-conforming messages, but continue.
                Oc_Log Log "$this Received bad message: $msg" \
                        warning $class
            }
        }
    }

    method Get {} {
        return $message
    }

    method Tell { args } {
        if {![info exists write_disabled] || $write_disabled} {
           # Tell request during $this shutdown.  Should we raise a
           # warning or error?  Caller isn't expecting a reply, so
           # it's probably OK to just eat it.
           #error "Tell request during Net_Connection shutdown"
           Oc_Log Log "Swallowed: $this Put [concat tell $args]" status $class
           return
        }
        if {!$ready} {
            error "connection not ready"
        }
        Oc_Log Log "$this Put [concat tell $args]" status $class
        $link Put [concat tell $args]
    }

    method Query { args } {
       if {![info exists write_disabled] || $write_disabled} {
          # Query request during $this shutdown won't be honored.
          # Should we ignore or raise an error?  For production work,
          # it may be safer to ignore...
          Oc_Log Log "Query request during Net_Connection shutdown" \
              warning $class
       }
       if {!$ready} {
          error "connection not ready"
       }
       set mid [incr id]
       array set toe [list $mid [after $timeout $this Timeout $mid]]
       Oc_Log Log "$this Put [concat query $mid $args]" status $class
       $link Put [concat query $mid $args]
       ## If '$link Put' fails then destructor for $this will be
       ## called, in which case instance variables may be unset.  Thus
       ## the local storage of the $id value.
       return $mid
    }

    method Timeout { qid } {
        Oc_Log Log "$this Timeout waiting for reply to query $qid" status $class
        Oc_EventHandler Generate $this Timeout$qid
    }

    method OwnsProtocol { p } {
	string match $myProtocol $p
    }

    method CloseServer {} {
       # Tell client that server is shutting down. This method should
       # only be invoked from the server side of a connection. If
       # variable server_close_request is true, then send 'netconn
       # serverclose' and wait on 'netconn ackserverclose'.
       if {!$ready} {
          # Can't send any messages across connection regardless
          set server_close_request 0
       } else {
          if {$server_close_request} {
             $this Tell netconn serverclose
          }
       }
       $this SafeClose
    }

    method SafeCloseReceive {} {
       # Replaces Receive method (q.v.) on SafeClose.  Discards all
       # input except Reply and tell netconn messages.  Calls $this
       # Delete when all queued replies have been received.
       set rawmessage [$link Get]
       Oc_Log Log "$this Received raw message: $rawmessage" status $class
       # Test for list structure
       if {[catch {set rawmessage [lrange $rawmessage 0 end]} msg] \
              || [llength $msg]<1} {
          return
       }
       set type [lindex $msg 0]
       switch -exact $type {
          reply {
             set rid [lindex $rawmessage 1]
             if {![catch {set toe($rid)} ev]} {
                after cancel $ev
                unset toe($rid)
             }
             if {[catch {lrange $rawmessage 2 end} message]} {
                set message "Net_Connection SafeCloseReceive Error: $message"
             }
             Oc_EventHandler Generate $this Reply$rid
             if {[llength [array names toe]]==0} {
                $this Delete
             }
          }
          tell {
             switch -exact [lindex $msg 1] {
                netconn {
                   # Control messages directed to Net_Connection
                   switch -exact [lindex $msg 2] {
                      serverclose {
                         # Server on other end of connection is shutting
                         # down. Send acknowledge, but catch in case it
                         # closes first. There should be no additional
                         # messages received from the server.
                         catch {$link Put [list tell netconn ackserverclose]}
                         set close_acknowledged 1
                         $this Delete
                      }
                      ackserverclose {
                         # Client on other end of connection acknowledges
                         # close notice. There should be no additional
                         # messages received from client.
                         set close_acknowledged 1
                         $this Delete
                      }
                   }
                }
             }
          }
       }
    }

    private variable inside_SafeClose = 0
    method SafeClose {} {
       if {![info exists inside_SafeClose] ||
           $inside_SafeClose} {
          return ;# Recursive callback.
       }
       set inside_SafeClose 1
       # Waits for replies from any preceding output messages, and
       # then calls the destructor.  Any additional output requests
       # raise an error.  Input is discarded.  This routine is non-blocking,
       # But the Destructor call can possibly block on the socket
       # close call.  Calling routines should register a handler on
       # the destructor DeleteEnd event.
       Oc_EventHandler Generate $this SafeClose
       if {![info exists write_disabled]} {
          # Apparently $this destructor was called while handling
          # $this SafeClose event. That is bad form, but there's
          # nothing to do now but bail.
          return
       }
       set write_disabled 1
       if {!$ready} {
          # Close request during initialization.  Delete immediately.
          $this Delete ; return
       }
       if {[llength [array names toe]]==0 && \
              (![info exists socket] || !$server_close_request)} {
          # If there are no TimeOut Events AND this is either a
          # client-side connection or else a server-side connection
          # where the client side requested closing, then there are no
          # events to wait on, so we can directly delete $this.
          $this Delete ; return
       }
       if {[info exists read_handler]} {
          $read_handler Delete
       }
       if {$close_acknowledged || ![info exists link]} {
          # Communications completed
          $this Delete ; return
       }
       # Otherwise, wait for link to drain final messages
       Oc_EventHandler New read_handler $link Readable \
           [list $this SafeCloseReceive] -groups [list $this]
    }

    method LinkDelete {} {
       # This is the handler for the $link Delete event.  In this code
       # path the $this Destructor doesn't need to call the $link
       # destructor (since that is the event being handled), which we
       # ensure by unsetting link before calling $this Delete.
       catch {unset link}
       if {[info exists inside_SafeClose] && $inside_SafeClose} {
          # SafeClose in process. Return and let SafeClose generate
          # destructor call when complete.
          return
       }
       $this Delete
    }

    # Variables used to insure that events are generated exactly once.
    # When Destructor is exited all instance variables are
    # automatically deleted.
    private variable this_Delete = 1
    private variable this_DeleteEnd = 1
    Destructor {
       if {[info exists write_disabled]} {
          # existence check necessary to protect against bad Delete
          # handlers that call back into this routine.
          set write_disabled 1
       }
       if {[info exists this_Delete]} {
          unset this_Delete
          # Note: The account server pkg/net/threads/account.tcl and
          #       host server pkg/net/threads/host.tcl have
          #       Oc_EventHandlers tied to Net_Connection Delete.
          Oc_EventHandler Generate $this Delete
       }
       # delete handlers before deleting event-generating members
       foreach i [array names toe] {
          after cancel $toe($i)
          unset toe($i)
       }
       if {[info exists link]} {
          Oc_EventHandler DeleteGroup $this-LinkDelete
          $link Delete
          unset link
       }
       if {[info exists myProtocol] && ![string match {} $myProtocol]} {
          set tmp $myProtocol
          set myProtocol {}
          catch {$tmp Delete}
          # Note: protocol may be deleted by Net_Server that made it.
       }
       if {[info exists this_DeleteEnd]} {
          unset this_DeleteEnd
          Oc_EventHandler Generate $this DeleteEnd
       }
       Oc_EventHandler DeleteGroup $this
       # SafeClose clients should use the DeleteEnd event to
       # determine when close is complete.
    }
}
