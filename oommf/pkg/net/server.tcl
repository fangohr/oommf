# FILE: server.tcl
#
# A generic server that can be instantiated and customized.
#
# Last modified on: $Date: 2015/09/30 07:41:35 $
# Last modified by: $Author: donahue $
#

Oc_Class Net_Server {

  # Options settable through the oommf/config/options.tcl file:
  #
  # forbidRemoteConnections either allows (0) or denies (1)
  #   connections from remote machines.
  #
  # checkUserIdentities should be either 0, 1, or 2.  It is active
  #   only when a connection is made originating from the local
  #   machine.  If 0, then no identity check is performed---the
  #   connection is allowed.  If 2, then an identity check is
  #   performed, and the connection will be refused unless the same
  #   user is on both sides of the connection.  If 1, then a check is
  #   made, but if the connection is refused only if the client user
  #   is positively different than the server user.  (On some systems,
  #   it may not be possible to uncover the identity of the client
  #   side user.)
  #
  # For example, to turn on strong local machine identity
  # verification, put this in the oommf/config/options.tcl file:
  #
  #    Oc_Option Add * Net_Server checkUserIdentities 2
  #
  const public common defaultAllowList {}
  const public common defaultDenyList {}
  const public common forbidRemoteConnections 1

  # Protocol to be used on connections established by this server
  # Need good check that doesn't depend on naming conventions
  const public variable protocol 

  # The alias by which this server identifies itself
  const public variable alias = {}

  # Boolean - does this server register with a services directory?
  const public variable register = 1

  # List of remote hostname patterns allowed to make connections.
  public variable allow {
        Oc_Log Log "$this: New allow: $allow" status $class
  }

  # List of remote hostname patterns forbidden to make connections.
  public variable deny {
        Oc_Log Log "$this: New deny: $deny" status $class
  }

  # Identity checks
  const public common checkUserIdentities 1
  const public variable user_id_check

  # Track whether the server instance owns, and must clean up
  # the protocol instances it uses.
  const private variable imadeprotocol = 0

  private variable forbidServiceStart = 0

  # The socket through which this server accepts connections
  private variable socket

  private variable port

  Constructor {args} {
    set allow $defaultAllowList
    set deny $defaultDenyList
    set user_id_check $checkUserIdentities
    set alias [Oc_Main GetAppName]  ;# Default alias is application name
    eval $this Configure $args
    if {![info exists protocol]} {
      # Provide a default protocol
      Net_Protocol New protocol -name "Default protocol"
      set imadeprotocol 1
    }
    # Die when $protocol dies.
    Oc_EventHandler New _ $protocol Delete [list $this Delete] \
        -groups [list $this]
  }

  method ForbidServiceStart {} {
    set forbidServiceStart 1
  }

  # Start the server accepting connections port
  method Start { _port } { 
    if {[info exists socket]} {
      error "Server already started"
    }
    if {$forbidServiceStart} {
      return -code error "Server (re-)start is forbidden!  (App shutting down?)"
    }
    if {$forbidRemoteConnections} {
        set socket [socket -server [list $this VerifyHostAccess] \
            -myaddr localhost $_port]
    } else {
        set socket [socket -server [list $this VerifyHostAccess] \
            $_port]
    }
    set port [lindex [fconfigure $socket -sockname] 2]
    Oc_Log Log "Server $this started on port $port..." status $class
    if {$register} {
        $this Register
    }
  }

  method VerifyHostAccess {accsock clientipaddr clientport} {
     set myinfo [fconfigure $accsock -sockname]
     set remoteinfo [fconfigure $accsock -peername]
     set myhostname [lindex $myinfo 1]
     set remotehostname [lindex $remoteinfo 1]
     if {$forbidServiceStart} {
        # Server is shutting down --- don't create any new
        # Net_Connections!
        $this RefuseConnection $accsock $remotehostname $clientport
        return
     }
     if {![string match $myhostname $remotehostname]} {
        if {[string match $clientipaddr $remotehostname]} {
           # Unable to resolve remote hostname
           $this RefuseConnection $accsock $remotehostname $clientport
           return
        }
        set allowconnection 0
        foreach pattern $allow {
           if {[string match $pattern $remotehostname]} {
              set allowconnection 1
              break
           }
        }
        foreach pattern $deny {
           if {[string match $pattern $remotehostname]} {
              set allowconnection 0
              break
           }
        }
        if {!$allowconnection} {
           $this RefuseConnection $accsock $remotehostname $clientport
           return
        }
     }

     if {$user_id_check} {
        set clearance [Net_CheckSocketAccess $accsock]
        if {$clearance < $user_id_check} {
           $this RefuseConnection $accsock $remotehostname $clientport \
              "User id check failed."
           return
        }
     }

     Net_Connection New _ -socket $accsock -protocol $protocol
  }

  private method RefuseConnection {accsock rhost cport {errmsg {}}} {
     puts $accsock "tell Connection refused"
     close $accsock
     set logmsg "Server $this refused connection from $rhost:$cport"
     if {![string match {} $errmsg]} {
        append logmsg "; $errmsg"
     }
     Oc_Log Log $logmsg status $class
  }

  # Returns true if server is accepting connections, false otherwise.
  method IsListening {} {
    return [info exists socket]
  }

  # Return port we're listening on
  method Port {} {
      if {[info exists port]} {
          return $port
      }
      error "server is not listening"
  }

  # Stop accepting connections
  method Stop {} {
    if {![info exists socket]} {
      error "Server not started"
    }
    Oc_EventHandler DeleteGroup $this-AccountRebirth
    Oc_Log Log "Stopping server $this ..." status $class
    if {$register} {
        set socketinfo [fconfigure $socket -sockname]
        set port [lindex $socketinfo 2]
        $this Deregister
    }
    unset port
    close $socket
    unset socket
  }

    private variable account

    private method Register {} {
        if {[catch {Net_Account New account -hostname localhost} msg]} {
            catch {unset account}
            $this RegisterFail $msg {}
            return
        }
        # Current implementation requires a persistent connection to the
        # account server through a Net_Account instance.  Consider whether
        # this can be relaxed after a successful registration?
        Oc_EventHandler New _ $account Delete \
                [list $this RegisterFail "account died" {}] -groups [list $this]
        if {[$account Ready]} {
            $this AccountReady
        } else {
            Oc_EventHandler New _ $account Ready [list $this AccountReady] \
                -oneshot 1 -groups [list $this]
        }
    }

    private variable sid
    method SID {} {
	return $sid
    }

    method AccountReady {} {
	regexp _${class}(.*) $this -> mySerial
	set sid [$account OID]:$mySerial
        set qid [$account Send register $sid $alias \
                    $port [$protocol Cget -name]]
        Oc_EventHandler New _ $account Reply$qid \
	     [list $this RegisterReply $qid] \
             -groups [list $this $this-$qid]
        Oc_EventHandler New _ $account Timeout$qid \
             [list $this RegisterFail "timeout on registration reply" $qid] \
             -groups [list $this $this-$qid]
	# setup to re-register in event of account rebirth
	Oc_EventHandler New _ $account Ready [list $this AccountReady] \
		-oneshot 1 -groups [list $this $this-AccountRebirth]
    }

    method RegisterReply {qid} {
        Oc_EventHandler DeleteGroup $this-$qid
        set reply [$account Get]
        switch -- [lindex $reply 0] {
            0 {
                Oc_Log Log "Server $this registered successfully" status $class
                Oc_EventHandler Generate $this RegisterSuccess
            }
            default {
                $this RegisterFail "registration error:\
			[lrange $reply 1 end]" $qid
            }
        }
    }

    method RegisterFail {msg qid} {
       Oc_EventHandler Generate $this RegisterFail
        Oc_EventHandler DeleteGroup $this-$qid
        set accountname [Net_Account DefaultAccountName]
        $this Delete
        global errorInfo
        set errorInfo "No stack available"
        Oc_Log Log "Unable to register server with\
                localhost:$accountname:\n\t$msg" warning $class
    }

    private method Deregister {} {
        if {![info exists account] || ![$account Ready]} {
            return
        }
        set qid [$account Send deregister $sid]
        Oc_EventHandler New _ $account Reply$qid \
                [list $this DeregisterReply $qid] \
                -groups [list $this $this-$qid]
        Oc_EventHandler New _ $account Timeout$qid \
                [list $this DeregisterFail "timeout on dereg. reply" $qid]] \
                -groups [list $this $this-$qid]
    }

    method DeregisterReply {qid} {
        Oc_EventHandler DeleteGroup $this-$qid
        set reply [$account Get]
        switch -- [lindex $reply 0] {
            0 {
                Oc_Log Log "Server $this deregistered successfully"\
			status $class
            }
            default {
                $this DeregisterFail "deregistration error" $qid
            }
        }
    }

    method DeregisterFail {msg qid} {
        Oc_EventHandler DeleteGroup $this-$qid
        set accountname [Net_Account DefaultAccountName]
        # Ought to send up fireworks; this is bad.
        $this Delete
        global errorInfo
        set errorInfo "No stack available"
        Oc_Log Log "Unable to deregister server with\
                localhost:$accountname:\n\t$msg" warning $class
    }

  Destructor {
    if {[info exists socket]} {
        # Stop method launches Deregister in background.  Should we wait
        # to confirm it before dying?   Proper usage is to Stop server,
        # then delete it.  More thought needed here...
        $this Stop
    }
    Oc_EventHandler DeleteGroup $this
    if {$imadeprotocol} {
        $protocol Delete
    }
  }

}
