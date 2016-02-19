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

    # The socket we hand off to our link
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
                if {[llength $msg] == 1} {
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
		}
            }
            tell {
                set message [lrange $msg 1 end]
                Oc_EventHandler Generate $this Readable
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
        if {$write_disabled} {
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
       ## called immediately, so at this point instance variables may
       ## be unset.  Thus the local storage of the $id value.
       return $mid
    }

    method Timeout { qid } {
        Oc_Log Log "$this Timeout waiting for reply to query $qid" status $class
        Oc_EventHandler Generate $this Timeout$qid
    }

    method OwnsProtocol { p } {
	string match $myProtocol $p
    }

    method SafeCloseReceive {} {
       # Replaces Receive method (q.v.) on SafeClose.  Discards all
       # input except Reply messages.  Calls $this Delete when all
       # queued replies have been received.
       set rawmessage [$link Get]
       Oc_Log Log "$this Received raw message: $rawmessage" status $class
       if {![catch {lindex $rawmessage 0} type] \
               && [string compare reply $type]==0} {
          set rid [lindex $rawmessage 1]
          if {![catch {set toe($rid)} ev]} {
             after cancel $ev
             unset toe($rid)
          }
          set message [lrange $rawmessage 2 end]
          Oc_EventHandler Generate $this Reply$rid
          if {[llength [array names toe]]==0} {
             $this Delete
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
       set write_disabled 1
       if {!$ready} {
          # Close request during initialization.  Delete immediately.
          $this Delete ; return
       }
       if {[llength [array names toe]]==0} {
          $this Delete ; return
       }
       if {[info exists read_handler]} {
          $read_handler Delete
       }
       Oc_EventHandler New read_handler $link Readable \
           [list $this SafeCloseReceive] -groups [list $this]
    }

    method LinkDelete {} {
       # This is the handler for the $link Delete event.  In this code
       # path the $this Destructor doesn't need to call the $link
       # destructor (since that is the event being handled), which we
       # insue by unsetting link before calling $this Delete.
       catch {unset link}
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
          $tmp Delete
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
