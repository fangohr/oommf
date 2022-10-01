# FILE: batchslave.tcl
#   This script brings up a Tcl interpreter that hooks up to a network
# socket and then processes Tcl commands sent over that socket.  The
# return value of each command is sent back across the same socket when
# the command is complete.
#
# Note: If batchsolve.tcl is to be used with this script, then this
#  script should be launched with mmsolve instead of omfsh.
package require Oc 2

Oc_IgnoreTermLoss  ;# Try to keep going, even if controlling terminal
## goes down.

Oc_ForceStderrDefaultMessage	;# Error messages to stderr, not dialog

Oc_Main SetAppName batchslave
Oc_Main SetVersion 2.0b0

Oc_CommandLine Option [Oc_CommandLine Switch] {
	{host {} {Hostname to connect to master}}
	{port {} {Port to connect to master}}
	{id {} {ID of this slave assigned by master}}
	{password {} {password to use with master}}
	{{script optional} {} {Script file slave should evaluate}}
	{{arg list} {} {Arguments to pass to script file}}
    } {
	global sock login aux_script argv
	# Hook up to master
	if {[catch {socket $host $port} sock]} {
	    return -code error "Error opening socket to $host:$port: $sock"
	}
	set login "connect verify $id $password"
	set aux_script $script
	set argv $arg
} {End of options; next argument is host}

Oc_CommandLine Parse $argv

set base_dir [info script]
set root_oommf [file join [file dirname $base_dir] .. .. ..]
set script_path [list . ./scripts ./data \
	$base_dir \
	[file join $base_dir scripts] \
	[file join $base_dir data] \
	[file join $root_oommf app mmsolve] \
	[file join $root_oommf app mmsolve scripts] \
	[file join $root_oommf app mmsolve data]]

# Global variables
global sock			;# Socket to master
global login			;# login message to master

# Message procs.  These escape newlines.
proc MyPuts { chan msg } {
    regsub -all -- "%" $msg "%25" msg      ;# Escape %'s
    regsub -all -- "\n" $msg "%10" msg     ;# Escape newlines
    puts $chan $msg
}
proc MyGets { chan msgname } {
    upvar $msgname msg
    set count [gets $chan msg]
    if {$count<0} {
	return $count    ;# Error or line not ready
    }
    regsub -all -- "%10" $msg "\n" msg     ;# Un-escape newlines
    regsub -all -- "%25" $msg "%" msg      ;# Un-escape %'s
    return [string length $msg]
}

# Exit handler
proc Die {} {
    global sock
    puts stderr "Slave dying on \
	    $sock=[lrange [fconfigure $sock -sockname] 1 end]"
    catch {BatchTaskExit}
    catch {MyPuts $sock "status bye"}
    catch {close $sock}
    exit
}

# This routine gets called to eval a command coming from
# the master.  The socket should be in non-blocking mode.
# The command is a 5 item list, of the form
#
#              request eval $taskno $idstring $script
#
# After task completion, the taskno, idstring, errorcode and result
# are sent back across the socket, via a
#
#              reply eval $taskno $idstring $errorcode $result
#
# message, followed by
#
#              status ready
#
# to indicate readiness to accept a new command.
proc ProcessCommand {} {
    global sock
    if {[catch {MyGets $sock line} count] || $count<0} {
	if {[eof $sock]} { Die } ;# Connection closed
	return  ;# Otherwise, input line not yet complete
    }
    if {[regexp -- "^\[ \t\]*\$" $line]} { return } ;# Ignore blank lines
    if {[catch {lindex $line 0} msggrp]} {
	puts stderr "Bad input line on channel $sock: $line"
	return
    }
    if {![string match "request" $msggrp]} {
	puts stderr "Unexpected input on channel $sock: $line"
	return
    }
    set msgcmd [lindex $line 1]
    if {[string match "die" $msgcmd] || [string match "bye" $msgcmd]} {
	Die
    }
    if {![string match "eval" $msgcmd]} {
	puts stderr "Unexpected input on channel $sock: $line"
	return
    }
    set taskno [lindex $line 2]
    set idstring [lindex $line 3]
    set cmd [lindex $line 4]
    set errorcode [catch {uplevel #0 $cmd} result]  ;# Eval at global scope
    MyPuts $sock [list reply eval $taskno $idstring $errorcode $result]
    MyPuts $sock "status ready"
}

# start connection to master
fconfigure $sock -buffering line -blocking 0
fileevent $sock readable ProcessCommand
MyPuts $sock $login

# Source in auxiliary script, if any
foreach dir $script_path {
    set attempt [file join $dir $aux_script]
    if {[file readable $attempt]} {
	set aux_script $attempt
	break
    }
}
if {[catch {source $aux_script} msg]} {
    puts stderr "Error sourcing $aux_script: $msg"
}

# Inform master we are ready to begin processing, and go into event loop
MyPuts $sock "status ready"

vwait forever
