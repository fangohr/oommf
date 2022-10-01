# FILE: killoommf.tcl
#
# This file must be evaluated by omfsh

# TODO: This app operates by sending an "exit" message to Net_Server
#       objects. Applications such as mmLaunch that don't have server
#       components can't be closed this way. Instead, killoommf should
#       be changed to use the account server "kill" message which can
#       terminate any application connected to the account
#       server. This is the method used by the current mmLaunch
#       iterate.

########################################################################
# Support procs

# Default initializer of the accountname.  Tries to determine account name
# under which current thread is running.  This code is uses the
# DefaultAccountName proc in the Net_Account class.
package require Net
proc DefaultAccountName {} {
   return [Net_Account DefaultAccountName]
}

set querycount -1
proc AskServer {chan args} {
   global querycount timeout
   set timeout_incr 100
   set timeout_count [expr {int(ceil(double($timeout)/$timeout_incr))}]
   incr querycount
   # Configure socket.  This is inefficient if we are making many
   # calls to AskServer, but this routine is intended to only be
   # called once or twice on a channel, so that shouldn't matter.
   fconfigure $chan -blocking 0 -buffering line -translation {auto crlf}
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

proc KillApp { port } {
}

########################################################################

package require Oc

Oc_ForceStderrDefaultMessage
catch {wm withdraw .}

Oc_Main SetAppName killoommf
Oc_Main SetVersion 2.0b0

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
      global oidlist; if {[llength $oid]} {set oidlist $oid}
} {End of options; next argument is oid}
set oidlist {}

Oc_CommandLine Parse $argv

if {$shownamesonly} {
   # The shownamesonly flag overrides showonly
   set showonly 0
}

