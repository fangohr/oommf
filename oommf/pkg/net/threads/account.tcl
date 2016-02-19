# FILE: account.tcl
#
# The server process for an OOMMF account.
#
# It listens on the port name by its first command line argument, and 
# directs communications among all OOMMF threads owned by this account.
#
# Last modified on: $Date: 2015/09/30 07:41:35 $
# Last modified by: $Author: donahue $
#
# For debugging:
# Oc_ForceStderrDefaultMessage
# Oc_Log SetLogHandler Oc_DefaultMessage status
# or
#Oc_FileLogger SetFile \
#    [format "C:/Users/donahue/oommf/account-[clock seconds]-%05d.log" [pid]]
#Oc_Log SetLogHandler [list Oc_FileLogger Log] panic
#Oc_Log SetLogHandler [list Oc_FileLogger Log] error
#Oc_Log SetLogHandler [list Oc_FileLogger Log] warning
#Oc_Log SetLogHandler [list Oc_FileLogger Log] info
#Oc_Log SetLogHandler [list Oc_FileLogger Log] status


if {([llength $argv] > 3) || ([llength $argv] == 0)} {
    error "usage: account.tcl <service_port> ?<creator_port>? ?<oid>?"
}

# It is the job of this server to store and provide access to a few
# arrays shared by all OOMMF apps running under this account.
#
# First, there is an array that maps a pair (process ID, timestamp)
# to a unique OOMMF process ID.  (Uniqueness is within the data
# managed by this account server; it could be expanded to be uniqueness
# over a host, with appropriate mods to the host server.)
array set process [list]
if {[llength $argv] > 2} {
    set nextOOMMFId [lindex $argv 2]
    if {[catch {incr nextOOMMFId 100}]} {
	error "usage: account.tcl <service_port> ?<creator_port>? ?<oid>?"
    }
} else {
    set nextOOMMFId 0
} 

# Second, there is a map from OOMMF process ID to a list of data describing
# that process: the name of the program (eg. mmDataTable), the connection
# over which we communicate to that process, the process ID.
array set info [list]

# Third, there is an array that maps a service ID (really a pair (OOMMF
# pid, serverType) joined by a :) to the port number on which that
# server is listening.  (serverType is a serial number identifying the
# server object inside the server application.  For example, service ID
# 2347:0 would be the first server in pid 2347, 2347:1 the second, etc.)
array set service [list]

# Fourth, an array that maps the service ID to its protocol
array set protocol [list]

# Fifth, an array that maps the service ID to the registration name
array set regname [list]

# Finally, the "name" array keeps track of names of processes, so
# that the same process (and its services) can be found again and
# again by multiple client processes.
#
# The "claim" message asserts ownership over a name -- only one client
# can win the race.  The "associate" message sets an OID as the referent
# of a name, only by a claimer, or for an unclaimed name.  The "find"
# message searches over this array to find applications by name.
# Every "newoid" message also registers a name $app:$oid so that
# every app has a name.

array set name [list]

global master Omf_export Omf_export_list Omf_export_mtime
set master(version) 1.0

# Set up for auto-loading
set master(relhdir) [string trimright [file dirname [info script]] ./]
set master(abshdir) [file join [pwd] $master(relhdir)]
set master(pdir) [file dirname $master(abshdir)]
set master(libdir) [file dirname $master(pdir)]
set master(root) [file dirname $master(libdir)]
cd $master(root)

if {[lsearch $auto_path $master(libdir)] < 0} {
    lappend auto_path $master(libdir)
}

# Support libraries
package require Oc
package require Net

# Ignore Ctrl-C's, Ctrl-Z's.
Oc_IgnoreInteractiveSignals
# set up to reap zombie children
proc Reap {} {
    catch {exec {}}
    after 30000 [info level 0]
}
Reap

# Try to keep going, even if controlling terminal goes down.
Oc_IgnoreTermLoss

proc FatalError {msg} {
    global master
    if {![string match *expired. $msg]} {
        Oc_Log Log $msg panic
    } else {
        Oc_Log Log $msg status
    }
    if {$master(registered)} {
        Deregister
        return
    }
    exit
}

