# FILE: pidinfo.tcl
#
# This file must be evaluated by omfsh

package require Oc
package require Net

########################################################################
# Support procs

# NB: For the purposes of this code, the host and account servers are
#     assigned the dummy OIDs -2 and -1, respectively. The PidReport
#     proc replaces the dummy OIDs with hyphens on output.

# Default initializer of the accountname.  Tries to determine account name
# under which current thread is running.  This code is uses the
# DefaultAccountName proc in the Net_Account class.
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
#puts stderr "QUERY: -->$args<--" ; flush stderr ;###
   puts $chan [concat query $querycount $args]
   while {$timeout_count>0} {
      if {[catch {gets $chan response} count]} {
         # Error reading socket
         return -code error $count
      }
#puts stderr "RESPONSE: -->$response<--" ; flush stderr ;###
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

proc PidReport { pidreply portsreply nicknames } {
    global header
    set mypid [pid]
    if {$header} {
        set oidlength [string length "OID"]
        set pidlength [string length "PID"]
        set namelength [string length "Application"]
    } else {
        set oidlength 0
        set pidlength 0
        set namelength 0
    }
    foreach elt $pidreply {
        foreach {oid pid appname} $elt { break }
        if {$pid != $mypid} {
            set pidarr($oid) [list $pid $appname]
            set prtoid [expr {$oid<0 ? "-" : $oid}]
            if {[string length $prtoid]>$oidlength} {
                set oidlength [string length $prtoid]
            }
            if {[string length $pid]>$pidlength} {
                set pidlength [string length $pid]
            }
            if {[string length $appname]>$namelength} {
                set namelength [string length $appname]
            }
        }
    }
   set doports [llength $portsreply]
   set donicknames [llength $nicknames]
   if {!$doports && !$donicknames} {
        if {$header} {
            Oc_RobustPuts [format "%*s %*s %s" $oidlength OID \
                      $pidlength PID Application]
        }
        foreach oid [lsort -integer [array names pidarr]] {
            foreach {pid appname} $pidarr($oid) { break }
            set prtoid [expr {$oid<0 ? "-" : $oid}]
            Oc_RobustPuts [format "%*s %*s %s" $oidlength $prtoid \
                     $pidlength $pid $appname]
        }
   } else {
      set portlength 0
      set maxportcount 0
      foreach elt $portsreply {
         if {[regexp {^(-?[0-9]+):[0-9]+$} [lindex $elt 1] dummy oid]} {
            set port [lindex $elt 2]
            lappend portarr($oid) $port
            if {[string length $port]>$portlength} {
               set portlength [string length $port]
            }
            if {[llength $portarr($oid)]>$maxportcount} {
               set maxportcount [llength $portarr($oid)]
            }
         }
      }
      if {$header} {
         Oc_RobustPuts -nonewline [format "%*s %*s %*s " \
                  $oidlength OID $pidlength PID $namelength Application]
         if {$doports} {
            Oc_RobustPuts -nonewline [format "%s" Ports]
            if {$donicknames} {
               set spaces [expr {$maxportcount*($portlength+1)}]
               if { $spaces < 6 } { set spaces 6 }
               # Note: Actual space count should be as above
               #  minus [string length "Ports"]
               #  plus  [string length "Names"]
               # but "Ports" and "Names" are both 5 chars long
               Oc_RobustPuts -nonewline [format "%*s" $spaces Names]

            }
            Oc_RobustPuts {}
         } else {
            # Do nicknames, but not ports
            Oc_RobustPuts [format "%s" Names]
         }
      }
      array set namearr $nicknames
      foreach oid [lsort -integer [array names pidarr]] {
         foreach {pid appname} $pidarr($oid) { break }
         set prtoid [expr {$oid<0 ? "-" : $oid}]
         Oc_RobustPuts -nonewline [format "%*s %*s %*s" \
                $oidlength $prtoid $pidlength $pid $namelength $appname]
         set portcount 0
         if {[info exists portarr($oid)]} {
            foreach port [lsort -integer $portarr($oid)] {
               Oc_RobustPuts -nonewline [format " %*s" $portlength $port]
            }
            set portcount [llength $portarr($oid)]
         }
         if {[info exists namearr($oid)]} {
            set ns $namearr($oid)
            # Remove default name from list.
            set defaultname [string tolower "${appname}:$oid"]
            if {[set index [lsearch -exact [string tolower $ns] \
                               $defaultname]]>=0} {
               set ns [lreplace $ns $index $index]
            }
            # Include spacing for ports info (if any)
            set portspace [expr {($maxportcount-$portcount)*($portlength+1)}]
            Oc_RobustPuts -nonewline [format "%*s %s" $portspace {} $ns]
         }
         Oc_RobustPuts {}
      }
   }
}
########################################################################

Oc_ForceStderrDefaultMessage
catch {wm withdraw .}

Oc_Main SetAppName pidinfo
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

Oc_CommandLine Option ports {
} {
   global showports; set showports 1
} {Display application server ports}
set showports 0

Oc_CommandLine Option names {
} {
   global shownames ; set shownames 1
} {Display application nicknames}
set shownames 0

Oc_CommandLine Option noheader {
} {
   global header; set header 0
} {Don't print column header}
set header 1

Oc_CommandLine Option pid {
} {
   global pidselect; set pidselect 1
} {Select by pid rather than oid}
set pidselect 0

Oc_CommandLine Option v {
} {
   global verbose; set verbose 1
} {Print extra info}
set verbose 0

Oc_CommandLine Option timeout {
   {secs {regexp {^[0-9]+$} $secs} {is timeout in seconds (default is 5)}}
} {
   global timeout;  set timeout [expr {$secs*1000}]
} {Maximum time to wait for response from servers}
set timeout 5000

Oc_CommandLine Option wait {
   {secs {regexp {^[0-9]+$} $secs} {is maximum wait time in seconds (default is 0)}}
} {
   global waittime;  set waittime $secs
} {If no match found, keep retrying for this long}
set waittime 0
set retrytime 250 ;# Retry period, in milliseconds

Oc_CommandLine Option [Oc_CommandLine Switch] {
   {{oid list} {} {is "all" (default) or one or more oids}}
} {
   global oidlist; if {[llength $oid]} {set oidlist $oid}
} {End of options; next argument is oid}
set oidlist "all"

Oc_CommandLine Parse $argv

# Only accounts on localhost currently supported
set host localhost

set stoptime [expr {[clock seconds]+$waittime}]

while {[catch {socket $host $hostport} hostchan]} {
   if {[clock seconds]>=$stoptime} {
      Oc_RobustPuts "No OOMMF applications detected."
      Oc_RobustPuts \
         "(No host server responding on host $host, port $hostport.)"
      exit
   }
   after $retrytime
}


if {$verbose || $pidselect} {
   if {[catch {AskServer $hostchan serverpid} hostpid]} {
      set hostpid {} ;# Error
   }
}
if {$verbose} {
   if {[string match {} $hostpid]} {
      puts stderr "Host server PID lookup failure: $hostpid"
      catch {close $hostchan}
      exit 1
   }
   Oc_RobustPuts "-----------------------"
   Oc_RobustPuts "        Host: $host"
   Oc_RobustPuts "    Host PID: $hostpid"
   if {$showports} { Oc_RobustPuts "   Host port: $hostport" }
}

while {[catch {AskServer $hostchan lookup $acctname} acctport]} {
   if {[clock seconds]>=$stoptime} {
      puts stderr "Account lookup failure: $acctport"
      close $hostchan
      exit 1
   }
   after $retrytime
}
close $hostchan

if {$verbose} {
   Oc_RobustPuts "     Account: $acctname"
}

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

if {$verbose || $pidselect} {
   if {[catch {AskServer $acctchan serverpid} acctpid]} {
      set acctpid {} ;# Error
   }
}
if {$verbose} {
   if {[string match {} $acctpid]} {
      puts stderr "Account server PID lookup failure: $acctpid"
      catch {close $acctchan}
      exit 1
   }
   Oc_RobustPuts " Account PID: $acctpid"
   if {$showports} { Oc_RobustPuts "Account port: $acctport" }
   Oc_RobustPuts "-----------------------"
}

if {$pidselect || [string match "all" $oidlist]} {
   set oidpat *
} else {
   set oidpat $oidlist
}

while {1} {
   if {[catch {AskServer $acctchan getpids $oidpat -appname} pidreply]} {
      if {[clock seconds]>=$stoptime} {
         puts stderr "PID lookup failure: $pidreply"
         close $acctchan
         exit 1
      }
      set pidreply {}
   } else {
      if {$pidselect} {
         # Include host and account server on pid list with fictitious
         # (negative) OIDs. The PidReport proc replaces any negative OID
         # with a hyphen.
         lappend pidreply [list -2 $hostpid Host] \
                          [list -1 $acctpid Account]
      }
      if {$pidselect && ![string match "all" $oidlist]} {
         catch {unset pidarr}
         foreach pat $oidlist {
            foreach elt $pidreply {
               set pid [lindex $elt 1]
               if {[string match $pat $pid]} {
                  set pidarr($pid) $elt
               }
            }
         }
         set pidreply {}
         foreach elt [lsort -integer [array names pidarr]] {
            lappend pidreply $pidarr($elt)
         }
         catch {unset pidarr}
      }
   }

   if {[llength $pidreply]>0} {
      break
   }

   if {[clock seconds]>=$stoptime} {
      puts stderr "PID lookup failure: No matches"
      close $acctchan
      exit 1
   }

   after $retrytime
}

set portsreply {}
if {$showports} {
    if {[catch {AskServer $acctchan threads} portsreply]} {
        puts stderr "Ports lookup failure: $portsreply"
        close $acctchan
        exit 1
    }
    # Include host and account servers on portsreply list with dummy
    # OIDs of -2 and -1 respectively.
    lappend portsreply \
       [list Host    -2:0 $hostport "OOMMF host protocol x.y"] \
       [list Account -1:0 $acctport "OOMMF account protocol x.y"]
    # Note: Upon making a connection, Net_Protocol servers report their
    # protocol name and version via a 'tell' message. The current
    # connection handling code in this program drops that message, so we
    # don't have the version number to insert here. But we aren't using
    # the version number, so just insert a placeholder instead.
}
set nicknames {}
if {$shownames} {
    if {[catch {AskServer $acctchan names} nicknames]} {
        puts stderr "Names lookup failure: $nicknames"
        close $acctchan
        exit 1
    }
}
close $acctchan

PidReport $pidreply $portsreply $nicknames

exit
