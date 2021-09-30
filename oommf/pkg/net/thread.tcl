# FILE: thread.tcl
#
# An object which plays the role of a thread within an interpreter.
#
# The Tcl event loop must be running in order for non-blocking socket
# communications to work.
#
# Last modified on: $Date: 2015/09/30 07:41:35 $
# Last modified by: $Author: donahue $
#

Oc_Class Net_Thread {

    # Index of thread objects by hostname,pid
    array common index {}

    # Name of host where the account this object represents resides
    const public variable hostname = localhost

    # Name of account this object represents
    const public variable accountname

    # Here "pid" is NOT the process ID of the thread, but rather
    # a reference of the form "oid:link#", e.g., 4:0 for the first
    # server on the application with oid 4. This is presumably a
    # historical artifact. Elsewhere this is referred to as "sid",
    # for service id.
    const public variable pid

    # The "alias" of this thread. If this value is not set in the
    # constructor call then it will be retrieved from the account
    # server.
    public variable alias

    # Set enable_cache true at construction time to allow the Net_Thread
    # method Send to be invoked before the connection reads Ready. In
    # this case any message requests are cached until Ready, at which
    # time all cached messages are sent. If you enable this then you
    # also must set protocol (below) at construction time, and an error
    # will be raised if the protocol read on Ready doesn't match.
    #   Typically usage is to ask the account server for "services", and
    # use that information to create a Net_Thread instance under the
    # assumption that the service is alive and using the protocol as
    # reported by the account server. The client then doesn't have to
    # include code to handle waiting for the Net_Thread to go ready, but
    # can use the Net_Thread Send message immediately.
    # Note: External access to enable_cache should be restricted to
    #       setting at construction time.
    const public variable enable_cache = 0
    private variable cache = {}

    # The public variable protocol is used for specifying the
    # anticipated protocol at construction time. It is required to be
    # set if enable_cache is true, but is otherwise optional. When
    # thread Ready occurs, if protocol is set then an error is raised
    # if it doesn't match the protocol reported by the account or
    # thread servers. The protocol format matches that reported by the
    # account server "services" message, e.g.,
    #     OOMMF DataTable protocol 0.1
    # Note that this is different than the return from the Net_Thread
    # method Protocol, which returns a shortened form of the protocol
    # name, in this example "DataTable", and without version info. (The
    # version info is available through the ProtocolVersion method.)
    public variable protocol

    # The account under which this thread is running. This may be
    # undefined if enough information (protocol, alias, port) is
    # included in the constructor call to bypass the account server.
    private variable account

    # Our connection to the actual running thread
    private variable connection

    # Port on host on which the thread server listens. Similar to the
    # "alias" and "protocol" variables, this value can be retrieved
    # from the account server. If the protocol, alias and port are
    # given in the constructor call, then the account server is not
    # queried, and the constructor-specified port is used directly.
    public variable port

    # Send id
    private variable id = 0

    # Reply message
    private variable reply

    # Is object ready to relay messages to and from the thread? Variable
    # ready is set true during connection initialization when the server
    # replies to "Query messages", and remains true until it is set to 0
    # in the Net_Thread destructor. The variable write_disabled is set
    # true as part of the SafeClose procedure.
    private variable ready = 0
    private variable write_disabled = 0

    private variable protocolname
    private variable protocolversion
    private variable messages

    method Id {} {
       return $pid   ;# pid format is "oid:link"
    }

    method Protocol {} {
        return $protocolname
    }

    method ProtocolVersion {} {
        return $protocolversion
    }

    method Messages {} {
        return $messages
    }

    public variable preserveGui = 0 {
	if {[catch {expr {$preserveGui && $preserveGui}} m]} {
	    return -code error $m
	}
    }
    public variable gui = 0 {
	if {[catch {expr {$gui && $gui}} m]} {
	    return -code error $m
	}
        if {$gui} {
            if {[info exists myWidget]} {
                return
            }
	    if {$hasGui == 2} {
		Net_ThreadWish New myWidget -thread $this
	    } else {
		Net_ThreadGui New myWidget -thread $this
	    }
        } else {
            if {![info exists myWidget]} {
                return
            }
            $myWidget Delete
            unset myWidget
        }
        set btnvar $gui
    }
    private variable myWidget
    private variable hasGui = 0
    private variable btnvar

    method HasGui {} {
        return $hasGui
    }

    method ToggleGui {args} {
       if {$ready} {
          $this Configure -gui $btnvar
       } else {
          Oc_EventHandler New _ $this Ready \
             [list $this Configure -gui $btnvar] \
             -oneshot 1 -groups [list $this]
       }
    }

    # Constructor is a non-blocking operation.
    Constructor { args } {
        eval $this Configure $args
        if {![info exists accountname]} {
            set accountname [Net_Account DefaultAccountName]
        }
        if {![info exists pid]} {
            error "Thread must have pid!"
        }
        if {[info exists protocol]} {
           if {![regexp {OOMMF ([^ ]+) protocol ([0-9.]+)} $protocol \
                    match name version]} {
              error "Net_Thread construction with invalid protocol\
                      string: $protocol"
           }
           set protocolname $name       ;# Allow client filtering on protocol
           set protocolversion $version ;# even if thread is not yet "Ready".
        } elseif {$enable_cache} {
           error "Net_Thread construction with cache enabled requires protocol"
        }

        # Enforce only one object per account
        if {![catch {set index($hostname,$pid)} existingthread]} {
            unset hostname
            unset pid
            $this Delete
            array set argarray $args
            catch {unset argarray(-hostname)}
            catch {unset argarray(-accountname)}
            catch {unset argarray(-pid)}
            catch {unset argarray(-alias)}
            eval $existingthread Configure [array get argarray]
            return $existingthread
        }
        array set index [list $hostname,$pid $this]

        # Do we need to query the account server?
        if {[info exists port] && [info exists alias] \
              && [info exists protocol]} {
           # Account server not needed. Connect to thread port directly.
           $this Connect
        } else {
           if {[catch { Net_Account New account -hostname $hostname \
                           -accountname $accountname} msg]} {
              error "Can't construct account object.\n$msg"
           } else {
              Oc_EventHandler New _ $account Delete [list $this Delete] \
                 -oneshot 1 -groups [list $this]
              if {[$account Ready]} {
                 $this LookupThreadPort
              } else {
                 Oc_EventHandler New _ $account Ready \
                    [list $this LookupThreadPort] -oneshot 1 \
                    -groups [list $this]
              }
           }
        }
    }

    method LookupThreadPort {} {
        set qid [$account Send lookup $pid]
        Oc_EventHandler New _ $account Reply$qid [list $this \
                GetThreadPort] -oneshot 1 -groups [list $this $this-gtp]
        Oc_EventHandler New _ $account Timeout$qid [list $this \
                NoAnswerFromAccount] -oneshot 1 -groups [list $this $this-gtp]
    }

    method GetThreadPort {} {
        Oc_EventHandler DeleteGroup $this-gtp
        set reply [$account Get]
        if {[llength $reply] < 2} {
            error "Bad reply from account $accountname: $reply"
        }
        switch -- [lindex $reply 0] {
            0 {
               set errmsg [list]
               lassign [lrange $reply 1 end] \
                  port_check alias_check protocol_check
               if {[info exists port] && \
                      [string compare $port $port_check]!=0} {
                  lappend errmsg "Thread server port mismatch:\
                                 $port != $port_check"
               }
               if {[info exists alias] && \
                      [string compare $alias $alias_check]!=0} {
                  lappend errmsg "Thread server alias mismatch:\
                                 $alias != $alias_check"
               }
               if {[info exists protocol] && \
                      [string compare $protocol $protocol_check]!=0} {
                  lappend errmsg "Thread server protocol mismatch:\
                                 $protocol != $protocol_check"
               }
               if {[llength $errmsg]} {
                  $this Delete
                  global errorInfo
                  set errorInfo "No stack available"
                  Oc_Log Log [join $errmsg "\n"] status $class
               }
               set port $port_check
               set alias $alias_check
               set protocol $protocol_check
               $this Connect
            }
            default {
                # Account returns an error - assume that means lookup failed
                set msg "No thread $pid registered with account\
                        [$account Cget -accountname]:\n\t[lrange $reply \
                        1 end]"
                $this Delete
                global errorInfo
                set errorInfo "No stack available"
                Oc_Log Log $msg status $class
            }
        }
    }

    method NoAnswerFromAccount {} {
        Oc_EventHandler DeleteGroup $this-gtp
        # Host not responding -- give up.
        set msg "No reply from account [$account Cget -accountname]"
        $this Delete
        global errorInfo
        set errorInfo "No stack available"
        Oc_Log Log $msg warning $class
    }

    method Connect {} {
        if {[catch {
                Net_Connection New connection -hostname $hostname -port $port
                }]} {
           if {[info exists write_disabled]} {
              set write_disabled 1
           }
           catch {unset connection}
           $this Delete
        } else {
             Oc_EventHandler New _ $connection Delete \
                 [list $this ConnectionDelete] \
                 -oneshot 1 -groups [list $this $this-ConnectionDelete]
             Oc_EventHandler New connection_safeclose_handler \
                 $connection SafeClose [list $this SafeClose] \
                 -oneshot 1 -groups [list $this $this-ConnectionDelete]
             Oc_EventHandler New _ $connection Ready \
                     [list $this ConnectionReady] -oneshot 1 \
                     -groups [list $this]
        }
    }

    method ConnectionReady {} {
        Oc_EventHandler New _ $connection Readable \
                [list $this VerifyOOMMFThread] -oneshot 1 -groups [list $this]
    }

    method VerifyOOMMFThread {} {
        set protocol_check [$connection Get]
        if {![regexp {OOMMF ([^ ]+) protocol ([0-9.]+)} $protocol_check \
                 match name version]} {
            # Not an OOMMF thread
            set msg "No OOMMF thread server answering on $hostname:$port"
            $this Delete
            global errorInfo
            set errorInfo "No stack available"
            Oc_Log Log $msg warning $class
            return
        }
        if {[info exists protocol] && \
               [string compare $protocol $protocol_check]!=0} {
            set msg "OOMMF thread server protocol mismatch:\
                    protocol \"$protocol_check\" doesn't match\
                    anticipated protocol \"$protocol\""
            $this Delete
            global errorInfo
            set errorInfo "No stack available"
            Oc_Log Log $msg warning $class
            return
        }
        set protocolname $name
        set protocolversion $version
        Oc_Log Log "OOMMF thread $hostname:$pid ready" status $class
        Oc_EventHandler New _ $connection Readable [list $this Hear] \
                -groups [list $this]
        incr id
	if {[string match 0.0 $protocolversion]} {
	    # Compatibility for old protocols
            set qid [$connection Query HasGui]
	    Oc_EventHandler New _ $connection Reply$qid [list $this \
		    ReceiveHasGui] -oneshot 1 -groups [list $this $this-gui]
	    Oc_EventHandler New _ $connection Timeout$qid [list $this \
		    Timeout gui] -oneshot 1 -groups [list $this $this-gui]
	    return
	}
        set qid [$connection Query messages]
	Oc_EventHandler New _ $connection Reply$qid [list $this \
		ReceiveMessages] -oneshot 1 -groups [list $this $this-messages]
	Oc_EventHandler New _ $connection Timeout$qid [list $this \
		Timeout messages] -oneshot 1 -groups [list $this $this-messages]
    }

    method ReceiveMessages {} {
        Oc_EventHandler DeleteGroup $this-messages
        set reply [$connection Get]
        if {[lindex $reply 0]} {
            set src {}
            if {[info exists alias]} { append src $alias }
            if {[info exists pid]}   { append src <$pid> }
            set emsg "Error report from $src:\n\t[lrange $reply \
                    1 end]\n\nIn reply to query:\n\tmessages"
            error $emsg $emsg
        }
        set messages [lindex $reply 1]
	if {[lsearch -exact $messages GetGui] >= 0} {
	    set hasGui 2
	} elseif {[lsearch -exact $messages Inputs] >= 0} {
	    set hasGui 1
	}
        set ready 1
        $this SendCache
        Oc_EventHandler Generate $this Ready
    }

    method ReceiveHasGui {} {
        Oc_EventHandler DeleteGroup $this-gui
        set reply [$connection Get]
        if {[lindex $reply 0]} {
            set emsg "Error report from $alias<$pid>:\n\t[lrange $reply \
                    1 end]\n\nIn reply to query:\n\tHasGui"
            error $emsg $emsg
        } else {
            set hasGui [lindex $reply 1]
        }
        set ready 1
        $this SendCache
        Oc_EventHandler Generate $this Ready
    }

    # Handle 'tell' messages from connection
    method Hear {} {
        set reply [$connection Get]
        Oc_EventHandler Generate $this Readable
    }

    method Receive { rid } {
        Oc_EventHandler DeleteGroup $this-$rid
        set reply [$connection Get]
        Oc_EventHandler Generate $this Reply$rid
    }

    method Get {} {
        return $reply
    }


    # Send cached message to thread server. This routine should be
    # called after "ready" is set true in method ReceiveHasGui or method
    # ReceiveMessages. The code here should be compatible with that in
    # method Send.
    method SendCache {} {
       if {[llength $cache]} {
          # Send cached messages, if any
          foreach msgpair $cache {
             lassign $msgpair msgid msg
             set qid [eval $connection Query $msg]
             if {[info exists connection]} {
                # Check that connection exists just in case Query failed
                # and initiated a Net deletion cascade.
                Oc_EventHandler New _ $connection Reply$qid [list $this \
                   Receive $msgid] -oneshot 1 -groups [list $this $this-$msgid]
                Oc_EventHandler New _ $connection Timeout$qid [list $this \
                   Timeout $msgid] -oneshot 1 -groups [list $this $this-$msgid]
             }
          }
          set cache {}
       }
    }

    # Send a command to the thread server. This method and SendCache
    # should have consistent behavior.
    method Send { args } {
        if {![info exists write_disabled]} {
           error "Net_Thread write_disabled doesn't exist???"
        }
        if {$write_disabled} {
           # Safe shutdown request from this side of the connection;
           # eat output.
           return -1
        }
        # Kludge: The generic Net_Protocol class defines handling for
        # the messages "bye" and "exit", which in both cases cause the
        # server to close. The server Replies to bye and exit, but
        # ignores any further messages because the client is not
        # suppose to Send any additional messages after bye or exit.
        # Build protection here ensure this contract. This is
        # considered a kludge because ideally both sides of this
        # contract (client and server) would be enforced in one
        # location in the code, easing future modifications. Moreover,
        # this "fix" is brittle in that any messages sent after a bye
        # or exit are swallowed, which in particular means that there
        # will not be Replies from the server for any such
        # messages. Any client code waiting on replies to such
        # messages will hang.
        if {[string compare bye $args]==0 || [string compare exit $args]==0} {
           # NB: Changes to this code should be reflected in bye and exit
           #     handling in Net_Protocol.
           set write_disabled 1
        }

        if {!$ready} {
           if {$enable_cache} {
              # Cache message for sending when initialization is complete
              incr id
              lappend cache [list $id $args]
              return $id
           } else {
              error "thread $hostname:$pid not ready"
           }
        }
        # Note: If cache is used, then it gets written out via call to SendCache
        # when "ready" first goes true, i.e., before program flow gets here. In
        # particular, [llength $cache] should be 0.
        set id_copy [incr id]
        set qid [eval $connection Query $args]
        if {[info exists connection]} {
           # Check that connection exists just in case Query failed
           # and initiated a Net deletion cascade. This is also the
           # reason for id_copy.
           Oc_EventHandler New _ $connection Reply$qid [list $this \
                  Receive $id] -oneshot 1 -groups [list $this $this-$id]
           Oc_EventHandler New _ $connection Timeout$qid [list $this \
                  Timeout $id] -oneshot 1 -groups [list $this $this-$id]
        }
        return $id_copy
    }

    method Timeout { rid } {
        Oc_EventHandler DeleteGroup $this-$rid
        Oc_EventHandler Generate $this Timeout$rid
    }

    method Ready {} {
        return $ready
    }


    # Variable inside_SafeClose is used to handle re-entrancy in method
    # SafeClose.
    private variable inside_SafeClose = 0
    method SafeClose {} {
       if {![info exists inside_SafeClose] || \
              $inside_SafeClose == 1       || \
              $inside_SafeClose>2} {
          # inside_SafeClose dne => Destructor already ran
          # inside_SafeClose = 0 => First pass
          # inside_SafeClose = 1 => SafeClose event servicing
          # inside_SafeClose = 2 => Waiting for cache to drain
          # inside_SafeClose = 3 => SafeClose completed
          return
       }

       # Phase 1: Inform users that thread is shutting down. Don't
       # generate this event more than once.
       if {$inside_SafeClose == 0} {
          set inside_SafeClose 1 ;# This inside_SafeClose setting
          ## guards against any re-entrancy attemps triggered by the
          ## following Oc_EventHandler Generate.
          Oc_EventHandler Generate $this SafeClose
          if {![info exists write_disabled]} {
             # Apparently $this destructor was called while handling
             # $this SafeClose event. That is bad form, but there's
             # nothing to do now but bail.
             return
          }
          set write_disabled 1 ;# Disable any further writes
       }

       # Phase 2: Check write cache
       if {[llength $cache]} {
          if {$ready} {
             # This shouldn't happen!
             error "Ready connection with non-empty cache shouldn't happen!"
          }
          if {$inside_SafeClose<2} {
             set inside_SafeClose 2
             # SafeClose requested before connection completed
             # initialization, and with data held in cache. Wait; when
             # connection goes ready the cached data will be sent and a
             # "$this Ready" will be generated. Re-call SafeClose then:
             Oc_EventHandler New _ $this Ready [list $this SafeClose] \
                -oneshot 1 -groups [list $this]
          }
          return
       }

       set inside_SafeClose 3
       if {[info exists connection_safeclose_handler]} {
          $connection_safeclose_handler Delete
          unset connection_safeclose_handler
       }
       if {[info exists connection]} {
          if {[catch {$connection SafeClose}]} {
             $this Delete
          }
       }
       # When $connection SafeClose completes it calls the $connection
       # destructor, which will generate a $connection Delete event
       # that will in turn trigger a call to $this destructor.
    }

    method ConnectionDelete {} {
       # This is the handler for the $connection Delete event.  In
       # this code path the $this Destructor doesn't need to call the
       # $connection destructor (since that is the event being
       # handled), which we insure by unsetting connection before
       # calling $this Delete.
       if {[info exists write_disabled]} {
          set write_disabled 1
       }
       catch {unset connection}
       if {[info exists inside_SafeClose] && \
              0<$inside_SafeClose && $inside_SafeClose<3} {
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
        set ready 0
        if {[info exists write_disabled]} {
           set write_disabled 1
        }
        if {[llength $cache]} {
           Oc_Log Log "Data loss: Cache dropped" warning $class
           Oc_EventHandler Generate $this DataLoss -data $cache
        }
        if {[info exists this_Delete]} {
           unset this_Delete
           Oc_EventHandler Generate $this Delete
        }
        if {[info exists connection]} {
           Oc_EventHandler DeleteGroup $this-ConnectionDelete
           $connection Delete
           unset connection
        }
        if {[info exists myWidget]} {
            $this Configure -gui 0
        }
        if {[info exists hostname] && [info exists pid]} {
           catch {unset index($hostname,$pid)}
        }
        if {[info exists this_DeleteEnd]} {
           unset this_DeleteEnd
           Oc_EventHandler Generate $this DeleteEnd
        }
        # SafeClose clients should use the DeleteEnd event to
        # determine when close is complete.
        Oc_EventHandler DeleteGroup $this
    }

}
