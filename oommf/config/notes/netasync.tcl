# FILE: netasync.tcl
#
# This script is used to check Tcl's asynchronous socket
# initialization.  Usage
#
#   tclsh netasync.tcl [port]
#
# where the optional "port" argument is an unused port
# number; if not specified then port 15137 will be used.
# There are four possible results:
#
#  OK: connection refused
#  OK: connection accepted
#  Connection initialization timed out
#  ERROR: connection error without writable event
#
# The first is the expected output, assuming the specified
# port is unused.  If, on the contrary, the port is in use
# then the second response may occur.  The third result
# indicates a slow networking system, and is inconclusive.
# The fourth indicates a malfunctioning Tcl socket command.

set default_port 15137
set timeout 15000  ;# 15 seconds

proc Usage {} {
   global default_port
   puts stderr {Usage: tclsh netasync.tcl [port]}
   puts stderr " where the optional port value is any\
                unused port.  Default is $default_port"
   exit 99
}

if {[llength $argv]==0} {
   set port $default_port
} elseif {[llength $argv]==1 && [regexp {^[0-9]+$} [lindex $argv 0]]} {
   set port [lindex $argv 0]
} else {
   Usage
}

proc WritableEvent { socket } {
   fileevent $socket writable {}
   if {[catch {fconfigure $socket -error} msg]} {
      puts stderr "Tcl too old? fconfigure error: $msg"
      exit 9
   }
   if {[string match {} $msg]} {
      puts "OK: connection accepted"
      exit 0
   } elseif {[string compare "connection refused" $msg]==0} {
      puts "OK: $msg"
      exit 0
   }
   puts stderr "Unrecognized error: \"$msg\""
   exit 9
}

proc TimeoutEvent { socket } {
   if {[catch {fconfigure $socket -error} msg]} {
      puts stderr "Tcl too old? fconfigure error: $msg"
      exit 9
   }
   if {[string match {} $msg]} {
      puts "Connection initialization timed out"
      exit 1
   }
   puts "ERROR: connection error without writable event"
   puts "Connection error message: \"$msg\""
   exit 2
}

if {[catch {socket -async localhost $port} msg]} {
   puts stderr "Socket command error: $msg"
   exit 9
}
set socket $msg
fileevent $socket writable [list WritableEvent $socket]

after $timeout [list TimeoutEvent $socket]

vwait forever
