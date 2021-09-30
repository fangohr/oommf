# FILE: protocol.tcl
#
# Each instance of the Oc_Class Net_Protocol describes a protocol to be
# used for replying to queries received over a Net_Connection.
#
# Last modified on: $Date: 2015/10/16 08:28:14 $
# Last modified by: $Author: donahue $

Oc_Class Net_Protocol {

    const public variable name

    const public variable connection = {}

    private common serial 0

    private array variable slaves

    private array common references

    private variable state = start

    private variable deleted = 0

    private variable init = {}

    Constructor { args } {
        eval $this Configure $args
    }

    private proc Reference {cmd} {
        if {![string match $class-msg* $cmd]} {return}
        if {[catch {incr references($cmd)}]} {
            set references($cmd) 1
        }
    }

    private proc Dereference {cmd} {
        if {![string match $class-msg* $cmd]} {return}
        incr references($cmd) -1
        if {$references($cmd) == 0} {
            rename $cmd {}
        }
    }

    method Clone { newconn } {
        if {$deleted} {
            return -code error "protocol has been deleted"
        }
        $class New clone -name $name -connection $newconn
        # Is this really necessary?  Probably good for now.
        # This forces all clones to die when the original Protocol
        # is Deleted.  Clones can die with no effect on others.
        Oc_EventHandler New _ $this Delete [list $clone Delete] \
            -groups [list $clone $this]
        $clone SetInit $init
        foreach s [array names slaves] {
            set mine $slaves($s)
            set new [$clone CreateSlave $s]
            foreach cmd [$mine aliases] {
                set a [$mine alias $cmd]
                $new alias $cmd $a
                $class Reference $a
            }
        }
        return $clone
    }

    private method SetInit {cmd} {
        set init $cmd
        if {[string length $init]} {
            $class Reference $init
            $init $connection
        }
    }

    private method CreateSlave {s} {
        if {[info exists slaves($s)]} {
            return $slaves($s)
        }
        # Create slave interp with no commands
        set i [interp create -safe]
        foreach c [$i eval info commands] {
            if {[string compare rename $c]!=0} {
                $i eval [list rename $c {}]
            }
        }
        $i eval [list rename rename {}]
        set slaves($s) $i
        if {!$deleted} {
            $this AddMessage $s messages {} \
                    "return \[[list list $s] \[list 0 \[[list $i aliases]]]]"
            $this AddMessage $s serverpid {} \
                    "return \[[list list $s] \[list 0 \[pid]]]"
            $this AddMessage $s serveroid {} \
                    "return \[[list list $s] \[list 0 \[Oc_Main GetOid]]]"
            $this AddMessage $s datarole {} \
                    "return \[[list list $s] \[list 0 \[Oc_Main GetDataRole]]]"
            $this AddMessage $s bye {} {list close [list 0 Bye!]}
            $this AddMessage $s exit {} {
                set id [after 0 exit]
                Oc_EventHandler New _ Oc_Main Shutdown [list after cancel $id]
                list close [list 0 $id]
            }
            ## NB: Changes to "bye" and "exit" handling, either here
            ##     or in child classes, should be reflected in the
            ##     Net_Thread client-side Send method.
            Oc_EventHandler New _ Oc_Main Shutdown [list $this Shutdown $s] \
                -groups [list $this]
        }
        return $i
    }

    private method Shutdown {s} {
        if {!$deleted} {
            $this AddMessage $s exit {} {return {close {0 {}}}}
        }
    }

    method Init {body} {
        if {$deleted} {
            return -code error "protocol has been deleted"
        }
        if {[string length $init]} {
            $class Dereference $init
        }
        set init $class-msg[incr serial]
        proc $init connection $body
        $class Reference $init
    }

    method AddMessage {s cmd arglist body} {
        if {$deleted} {
            return -code error "protocol has been deleted"
        }
        set i [$this CreateSlave $s]
        set a $class-msg[incr serial]
        proc $a [concat connection $arglist] $body
        if {[llength [$i alias $cmd]]} {
            $this DeleteMessage $s $cmd
        }
        $i alias $cmd $a
        $class Reference $a
    }

    method DeleteMessage {s cmd} {
        set i [$this CreateSlave $s]
        $class Dereference [lindex [$i alias $cmd] 0]
        $i alias $cmd {}
    }

    method Reply {cmd} {
        if {$deleted} {
            # Kludge to ignore "bye" messages that follow deletion.
            # We see this when a single connection sends more than
            # one "bye" message.  Can happen when "Exit all OOMMF"
            # signals all apps to send "bye" while in addition, apps
            # like oxsii and boxsi send "bye" when they get notified
            # about a server deregistration by the account server.
            if {[string match bye $cmd]} {return}
            return -code error "protocol has been deleted"
        }
        if {[catch {
                $slaves($state) eval [lindex $cmd 0] $connection \
                        [lrange $cmd 1 end]
                } msg]} {
            # Report the error locally -- may want to remove this if/when the
            # caller handles errors properly.  Somehow need to ID the app.
            # This is wrapped up in a catch in case Oc_Log is trying to
            # write to a tty that no longer exists.
            catch {Oc_Log Log "Error replying to socket\
                     message\n\t'$cmd':\n\n\t$msg" warning $class}
            return [list 1 $msg]
        } else {
            # The message must be a two element Tcl list to make
            # the caller happy.  Check that the protocol follows the
            # rules
            if {[catch {set l [llength $msg]}] || ($l != 2)} {
                set emsg "Programming error: Bad return\
                        value:\n\t$msg\nfrom $this message '[lindex \
                        $cmd 0]'.\nShould be a two element list."
                error $emsg $emsg
            }
            # Must check existence in case eval above called our destructor
            if {[info exists state]} {
                set state [lindex $msg 0]
                if {[string match close $state]} {
                   # This branch occurs when the client side of the
                   # connection issues a "bye" or "exit" request. In the
                   # case of "bye", put a call to $this Delete onto the
                   # timer event stack. Otherwise, the message handling
                   # should invoke a call Oc_Main Exit, which generates an
                   # Oc_Main LibShutdown event, which triggers
                   # Net_StartCleanShutdown, which shuts down all
                   # servers. The server that owns this protocol will call
                   # $this Delete to destroy this protocol. If for some
                   # reason this command chain doesn't work, try removing
                   # the cmd check about the following command.
                   Oc_EventHandler Generate $this ClientCloseRequest
                   ## Net_Connection has a handler that sets its
                   ## close_sent variable.
                   if {![string match exit $cmd]} {
                      Oc_EventHandler New _ $this Delete \
                         [list after cancel [after 0 [list $this Delete]]] \
                         -groups [list $this]
                   }
                   set deleted 1
                   catch {Oc_Log Log "Closing $connection on remote command"\
                             status $class}
                }
            }
            return [lindex $msg 1]
        }
    }

    method Deleted {} {
       # Returns true if protocol is scheduled for deletion. This
       # happens upon receipt of a 'bye' or 'exit' query.
       return [expr {![info exists deleted] || $deleted}]
    }

    Destructor {
        Oc_EventHandler Generate $this Delete
        Oc_EventHandler DeleteGroup $this
        if {[string length $connection]} {
            if {[$connection OwnsProtocol $this]} {
               # Note: The Net_Connection instance destructor calls the
               # Net_Protocol instance destructor, but not before
               # resetting its internal members so that $connection
               # OwnsProtocol returns false. So this code encapsulates a
               # bit of trickery: If this Destructor is called from a
               # clone, then this branch is false and the tail code runs,
               # destroying the instance. If Destructor is called from
               # the owner, then this branch is true on the first pass,
               # and $connection Delete is called, but it then calls this
               # destructor again and on the second pass this branch is
               # false, so then the tail code runs.
               $connection CloseServer
               return
            }
        }
        if {[string length $init]} {
            $class Dereference $init
            set init {}
        }
        foreach s [array names slaves] {
            set i $slaves($s)
            foreach cmd [$i aliases] {
                $this DeleteMessage $s $cmd
            }
            interp delete $i
            unset slaves($s)
        }
    }

}
