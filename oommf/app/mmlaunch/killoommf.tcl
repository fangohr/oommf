# FILE: killoommf.tcl
#
# PURPOSE: Issues "kill" messages through the account server to
#    terminate a specified list of OOMMF applications. Can also check
#    that account connections from OOMMF applications are active (i.e.,
#    not zombies).

# Ensure pkg directory is on auto_path in case sourcing executable is
# unadorned tclsh
set chkpath [file join [file dirname [file dirname [file dirname \
             [info script]]]] pkg]
if {[lsearch -exact $auto_path $chkpath]<0} {
   lappend auto_path $chkpath
}
unset chkpath

########################################################################
# Support procs

# Clean shutdown proc
set oidchan [set acctchan {}]
proc CloseSockets {} {
   global oidchan acctchan
   foreach channame [list oidchan acctchan] {
      set chan [set $channame]
      if {![string match {} $chan]} {
         catch { AskServer $chan bye }
         catch { close $chan }
         set $channame {}
      }
   }
}

# Default initializer of the accountname.  Tries to determine account
# name under which current thread is running.
proc DefaultAccountName {} {
   return [Oc_AccountName]
}

set querycount -1
proc AskServer {chan args} {
   global querycount timeout
   set timeout_incr 100
   set timeout_count [expr {int(ceil(double($timeout)/$timeout_incr))}]
   incr querycount
   puts $chan [concat query $querycount $args]
   while {$timeout_count>0} {
      if {[catch {gets $chan response} count]} {
         # Error reading socket
         return -code error $count
      }
      if {$count<0} {
         # Nothing read
         if {[eof $chan]} {
	    return -code error "EOF waiting for reply on server socket"
	 }
	 after $timeout_incr
	 incr timeout_count -1
      } else {
         # Record read
	 if {[string compare reply [lindex $response 0]]==0 \
		&& [lindex $response 1]==$querycount} {
            # This is response to the puts query
	    if {[lindex $response 2]} {
               # Response indicates error
	       return -code error [lindex $response 3]
	    }
            # Otherwise, we have a successful response
	    return [lrange $response 3 end]
	 }
      }
   }
   return -code error "Timeout waiting for reply from server socket"
}

########################################################################

package require Oc

Oc_ForceStderrDefaultMessage
catch {wm withdraw .}

Oc_Main SetAppName killoommf
Oc_Main SetVersion 2.1a0

# Remove a bunch of inapplicable default options from -help message
Oc_CommandLine Option console {} {}
Oc_CommandLine Option tk {} {}
Oc_CommandLine Option name {} {}
Oc_CommandLine Option sync {} {}
Oc_CommandLine Option id {} {}
Oc_CommandLine Option visual {} {}
Oc_CommandLine Option geometry {} {}
Oc_CommandLine Option colormap {} {}
Oc_CommandLine Option display {} {}
Oc_CommandLine Option use {} {}

set acctname [DefaultAccountName]
Oc_CommandLine Option account {
    {name {}}
} {
    global acctname;  set acctname $name
} [subst {Account (default is $acctname)}]

Oc_Option Get Net_Host port hostport  ;# Default setting
Oc_CommandLine Option hostport {
    {port {regexp {^[0-9]+$} $port}}
} {
    global hostport;  set hostport $port
} [subst {Host server port (default is $hostport)}]

Oc_CommandLine Option noaccesschecks {
} {
   # Override account access check settings in options.tcl
   Oc_Option Add * Net_Server checkUserIdentities 0
   Oc_Option Add * Net_Link checkUserIdentities 0
} {Disable access checks}

Oc_CommandLine Option timeout {
   {secs {regexp {^[0-9]+$} $secs} {is timeout in seconds (default is 5)}}
} {
   global timeout;  set timeout [expr {$secs*1000}]
} {Maximum time to wait for response from servers}
set timeout 5000

