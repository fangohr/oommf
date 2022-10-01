# FILE: batchmaster.tcl
#   This script brings up a Tcl interpreter that launches a specified
# number of slave processes, and controls them with Tcl commands sent
# across network sockets.
#
# This script should be run using the omfsh shell.
package require Oc 2

Oc_IgnoreTermLoss  ;# Try to keep going, even if controlling terminal
## goes down.
Oc_ForceStderrDefaultMessage	;# Use stderr, not dialog to report errors

Oc_Main SetAppName batchmaster
Oc_Main SetVersion 2.0b0

Oc_CommandLine Option [Oc_CommandLine Switch] {
	{task_script {} {Task definition file}}
	{{host optional} {} {Network host for master server}}
	{{port optional} {} {Network port for master server}}
    } {
	global job_script; set job_script $task_script
	global server
	set server(host) [expr {[string length $host] ? $host : "localhost"}]
	set server(port) [expr {[string length $port] ? $port : 0}]
} {End of options; next argument is task_script}
Oc_CommandLine Parse $argv

# Name of tclsh to use to run oommf.tcl
set tclsh [Oc_Application CommandLine tclsh]
if {[string match & [lindex $tclsh end]]} {
    set tclsh [lreplace $tclsh end end]
}

# OOMMF root directory
set default_script_dir [file dirname [info script]]
set oommf_root [file join $default_script_dir .. .. ..]

# Script search path
set script_path [list . ./scripts ./data \
	$default_script_dir \
	[file join $default_script_dir scripts] \
	[file join $default_script_dir data] \
	[file join $oommf_root app mmsolve] \
	[file join $oommf_root app mmsolve scripts] \
	[file join $oommf_root app mmsolve data]]

# Class to hold information from task script: Slave launch list,
# slave init script, task scripts, and slave host list.
Oc_Class BatchTaskObj {
    # Default slave launch list is used only if the task script doesn't
    # specify any slaves via the AppendSlave method.  The first time
    # that method is called the variable launch_list_set is set true
    # and the default slave list is thrown away.
    private variable launch_list_set = 0
    private variable launch_list = {
	1 {Oc_Application Exec batchslave -tk 0 %connect_info batchsolve.tcl}
    }
    private variable slave_init_script = {}
    private variable tasks = {}
    private variable slave_hosts = localhost
    Constructor {args} {
	eval $this Configure $args
    }
    method AppendSlave { count launch_script } {
	if {!$launch_list_set} {
	    set launch_list_set 1
	    set launch_list {}
	}
	lappend launch_list $count
	lappend launch_list $launch_script
    }
    method GetSlaveList {} { return $launch_list }
    method AppendTask { task_label task_script } {
	lappend tasks $task_label
	lappend tasks $task_script
    }
    method GetTaskList {} { return $tasks }
    method GetTask { index } {
	set idstring [lindex $tasks [expr 2*$index]]
	set script   [lindex $tasks [expr 2*$index+1]]
	return [list $idstring $script]
    }
    method ModifyHostList { host } {
	# Use "host" or "+host" to append host to slave_host_list,
	# or "-host" to remove host from slave_host_list.
	set host [string tolower $host]
	if {[regexp {([+-]?)(.*)} $host dummy op host]} {
	    if {[string compare "-" $op]==0} {
		set index [lsearch -exact $slave_hosts $host]
		if {$index>=0} {
		    set slave_hosts [lreplace $slave_hosts $index $index]
		}
	    } else {
		# Add host to host list (if not already on list)
		if {[lsearch -exact $slave_hosts $host]<0} {
		    lappend slave_hosts $host
		}
	    }
	}
    }
    method GetHostList {} { return $slave_hosts }
    method SetSlaveInitScript { cmd } {
	set slave_init_script $cmd
    }
    method GetSlaveInitScript {} { return $slave_init_script }
}

