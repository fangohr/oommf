# FILE: threadWish.tcl
#
# This class is used by Net_Thread clients, namely mmLaunch, that
# display a control GUI on behalf of other applications such as
# mmArchive, Oxsii, and Boxsi. These latter applications create a
# Net_Server using the Net_GeneralInterfaceProtocol, and when mmLaunch
# detects a server using this protocol it creates a threadWish
# instance that it ties to that server.
#
# NB: This class supersedes Net_ThreadGui. Note that unlike
#     Net_ThreadGui, which runs directly in the mmLaunch Tcl
#     interpreter, Net_ThreadWish runs its GUI in a slave interp.

package require Tk

Oc_Class Net_ThreadWish {
    const public variable thread

    private variable slave
    private variable breakcmds = {}
    private array variable shared = {}

    Constructor {args} {
        eval $this Configure $args

        # Create slave interpreter for the GUI
        set slave [interp create -- slave-$this]

        # set up for slave destruction
        # NOTE: THIS MUST COME BEFORE [package require Oc]
        # in the slave !!!!!
        interp alias $slave exit {} $this Exit
        interp alias $slave closeGui {} $this Close

        # Load Tk, so we can rename [tkwait]
        # Hide the main window until we can load the GUI
        $slave eval {
            package require Tk
            package require Oc
            Oc_CommandLine Parse [list -console]
            console hide
            wm withdraw .
        }

        # For each nested event loop, stack a command which will break it
        $slave eval {rename tkwait Tk_tkwait}
        interp alias $slave tkwait {} $this Tkwait
        $slave eval {rename vwait Tcl_vwait}
        interp alias $slave vwait {} $this Vwait

        # Commands for interaction with the "thread"
        # Keep this only for (unlikely) backward compatibility need
        if {[string match 0.0 [$thread ProtocolVersion]]} {
            interp alias $slave appexit {} $thread Send delete
        }

        interp alias $slave remote {} $this Remote
        interp alias $slave share {} $this Share
        interp alias $slave $class-Reply {} $this Reply

        interp alias $slave Net_GetNicknames {} $this GetNicknames

        # Ought to have thread send for this and cache it, so GUI can
        # start up quicker.
        # Send for GUI Script
        set id [$thread Send GetGui]
        Oc_EventHandler New _ $thread Reply$id \
                [list $this ReceiveGui 0 $id] -oneshot 1 \
                -groups [list $this $this-$id]
        Oc_EventHandler New _ $thread Timeout$id \
                [list $this ReceiveGui 1 $id] -oneshot 1 \
                -groups [list $this $this-$id]

        # Set up to get Tell messages from remote app
        Oc_EventHandler New _ $thread Readable [list $this Receive] \
                -groups [list $this]

        # Set slave to identify as the remote app
        regexp {[0-9]+} [$thread Cget -pid] oid
        $slave eval Oc_Main SetAppName [$thread Cget -alias]
        $slave eval [list wm title . "<$oid> [$thread Cget -alias]"]
        $slave eval Oc_Main SetOid $oid
        $slave eval {
            package require Ow
            wm iconname . [wm title .]
            console title "[wm title .] Console"
            proc VerifyExit {} {
                set response [Ow_Dialog 1 "Exit [Oc_Main GetInstanceName]?" \
                        warning "Are you *sure* you want to exit \
                        [Oc_Main GetInstanceName]?" {} 1 Yes No]
                if {$response == 0} {
                        after 10 exit
                }
            }
            wm protocol . WM_DELETE_WINDOW VerifyExit
            Ow_SetIcon .
        }
    }

    Destructor {
        Oc_EventHandler Generate $this Delete
        Oc_EventHandler DeleteGroup $this

        # Invoke all nested event loop breaking commands, so we
        # break out of all event loops (eventually...)
        foreach cmd $breakcmds {
            catch {$slave eval [list uplevel #0 $cmd]}
        }

        # Convert all commands into [return],
        # so nothing more happens in $slave
        if {[package vsatisfies [package provide Tcl] 7]} {
            set cmdList [$slave eval [list uplevel #0 {info commands}]]
            set saveExp {^(proc|return)$}
        } else {
            set cmdList [$this Commands]
            set saveExp {^(::proc|::return)$}
        }
        foreach cmd $cmdList {
            if {[regexp $saveExp $cmd]} {continue}
            $slave eval [list uplevel #0 \
                    [list proc $cmd args [list return -code return]]]
        }
        $slave eval [list uplevel #0 \
                [list proc proc args [list return -code return]]]
        after idle [list after 1 [list interp delete $slave]]
    }

    callback method ReceiveGui {code id} {
        Oc_EventHandler DeleteGroup $this-$id
        if {!$code} {
            set reply [$thread Get]
            set code [lindex $reply 0]
        } else {
            set reply [list 1 "Timed out waiting to receive GUI"]
        }
        if {!$code} { ;# Check for error
            set script {
                Oc_EventHandler New _ Oc_Main LibShutdown \
                        {Oc_Log Log {[exit] called from startup script} info} \
                        -groups [list startup] -oneshot 1
            }
            append script [lindex $reply 1]
            set code [catch {$slave eval [list uplevel #0 $script]} message]
            # Script evaluation might have destroyed us
            if {![info exists slave]} {return}
        } else {
            regexp {[0-9]+} [$thread Cget -pid] id
            set message "Error retrieving GUI from [$thread Cget \
                    -alias]<$id>:\n[lindex $reply 1]"
        }
        $slave eval {Oc_EventHandler DeleteGroup startup}
        if {$code} {
            $slave eval [list catch [list Oc_Log Log $message error]]
        }
        if {[info exists slave]} {
            $slave eval {wm deiconify .; raise .}
        }
    }

    private method Commands {{ns {}}} {
        set cmds [$slave eval [list info commands ${ns}::*]]
        foreach child [$slave eval [list namespace children $ns]] {
            set cmds [concat $cmds [$this Commands $child]]
        }
        return $cmds
    }

    method GetNicknames {} {
       set oid [$slave eval Oc_Main GetOid]
       return [Net_GetNicknames $oid]
    }

    callback method Exit {args} {
        $thread Send exit
        $this Close
    }

    callback method Close {} {
        # This will call our Destructor
        $thread Configure -gui 0
    }

    callback method Tkwait {sub name} {
        switch -glob -- $sub {
            va* {
                set cmd [list set $name 0]
            }
            vi* {
                set cmd [list event generate $name <Visibility>]
            }
            w* {
                set cmd [list destroy $name]
            }
        }
        set breakcmds [linsert $breakcmds 0 $cmd]
        set code [catch {$slave eval [list Tk_tkwait $sub $name]} msg]
        if {[info exists breakcmds]} {
            set breakcmds [lrange $breakcmds 1 end]
        }
        return -code $code $msg
    }

    callback method Vwait {name} {
        set breakcmds [linsert $breakcmds 0 [list set $name 0]]
        set code [catch {$slave eval [list Tcl_vwait $name]} msg]
        if {[info exists breakcmds]} {
            set breakcmds [lrange $breakcmds 1 end]
        }
        return -code $code $msg
    }

    callback method Remote {switch args} {
        if {[string compare -callback $switch]} {
            # Default callback is the Reply method below.
            # It logs errors here on the client side, but ignores
            # everything else.  Caller should register its own
            # callback to capture the result.
            set callback $class-Reply
            set message [concat $switch $args]
        } else {
            set callback [lindex $args 0]
            set message [lrange $args 1 end]
        }
        if {[lsearch -exact [$thread Messages] [lindex $message 0]] < 0} {
            return -code error \
                    "Message ``$message'' not supported by protocol\
                    [$thread Protocol] [$thread ProtocolVersion]"
        }
        set id [eval [list $thread Send] $message]
        Oc_EventHandler New _ $thread Reply$id \
                [list $this ReceiveRemoteReply $callback] -oneshot 1 \
                -groups [list $this]
        # Could add timeout handling here too; not for now
    }
    callback method ReceiveRemoteReply {callback} {
        set reply [$thread Get]
        if {[catch {$slave eval [list uplevel #0 $callback $reply]} result]} {
            Oc_Log Log $result error
        }
    }

    # The default handler for replies.
    callback method Reply {code result args} {
        # Default is to report response errors in the interface
        if {$code == 1} {
            # Set error environment for capture by Oc_Log
            $slave eval [list uplevel #0 [list set errorInfo [lindex $args 0]]]
            $slave eval [list uplevel #0 [list set errorCode [lindex $args 1]]]
            set cmd [lindex $args 2]

# NOTE: The Oc_Log calls can block, yet process events, leading to
# re-entrancy as more messages come in.  Check this for problems,
# either possible loss of error messages, or flooding of screen
# with pop-ups.  Perhaps the right place to address that is in
# the definition of default Oc_Log handlers.

            if {[string length $cmd]} {
                $slave eval [list Oc_Log Log "$result\n($cmd)" error]
            } else {
                $slave eval [Oc_Log Log $result error]
            }
        }
    }

    callback method Share {remote {local {}}} {
        # Default: use same local name as in the app
        if {[string match {} $local]} {
            set local $remote
        }
        set trace "[list remote set $remote] \[[list uplevel #0 set $local]] ;#"

        # Set up traces in server
        $thread Send Share $remote $local $trace

        # Set up trace here.
        # Hmmm, this seemed like the right thing to do, but
        # the second time the interface was opened, there was
        # some kind of infinite write trace loop.  So, don't
        # set up a local write trace.  Instead wait for a command
        # from the server to do it for you.  The down side is that
        # any writes to $local before that write trace instruction
        # arrives from the server will not be sent out.
#       $slave eval [list trace variable $local w $trace]

# NOT YET ACTIVE:
if 0 {
        if {[info exists shared($local)]} {
            # $local is already a shared variable
            if {[string compare $remote $shared($local)] == 0} {
                # $local is already shadowing $remote.  Nothing to do.
                return
            }
            # $local shadows a variable other than $remote.  Stop it.
            trace vdelete $local w 
        }
}
    }

    # This is in place as a low-level mechanism to support shared variables
    # Otherwise not used;  Should look for ways to secure this better.
    # Second use: Log messages.
    method Receive {} {
        set message [$thread Get]
        switch [lindex $message 0] {
            Result {
                if {[catch {
                        $slave eval [list uplevel #0 [lindex $message 1]]
                } result]} {
                    Oc_Log Log $result error
                }
            }
            Log {
                foreach {_ m t s ei ec} $message break
                $slave eval [list uplevel #0 [list set errorInfo $ei]]
                $slave eval [list uplevel #0 [list set errorCode $ec]]
                # Don't have same sources on client side, so fold
                # source into the error message.
                if {[string match info $t]} {
                    $slave eval [list Oc_Log Log $m info]
                } else {
                    set augmsg $m
                    if {[string length $s]} {
                        append augmsg "\n($s)"
                    }
                    $slave eval [list Oc_Log Log $augmsg $t]
                }
            }
        }
    }
}