Oc_EventHandler Bindtags NotifyClaim NotifyClaim
Oc_EventHandler Bindtags NotifyNewOid NotifyNewOid

proc FreeOid {oid} {
    global process info service name protocol regname
    Oc_Log Log "Freeing OID $oid" status
    Oc_EventHandler DeleteGroup oid-$oid
    foreach n [array names service $oid:*] {
        unset service($n)
        unset protocol($n)
        unset regname($n)
        Oc_EventHandler Generate Oc_Main DeleteThread -id $n
    }
    Oc_EventHandler Generate Oc_Main DeleteOid -oid $oid
    set app [lindex $info($oid) 0]
    set pid [lindex $info($oid) 2]
    set names [lindex $info($oid) 3]
    unset info($oid)
    foreach n $names {
        Oc_EventHandler Generate NotifyClaim $n
        unset name($n)
    }
    foreach n [array names process $pid,*] {
        unset process($n)
    }
}

proc LostConnection {oid} {
    global info service

    Oc_Log Log "LostConnection to OID $oid" status

    # Don't send thread updates to now-dead connection
    set connection [lindex $info($oid) 1]
    Oc_EventHandler DeleteGroup NewThread-$connection

    set info($oid) [lreplace $info($oid) 1 1 {}]
    # We lost connection to process $oid.  Is it coming back?
    # Attempt to connect to its service(s).  Success,
    # then assume process will reconnect.
    foreach sid [array names service $oid:*] {
	set port $service($sid)
	if {[catch {socket localhost $port} s] == 0} {
	    catch {close $s}
	    return
	}
    }
    # No service is alive.  Clear away data for presumed dead process
    FreeOid $oid
}

# Multiple connections share one protocol, and one set of globals
# Define protocol
Net_Protocol New master(protocol) \
        -name [list OOMMF account protocol $master(version)]

# Replace default "exit" message with no-op.
$master(protocol) AddMessage start exit {} {
    return [list start [list 0 ""]]
}

$master(protocol) AddMessage start newoid {app pid start {oid -1}} {
    # Generate an "OOMMF ID" for the instance of application named $app
    # (for example, mmDisp) with system process id $pid that was started
    # up at time $start (protection against $pid re-use by system).
    # Return the "OOMMF ID" (a non-negative integer) to the sender.
    global nextOOMMFId process info name

    # Check for re-use of $pid
    set names [array names process $pid,*]
    if {[llength $names]} {
	if {![info exists process($pid,$start)]
		|| ($process($pid,$start) != $oid)} {
	    return [list start [list 1 "Duplicate pid: $pid"]]
	}
    }
    if {$oid < 0} {
	set oid $nextOOMMFId
	incr nextOOMMFId
    }
    set process($pid,$start) $oid
    Oc_EventHandler Generate NotifyNewOid $pid -oid $oid
    Oc_EventHandler Generate Oc_Main NewOid -oid $oid
    
    foreach x [array names info] {
	set c [lindex $info($x) 1]
	if {[llength [info commands $c]]} {
	    $c Tell private lastoid $nextOOMMFId
	}
    }

    if {[info exists info($oid)]} {
       set info($oid) [list $app $connection $pid [lindex $info($oid) 3]]
    } else {
       set unique [string tolower $app:$oid]
       set info($oid) [list $app $connection $pid [list $unique]]
       set name($unique) [list associate $oid]
       Oc_EventHandler Generate NotifyClaim $unique
    }

    # When the connection goes away, de-register the OOMMF ID.
    Oc_EventHandler New _ $connection Delete [list LostConnection $oid] \
        -groups [list oid-$oid $connection]
    return [list start [list 0 $oid]]
}

$master(protocol) AddMessage start freeoid {oid} {
    set code [catch {FreeOid $oid} result]
    return [list start [list $code $result]]
}