# This proc if for performing % substitutions on slave launch command lines.
# The following substitutions are supported:
#      tclsh         => Tclsh shell to use to run oommf.tcl
#      connect_info  => Master-slave connection info
#      oommf_root    => OOMMF root directory on master
#      oommf         => Absolute path to oommf.tcl on master
#      oommf_rel     => Path to oommf.tcl on master, relative to oommf_root.
#      %%            => % (single percent)
# If necessary, use brace grouping to set off a substitution token, e.g.,
# if OOMMF root directory is /foo/bar, then
#      aaa%{oommf_root}bbb => aaa/foo/barbbb
#
proc DoLaunchSubstitions { cmd } {
    global oommf_root tclsh
    set sub(tclsh) $tclsh
    set oommf_app_rel_path [file join oommf.tcl]
    set sub(oommf_root) $oommf_root
    set sub(oommf) [file join $oommf_root $oommf_app_rel_path]
    set sub(oommf_rel) $oommf_app_rel_path
    set sub(connect_info) \
	    {-- $server(host) $server(port) $slaveid $passwd}
    foreach str [array names sub] {
	regsub -all -- "(\[^%\]|^)%${str}(\[^a-zA-Z0-9_\]|$)" $cmd \
		"\\1$sub($str)\\2" cmd
	regsub -all -- "(\[^%\]|^)%\{${str}\}" $cmd "\\1$sub($str)" cmd
	# The second substitution allows one to mark the end of the
	# % substitution token by brace grouping, e.g., %{oommf}.
	# Otherwise, in the first substitution the end of the token
	# is determined by a character outside the set a-zA-Z0-9_
	# (or an end of line).
    }
    regsub -all -- {%%} $cmd {%} cmd
    return $cmd
}

# 
BatchTaskObj New TaskInfo

# Server/slave connection info---
#    $TaskInfo GetSlaveList    ;# Slave launch command list
#    server(host)              ;# Master's server host
#    server(domain)            ;#        ""       network domain
#    server(port)              ;#        ""       port
#    server(channel)           ;#        ""       channel
#    $TaskInfo GetHostList     ;# Hosts connections are allowed from
#    slave(count)              ;# Total (maximum) number of slaves
#    slave(timeout)            ;# Milliseconds to wait before timing out
#                              ;#  on launch.
#    slave(slave#,channel)     ;# Here '#' is an integer id.
#    slave(slave#,passwd)      ;# Connection password
#    slave(slave#,status)      ;# Slave status: launch, init, running, dead
#    slave(channel,#)          ;# Maps channels to slaves
#
#   The slave list is a list of slave launch count + launch command
# pairs.  The launch command should use the substitution code
# %master_info, for which batchmaster.tcl will substitute
# $server(host), $server(port), $slaveid and $passwd, which is passed
# on to the slave, and used as described below.  This should be
# defined in the job_script specified on the master.tcl command line.
#
# Slave launch & connection sequence:
#    The master program goes through the slave launchlist, launching
# slaves until either it runs out of launch commands, or until the
# number of slaves equals the number of tasks.  For each launched
# process the corresponding slave(slave#,status) variable is set to
# "launch".
#   Each launch command should make use of $server(host) and
# $server(port) so the slave can connect up with the master.  After
# connecting up with the server, each slave sends the message
#
#              connect verify $slaveid $passwd
#
# where the corresponding slave integer id number and password are
# substituted.  Currently this information is passed on the command
# line along with $server(host) and $server(port).  After this the
# slave sends a
#              status ready
#
# message and waits for commands to process.
#    Inside the master slave connection is a multi-step process.
# After a connection is made 'fileevent readable' for the new channel
# is directed to the ConnectVerify proc, which expects the first
# non-blank input line to be of the form "connect verify $slaveid
# $passwd".  If slave(slave$slaveid,status) is "launch" and the
# passwords match up, then the connection is allowed, 'fileevent
# readable' for the channel is redirected to proc SlaveControl, and
# slave(slave#,status) is changed to init.  Upon the next 'ready'
# message from the slave, the slave init script is sent to the slave,
# and slave(slave#,status) is set to "running".  Future readable
# events cause tasks from the task list to be sent to the slave.#
#    Once all the launched slaves are connected, the server is
# shutdown.  When all the slaves channels are closed, the master
# itself is terminated.  The slaves should terminate themselves if
# their connection to the master is closed, and upon receipt of a
#
#              request die
# message.
#
#
# Task list info---
#    $TaskInfo GetTaskList
#    task(count)           ;# Total number of jobs
#    task(sched)           ;# Number of jobs that have been scheduled
#    task(task#,idstring)  ;# Id string from task list entry
#    task(task#,slave)     ;# Slave task assigned to
#    task(task#,status)    ;# Task status: pending or done
#    task(task#,result)    ;# Holds result if task status is "done".
#
#   The task list is a list of id_string + Tcl script pairs.  Newlines
# in the script are escaped on transmission and reception so that the
# script can be sent across the socket as a single message.  This should
# be defined in the job_script specified on the master.tcl command line.
#
# Task processing:
#   Inside proc SlaveControl, whenever a "status ready" message is
# received from a slave, the GetNextTask proc is called to retrieve a
# triple consisting of the task#, task idstring, and task script.  The
# corresponding entries are filled into the task array, and the message
#
#              request eval $taskno $idstring $script
#
# is sent back across the channel to the slave.  This message may be
# parsed on the receiving end as a 5 element list.  The slave should
# eval the script, and pass back the result with the message
#
#              reply eval $taskno $idstring $errorcode $result
#
# followed by
#              status ready
#
# and the slave waits for a new command to eval.
#
# The slave should also process the
#
#              request die
#
# message to shutdown the connection to the master.  Anytime this
# connection is closed the slave should itself shutdown.


