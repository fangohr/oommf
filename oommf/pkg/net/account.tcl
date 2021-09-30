# FILE: account.tcl
#
# An object which plays the role of an account within an interpreter.
#
# Each host on the internet may have many accounts -- separate areas
# of storage each for the use of one or more users, usually with access
# controlled by some sort of security measure such as a password.  Each
# thread which runs on a host is owned by exactly one of the accounts on
# that host.  Thus, in a hierarchy of "ownership," accounts lie between
# hosts and threads.
#
# The Tcl event loop must be running in order for non-blocking socket
# communications to work.
#
# Last modified on: $Date: 2015/09/30 07:41:34 $
# Last modified by: $Author: donahue $
#

Oc_Class Net_Account {

    # Index of account objects by name -- used to avoid duplicate objects.
    array common index {}
    common dir

    private variable oid
    private variable lastoid

    # send_stack is a holding area for outgoing messages received
    # during a account server reset.
    private variable send_stack = {}

    # The command line -nickname option (in the Net option set) loads
    # nickname strings into the Oc_Option database, which in turn
    # initializes the class common variable "nicknames".  Each
    # instantiation of Net_Account registers these nicknames with its
    # account server.  Any nickname associations made after instance
    # construction are appended to this list.  If the connection to the
    # account server is lost, then upon a new connection to a
    # (presumably new) account server, all names in the nicknames
    # variable are reloaded into the account server.
    #
    # This design is built around the assumption that there is at most
    # one instance of Net_Account.  If a use-case for multiple
    # Net_Account instances is found, this design should be revisited.
    public common nicknames {}

    ClassConstructor {
        set dir [file dirname [info script]]
    }

    # Name of host where the account this object represents resides
    const public variable hostname = localhost

    # Name of account this object represents
    const public variable accountname

    # Default initializer of the accountname.  Tries to determine
    # account name under which current thread is running.
    proc DefaultAccountName {} {
       global env tcl_platform
       set name {}
       if {[info exists env(OOMMF_ACCOUNT_NAME)]
           && [string length $env(OOMMF_ACCOUNT_NAME)]} {
          set name $env(OOMMF_ACCOUNT_NAME)
       } elseif {[info exists tcl_platform(user)]
           && [string length $tcl_platform(user)]} {
          set name $tcl_platform(user)
       } else {
          switch -- $tcl_platform(platform) {
             unix {
                if {[info exists env(USER)]
                    && [string length $env(USER)]} {
                   set name $env(USER)
                } elseif {[info exists env(LOGNAME)]
                          && [string length $env(LOGNAME)]} {
                   set name $env(LOGNAME)
                } elseif {![catch {exec whoami} check]
                          && [string length $check]} {
                   set name $check
                } elseif {![catch {exec id -u -n} check]
                          && [string length $check]} {
                   # whoami is deprecated by posix id command
                   set name $check
                } elseif {![catch {exec id} check]
                          && [string length $check]} {
                   # Solaris has a non-posix id command that
                   # doesn't support the -u and -n switches.
                   if {[regexp {uid=[0-9]+\(([^)]+)\)} $check dummy check2]
                       && [string length $check2]} {
                      set name $check2
                   }
                }
             }
             windows {
                # Check environment
                if {[info exists env(USERNAME)]
                    && [string length $env(USERNAME)]} {
                   set name $env(USERNAME)
                }
                if {![catch {file tail [Nb_GetUserName]} check]} {
                   set name $check
                }
                # Find some reliable entry in the registry?
             }
          }
       }
       if {[string match {} $name]} {
          # Ultimate fallback
          set name oommf
       }
       return $name
    }

    # Milliseconds to wait for account thread to start on localhost
    public variable startWait = 60000
    # The ID of the current timeout (returned from 'after' command)
    private variable timeoutevent
    # Brake on host+account server restarts
    private variable do_restarts = 1
    private variable shutdownHandler

    # The host which on which this account resides
    private variable host

    # Our connection to the actual account thread
    private variable connection

    # Port on host on which account thread listens
    private variable port

    # Socket for receiving ping from localhost OOMMF account server
    private variable listensocket

    # Send id
    private variable id = 0

    # Reply message
    private variable reply = {}

    # Is object ready to relay messages to and from the host?
    private variable ready = 0

    private variable hostDeathHandler

    private variable protocolversion

    private variable ignore_die = 0 ;# Ignore die requests from
                                    ## account server?
    public variable gui = 0 {
	package require Tk
	wm geometry . {} ;# Make sure we have auto-sizing root window
        if {$gui} {
            if {[info exists myWidget]} {
                return
            }
            Net_AccountGui New myWidget $parentWin -account $this
            pack [$myWidget Cget -winpath] -side left -fill both -expand 1
        } else {
            if {![info exists myWidget]} {
                return
            }
            $myWidget Delete
            unset myWidget
	    $parentWin configure -width 1 -height 1  ;# Auto-resize
	    $parentWin configure -width 0 -height 0
        }
        set btnvar $gui
    }
    public variable parentWin = .
    private variable myWidget
    private variable btnvar

    method ToggleGui { pwin } {
       set parentWin $pwin
       $this Configure -gui $btnvar
       Ow_PropagateGeometry $pwin
    }

    # Constructor is a non-blocking operation.  After it returns an instance
    # id, the Ready method of that instance will return a boolean indicating
    # whether the object is ready.  If not, then when it becomes ready, it
    # will generate an event with id 'Ready'.  If the object cannot become
    # ready, it will delete itself, generating an event with its 'delete'.
    # Users of instances of this class should set up to handle those two
    # events, when the Ready method returns false.
    #
    Constructor { args } {
        eval $this Configure $args
        if {![info exists accountname]} {
            set accountname [$class DefaultAccountName]
        }
        # Enforce only one object per account
        if {![catch {set index($hostname,$accountname)} existingaccount]} {
            unset hostname
            unset accountname
            $this Delete
            array set argarray $args
            catch {unset argarray(-hostname)}
            catch {unset argarray(-accountname)}
            eval $existingaccount Configure [array get argarray]
            return $existingaccount
        }
        array set index [list $hostname,$accountname $this]
        Oc_EventHandler New shutdownHandler Oc_Main Shutdown \
            [list $this PrepareShutdown] -oneshot 1 -groups [list $this]

 	$this Restart
    }

    method StartTimeout {} {
        $this FatalError "Timed out waiting for OOMMF account server\
		connection\nfor $accountname on localhost"
    }

    method PrepareShutdown {} {
       if {[info exists do_restarts]} {
          set do_restarts 0
          Oc_EventHandler DeleteGroup $this-Restart
       }
    }

    method Restart {{msg {}}} {
       if {![string match {} $msg]} {
          Oc_Log Log "$this Restart: $msg" status $class
       }
       Oc_EventHandler DeleteGroup $this-Restart
       if {![info exists do_restarts] || !$do_restarts} { return }

       if {![info exists timeoutevent]} {
          set timeoutevent [after $startWait $this StartTimeout]
          # This time is started when we begin to try to connect to
          # the account server.  It is only retired by a successful
          # GetAccountPort call.
       } else {
          # Pause some random time before restart retry
          after [expr {int(100*rand())}]
       }

       if {[info exists listensocket]} {
          close $listensocket
          unset listensocket
       }
       if {[info exists connection]} {
          catch {$connection Delete}
          unset connection
       }

       set ready 0
       if {[catch {Net_Host New host -hostname $hostname} msg]} {
          catch {unset host}
          after idle [list $this Restart "Can't construct host object.\n$msg"]
       } else {
          Oc_EventHandler New hostDeathHandler $host Delete \
              [list after idle \
                   [list $this Restart "Lost connection to host $hostname"]] \
              -oneshot 1 -groups [list $this $this-Restart]
          $this HandlePing
       }
    }

    private method HandlePing {args} {
        if {[llength $args]} {
            if {[info exists listensocket]} {
               catch {close $listensocket}
               unset listensocket
            }
            Oc_Log Log "$this: Received ping from account server!" \
                status $class
            close [lindex $args 0]
        }
        if {[$host Ready]} {
	    $this LookupAccountPort
        } else {
	    Oc_EventHandler New _ $host Ready [list $this LookupAccountPort] \
		    -oneshot 1 -groups [list $this $this-Restart]
        }
    }

    method LookupAccountPort {args} {
        set qid [$host Send lookup $accountname]
        Oc_EventHandler New _ $host Reply$qid \
                [list $this GetAccountPort $qid] \
                -groups [list $this $this-Restart $this-$qid]
        Oc_EventHandler New _ $host Timeout$qid \
                [list $this NoAnswerFromHost $qid] \
                -groups [list $this $this-Restart $this-$qid]
    }

    method GetAccountPort { qid } {
       Oc_EventHandler DeleteGroup $this-$qid
       set reply [$host Get]
       if {[llength $reply] < 2} {
          set msg "Bad reply from host $hostname: $reply"
          if {[info exists hostDeathHandler]} {
             $hostDeathHandler Delete  ; unset hostDeathHandler
          }
          catch {$host Delete}
          after idle [list $this Restart $msg]
          return
       }
       switch -- [lindex $reply 0] {
          0 {
             set port [lindex $reply 1]
             $this Connect
          }
          default {
             # Host returns an error - assume that means lookup failed
             if {[string match localhost [$host Cget -hostname]]} {
                # Try starting or restarting account server.  One case
                # where multiple retries happen is this race condition:
                #    1) Two processes, A & B, find no account server
                #       registered with host server, so both start one.
                #    2) Account server A wins the race.  Both A & B
                #       ping their creators and then server B exits.
                #    3) Process and account server A exit.
                #    4) Process B queries host server for account server
                #       port.  If 4 happens before 3 then the port for account
                #       A will be returned.  But if 4 happens after 3 then
                #       the host server reports (again) that there is no
                #       account registered.
                # Presumably we want process B to try launching another
                # account server, i.e., call $this StartLocalAccount as
                # below.  But how many times to retry?  Rather than
                # count, we rely on the startWait timeout to pull the
                # plug.  Note that the startWait timer is started on the
                # first pass, and not reset until a successful account
                # connection is made.
                $this StartLocalAccount
             } else {
                $this FatalError "No account $accountname registered\
			    with host [$host Cget -hostname]"
             }
          }
       }
    }

    method NoAnswerFromHost { qid } {
       Oc_Log Log "$this: NoAnswerFromHost $qid" status $class
       Oc_EventHandler DeleteGroup $this-$qid
       # Host not responding.
       set msg "No reply from host"
       catch {append msg " [$host Cget -hostname]"}
       if {[info exists hostDeathHandler]} {
          $hostDeathHandler Delete  ; unset hostDeathHandler
       }
       catch {$host Delete}
       unset host
       $this Restart $msg
    }

    # Launch an OOMMF account server on localhost
    private method StartLocalAccount {} {
        set acctthread [file join $dir threads account.tcl]
        if {[file readable $acctthread]} {
            # Set up to receive ping from account thread or timeout
            if {[info exists listensocket]} {
               catch {close $listensocket}
            }
            set listensocket [socket -server [list $this HandlePing] \
		    -myaddr localhost 0]  ;# Force localhost for now
            set listenport [lindex [fconfigure $listensocket -sockname] 2]
            Oc_Log Log "Starting OOMMF account server for $accountname\
		    on localhost" status $class
            # Ought to redirect std channels somewhere
	    if {[info exists lastoid]} {
               Oc_Application Exec {omfsh 2} \
                    $acctthread -tk 0 0 $listenport $lastoid \
                    > [[Oc_Config RunPlatform] GetValue path_device_null]  \
                   2> [[Oc_Config RunPlatform] GetValue path_device_null] &
	    } else {
               Oc_Application Exec {omfsh 2} \
                    $acctthread -tk 0 0 $listenport \
                    > [[Oc_Config RunPlatform] GetValue path_device_null]  \
                   2> [[Oc_Config RunPlatform] GetValue path_device_null] &
	    }
        } else {
            set msg "Install error: No OOMMF account server available to start"
            error $msg $msg
        }
    }

    method Connect {} {
       if {[catch {
          Net_Connection New connection -hostname $hostname -port $port
       } msg]} {
          catch {unset connection}
          $this DeregisterBadAccount "Can't create connection:\n$msg"
       } else {
          # Using [socket -async], it's possible that we haven't really
          # yet established a valid connection.  So, until the connection
          # is "Ready", handle all Net_Connection destruction as though
          # caused by failure to connect.
          set h1 [Oc_EventHandler New _ $connection Delete \
                      [list $this ConnectDelete \
                           "Connection to $hostname:$port failed"] \
                      -oneshot 1 \
                      -groups [list $this $this-Restart $this-ConnectDelete]]

          set h2 [Oc_EventHandler New _ $connection SafeClose \
                      [list $this ConnectDelete \
                           "Connection to $hostname:$port failed"] \
                      -oneshot 1 \
                      -groups [list $this $this-Restart $this-ConnectDelete]]

          Oc_EventHandler New _ $connection Ready \
              [list $this ConnectionReady $h1 $h2] -oneshot 1 \
              -groups [list $this $this-Restart]
       }
    }

    method ConnectionReady {handler1 handler2} {
       $handler1 Delete
       $handler2 Delete
       Oc_EventHandler New _ $connection Delete [list $this Restart] \
           -oneshot 1 -groups [list $this]
       Oc_EventHandler New _ $connection SafeClose [list $this Restart] \
           -oneshot 1 -groups [list $this]
       Oc_EventHandler New _ $connection Readable \
           [list $this VerifyOOMMFAccount] -oneshot 1 \
           -groups [list $this $this-Restart]
    }

    method VerifyOOMMFAccount {} {
       if {![regexp {OOMMF account protocol ([0-9.]+)} [$connection Get] \
                 match version]} {
          # Not an OOMMF account
          $this FatalError \
              "No OOMMF account server answering on $hostname:$port"
       }
       set protocolversion $version
       # Update when account server protocol version changes!
       if {![package vsatisfies $protocolversion 1]} {
          # Old version of account server running!
          $this FatalError \
              "Incompatible OOMMF account server is running\
		    at $hostname:$port."
       }
       Oc_Log Log "OOMMF account $hostname:$accountname ready" status $class
       Oc_EventHandler New _ $connection Readable [list $this Hear] \
           -groups [list $this]
       set ready 1

       # Looks like we have a valid connection.  Wrap up the server
       # start-up sequence.
       if {[info exists timeoutevent]} {
          after cancel $timeoutevent
          unset timeoutevent
       }
       # Once we've connected to the account server, we don't need the
       # host server anymore.  Let it die and release its sockets.
       $hostDeathHandler Delete  ; unset hostDeathHandler
       $host Delete              ; unset host
       Oc_EventHandler DeleteGroup $this-Restart


       # Now get an OOMMF Id from the account server to represent our
       # application
       if {[info exists oid]} {
          set qid [$connection Query \
                       newoid [Oc_Main GetAppName] \
                       [pid] [Oc_Main GetStart] $oid]
       } else {
          set qid [$connection Query \
                       newoid [Oc_Main GetAppName] [pid] [Oc_Main GetStart]]
       }
       if {[info exists connection]} {
          # In principle, the preceding $connection Query may result
          # in a $this Delete call.
          Oc_EventHandler New _ $connection Reply$qid \
              [list $this Getoid $qid] \
              -oneshot 1 -groups [list $this $this-$qid]
          Oc_EventHandler New _ $connection Timeout$qid \
              [list $this Nooid $qid] \
              -oneshot 1 -groups [list $this $this-$qid]
          # Advise any interested parties that a (new) account server
          # is being used.
          Oc_Log Log "$this: New account server" status $class
          Oc_EventHandler Generate $this NewAccountServer

          # Process any messages in send_stack
          set stack_copy $send_stack
          set send_stack {}
          foreach pair $stack_copy {
             foreach {id args} $pair break
             $this Resend $id $args
          }
       }
    }

    method OID {} {
	return $oid
    }

    method AccountConnection {} {
       # Used for clean shutdowns
       if {![info exists connection]} {
          return {}
       }
       return $connection
    }

    # The Names method returns a two item list.  If the first item is 0,
    # then the second item is the list of nicknames for the OID.  Note
    # that by default each application is assigned a nickname of the
    # form appname:OID.
    #   If the first item is non-zero, then the second item is an error
    # message.
    # WARNING: The Names method blocks until account server responds,
    #          or specified timeout (in milliseconds) occurs.
    private variable NamesCallbackResult
    private method NamesCallback { qid } {
       Oc_EventHandler DeleteGroup $this-Names-$qid
       set answer [$this Get]
       if {[llength $answer] != 3 || [lindex $answer 0] != 0} {
          set NamesCallbackResult [list 1 "Unrecognized error"]
       } else {
          set NamesCallbackResult [list 0 [lindex $answer 2]]
       }
    }
    private method NamesCallbackTimeout { qid } {
       Oc_EventHandler DeleteGroup $this-Names-$qid
       set NamesCallbackResult [list 1 timeout]
    }
    method Names { {checkoid {}} {timeoutms 5000} } {
       if {[string match {} $checkoid]} {
          set checkoid $oid
       }
       set qid [$this Send names $checkoid]
       set NamesCallbackResult {}
       Oc_EventHandler New _ $this Reply$qid \
          [list $this NamesCallback $qid] \
          -oneshot 1 -groups [list $this $this-Names-$qid]
       Oc_EventHandler New _ $this Timeout$qid \
          [list $this NamesCallbackTimeout $qid] \
          -oneshot 1 -groups [list $this $this-Names-$qid]

       set atid [after $timeoutms \
          [list set [$this GlobalName NamesCallbackResult] {1 timeout}]]
       vwait [$this GlobalName NamesCallbackResult]
       after cancel $atid

       return $NamesCallbackResult
    }

    method Getoid {qid} {
       Oc_EventHandler DeleteGroup $this-$qid
       set result [$connection Get]
       if {[lindex $result 0]} {
          $this FatalError "Did not receive OID: [lindex $result 1]"
          return
       }
       set oid [lindex $result 1]
       if {![info exists lastoid]} {
          set lastoid $oid
          incr lastoid
       }
       Oc_Main SetOid $oid

       # Update root window title
       if {[llength [info commands Ow_SetWindowTitle]]} {
          set wintitle [Oc_Main GetTitle]
          set winwidth [Ow_SetWindowTitle . $wintitle]
          wm iconname . $wintitle
          Oc_EventHandler Generate $this NewTitle \
             -title $wintitle -winwidth $winwidth
          # Code can use the NewTitle event to resize the root window
          # width to accomodate the new title. Here is some example
          # client code, assuming variables brace (pointing to a frame
          # or canvas widget) and menu_width are defined in the
          # enclosing scope:
          #
          #  Oc_EventHandler New _ Net_Account NewTitle [subst -nocommands {
          #    $brace configure -width \
          #      [expr {%winwidth<$menuwidth ? $menuwidth : %winwidth}]
          #  }]
       }

       # Register nicknames with server
       set worknames {}
       if {[llength $nicknames]>0} {
          set appname [string tolower [Oc_Main GetAppName]]
          foreach elt $nicknames {
             lappend worknames "${appname}:[string tolower $elt]"
          }
       }
       $this DefaultNicknames start -1 $worknames
    }

    # Register any nicknames pre-stored in the class common variable
    # "nicknames" with account server.
    private method DefaultNicknames { mode qid names } {
       # Process preceding result --- if any.  On first pass mode is set
       # to "start", indicating no preceding result.
       if {[string compare start $mode]!=0} {
          Oc_EventHandler DeleteGroup $this-DefaultNickname-$qid
          if {[string compare reply $mode]==0} {
             set answer [$this Get]
             if {[lindex $answer 0]!=0} {
                # Error
                set msg "Registration for nickname \"[lindex $names 0]\"\
                      to oid $oid failed: [lindex $answer 1]"
                Oc_Log Log "$this: $msg" info $class
             }
          } elseif {[string compare timeout $mode]==0} {
             # Timeout
             set msg "Registration for nickname \"[lindex $names 0]\"\
                      to oid $oid failed: timeout"
             Oc_Log Log "$this: $msg" info $class
          }
          # For any case other than first pass, remove top name from list
          set names [lrange $names 1 end]
       }

       # Register nickname at top of list
       if {[llength $names]>0} {
          set qid [$this Send associate [lindex $names 0] $oid]
          Oc_EventHandler New _ $this Reply$qid \
             [list $this DefaultNicknames reply $qid $names] \
             -oneshot 1 -groups [list $this $this-DefaultNickname-$qid]
          Oc_EventHandler New _ $this Timeout$qid \
             [list $this DefaultNicknames timeout $qid $names] \
             -oneshot 1 -groups [list $this $this-DefaultNickname-$qid]
       } else {
          # All done.  Net_Account is ready for general use.
          Oc_EventHandler Generate $this Ready
       }
    }

    method Nooid {qid} {
        Oc_EventHandler DeleteGroup $this-$qid
	$this FatalError "No response to OID request"
    }

    method ConnectDelete {msg} {
       Oc_EventHandler DeleteGroup $this-ConnectDelete
       unset connection
       $this DeregisterBadAccount $msg
    }

    method DeregisterBadAccount {msg} {
        Oc_Log Log "Sending deregistration request for non-answering account" \
            status $class
        set qid [$host Send deregister $accountname $port]
        Oc_EventHandler New _ $host Reply$qid \
                [list $this DeregisterBadAccountReply $msg $id] \
                -groups [list $this $this-Restart $this-$qid]
        Oc_EventHandler New _ $host Timeout$qid \
                [list $this NoAnswerFromHost $qid] \
                -groups [list $this $this-Restart $this-$qid]
    }

    method DeregisterBadAccountReply {msg qid} {
       Oc_EventHandler DeleteGroup $this-$qid
       set reply [$host Get]
       switch -- [lindex $reply 0] {
          0 {
             if {[string match localhost [$host Cget -hostname]]} {
                $this StartLocalAccount
             } else {
                $this FatalError $msg
             }
          }
          default {
             set msg "Unable to correct bad account server\
			registration after:\n$msg"
             if {[info exists hostDeathHandler]} {
                $hostDeathHandler Delete  ; unset hostDeathHandler
             }
             catch {$host Delete}
             after idle [list $this Restart $msg]
          }
       }
    }

    method IgnoreDie {} {
       # Ignore "die" requests from account server
       set ignore_die 1
    }
    method EnableDie {} {
       # Enable "die" requests from account server
       set ignore_die 0
    }

    # Handle 'tell' messages from connection
    method Hear {} {
       set reply [$connection Get]
       if {[string compare [lindex $reply 0] private]} {
          Oc_EventHandler Generate $this Readable
          return
       }
       switch -exact [lindex $reply 1] {
          lastoid { set lastoid [lindex $reply 2] }
          nickname {
             set tempname [string tolower [lindex $reply 2]]
             set appname [string tolower [Oc_Main GetAppName]]
             if {[regexp {^([^:]*):(.*)$} $tempname dummy prefix suffix] \
                    && [string compare $appname $prefix]==0} {
                # Canonical form for nicknames is appname:shortname.
                # Only the shortname part is stored in the nicknames
                # variable.  Note that this will misbehave if we ever
                # get a nickname not of this form, because if the name
                # ever needs to be re-registered (for example, if the
                # account server goes down and a new one is started)
                # the new registration will get appname prepended.
                # For now, assume all nicknames have the canonical
                # form.
                set tempname $suffix
             }
             if {[lsearch -exact $nicknames $tempname] == -1} {
                lappend nicknames $tempname
             }
          }
          die {
             if {!$ignore_die} {
                after 0 exit
             }
          }
       }
    }

    method Receive { rid } {
        Oc_EventHandler DeleteGroup $this-$rid
        set reply [$connection Get]
        Oc_EventHandler Generate $this Reply$rid
    }

    method Get {} {
        return $reply
    }

    # Send a command to the account server
    method Send { args } {
        incr id
        if {!$ready} {
           lappend send_stack [list $id $args]
        } else {
           $this Resend $id $args
        }
        return $id
    }

    private method Resend { x argList } {
       if {[catch {eval $connection Query $argList} qid]} {
          lappend send_stack [list $x $argList]
       } else {
          Oc_EventHandler DeleteGroup $this-$x
          if {[info exists connection]} {
             # The preceding $connection Query command may result in
             # $this being destroyed.
             Oc_EventHandler New _ $connection Reply$qid \
                 [list $this Receive $x] \
                 -oneshot 1 -groups [list $this $this-$x]
             Oc_EventHandler New _ $connection Timeout$qid \
                 [list $this Timeout $x] \
                 -oneshot 1 -groups [list $this $this-$x]
             Oc_EventHandler New _ $this Ready [list $this Resend $x $argList] \
                 -oneshot 1 -groups [list $this $this-$x]
          }
       }
    }

    # This routine silently cleans up after a query to the account server
    # times out with no response.
    method Timeout { rid } {
        Oc_EventHandler DeleteGroup $this-$rid
        Oc_EventHandler Generate $this Timeout$rid
    }

    method Ready {} {
        return $ready
    }

    method FatalError { msg } {
       global errorInfo
       set errorInfo "No stack available"
       Oc_Log Log "$this: $msg" warning $class
       error $msg
    }

    Destructor {
        set ready 0
        if {[info exists timeoutevent]} {
           after cancel $timeoutevent
           unset timeoutevent
        }
        if {[info exists hostDeathHandler]} {
           if {[llength [info commands $hostDeathHandler]]} {
              # hostDeathHandler is -oneshot; if we get here due to it
              # being triggered then the command will already be deleted.
              $hostDeathHandler Delete
           }
           unset hostDeathHandler
        }
        if {[info exists shutdownHandler]} {
           if {[llength [info commands $shutdownHandler]]} {
              $shutdownHandler Delete
           }
           unset shutdownHandler
        }

        Oc_EventHandler Generate $this Delete
        Oc_EventHandler DeleteGroup $this

	if {[info exists oid]} {
           if {[llength [info commands wm]]} {
              wm title . [Oc_Main GetInstanceName]
              wm iconname . [wm title .]
           }
           set oid_copy $oid
           unset oid
           if {[info exists connection]} {
              $connection Query freeoid $oid_copy
           }
        }
        if {[info exists connection]} {
           $connection Query bye
           $connection SafeClose
        }
        if {[info exists listensocket]} {
           catch {close $listensocket}
           unset listensocket
        }
        if {[info exists myWidget]} {
            $this Configure -gui 0
        }
        if {[info exists hostname] && [info exists accountname]} {
            unset index($hostname,$accountname)
        }
    }
}