$master(protocol) AddMessage start find {args} {
    # $args is a list of patterns to look for in the "name" array.
    # Return a list -- one per pattern -- of OID lists that match.
    global name
    set result [list]
    foreach pattern $args {
        set oidlist [list]
	set pattern [string tolower $pattern]
        foreach n [array names name $pattern] {
            foreach {mode value} $name($n) break
            switch -exact -- $mode {
                associate {
                    lappend oidlist $value
                }
                claim {
                    # Name is claimed, but not associated to any process
                    # yet.  Do nothing.
                }
            }
        }
        lappend result [lsort -integer $oidlist]
    }
    return [list start [list 0 $result]]
}

proc NotifyClaim {conn n} {
    if {[llength [info commands $conn]]} {
        $conn Tell notify claim $n
    }
}

$master(protocol) AddMessage start claim {n args} {
    # Client wants to claim a name.
    global name
    set watch 1
    if {[llength $args]} {
        set switch [lindex $args 0]
        if {[llength $args] > 1} {
            return [list start [list -1 \
                    "wrong # args: should be \"claim name ?-nowatch?\""]]
        }
        # check for the -nowatch switch
        if {[string compare $switch -nowatch]} {
            return [list start [list -1 "invalid argument \"$switch\""]]
        }
        set watch 0
    }
    set n [string tolower $n]
    if {[info exists name($n)]} {
        # The name is already taken or claimed.
        if {$watch} {
            # set up to notify when the name status changes
            switch -exact -- [lindex $name($n) 0] {
                associate {
                    # Name already associated with an OID.  Set up to
                    # send a message to that effect.
                    after idle [list NotifyClaim $connection $n]
                }
                claim {
                    # Name is claimed, but not associated.  Set up to
                    # send message when association is done (or claim
                    # is released).
                    Oc_EventHandler New _ NotifyClaim $n \
                            [list NotifyClaim $connection $n] \
                            -oneshot 1 -groups [list $connection]
                }
            }
        }
        return [list start [list 1 "Name \"$n\" already claimed."]]
    }
    # Name is unclaimed.  Claim in the name of the client.
    # Arrange for claim to be released if the client drops connection.
    Oc_EventHandler New _ $connection Delete \
            [list unset name($n)] -oneshot 1 -groups [list claim-name-$n]
    set name($n) [list claim $connection $_]
    return [list start 0]
}

$master(protocol) AddMessage start names { args } {
   # Returns a list of names associated with each oid specified
   # in args.  If args is empty, then returns names for all oids.
   global name
   foreach n [lsort [array names name]] {
      set val $name($n)
      if {[string match "associate" [lindex $val 0]]} {
         set oid [lindex $val 1]
         lappend oidnames($oid) $n
      }
   }
   set result [list]
   if {[llength $args] == 0} {
      foreach oid [lsort -integer [array names oidnames]] {
         lappend result $oid $oidnames($oid)
      }
   } else {
      foreach oid $args {
         if {[info exists oidnames($oid)]} {
            lappend result $oid $oidnames($oid)
         } else {
            lappend result $oid {}
         }
      }
   }
   return [list start [concat 0 $result]]
}

$master(protocol) AddMessage start associate {n oid} {
    # Client wants us to associate the name $n with an $oid.
    global name info
    # Check for a claim on the name, or existing use.
    set n [string tolower $n]
    if {[info exists name($n)]} {
       switch -exact -- [lindex $name($n) 0] {
          associate {
             #puts stderr "Already assoc"
             return [list start [list 1 "Name \"$n\" already associated"]]
          }
          claim {
             if {[string compare $connection [lindex $name($n) 1]]} {
                #puts stderr "Already claimed"
                return [list start \
                           [list 1 "Name \"$n\" claimed by another."]]
             }
          }
       }
       # Flow to this point means name is claimed by current connection.
       # Below, either an association is made or else an error occurs.
       # Either way, we want to clear the claim, so go ahead and do it
       # now.
       Oc_EventHandler DeleteGroup claim-name-$n
       unset name($n)   ;# Clear claim
    }
    # Reaching here means no claim, or our claim, on the name.
    # Next check that the $oid is legal.
    if {![info exists info($oid)]} {
        return [list start [list 1 "No process with OID \"$oid\""]]
    }
    # Finally, check that the name is legal.  Names of the form
    # appname:number are only valid if appname matches the application
    # name for the oid, and number matches oid.  Otherwise naming can
    # be confusing.
    if {[regexp {^([^:]+):([0-9]+)$} $n dummy appname appnumber]} {
       set checkname [string tolower [lindex $info($oid) 0]]
       if {[string compare $checkname $appname]!=0} {
          return [list start [list 1 "Invalid name; \"$n\"\
                        doesn't match application name \"$checkname\""]]
       }
       if {[string compare $oid $appnumber]!=0} {
          return [list start [list 1 "Invalid name; \"$n\"\
                                   doesn't match process OID \"$oid\""]]
       }
    }

    # Everything is OK.  Make the association.
    # First, cancel any claim-clearing handler.
    Oc_EventHandler DeleteGroup claim-name-$n

    set name($n) [list associate $oid]
    Oc_EventHandler Generate NotifyClaim $n
    foreach {app c pid nameList} $info($oid) break
    set info($oid) [list $app $c $pid [linsert $nameList end $n]]

    if {[llength [info commands $c]]} {
       $c Tell private nickname $n ;# Inform name holder of association
    }

    return [list start 0]
}