# Process command line and initialize global variables
catch {unset slave} ; catch {unset task} ;# Safety
set task(count) 0    ; set task(sched) 0
set slave(count) 0
set slave(timeout) 30000

$TaskInfo ModifyHostList +[string tolower $server(host)]

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

# Exit handlers
proc SlaveCount {} {
    # Returns the number of currently launched or running slaves
    global slave
    set upcount 0
    for {set i 0} {$i<$slave(count)} {incr i} {
	if {[info exists slave(slave$i,status)] && \
		![string match dead $slave(slave$i,status)]} {
	    incr upcount
	}
    }
    return $upcount
}

proc KillSlave { id } {
    global slave
    if {![info exists slave(slave$id,status)]} { return -1 }
    if {![string match dead $slave(slave$id,status)]} {
	if {[info exists slave(slave$id,channel)]} {
	    set chan $slave(slave$id,channel)
	    catch {MyPuts $chan "request die"}
	    catch {close $chan}
	}
	set slave(slave$id,status) dead
	# If this was the last slave, kill master also
	if {[SlaveCount]<1} {
	    after 20 KillMaster
	}
    }
    return 0
}

set semaphore(procKillMaster) 0
proc KillMaster {} {
    global server slave task semaphore
    if {$semaphore(procKillMaster)} { return } ;# Reentrancy not allowed
    set semaphore(procKillMaster) 1
    for {set i 0} {$i<$slave(count)} {incr i} {
	KillSlave $i
    }
    if {[info exists server(channel)]} {
	catch {close $server(channel)}
    }

    # Print out results
    after 500  ;# Give slaves a chance to clean up
    flush stdout ; flush stderr
    puts stderr "\nTASKS COMPLETE\n"
    puts stderr "Results----------------------------------------"
    puts stderr "Task Slave      ID               Status  Result"
    for {set i 1} {$i<=$task(count)} {incr i} {
	if {[info exists task(task$i,status)]} {
	    puts -nonewline stderr [format "%3d  %3d     %-20s" \
		    $i $task(task$i,slave) $task(task$i,idstring)]
	    if {[string match done $task(task$i,status)]} {
		puts stderr " Done   $task(task$i,result)"
	    } else {
		puts stderr "Incomplete"
	    }
	}
    }
    puts stderr "-----------------------------------------------\n"
    flush stderr
    exit
}

