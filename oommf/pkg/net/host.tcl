# FILE: host.tcl
#
# An object which plays the role of a host within an interpreter.
#
# The Tcl event loop must be running for this event-driven object to function.
#
# Last modified on: $Date: 2015/09/30 07:41:34 $
# Last modified by: $Author: donahue $
#

Oc_Class Net_Host {

    # Index of host objects by name -- used to avoid duplicate objects.
    array common index {}
    common dir

    ClassConstructor {
        set dir [file dirname [info script]]
    }

    # The port on any host to which this object sends its commands
    public common port 15136

    # Milliseconds to wait for host thread to start on localhost
    public variable startWait = 60000
    # The ID of the current timeout (returned from 'after' command)
    private variable timeoutevent

    # The name of the host this object acts as a proxy for
    const public variable hostname = localhost
    
    # The connection through which we communicate to the host.
    private variable connection

    # Socket for receiving ping from localhost OOMMF host server
    private variable listensocket

    # Send id
    private variable id = 0

    # Reply message
    private variable reply

    # Is object ready to relay messages to and from the host?
    private variable ready = 0

    private variable protocolversion

    # Do we display a gui?
    public variable gui = 0 {
	package require Tk
	wm geometry . {} ;# Make sure we have auto-sizing root window
        if {$gui} {
            if {[info exists myWidget]} {
                return
            }
            Net_HostGui New myWidget $parentWin -host $this
            pack [$myWidget Cget -winpath] -side top -fill both -expand 1
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

    private variable refCount = 0

    # Constructor is a non-blocking operation.  After it returns an instance
    # id, the Ready method of that instance will return a boolean indicating
    # whether the object is ready.  If not, then when it becomes ready, it
    # will generate an event with id 'Ready'.  If the object cannot become
    # ready, it will delete itself, generating an event with it's 'delete'.
    # Users of instances of this class should set up to handle those two
    # events, when the Ready method returns false.
    #
    Constructor { args } {
        eval $this Configure $args
        # Enforce only one object per host
        if {![catch {set index($hostname)} existinghost] \
                && [llength [info commands $existinghost]]} {
            unset hostname
            $this BoilerDelete
            array set argarray $args
            catch {unset argarray(-hostname)}
            eval $existinghost Configure [array get argarry]
            $existinghost BumpRefCount
            return $existinghost
        }
        array set index [list $hostname $this]
        after 0 $this Connect
        $this BumpRefCount
    }

    private method BumpRefCount {} {
        incr refCount
    }

    public method HostConnection {} {
       # This method used by account server to check when it has
       # no non-host connections.
       if {![info exists connection]} {
          return {}
       }
       return $connection
    }

    method Connect {args} {
       # The first pass (which has $args == {}) tries to connect to a
       # pre-existing host server.  If that fails then the
       # StartLocalHost method is called to start a new server.  The
       # child process tries to open a server on the host port and
       # pings back to here regardless of whether or not it succeeded
       # in grabbing the host port.  The second pass through this
       # method ($args != {}) should succeed by connecting to either
       # the host server launch by $this or else to a host server
       # launched by some other process.  Aside from abnormal
       # conditions such as a foreign process sitting on the host
       # server port, the only anticipated failure mode is a race
       # condition where a new host server is bought up after the
       # first call to Connect, which then dies after the child host
       # server tries to grab the server port but before the second
       # Connect pass tries to connect.
        if {[llength $args]} {
            Oc_Log Log "Received ping!" status $class
        }
        # Open connection to host server.  Turn off server identity checks
        # to allow multiple users to use one host server.
        if {[catch {
                Net_Connection New connection \
                   -hostname $hostname -port $port -user_id_check 0
                } msg]} {
            $this ConnectionFail $msg
        } else {
            Oc_Log Log "Net_Host Connect set connection to $connection"\
		    status $class
            Oc_EventHandler New _ $connection Delete \
                    [list $this ConnectionFail] -groups [list $this]
            Oc_EventHandler New _ $connection Ready \
                    [list $this ConnectionReady] -oneshot 1 \
                    -groups [list $this]
        }
        # NOTE: It is important that the ping channel be closed *after*
        # the new Net_Connection instance is created.  Otherwise Tcl may
        # try to re-use the same socket name, and if there are any bugs
        # in the Tcl library code which cleans up closed sockets (as it
        # appears there are, at least on some platforms), then there is the
        # danger of "cross-talk" between the closed ping socket and the new
        # connection socket.  In particular, we have observed spurious EOF
        # conditions appearing on the new Net_Connection's socket when the
        # order of these operations was reversed in an earlier version of this
        # software.  Better to avoid trouble by forcing Tcl to give us a 
        # socket with a new name.
        if {[llength $args]} {
            catch {Oc_Log Log "Closing ping channel!" status $class}
            close [lindex $args 0]
        }
    }

    method ConnectionFail {{why ""}} {
        #if {$ready} {
        #    $this FatalError "Connection to $hostname died.\n$why"
        #    return
        #}
	set ready 0
        if {[string match localhost $hostname] && ![info exists listensocket]} {
            if {[info exists connection]} {
                unset connection
            }
            $this StartLocalHost
        } else {
            $this FatalError "Can't open connection to $hostname.\n$why"
        }
    }

    method ConnectionReady {} {
        Oc_EventHandler New _ $connection Readable \
                [list $this VerifyOOMMFHost] -oneshot 1 -groups [list $this]
    }

    method VerifyOOMMFHost {} {
        if {[info exists listensocket]} {
            close $listensocket
            unset listensocket
        }
        if {[info exists timeoutevent]} {
            after cancel $timeoutevent
            unset timeoutevent
        }
        if {![regexp {OOMMF host protocol ([0-9.]+)} [$connection Get] \
                match version]} {
            # Not an OOMMF host
            $this FatalError "No OOMMF host server answering on $hostname"
            return
        }
        set protocolversion $version
        # Update when account server protocol version changes!
        if {![package vsatisfies $protocolversion 0]} {
            # Old version of host server running!
            $this FatalError "Incompatible OOMMF host server is running\
                    at $hostname:$port."
            return
        }
        Oc_Log Log "OOMMF host $hostname ready" status $class
        set ready 1
        Oc_EventHandler Generate $this Ready
    }

    method ProtocolVersion {} {
        if {!$ready} {
            error "host $hostname not ready"
        }
        return $protocolversion
    }

    method HostPort {} {
       return $port
    }

    # Launch an OOMMF host server on localhost
    private method StartLocalHost {} {
        set hostthread [file join $dir threads host.tcl]
        if {[file readable $hostthread]} {
           # Set up to receive ping from host thread or timeout
           # Note: listensocket and timeoutevent should not exist
           # coming into StartLocalHost, but handle just in case
           # program flow is changed in the future.
           if {[info exists listensocket]} {
              catch {close $listensocket}
           }
           set listensocket [socket -server [list $this Connect] \
		    -myaddr localhost 0]  ;# Force to localhost for now
           set listenport [lindex [fconfigure $listensocket -sockname] 2]
           if {![info exists timeoutevent]} {
              set timeoutevent [after $startWait $this StartTimeout]
           }
           Oc_Log Log "Starting OOMMF host server on localhost" status $class
           # Ought to redirect std channels somewhere
           Oc_Application Exec {omfsh 2} \
               $hostthread -tk 0 $port $listenport \
               > [[Oc_Config RunPlatform] GetValue path_device_null] \
              2> [[Oc_Config RunPlatform] GetValue path_device_null] &
        } else {
           error "Installation error: No OOMMF host server available to start"
        }
    }

    method StartTimeout {} {
        $this FatalError \
                "Timed out waiting for OOMMF host server to start on localhost"
    }

    method Receive { rid } {  
        Oc_EventHandler DeleteGroup $this-$rid
        set reply [$connection Get] 
        Oc_EventHandler Generate $this Reply$rid
    }

    method Timeout { rid } {
        Oc_EventHandler DeleteGroup $this-$rid
        Oc_Log Log "Timed out waiting for host $hostname reply to query $rid" \
		status $class
        Oc_EventHandler Generate $this Timeout$rid
    }

    method Get {} {
        return $reply
    }

    # Send a command to the host
    method Send { args } {
        if {!$ready} {
            error "host $hostname not ready"
        }
        incr id
	$this Resend $id $args
        return $id
    }

    private method Resend { x argList } {
       Oc_EventHandler DeleteGroup $this-$x
       set qid [eval $connection Query $argList]
       if {[info exists connection]} {
          # In principle, the preceding $connection Query may result
          # in a $this Delete call.
          Oc_EventHandler New _ $connection Reply$qid \
              [list $this Receive $x] \
              -groups [list $this-$x $this]
          Oc_EventHandler New _ $connection Timeout$qid \
              [list $this Timeout $x] \
              -groups [list $this-$x $this]
          Oc_EventHandler New _ $this Ready [list $this Resend $x $argList] \
              -groups [list $this-$x $this]
       }
    }

    method Ready {} {
        return $ready
    }

    method FatalError { msg } {
        $this BoilerDelete
        global errorInfo 
        set errorInfo "No stack available"
        Oc_Log Log $msg warning $class
    }

    rename method Delete BoilerDelete
    method Delete {} {
        incr refCount -1
        if {$refCount == 0} {
            $this BoilerDelete
        }
    }

    Destructor {
        set ready 0
        Oc_EventHandler Generate $this Delete
        Oc_EventHandler DeleteGroup $this
        if {[info exists connection]} {
            Oc_Log Log "Net_Host delete destroying connection $connection" \
		    status $class
           $connection Query bye
           $connection SafeClose
        }
        if {[info exists timeoutevent]} {
            after cancel $timeoutevent
        }
        if {[info exists listensocket]} {
           catch {close $listensocket}
        }
        if {[info exists myWidget]} {
            $this Configure -gui 0
        }
        if {[info exists hostname]} {
            unset index($hostname)
        }
    }

}
