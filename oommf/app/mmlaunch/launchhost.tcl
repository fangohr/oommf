# FILE: launchhost.tcl
#
# Command line program to launch host server on a specified port.  On
# success, prints the host server port to stdout.  In particular, if the
# requested port number is 0, then a random available port is chosen,
# and reported back to stdout.  So one can do (from Bourne shell)
#
#   export OOMMF_HOSTPORT=`tclsh oommf.tcl launchhost 0`
#
# to setup the OOMMF_HOSTPORT environment variable to point to the
# selected port.
#
# Important note: This program launches the host server but does *not*
# connect to it.  This program also does not launch an account server.
# The host server will automatically shut down when the last connection
# to it goes away.  So, for example, if you run this program to launch
# the host server, and then run 'tclsh oommf.tcl pidinfo', what will
# happen is that pidinfo will report failure to connect to an account
# server, and then the host server itself will exit when pidinfo 
# disconnects from it.
#
# This file must be evaluated by omfsh.

########################################################################

package require Oc

Oc_ForceStderrDefaultMessage
catch {wm withdraw .}

Oc_Main SetAppName launchhost
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

Oc_CommandLine Option timeout {
  {secs {regexp {^[0-9]+$} $secs} {is timeout in seconds (default is 5)}}
} {
  global timeout;  set timeout [expr {$secs*1000}]
} {Maximum time to wait for response from servers}
set timeout 5000

#Oc_Option Get Net_Host port hostport  ;# Default setting
Oc_CommandLine Option [Oc_CommandLine Switch] {
   {port {regexp {^[0-9]+$} $port} {is port for host server to listen on}}
} {
   global hostport; set hostport $port
} {End of options; next argument is port}

Oc_CommandLine Parse $argv

proc PingTimeout {} {
   puts stderr "FATAL ERROR:\
          Timed out waiting for OOMMF host server to start on localhost"
   flush stderr
   exit 2
}

proc ReadPingChannel { chan } {
   set count [gets $chan line]
   if {$count==0} { return }
   if {$count<0} {
      puts stderr "Premature EOF on host ping channel"
      flush stderr
      exit 3
   }
   if {[llength $line]!=2
       || ![string match OOMMF_HOSTPORT [lindex $line 0]]
       || ![regexp {^[0-9]+$} [lindex $line 1]]} {
      puts stderr "Invalid response on host ping channel"
      flush stderr
      exit 4
   }
   puts [lindex $line 1]  ;# Write host port to stdout
   close $chan
   global forever
   set forever 1
}

proc PingServer {chan clientaddr clientport} {
   fconfigure $chan -blocking 0 -buffering line 
   fileevent $chan readable [list ReadPingChannel $chan]
}

# Only accounts on localhost currently supported
set hostaddr localhost

set hostprog [file join [Oc_Main GetOOMMFRootDir] \
                 pkg net threads host.tcl]

if {[file readable $hostprog]} {
   # Set up to receive ping from host thread or timeout
   set listensocket [socket -server PingServer -myaddr $hostaddr 0]
   set listenport [lindex [fconfigure $listensocket -sockname] 2]
   set timeoutevent [after $timeout PingTimeout]

   # Redirect host server stdout to /dev/null, to insure that the
   # only output to stdout (if any) is the port number as reported
   # back on the ping channel.  Allow stderr to flow through,
   # however.
   Oc_Application Exec {omfsh 2} \
      $hostprog -tk 0 $hostport $listenport \
      > [[Oc_Config RunPlatform] GetValue path_device_null] &
} else {
   error "Installation error:\
          No OOMMF host server (\"$hostprog\") available to start"
}

vwait forever
