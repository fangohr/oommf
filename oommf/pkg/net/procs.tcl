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
    # Stop servers, and prevent them from restarting.
    foreach server [Net_Server Instances] {
       catch {
          $server ForbidServiceStart
          if {[$server IsListening]} {
             $server Stop
          }
       } ;# Catch for robust shutdown
    }
    Oc_EventHandler New _ Oc_Main Exit Net_CloseClients
}

proc Net_CloseClients {} {
    # Cleanly shutdown any client sides
    foreach client [Net_Thread Instances] {
       catch {
          if {[$client Ready]} {
             $client Send bye
          } else {
             Oc_EventHandler New _ $client Ready [list $client Send bye]
          } ;# Catch for robust shutdown
       }
    }
    if {[llength [Net_Connection Instances]] > 
	    [llength [concat [Net_Host Instances] [Net_Account Instances]]]} {
	# Monitor for shutdown of all connections
	foreach conn [Net_Connection Instances] {
	    Oc_EventHandler New _ $conn Delete \
		    [list Net_LastConnectionCheck $conn] -oneshot 1
	}
	Oc_Main BlockExit
        after 2000 {Oc_Main AllowExit}
    }
}

proc Net_LastConnectionCheck {conn} {
    if {[llength [Net_Connection Instances]] - 1 ==
	[llength [concat [Net_Host Instances] [Net_Account Instances]]]} {
	Oc_Main AllowExit
    }
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