# Slave control
proc SlaveControl { chan } {
    global slave task
    if {![info exists slave(channel,$chan)]} {
	error "Message on unregistered channel: $chan"
    }
    set slaveid $slave(channel,$chan)
    if {[catch {MyGets $chan line} count] || $count<0} {
	if {[eof $chan]} { KillSlave $slaveid }
	return  ;# Socket closed or input line not yet complete
    }
    if {[regexp -- "^\[ \t\]*\$" $line]} { return } ;# Skip blank lines
    if {[catch {lindex $line 0} msggrp]} {
	puts stderr "Bad input line on channel $chan: $line"
	return
    }
    if {[string match status $msggrp]} {
	# Status change
	switch -- [lindex $line 1] {
	    ready {
		# Send new command
		if {[string match init $slave(slave$slaveid,status)]} {
		    # Finish initializing slave
		    global TaskInfo
		    MyPuts $chan [list request eval 0 init-slave$slaveid\
			    [$TaskInfo GetSlaveInitScript]]
		    set slave(slave$slaveid,status) running
		} else {
		    # Send real task
		    set newtask [GetNextTask]
		    set taskno [lindex $newtask 0]
		    if {$taskno<0} {
			# No more tasks
			MyPuts $chan "request die"
			KillSlave $slaveid
		    } else {
			set idstring [lindex $newtask 1]
			set script [lindex $newtask 2]
			MyPuts $chan [list request eval\
				$taskno $idstring $script]
			set task(task$taskno,idstring) $idstring
			set task(task$taskno,slave) $slaveid
			set task(task$taskno,status) pending
			set task(task$taskno,result) {}      ;# Safety
			puts stderr "Task $idstring sent to slave $slaveid"
			flush stderr
		    }
		}
	    }
	    bye   {
		KillSlave $slaveid
	    }
	    default {
		puts stderr "Unknown slave status: [lindex $line 1]"
		flush stderr
	    }
	}
    } elseif {[string match reply $msggrp]} {
	switch -- [lindex $line 1] {
	    eval {
		# Result from previous command
		set taskno    [lindex $line 2]
		set idstring  [lindex $line 3]
		set errorcode [lindex $line 4]
		set result    [lindex $line 5]
		if {[llength $line]!=6 || [string match {} $errorcode]} {
		    puts stderr "INVALID reply eval line: $line"
		} elseif {$errorcode} {
		    puts stderr "ERROR on task $idstring: $result"
		}
		if {$taskno==0} {
		    # Slave initialization task
		    puts "Task $idstring done"
		} else {
		    # Non-initialization task
		    if {![info exists task(task$taskno,idstring)] || \
			    [string compare $idstring \
			    $task(task$taskno,idstring)]!=0} {
			puts stderr "Reply to unknown task:\
				[list $taskno $idstring]"
		    } else {
			set task(task$taskno,status) done
			set task(task$taskno,result) $result
			if {[string match {} $result]} {
			    puts stderr "Task $idstring complete."
			} else {
			    puts stderr "Task $idstring complete: $result"
			}
			flush stderr
		    }
		}
	    }
	}
    }
}

# Server connection handlers
proc ServerShutdown {} {
    global server
    if {[info exists server(channel)]} {
	catch [close $server(channel)]
	unset server(channel)
    }
}

proc SockConnect { chan } {
    # This proc is called when a new channel becomes readable
    # to a legal connection.
    global server slave
    if {[MyGets $chan line]<0} {
	if {[eof $chan]} {catch {close $chan}}
	return  ;# Socket closed or input line not yet complete
    }
    if {[regexp -- "^\[ \t\]*\$" $line]} { return } ;# Skip blank lines
    if {[catch {lindex $line 0} msggrp]} {
	puts stderr "Bad connect input line on channel $chan: $line"
	catch {close $chan}
	return
    }
    if {![string match connect $msggrp] || \
	    ![string match verify [lindex $line 1]]} {
	puts stderr "Invalid connection attempt on channel $chan: $line"
	catch {close $chan}
	return
    }
    set slaveid [lindex $line 2]
    set passwd [lindex $line 3]
    if {![info exists slave(slave$slaveid,passwd)] || \
	    [string compare $slave(slave$slaveid,passwd) $passwd]!=0} {
	puts stderr "Illegal connection attempt for slave $slaveid: $line"
	catch {close $chan}
	foreach elt [array names slave "slave$slaveid,*"] {
	    unset slave($elt)      ;# Prevent retries
	}
	return
    }
    # Otherwise the connection is valid.
    set slave(channel,$chan) $slaveid
    set slave(slave$slaveid,channel) $chan
    set slave(slave$slaveid,status) init
    fileevent $chan readable [list SlaveControl $chan]
}