proc NotifyNewOid {conn pid oid} {
    if {[llength [info commands $conn]]} {
        $conn Tell notify newoid $pid $oid
    }
}

$master(protocol) AddMessage start getoid {pid args} {
    # Return to client the oid registered for a particular pid.
    set watch 1
    if {[llength $args]} {
        set switch [lindex $args 0]
        if {[llength $args] > 1} {
            return [list start [list -1 \
                    "wrong # args: should be \"getoid pid ?-nowatch?\""]]
        }
        # check for the -nowatch switch
        if {[string compare $switch -nowatch]} {
            return [list start [list -1 "invalid argument \"$switch\""]]
        }
        set watch 0
    }
    global process
    set candidates [array names process $pid,*]
    switch [llength $candidates] {
        0 {
            if {$watch} {
                # Set up to notify when desired registration is available.
                Oc_EventHandler New _ NotifyNewOid $pid  \
                        [list NotifyNewOid $connection $pid %oid] \
                        -oneshot 1 -groups [list $connection]
            }
            return [list start [list 1 "Process <$pid> not registered"]]
        }
        1 {
            return [list start [list 0 $process([lindex $candidates 0])]]
        }
        default {
            return [list start [list -1 "Programming error?\
                    Multiple registrations for process <$pid>"]]
        }
    }
}

$master(protocol) AddMessage start getpids {oidpats args} {
    # Given a list of glob-style oid patterns (probably either a single
    # oid or "*"), return to the client a list of all processes matching
    # each pattern.  The return list is a list of pairs, {oid pid},
    # unless the option "-appname" is requested in which case the return
    # is a list of triplets, {oid pid appname}.
    set appname 0
    if {[llength $args]} {
        set option [lindex $args 0]
        if {[llength $args] > 1} {
            return [list start [list -1 \
                    "wrong # args: should be \"getpids oidpats ?-appname?\""]]
        }
        # check for the -appname switch
        if {[string compare $option -appname]} {
            return [list start [list -1 "invalid argument \"$option\""]]
        }
        set appname 1
    }
    global info
    set oidlist {}
    foreach pat $oidpats {
        set oidlist [concat $oidlist [array names info $pat]]
    }
    if {[llength $oidlist]==0} {
        return [list start [list 1 \
                "No processes match OID pattern(s) \"$oidpats\""]]
    }
    set result 0
    foreach oid [lsort -integer -unique $oidlist] {
        set oidinfo $oid
        lappend oidinfo [lindex $info($oid) 2]
        if {$appname} { lappend oidinfo [lindex $info($oid) 0] }
        lappend result $oidinfo
    }
    return [list start $result]
}