Oc_CommandLine Option show {} {
	global showonly; set showonly 1
} {Don't kill anything, just print matching targets}
set showonly 0

Oc_CommandLine Option shownames {} {
	global shownamesonly; set shownamesonly 1
} {Don't kill anything, just print matching target nicknames}
set shownamesonly 0

Oc_CommandLine Option test {} {
	global testflag; set testflag 1
} {Don't kill anything, just test that targets are responding}
set testflag 0

Oc_CommandLine Option pid {
} {
    global pidselect; set pidselect 1
} {Select by pid rather than oid}
set pidselect 0

Oc_CommandLine Option q {} {
	global quietflag; set quietflag 1
} {Quiet; do not print informational messages}
set quietflag 0

Oc_CommandLine Option [Oc_CommandLine Switch] {
   {{oid list} {} {is one or more oids,\
     appnames, or "all".  REQUIRED  (No default targets)}}
   } {
      global killrequests; if {[llength $oid]} {set killrequests $oid}
} {End of options; next argument is oid/app}
set killrequests {}

Oc_CommandLine Parse $argv

if {[catch {package require Net} netloadmsg]} {
   Oc_RobustPuts stderr "ERROR: $netloadmsg"
   exit 86
}

if {$shownamesonly} {
   # The shownamesonly flag overrides showonly
   set showonly 0
}

if {[llength $killrequests]==0} {
    Oc_Log Log [Oc_CommandLine Usage] info
    exit
}

# Only accounts on localhost currently supported
set host localhost

if {[catch {socket $host $hostport} hostchan]} {
   Oc_RobustPuts "No OOMMF applications detected."
   Oc_RobustPuts "(No host server responding on host $host, port $hostport.)"
   exit
}
fconfigure $hostchan -blocking 0 -buffering line -translation {auto crlf}

if {[catch {AskServer $hostchan lookup $acctname} acctport]} {
    Oc_RobustPuts stderr "Account lookup failure: $acctport"
    close $hostchan
    exit 1
}
close $hostchan

# Should we perform user id checks on connections?
set do_id_check 0
if {[info exists tcl_platform(user)] &&
    [string compare $tcl_platform(user) $acctname]==0} {
   set server_check 1
   if {[Oc_Option Get Net_Server checkUserIdentities sc] == 0} {
      set server_check $sc
   }
   set link_check 1
   if {[Oc_Option Get Net_Link checkUserIdentities lc] == 0} {
      set link_check $lc
   }
   if {$server_check!=0 || $link_check!=0} {
      set do_id_check 1
   }
}

if {[catch {socket $host $acctport} acctchan]} {
    Oc_RobustPuts stderr "ERROR: Unable to connect to account server\
                 on host $host, port $acctport"
    exit 1
}
fconfigure $acctchan -blocking 0 -buffering line -translation {auto crlf}

if {$do_id_check && [Net_CheckSocketAccess $acctchan]==0} {
   Oc_RobustPuts stderr "WARNING: Account server owner is not \"$acctname\""
}

# Open a separate channel to the account server for tracking oid updates
set oid_rip {}
set tickle_detail [list killoommf [pid]]
proc OidMsgHandler { chan } {
   if {[catch {gets $chan line} readCount]} { ;# socket error
      CloseSockets ; return
   }
   if {$readCount < 0} {
      if {[fblocked $chan]} { return }
      if {[eof $chan]} { CloseSockets ; return }
   }
   global testflag
   if {$testflag} {
      global tickle_detail
      if {[regexp {^tell aliveping ([0-9]+) (.*)$} $line _ oid detail] \
             && [string compare $tickle_detail [lindex $detail 0]]==0} {
         global oid_rip
         lappend oid_rip $oid
      }
   } else {
      if {[regexp {^tell deleteoid ([0-9]+)$} $line _ oid]} {
         global oid_rip
         lappend oid_rip $oid
      }
   }
}
if {[catch {socket $host $acctport} oidchan]} {
    Oc_RobustPuts stderr "ERROR: Unable to connect to account server\
                 on host $host, port $acctport (oid channel)"
    exit 1
}
fconfigure $oidchan -blocking 0 -buffering line -translation {auto crlf}
if {[catch {AskServer $oidchan trackoids} trackreply]} {
   Oc_RobustPuts stderr "Track oid failure: $trackreply"
   CloseSockets
   exit 1
}

fileevent $oidchan readable [list OidMsgHandler $oidchan]


# Get list of application names and nicknames
if {[catch {AskServer $acctchan names} namesreply]} {
   Oc_RobustPuts stderr "Names lookup failure: $namesreply"
   CloseSockets
   exit 1
}

set names [dict create]
dict for {key value} [dict create {*}$namesreply] {
   if {![regexp -- {^([^:]+):([0-9]+)$} [lindex $value 0] _ appname oid] \
       || $key != $oid} {
      Oc_RobustPuts stderr "Invalid name response for oid $key: $value"
      CloseSockets
      exit 1
   }
   # Insert unadorned appname at front of names list for each oid
   dict append names $key [linsert $value 0 $appname]
}

# Get oid+pid pairs
if {[catch {AskServer $acctchan getpids *} pidlist]} {
   Oc_RobustPuts stderr "Pid lookup failure: $pidlist"
   CloseSockets
   exit 1
}

# Construct oid kill list
set oidlist {}
if {[lsearch -exact $killrequests all]>=0} {
   # Terminate all OOMMF apps
   set oidlist [dict keys $names]
} else {
   foreach elt $killrequests {
      if {[regexp {^[0-9]+$} $elt]} {
         # Numeric selection
         if {$pidselect} {
            set index [lsearch -exact -integer -index 1 $pidlist $elt]
            if {$index<0} {
               Oc_RobustPuts stderr "WARNING: No match for pid $elt; skipping"
               continue
            }
            lappend oidlist [lindex $pidlist $index 0]
         } else {
            if {[dict exists $names $elt]} {
               lappend oidlist $elt
            } else {
               Oc_RobustPuts stderr "WARNING: No match for oid $elt; skipping"
               continue
            }
         }
      } else {
         # Selection by name
         set oidsize [llength $oidlist]
         dict for {key value} $names {
            # For each oid, check if the killrequest pattern
            # matches any of the names associated with that
            # oid. If so, append oid.
            if {[lsearch -glob -nocase $value $elt]>=0} {
               lappend oidlist $key
            }
         }
         if {[llength $oidlist]==$oidsize} {
            Oc_RobustPuts stderr "WARNING: No matches for pattern $elt"
         }
      }
   }
}
if {[llength oidlist]==0} {
   Oc_RobustPuts "No applications match oid list"
   CloseSockets
   exit
}

# Remove duplicates
set kill_list [lindex $oidlist 0]
foreach oid [lrange $oidlist 1 end] {
   if {[lsearch -exact -integer $kill_list $oid]<0} {
      lappend kill_list $oid
   }
}

# Determine report table column widths
set oidwidth 1   ;# OID field add angle bracket padding (2 characters)
set pidwidth 3
set appwidth 0
foreach oid $kill_list {
   if {[string length $oid]>$oidwidth} {
      set oidwidth [string length $oid]
   }
   set pid [lindex [lsearch -exact -integer -index 0 -inline $pidlist $oid] 1]
   if {[string length $pid]>$pidwidth} {
      set pidwidth [string length $pid]
   }
   set appname [lindex [dict get $names $oid] 0]
   if {[string length $appname]>$appwidth} {
      set appwidth [string length $appname]
   }
}
set oidpad [expr {($oidwidth+2-3)/2}]
set oidlead [expr {$oidwidth+2-$oidpad}]
set pidpad [expr {($pidwidth-3)/2}]
set pidlead [expr {$pidwidth-$pidpad}]

if {$showonly || $shownamesonly} {
   Oc_RobustPuts "Application selection (no exit requests sent):"
   Oc_RobustPuts [format \
          " %${oidlead}s%${oidpad}s  %${pidlead}s%${pidpad}s  Application" \
          OID "" PID ""]
   foreach oid $kill_list {
      set pid [lindex \
                  [lsearch -exact -integer -index 0 -inline $pidlist $oid] 1]
      if {$shownamesonly} {
         set appname [lrange [dict get $names $oid] 1 end]
      } else {
         set appname [lindex [dict get $names $oid] 0]
      }
      Oc_RobustPuts [format " <%${oidwidth}d>  %${pidwidth}d  %s" \
                        $oid $pid $appname]
   }
   CloseSockets
   exit
}

set kill_check {}
if {$testflag} {
   # Test only
   set check_result "alive"
   foreach oid $kill_list {
      if {[catch {AskServer $acctchan tickle $oid $tickle_detail} reply]} {
         Oc_RobustPuts [format " <%*d> : Tickle error --- $reply" $oidwidth $oid]
      } else {
         lappend kill_check $oid
      }
   }
} else {
   # Kill
   set check_result "killed"
   foreach oid $kill_list {
      if {[catch {AskServer $acctchan kill $oid} reply]} {
         Oc_RobustPuts [format " <%*d> : Kill error --- $reply" $oidwidth $oid]
      } else {
         lappend kill_check $oid
      }
   }
}

# Enter event loop to process trackoid messages
while {[llength $kill_check]>0} {
   after $timeout {lappend oid_rip -1}
   vwait oid_rip
   foreach oid $oid_rip { ;# Remove each oid from kill_check
      if {$oid<0} { ;# Timeout
         break
      }
      set index [lsearch -exact -integer $kill_check $oid]
      if {$index>=0} {
         set kill_check [lreplace $kill_check $index $index]
      }
   }
   if {$oid<0} { break }
   set oid_rip {}  ;# Clear processed messages
}
set exitcode [llength $kill_check]

flush stdout
flush stderr
if {!$quietflag} {
   Oc_RobustPuts [format " %${oidlead}s%${oidpad}s Application" \
       OID ""]
   foreach oid $kill_list {
      set appname [lindex [dict get $names $oid] 0]
      Oc_RobustPuts -nonewline [format " <%*d> %*s " \
                                   $oidwidth $oid $appwidth $appname]
      if {[lsearch -exact -integer $kill_check $oid]<0} {
         Oc_RobustPuts $check_result
      } else {
         Oc_RobustPuts "not responding"
      }
   }
}

CloseSockets

exit $exitcode
