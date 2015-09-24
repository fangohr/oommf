# FILE: thread.tcl
#
# An object which plays the role of a thread within an interpreter.
#
# The Tcl event loop must be running in order for non-blocking socket
# communications to work.
#
# Last modified on: $Date: 2007-03-21 01:17:06 $
# Last modified by: $Author: donahue $
#

Oc_Class Net_Thread {

    # Index of thread objects by hostname,pid
    array common index {}

    # Name of host where the account this object represents resides
    const public variable hostname = localhost

    # Name of account this object represents
    const public variable accountname

    # Process ID of the thread
    const public variable pid

    # The "alias" of this thread
    public variable alias

    # The account under which this thread is running
    private variable account

    # Our connection to the actual running thread
    private variable connection

    # Port on host on which the thread server listens
    private variable port
    
    # Send id
    private variable id = 0

    # Reply message
    private variable reply

    # Is object ready to relay messages to and from the thread?
    private variable ready = 0

    private variable protocolname
    private variable protocolversion
    private variable messages

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
        $this Configure -gui $btnvar
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
        if {[catch {
                Net_Account New account -hostname $hostname \
                -accountname $accountname} msg]} {
            error "Can't construct account object.\n$msg"
        } else {
            Oc_EventHandler New _ $account Delete [list $this Delete] \
                    -groups [list $this]
            if {[$account Ready]} {
                $this LookupThreadPort
            } else {
                Oc_EventHandler New _ $account Ready \
                        [list $this LookupThreadPort] -oneshot 1 \
                        -groups [list $this]
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
                set port [lindex $reply 1]
                set alias [lindex $reply 2]
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
            $this Delete
        } else {
             Oc_EventHandler New _ $connection Delete [list $this Delete] \
                     -groups [list $this]
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
        if {![regexp {OOMMF ([^ ]+) protocol ([0-9.]+)} [$connection Get] \
                match name version]} {
            # Not an OOMMF thread
            set msg "No OOMMF thread server answering on $hostname:$port"
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
            set emsg "Error report from $alias<$pid>:\n\t[lrange $reply \
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

    # Send a command to the thread server
    method Send { args } {
        if {!$ready} {
            error "thread $hostname:$pid not ready"
        }
        incr id
        set qid [eval $connection Query $args]
        Oc_EventHandler New _ $connection Reply$qid [list $this \
                Receive $id] -oneshot 1 -groups [list $this $this-$id]
        Oc_EventHandler New _ $connection Timeout$qid [list $this \
                Timeout $id] -oneshot 1 -groups [list $this $this-$id]
        return $id
    }

    method Timeout { rid } {
        Oc_EventHandler DeleteGroup $this-$rid
        Oc_EventHandler Generate $this Timeout$rid
    }

    method Ready {} {
        return $ready
    }

    Destructor {
        set ready 0
        Oc_EventHandler Generate $this Delete
        Oc_EventHandler DeleteGroup $this
        if {[info exists connection]} {
            $connection Delete
        }
        if {[info exists myWidget]} {
            $this Configure -gui 0
        }
        if {[info exists hostname] && [info exists pid]} {
            unset index($hostname,$pid)
        }
    }

}