$master(protocol) AddMessage start kill {oidpats args} {
   # Given a list of glob-style oid patterns (probably either a single
   # oid or "*"), does a tell "die" on each account server connection.
   # This requests each app to shutdown.
   set appname 0
   if {[llength $args]} {
      set option [lindex $args 0]
      if {[llength $args] > 1} {
         return [list start [list -1 \
                "wrong # args: should be \"kill oidpats ?-appname?\""]]
      }
      # check for the -appname switch
      if {[string compare $option -appname]} {
         return [list start [list -1 "invalid argument \"$option\""]]
      }
      set appname 1
   }
   global info
   set oidlist {}
   foreach pat $oidpats {
      set oidlist [concat $oidlist [array names info $pat]]
   }
   if {[llength $oidlist]==0} {
      return [list start [list 1 \
                   "No processes match OID pattern(s) \"$oidpats\""]]
   }
   set oidlist [lsort -integer -unique $oidlist]
   foreach oid  $oidlist {
      set oidconn [lindex $info($oid) 1]
      $oidconn Tell private die
   }
   return [list start $oidlist]
}

$master(protocol) AddMessage start threads {} {
    # Function is to return a list of running services registered
    # with this account server.  A quartet for each one: 
    # ($regName $serviceId $port $protocol).
    #
    # Name of this message should really be "services"
    global service protocol regname info
    set ret [list]
    foreach sid [array names service] {
        set port $service($sid)
        set protocolname $protocol($sid)
        set advertisedname $regname($sid)
        lappend ret [list $advertisedname $sid $port $protocolname]
    }

    # Assume any client who asked for a list of threads wants to be
    # informed when threads are (de)registered, and oids are
    # created/freed, but only one handler per connection.
    Oc_EventHandler DeleteGroup NewThread-$connection
    Oc_EventHandler New _ Oc_Main NewThread \
            [list $connection Tell newthread %id] \
            -groups [list $connection NewThread-$connection]
    Oc_EventHandler New _ Oc_Main DeleteThread \
            [list catch [list $connection Tell deletethread %id]] \
            -groups [list $connection NewThread-$connection]
    Oc_EventHandler New _ Oc_Main NewOid \
            [list $connection Tell newoid %oid] \
            -groups [list $connection NewThread-$connection]
    Oc_EventHandler New _ Oc_Main DeleteOid \
            [list catch [list $connection Tell deleteoid %oid]] \
            -groups [list $connection NewThread-$connection]
    return [list start [concat 0 $ret]]
}
$master(protocol) AddMessage start lookup {sid} {
    # Return information about a service, given a service ID
    global service protocol regname info
    if {[info exists service($sid)]} {
        set port $service($sid)
        set protocolname $protocol($sid)
        set advertisedname $regname($sid)
        # Any checking here?  connection good? port registered?
        return [list start [list 0 $port $advertisedname $protocolname]]
    }
    return [list start [list 1 service $sid not registered]]
}
$master(protocol) AddMessage start register {sid alias port protocolname} {
    # Register the availability of service $sid on port $port
    global service protocol regname info

    # Verify registration request comes from correct app
    foreach {oid type} [split $sid :] break
    set approvedConnection [lindex $info($oid) 1]
    if {[string compare $connection $approvedConnection]} {
        return [list start [list 1 "Registration permission denied for $sid"]]
    }

    # Prevent duplicates
    if {[info exists service($sid)] && ($service($sid) != $port)} {
        return [list start [list 1 "Duplicate registration of $sid denied"]]
    }
    if {![info exists service($sid)]} {
	set service($sid) $port
        set protocol($sid) $protocolname
        set regname($sid) $alias
	Oc_EventHandler Generate Oc_Main NewThread -id $sid
    }
    return [list start [list 0 {}]]
}
$master(protocol) AddMessage start deregister { sid } {
    # De-register a service.  
    global service protocol regname info

    # First verify de-registration request comes from correct app
    foreach {oid type} [split $sid :] break
    set usedConnection [lindex $info($oid) 1]
    if {[string compare $connection $usedConnection]} {
        return [list start [list 1 "permission denied"]]
    }

    # Should add error check
    # Could also add service-death notification
    set result [unset service($sid) protocol($sid) regname($sid)]
    Oc_EventHandler Generate Oc_Main DeleteThread -id $sid
    return [list start [list 0 $result]]
}