if {[llength $oidlist]==0} {
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
if {[catch {AskServer $hostchan lookup $acctname} acctport]} {
    puts stderr "Account lookup failure: $acctport"
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
    puts stderr "ERROR: Unable to connect to account server\
                 on host $host, port $acctport"
    exit 1
}

if {$do_id_check && [Net_CheckSocketAccess $acctchan]==0} {
   puts stderr "WARNING: Account server owner is not \"$acctname\""
}

if {[catch {AskServer $acctchan threads} portsreply]} {
    puts stderr "Ports lookup failure: $portsreply"
    close $acctchan
    exit 1
}

foreach entry $portsreply {
    foreach {appname server port} $entry break
    set appname [string tolower $appname]
    regexp {^([0-9]+):([0-9]+)$} $server dummy oid servnumber
    if {![info exists oidport($oid)]} {
	set oidport($oid) $port
	set oidapp($oid) $appname
	lappend appoid($appname) $oid
    }
}

if {[string match all $oidlist]} {
   set oidlist [lsort -integer [array names oidport]]
} else {
   if {$pidselect} {
      # Input oidlist is really a pidlist.  Build convert table.
      if {[catch {AskServer $acctchan getpids *} portsreply]} {
         puts stderr "PID lookup failure: $portsreply"
         close $acctchan
         exit 1
      }
      foreach elt $portsreply {
         set pidoid_table([lindex $elt 1]) [lindex $elt 0]
      }
   }

   set newlist {}
   foreach elt $oidlist {
      if {[regexp {^[0-9]+$} $elt]} {
         # Numeric selection
         if {$pidselect} {
            if {![info exists pidoid_table($elt)]} {
               puts stderr "Application with pid $elt does not\
                            exist or is not registered; skipping."
               continue
            }
            set pid $elt ;# For error handling
            set elt $pidoid_table($elt)
         }
         # Selection by oid
         if {[info exists oidport($elt)]} {
            lappend newlist $elt
         } else {
            if {$pidselect} {
               puts stderr "Application with pid $pid (oid=$elt)\
                            has no servers; skipping."
            } else {
               puts stderr "Application with oid $elt either\
                   does not exist or has no servers; skipping."
            }
         }
      } else {
         # Selection by name
         if {0} {
            set elt [string tolower $elt]
            if {[info exists appoid($elt)]} {
               set newlist [concat $newlist \
                               [lsort -integer $appoid($elt)]]
            } else {
               puts stderr "Either no $elt application running,\
                   or application $elt has no servers; skipping."
            }
         } else {
            if {[string first ":" $elt] == -1} {
               append elt ":*"
            }
            if {[catch {AskServer $acctchan find $elt} matches]} {
               puts stderr "Account server find error: $matches"
               close $acctchan
               exit 1
            }
            set matches [lindex [lindex $matches 0] 0]
            if {[llength $matches]>0} {
               set newlist [concat $newlist $matches]
            }
         }
      }
   }
   set oidlist $newlist
}

if {[llength $oidlist]==0} {
    Oc_RobustPuts "No applications match oid list"
    close $acctchan
    exit
}

# Remove duplicates, and any entries not in oidport array.
# The latter can happen in particular if the account server
# "find" query returns applications such as mmLaunch that
# don't have a server.
set newlist {}
foreach elt $oidlist {
    if {[info exists oidport($elt)] && \
	    [lsearch -exact $newlist $elt] == -1} {
	lappend newlist $elt
    }
}
set oidlist $newlist

set oidstrsize 1
set appstrsize 1
foreach oid $oidlist {
    set tsize [string length $oid]
    if {$tsize>$oidstrsize} { set oidstrsize $tsize }
    set tsize [string length $oidapp($oid)]
    if {$tsize>$appstrsize} { set appstrsize $tsize }
}

if {$showonly} {
    Oc_RobustPuts "Application selection (no exit requests sent):"
    foreach oid $oidlist {
	Oc_RobustPuts [format " <%*d> %*s" \
		  $oidstrsize $oid $appstrsize $oidapp($oid)]
    }
    close $acctchan
    exit
}

if {$shownamesonly} {
   Oc_RobustPuts "Application selection (no exit requests sent):"
   set appstrsize [expr {$appstrsize+1+$oidstrsize}]
   foreach oid $oidlist {
      if {[catch {AskServer $acctchan names $oid} nicknames]} {
         puts stderr "Account server names error: $nicknames"
         close $acctchan
         exit 1
      }
      set nicknames [lindex $nicknames 1]  ;# First element is oid
      # Pull default name from list for separate handling.
      set appname $oidapp($oid)
      set defaultname [string tolower "${appname}:$oid"]
      if {[set index [lsearch -exact [string tolower $nicknames] \
                         $defaultname]]>=0} {
         set nicknames [lreplace $nicknames $index $index]
         set rightspace [expr {$oidstrsize - [string length $oid]}]
         if {$rightspace>0} {
            set defaultname [format "%s%*s" $defaultname $rightspace {}]
         }
      } else {
         set defaultname {}
      }

      Oc_RobustPuts [format " <%*d> %*s %s" \
               $oidstrsize $oid $appstrsize $defaultname $nicknames]
   }
   close $acctchan
   exit
}

close $acctchan

foreach oid $oidlist {
    flush stdout
    flush stderr
    if {[catch {socket $host $oidport($oid)} chan]} {
	puts stderr "ERROR: Unable to connect to server\
                 for $oidapp($oid) <$oid>, port $oidport($oid): $chan"
	continue
    }

    if {$do_id_check && [Net_CheckSocketAccess $chan]==0} {
       puts stderr "WARNING: $oidapp($oid) <$oid> owner is not \"$acctname\""
    }

    if {!$testflag} {
	if {[catch {AskServer $chan exit} exitreply]} {
	    Oc_RobustPuts [format " <%*d> %*s not responding: $exitreply" \
		      $oidstrsize $oid $appstrsize $oidapp($oid)]
	    close $chan
	    continue
	}
	close $chan
	if {!$quietflag} {
	    Oc_RobustPuts [format " <%*d> %*s killed" \
		      $oidstrsize $oid $appstrsize $oidapp($oid)]
	}
    } else {
	if {[catch {AskServer $chan bye} byereply]} {
	    Oc_RobustPuts [format " <%*d> %*s not responding: $byereply" \
		      $oidstrsize $oid $appstrsize $oidapp($oid)]
	    close $chan
	    continue
	}
	close $chan
	if {!$quietflag} {
	    Oc_RobustPuts [format " <%*d> %*s is alive" \
		      $oidstrsize $oid $appstrsize $oidapp($oid)]
	}
    }
}

exit