proc ServerConnect { chan addr port } {
    global server TaskInfo
    # Check that connection is from allowed host
    set remote_host [string tolower [lindex [fconfigure $chan -peername] 1]]
    foreach elt [$TaskInfo GetHostList] {
	if {[string compare $elt $remote_host]==0 || \
		[string compare "${elt}.$server(domain)" $remote_host]==0} {
	    puts stderr "Accepted connection on $chan from $remote_host:$port"
	    fconfigure $chan -buffering line -blocking 0
	    fileevent $chan readable [list SockConnect $chan]
	    return
	}
    }
    puts stderr "Connection on $chan from $remote_host ($addr)\
	    on port $port denied"
    catch {close $chan}
}

# Task control
proc GetNextTask {} {
    # Default task scheduler.  Returns a 3 item list: the first elt
    # is the task number, the second is the task id string, and the
    # third is the actual Tcl script defining the task. The returned
    # task number is -1 if there are no more tasks.
    #  Task indexing starts with 1, because the index 0 is reserved
    # for use by the slave initialization tasks.

    global TaskInfo task
    if {$task(sched)>=$task(count)} {
	return [list -1 {} {}]
    }
    set nexttask [$TaskInfo GetTask $task(sched)]
    set idstring [lindex $nexttask 0]
    set script   [lindex $nexttask 1]
    incr task(sched) ;# First task index is 1
    return [list $task(sched) $idstring $script]
}


# Source job script
if {![string match {} $job_script]} {
    set load_script {}
    foreach dir $script_path {
	set attempt [file join $dir $job_script]
	if {[file readable $attempt]} {
	    set load_script $attempt
	    break
	}
    }
    if {[string match {} $load_script]} {
	Oc_Log Log "Unable to open job script\
		$job_script\n[Oc_CommandLine Usage]" panic
	exit 1
    }
    source $load_script
    set task(count) [expr {[llength [$TaskInfo GetTaskList]]/2}]
}

# Bring up connection server
if {[catch {socket -server ServerConnect -myaddr $server(host) $server(port)} \
	server(channel)]} {
    puts stderr "ERROR: Unable to open slave connect service on \
	    $server(host):$server(port)"
    puts stderr "ERROR: $server(channel)"
    unset server(channel)
    KillMaster
}
set temp [fconfigure $server(channel) -sockname]
set hostname [string tolower [lindex $temp 1]]
set server(port) [lindex $temp 2]
set server(domain) {}
regexp -- {[^.]+\.(.*)} $hostname dummy server(domain)
puts stderr "Slave connect service brought up on $server(host):$server(port)"
fconfigure $server(channel) -blocking 0 -buffering line
$TaskInfo ModifyHostList +$hostname

# Launch slaves
set slaveid 0
set slave(count) $task(count)   ;# Don't launch more slaves than tasks
foreach {count cmd} [$TaskInfo GetSlaveList] {
    if {$slaveid>=$slave(count)} { break}
    # Do % substitutions
    set cmd [DoLaunchSubstitions $cmd]
    for {set j 0} {$j<$count} {incr j} {
	set passwd [clock clicks]
	if {[catch {eval $cmd &} msg]} {
	    puts stderr "Error launching slave $slaveid: $cmd"
	    puts stderr "Error message: $msg"
	} else {
	    set slave(slave$slaveid,passwd) $passwd
	    set slave(slave$slaveid,status) launch
	}
	incr slaveid
	if {$slaveid>=$slave(count)} { break}
    }
}
set slave(count) $slaveid ;# Number of slaves actually launched.
if {[SlaveCount]<1} {KillMaster}

proc LaunchTest {} {
    global slave
    set goodlaunch 0
    for {set i 0} {$i<$slave(count)} {incr i} {
	if {[string match launch $slave(slave$i,status)]} {
	    puts stderr "ERROR: Timeout on slave $i launch"
	} else {
	    incr goodlaunch
	}
    }
    if {$goodlaunch<1} {
	after 20 KillMaster    ;# Give up
    }
}

after $slave(timeout) LaunchTest

vwait forever