# Define and start server 
# add -allow and -deny options to control access
# We have to do a manual registration with host server
set master(registered) 0
Net_Server New master(server) -protocol $master(protocol) -register 0
set serviceport [lindex $argv 0]
if {[catch {$master(server) Start $serviceport} master(msg)]} {
    FatalError "Can't start OOMMF account server on port $serviceport:
        $master(msg)."
}
set master(port) [$master(server) Port]

# Our account name
set master(account) [Net_Account DefaultAccountName]

# Get export list -- read into global array
set master(exportFiles) {}
if {[file readable [file join $master(pdir) .omfExport.tcl]]} {
    lappend master(exportFiles) [file join $master(pdir) .omfExport.tcl]
}
if {[file readable [file join $master(pdir) omfExport.tcl]]} {
    lappend master(exportFiles) [file join $master(pdir) omfExport.tcl]
}

proc CheckExportTimes {} {
    # Check .omfExport file modification times against the 
    # modification time when last sourced.  Returns 1 if any
    # of the files have a different mtime, 0 otherwise
    global Omf_export_mtime
    foreach file [array names Omf_export_mtime] {
        if {$Omf_export_mtime($file) != [file mtime $file]} {
            return 1
        }
    }
    return 0
}

proc SourceExportFiles {} {
    # Reads in .omfExport files
    global master Omf_export Omf_export_list Omf_export_mtime

    catch {unset Omf_export}
    catch {unset Omf_export_list}
    catch {unset Omf_export_mtime}

    foreach file $master(exportFiles) {
        if {![info exists Omf_export] && ![info exists Omf_export_list]} {
            Oc_Log Log "Sourcing $file ..." status
            if {[catch {uplevel #0 [list source $file]} msg]} {
                Oc_Log Log "Error sourcing $file: $msg" warning
            }
        }
        set Omf_export_mtime($file) [file mtime $file]
    }
    if {![info exists Omf_export] && ![info exists Omf_export_list]} {
        error "Neither Omf_export nor Omf_export_list\
                set in $master(exportFiles)"
    }
    if {![info exists Omf_export]} {
        # Fill Omf_export array from Omf_export_list
        foreach {id action} $Omf_export_list {
            set Omf_export($id) $action
        }
    }
    if {![info exists Omf_export_list]} {
        # Construct Omf_export_list from Omf_export array
        foreach id [array names Omf_export] {
            lappend Omf_export_list $id $Omf_export($id)
        }
    }
    Oc_Log Log "Exporting: $Omf_export_list" status
}

SourceExportFiles

# The host we register with
Net_Host New master(host) -hostname localhost
Oc_EventHandler New _ $master(host) Delete [list FatalError "host died"]
Oc_EventHandler New _ $master(host) Ready Register
proc Register {} {
    global master
    Oc_Log Log "Sending registration message to host" status
    set qid [$master(host) Send register $master(account) $master(port)]
    Oc_EventHandler New _ $master(host) Reply$qid RegisterReply -oneshot 1
    Oc_EventHandler New _ $master(host) Timeout$qid RegisterTimeout
}
proc RegisterTimeout {} {
    FatalError "Timed out waiting to complete registration"
}
proc RegisterReply {} {
    global master
    set reply [$master(host) Get]
    switch -- [lindex $reply 0] {
        0 {
           # This instance registered
           CallbackCreator
        }
        default {
           # Registration failed; some other account server probably
           # beat us.
           after [expr {500 + int(500*rand())}]
           CallbackCreator
           exit
           # set master(registered) 0
           # FatalError "Registration error: [lrange $reply 1 end]"
        }
    }
}
proc CallbackCreator {} {
    proc CallbackCreator {} {}
    global argv master
    set master(registered) 1
    if {[llength $argv] >= 2} {
        # Inform my creator that an account server is running.
        set port [lindex $argv 1]
        if {[catch {socket localhost $port} s]} {
            FatalError "Unable to call back $port: $s"
        } else {
            Oc_Log Log "Called back $port" status
            catch { puts $s ""} ;# Seems to help close port
                                ## on other side. -mjd 981009
            catch {close $s} msg
            if {[string length $msg]} {
                return -code error "close error: $msg"
            }
        }
    }
}
proc Deregister {} {
    global master
    Oc_Log Log "Sending deregistration message to host" status
    if {[catch {
            $master(host) Send deregister $master(account) $master(port)
            } qid]} {
        # Can't send message -- host is probably dead.
        Oc_Log Log "Dying without deregistering!" warning
        exit
    }
    Oc_EventHandler New _ $master(host) Reply$qid DeregisterReply
    Oc_EventHandler New _ $master(host) Timeout$qid DeregisterTimeout
}
proc DeregisterReply {} {
    global master
    set reply [$master(host) Get]
    switch -- [lindex $reply 0] {
        0 {
            Oc_Log Log "Deregistered.  Exiting clean." status
            exit
        }
        default {
            Oc_Log Log "Deregistration error:\
                    [lrange $reply 1 end]\n\tExiting dirty." warning
            exit
        }
    }
}
proc DeregisterTimeout {} {
    Oc_Log Log "Deregistration timeout.  Exiting dirty." warning
    exit
}

if {0} {
   # Debug logging
   Oc_EventHandler New _ Net_Connection Ready \
       [list ConnectReport %object connect]
   Oc_EventHandler New _ Net_Connection Readable \
       [list ConnectReport %object read]
   Oc_EventHandler New _ Net_Connection Query \
       [list ConnectReport %object query]
   Oc_EventHandler New _ Net_Connection Delete \
       [list ConnectReport %object die]
   proc ConnectReport { object event } {
      if {![catch {clock milliseconds} ms]} {
         set secs [expr {$ms/1000}]
         set ms [format {%03d} [expr {$ms - 1000*$secs}]]
         set timestamp [clock format $secs \
                            -format "%H:%M:%S.$ms %Y-%m-%d"]
      } else {
         set secs [clock seconds]
         set timestamp [format [clock format $secs \
                                    -format %H:%M:%S.???\ %Y-%m-%d]]
      }
      switch -exact $event {
         connect { set msg "CONNECTION: $object" }
         read { set msg "Read $object: [$object Get]" }
         query { set msg "Query $object: [$object Get]" }
         die { set msg "DIE: $object" }
      }
      set chan [open "C:/Users/donahue/tmp/oommf/account.log" a]
      puts $chan [format "%s %6d %s" $timestamp [pid] $msg]
      close $chan
   }
}


# Once a connection becomes ready, set up handler to catch
# connection destructions.  On last one, exit.
Oc_EventHandler New _ Net_Connection Delete [list CheckConnect %object]

proc CheckConnect { object } {
   # A Net_Connection is being destroyed.  If there are no connections
   # remaining aside from the usual one to the host server, then schedule
   # our suicide.
   global master
   set connection_instances [Net_Connection Instances]
   # Cases:
   #  A) More than two connections => don't die
   #  B) Two connections, one going down is host => don't die
   #  C) Two connections, one going down is not host => die
   #  D) One connection, one going down is host => ???
   #  E) One connection, one going down is not host => die
   # Best behavior for case D is unclear.  This case can only
   # occur if the host dies before any other app connects to
   # the account server, since otherwise to get to a single
   # connection with only the host we have to pass through
   # state C.  Previously the CheckConnect handler was only
   # set after the second app connected to the account server,
   # in which case D resolves to don't die.  But recent account
   # object (i.e., the Net_Account class) code is robust wrt
   # to restarting host and account servers, so it is probably
   # more robust to die on case D.
   if {[llength $connection_instances]>2} { return }
   if {[llength $connection_instances]==2 && [info exists master(host)]} {
      set host_connection [$master(host) HostConnection]
      if {[string compare $host_connection $object]==0} {
         return  ;# Don't die on host destruction
      }
   }
   # Begin account server shutdown: First, stop accepting new
   # connections.
   $master(server) ForbidServiceStart
   if {[$master(server) IsListening]} {
      $master(server) Stop
   }
   # Shut down
   after idle [list FatalError\
                   "All connections closed.  Account server expired."]
}

vwait master(forever)
