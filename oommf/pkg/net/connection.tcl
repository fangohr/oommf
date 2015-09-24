# FILE: connection.tcl
#
# An object which manages asynchronous communication over a Net_Link.
#
# Query, expects reply
# Tell, just sends message
#
# The Tcl event loop must be running for this event-driven object to function.
#
# Last modified on: $Date: 2008-05-20 07:13:39 $
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

    # query id
    private variable id = 0

    # Ready to send/receive?
    private variable ready = 0

    # Array of timeout events
    private array variable toe = {}
    public variable timeout = 6000000

    # Identity checks
    const public variable user_id_check

    # Constructor is a non-blocking operation.  
    Constructor { args } {
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
        Oc_EventHandler New _ $link Delete [list $this Delete] \
                -oneshot 1 -groups [list $this]
    }

    method ServerLinkReady {} {
        Oc_Log Log "Put [concat tell [$myProtocol Cget -name]]" status $class
        $link Put [concat tell [$myProtocol Cget -name]]
        $this LinkReady
    }

    method LinkReady {} {
        Oc_EventHandler New _ $link Readable [list $this Receive] \
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

        # Get the string
        set rawmessage [$link Get]
        Oc_Log Log "Received raw message: $rawmessage" status $class
        # Test for list structure 
        if {[catch {set rawmessage [lrange $rawmessage 0 end]} msg]} {
            Oc_Log Log "Message not a Tcl list: $rawmessage" \
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
                    Oc_Log Log "Received bad message: $msg" \
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
                    Oc_Log Log "Received bad message: $msg" \
                            warning $class
                    return
                }
                set id [lindex $msg 1]
                set message [lrange $msg 2 end]
		# Save link in case our destructor is called and instance
		# variable is lost.
		set saveLink $link
                Oc_Log Log "Handling query: $message" status $class
                # This doesn't allow for event-driven query handling.
                # Should it?  If so, should it timeout?
		set reply [$myProtocol Reply $message]
		# Must check existence in case the call above
		# called $saveLink destructor.
		if {[llength [info commands $saveLink]]} {
                    Oc_Log Log "Put [concat reply $id $reply]" status $class
                    $saveLink Put [concat reply $id $reply]
		}
            }
            tell {
                set message [lrange $msg 1 end]
                Oc_EventHandler Generate $this Readable
            }
            default {
                # Log non-conforming messages, but continue.
                Oc_Log Log "Received bad message: $msg" \
                        warning $class
            }
        }
    }

    method Get {} {
        return $message
    }

    method Tell { args } {
        if {!$ready} {
            error "connection not ready"
        }
        Oc_Log Log "Put [concat tell $args]" status $class
        $link Put [concat tell $args]
    }

    method Query { args } {
        if {!$ready} {
            error "connection not ready"
        }
        incr id
        array set toe [list $id [after $timeout $this Timeout $id]]
        Oc_Log Log "Put [concat query $id $args]" status $class
        $link Put [concat query $id $args]
        return $id
    }

    method Timeout { qid } {
        Oc_Log Log "Timeout waiting for reply to query $qid" status $class
        Oc_EventHandler Generate $this Timeout$qid
    }

    method OwnsProtocol { p } {
	string match $myProtocol $p
    }

    Destructor {
        Oc_EventHandler Generate $this Delete
        # delete handlers before deleting event-generating members
        foreach i [array names toe] {
            after cancel $toe($i)
            unset toe($i)
        }
        Oc_EventHandler DeleteGroup $this
        if {[info exists myProtocol]} {
	    set tmp $myProtocol
	    set myProtocol {}
            $tmp Delete
        }
        if {[info exists link]} {
            $link Delete
        }
    }

}
