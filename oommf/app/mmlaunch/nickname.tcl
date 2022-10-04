# FILE: nickname.tcl
#
# This file must be evaluated by omfsh

package require Oc
package require Net

########################################################################
# Support procs

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

########################################################################

Oc_ForceStderrDefaultMessage
catch {wm withdraw .}

Oc_Main SetAppName nickname
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

Oc_CommandLine Option pid {
} {
    global pidselect; set pidselect 1
} {Select by pid rather than oid}
set pidselect 0

Oc_CommandLine Option timeout {
  {secs {regexp {^[0-9]+$} $secs} {is timeout in seconds (default is 5)}}
} {
  global timeout;  set timeout [expr {$secs*1000}]
} {Maximum time to wait for response from servers}
set timeout 5000

set oidrequest {}
Oc_CommandLine Option [Oc_CommandLine Switch] {
   {oid {regexp {^[0-9]+$} $oid} {OOMMF ID}}
   {name {regexp {[^0-9]} $name}
      {requested nickname (must contain a non-numeric character)}}
   {{name2 list} {expr {[lsearch -regexp $name2 {^[0-9]+$}]==-1}}
      {additional nicknames}}
    } {
       global oidrequest; set oidrequest $oid
       global namelist; set namelist [concat $name $name2]
} {End of options; next argument is oid}

Oc_CommandLine Parse $argv

# Only accounts on localhost currently supported
set host localhost

while {[catch {socket $host $hostport} hostchan]} {
   puts stderr \
      "ERROR: No host server responding on host $host, port $hostport."
   exit 1
}

while {[catch {AskServer $hostchan lookup $acctname} acctport]} {
   puts stderr "ERROR: Account lookup failure: $acctport"
   close $hostchan
   exit 1
}
close $hostchan

if {[catch {socket $host $acctport} acctchan]} {
    puts stderr "ERROR: Unable to connect to account server\
                 on host $host, port $acctport"
    exit 1
}

if {$pidselect} {
   if {[catch {AskServer $acctchan getoid $oidrequest} pidreply]} {
      puts stderr "PID lookup failure: $pidreply"
      close $acctchan
      exit 1
   }
   set oid $pidreply
} else {
   set oid $oidrequest
}

if {[catch {AskServer $acctchan getpids $oid -appname} appreply]} {
   puts stderr "Application name lookup failure: $appreply"
   close $acctchan
   exit 1
}
set appname [lindex [lindex $appreply 0] 2]

set errorcount 0
while {[llength $namelist]>0} {
   set tryname [lindex $namelist 0]
   if {[regexp {^[0-9]+$} $tryname]} {
      puts stderr "Invalid nickname: \"$tryname\""
      puts stderr "Nicknames must contain a non-numeric character"
      incr errorcount
   } else {
      set tryname "${appname}:${tryname}"
      set namelist [lrange $namelist 1 end]
      if {[catch {AskServer $acctchan associate $tryname $oid} reply]} {
         puts stderr "Associate failure for name \"$tryname\": $reply"
         incr errorcount
      }
   }
}

close $acctchan

if {$errorcount>0} {
   exit [expr {100+$errorcount}]
}

exit 0
