# FILE: procs.tcl
#
# A collection of Tcl procedures (not Oc_Classes) which are part of the
# Net extension
#
# Last modified on: $Date: 2015/09/30 07:41:35 $
# Last modified by: $Author: donahue $

# [Net_GeneralInterfaceProtocol] returns an instance of the Oc_Class
# Net_Protocol that serves as the server-side half corresponding to
# the client-side found in the Oc_Class Net_ThreadWish.
#

proc Net_GIPLog {connection msg type src} {
    # Send over Oc_Log Log data for client to act on.
    global errorInfo errorCode
    if {[string match status $type]} {
        # Don't bother sending status messages
        # Would send lots of network debugging msgs
        # (which would generate more...)
        return
    }
    $connection Tell Log $msg $type $src $errorInfo $errorCode
}
proc Net_GeneralInterfaceProtocol {gui cmds} {

    Net_Protocol New protocol -name "OOMMF GeneralInterface protocol 0.1"

    set body {
        # Close any prior GUIs on this connection
        Oc_EventHandler Generate $connection CloseGui
        Oc_EventHandler DeleteGroup $connection-CloseGui
        Oc_EventHandler New _ Oc_Log Log \
                [list Net_GIPLog $connection %msg %type %src] \
                -groups [list $connection-CloseGui $connection]
        Oc_EventHandler New _ $connection Delete \
                [list Oc_EventHandler Generate $connection CloseGui] \
                -groups [list $connection-CloseGui $connection]
        return [list start [list 0 }
    append body [list $gui]
    append body {]]}
    $protocol AddMessage start GetGui {} $body

    foreach cmd [concat set $cmds] {
        set body "set code \[catch {uplevel #0 "
        append body [list $cmd]
        append body " \$args} result]\n"
        append body {if {$code == 1}}
        append body " \{\n"
        append body {return [list start [list $code $result \
                        $::errorInfo $::errorCode }
        append body [list $cmd]
        append body "]]\n\}\n"
        append body {return [list start [list $code $result]]}
        $protocol AddMessage start $cmd args $body
    }

    $protocol AddMessage start Share {local remote trace} {
        global $local

        # The script to be run in the client, when $local is written in the
        # server.  %s holds the place for the [set] command to go along with
        # the new value.
        set ctraceFmt "[list trace vdelete $remote w $trace]; %s;\
            [list trace variable $remote w $trace] ;#"

        # A common prefix for the local write trace, and the script which
        # only starts write traces in the client.  The latter is used
        # when the variable $local does not exist.
        set pfx "[list $connection Tell Result] \[[list format $ctraceFmt]"

        # The script to be run server-side, when $local is written.  It
        # substitutes the [set] command into $ctraceFmt, then sends the
        # script off to be evaluated in the client.
        set strace "$pfx \[[list list set $remote] \[[list uplevel #0\
                set $local]]]] ;#"

        # If $local already has a value; initialize the client's copy to
        # also have that value.  If not, just set write traces in the client.
        if {[info exists $local]} {eval $strace} else {eval "$pfx {}]"}

        # Set the trace, so changes here get reflected in the client.
        trace variable $local w $strace

        # Make sure this write trace is destroyed if we ever lose contact
        # with the client
        regsub -all -- % $strace %% strace
        Oc_EventHandler New _ $connection CloseGui \
                [list trace vdelete $local w $strace]
        return [list start [list 0 ""]]
    }
    return $protocol
}

proc Net_StartCleanShutdown {} {
   # Only shutdown once per app.
   proc [lindex [info level 0] 0] {} {}
   Oc_Log Log "Net_StartCleanShutdown start" status

   # Set up closure tracking. Use dict with values set to event handler.
   # There are three special keys: stage, timeout, and dummy. Stage is
   # one of
   #  0 : Setting up tracking inside Net_StartCleanShutdown
   #  1 : Setup complete, waiting on Net_Server and Net_Thread deletes
   #  2 : Host and account server tracking setup
   #  3 : Waiting on host and account server deletes
   # Stage values of 0 and 2 are used to handle cases where
   # Net_LastConnectionCheck is triggered in the midst of tracker
   # configuration.
   #  The timeout entry holds the afterid for the timeout backstop. The
   # key "dummy" is used with an empty value to initialize the system
   # when there are no server or thread shutdowns to wait on.
   global net_startcleanshutdown_tracker
   set net_startcleanshutdown_tracker [dict create stage 0]

   # Shutdown servers and any connections spawned off of each
   foreach server [Net_Server Instances] {
      Oc_EventHandler New handler $server DeleteEnd \
         [list Net_LastConnectionCheck $server] -oneshot 1 \
         -groups [list $server net_startcleanshutdown_tracker]
      dict set net_startcleanshutdown_tracker $server $handler
      if {[catch {$server Shutdown}]} {
         # Catch for robust shutdown
         dict unset net_startcleanshutdown_tracker $server
         $handler Delete
      }
      # NOTE: $server Shutdown doesn't actually delete the server
      #       until all client connections have closed.
   }

   # Cleanly shutdown any client side connections. The bye
   # request causes the server to close the connection.
   foreach client [Net_Thread Instances] {
      Oc_EventHandler New handler $client DeleteEnd \
         [list Net_LastConnectionCheck $client] -oneshot 1 \
         -groups [list $client net_startcleanshutdown_tracker]
      dict set net_startcleanshutdown_tracker $client $handler
      if {[catch {
         if {[$client Ready]} {
            $client Send bye
         } else {
            Oc_EventHandler New _ $client Ready [list $client Send bye]
         }
      }]} {
         # Catch for robust shutdown
         dict unset net_startcleanshutdown_tracker $client
         $handler Delete
      }
   }

   Oc_Main BlockLibShutdown
   dict set net_startcleanshutdown_tracker stage 1
   dict set net_startcleanshutdown_tracker timeout \
      [after 15000 {Net_LastConnectionCheck {}}]

   if {[dict size $net_startcleanshutdown_tracker]==2} {
      # No server or thread connections to wait on? Add dummy element to
      # tracker and call Net_LastConnectionCheck directly
      dict set net_startcleanshutdown_tracker dummy {}
      Net_LastConnectionCheck dummy
   } ;# Otherwise, wait for service or thread shutdown to trigger
     ## Net_LastConnectionCheck
   Oc_Log Log "Net_StartCleanShutdown end" status
}

proc Net_LastConnectionCheck { key } {
   global net_startcleanshutdown_tracker
   Oc_Log Log "Net_LastConnectionCheck $key;\
      tracker: $net_startcleanshutdown_tracker" status
   # If key is empty string, then timeout has occurred
   if {[string match {} $key]} {
      Oc_EventHandler DeleteGroup net_startcleanshutdown_tracker
      unset -nocomplain net_startcleanshutdown_tracker
      Oc_Main AllowLibShutdown
      return
   }

   # Remove key from dict
   dict unset net_startcleanshutdown_tracker $key

   # If call occurs during tracker setup, then simply return.
   set stage [dict get $net_startcleanshutdown_tracker stage]
   if {$stage == 0 || $stage == 2} {
      return
   }

   # If in stage 1 or 3 with keys beyond the standard stage and timeout,
   # return and wait for additional closures.
   if {[dict size $net_startcleanshutdown_tracker]>2} {
      return
   }

   # If in stage 1 and all connection keys have been removed, then move
   # on to stage 2/3 and set up waiting on host and account server
   # shutdowns.
   if {$stage == 1} {
      dict set net_startcleanshutdown_tracker stage 2
      foreach acct [Net_Account Instances] {
         set conn [$acct AccountConnection]
         if {[string match {} $conn]} { continue }
         Oc_EventHandler New handler $conn DeleteEnd \
            [list Net_LastConnectionCheck $conn] -oneshot 1 \
            -groups [list $conn net_startcleanshutdown_tracker]
         dict set net_startcleanshutdown_tracker $conn $handler
         if {[catch {$acct Delete}]} {
            dict unset net_startcleanshutdown_tracker $conn
            $handler Delete
         }
      }
      foreach host [Net_Host Instances] {
         set conn [$host HostConnection]
         if {[string match {} $conn]} { continue }
         Oc_EventHandler New handler $conn DeleteEnd \
            [list Net_LastConnectionCheck $conn] -oneshot 1 \
            -groups [list $conn net_startcleanshutdown_tracker]
         dict set net_startcleanshutdown_tracker $conn $handler
         if {[catch {$host Delete}]} {
            dict unset net_startcleanshutdown_tracker $conn
            $handler Delete
         }
      }
      dict set net_startcleanshutdown_tracker stage 3
      if {[dict size $net_startcleanshutdown_tracker]>2} {
         return
      }
   }

   # If in stage 3 and all connection keys have been removed,
   # then clean up and remove Oc_Main LibShutdown block.
   Oc_EventHandler DeleteGroup net_startcleanshutdown_tracker
   unset net_startcleanshutdown_tracker
   Oc_Main AllowLibShutdown
   Oc_Log Log "Net_LastConnectionCheck complete" status
}


proc Net_GetNicknames { {oid {}} } {
   # Queries the account server, if any, and returns a list of instance
   # names for specified oid.  Default oid is oid of current process.
   set nicknames {}
   set account_server [Net_Account Instances]
   if {[llength $account_server]>0} {
      set account_server [lindex $account_server 0]
      set reply [$account_server Names $oid]
      if {[lindex $reply 0]==0} {
         set nicknames [lindex $reply 1]
      }
   }
   return $nicknames
}
