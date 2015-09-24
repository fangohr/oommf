# CheckSocketAccess takes server port and client port as imports, and
# returns 2 if both ends are running as the same user, 0 if the ends
# are running as different users, and 1 if the client user can't be
# determined.  The server and client address must be in numeric form,
# e.g., 127.0.0.1.  The addresses and ports are distinguished as
# "local" versus "foreign".  One should be the server side of a
# connection, the other the client side.  The local side should
# be held by the current process, but it may be either server
# or client.  Thus, both server and client sides of the connection
# can validate the connection.
#   This code is very much OS dependent, so the default implementation
# just returns 1.  The default implementation is redefined if the
# following OS-dependent script can find a valid method of discovering
# the client user.
#   NOTE 1: On some systems (Windows is one), this code may not be
# able to identify the owner (user) of the foreign end, but only
# tell if that owner is different from the current process user.
# This should be sufficient for present needs.
#   NOTE 2: This code only works if both ends of the connection are on
# the same (local) machine.

# Top-level use of this function is controlled by the
#    Net_Server checkUserIdentities
#    Net_Link   checkUserIdentities
# options in oommf/config/options.tcl.  Set to 0 to disable
# checking, 2 to fully enable, and 1 to fail only if the owner
# of the foreign end of the socket is positively identified to
# be different than owner of local end.

proc Net_CheckSocketAccess { sock } {
   set localinfo [fconfigure $sock -sockname]
   set localaddr [lindex $localinfo 0]
   set localport [lindex $localinfo 2]
   if {[catch {fconfigure $sock -peername} remoteinfo]} {
      error "Error in Net_CheckSocketAccess: $remoteinfo\n\
              Net_CheckSocketAccess should only be called with\
              client or accepted sockets."
   }
   set remoteaddr [lindex $remoteinfo 0]
   set remoteport [lindex $remoteinfo 2]
   set clearance [Net_CheckSocketPairAccess $localaddr $localport \
                     $remoteaddr $remoteport]
   return $clearance
}

# Default implementation
proc Net_CheckSocketPairAccess { local_addr local_port \
                                foreign_addr foreign_port } {
   return 1
}

package require Oc
package require Nb

proc Net_CheckSocketAccessInit {} {
   # Wrap proc generation in a proc in order to protect
   # variable namespace.  This init proc is called at
   # the end of this file.

   global tcl_platform

   if {[string match windows $tcl_platform(platform)]} {
      # Windows machine.  Might want to check os and osVersion
      # entries of tcl_platform, but for now just rely on test
      # runs of introspection code.

      # Do we have a version of netstat that supports the -o (owner)
      # switch?  (Rumor has it that the -o switch is not supported on
      # Win2K.  However, there is a program called tcpvcon, which is part
      # of the TcpView utility --- distributed originally by
      # sysinternals.com, but now distributed by Microsoft since their
      # acquisition of sysinternals.com --- that provides this
      # functionality.  The docs say tcpvcon runs on Win2K.)

      if {[catch {exec netstat -p tcp -o -n \
            2> [[Oc_Config RunPlatform] GetValue path_device_null]} result]} {
         return  ;# Either non-existent or bad netstat
      }
      set netstat_output [split $result "\n"]
      set header_index [lsearch -regexp $netstat_output \
                    "^ *Proto *Local Address *Foreign Address *State *PID *$"]
      if {$header_index<0} {
         return ;# Netstat output format not recognized
      }

      # We have a good netstat, which can return the PID of the
      # foreign process.  Next, we have to convert the foreign PID
      # to a username.  There are two choices: Nb_GetPidUserName,
      # or and exec call to tasklist.  The latter is more powerful,
      # and seems to be able to map the pid to any user, whereas
      # Nb_GetPidUserName will generally only succeed if the pid
      # owner is the same as the current process owner.  However,
      # the former is sufficient here, and moreover Nb_GetPidUserName
      # is probably both faster and more widely supported.
      #    Note: According to some web sources, tasklist is not on Win2K,
      # but there is a similar command called "tlist".

      set have_getpidusername 0
      set have_tasklist 0

      # Try Nb_GetPidUserName first
      set user [file tail [Nb_GetPidUserName [pid]]]
      if {[string compare [file tail [Nb_GetUserName]] $user]==0} {
         set have_getpidusername 1
      }

      if {!$have_getpidusername} {
         # See if we have a working tasklist command
         if {![catch {exec tasklist /v /fi "PID eq [pid]" /fo list \
                    2> [[Oc_Config RunPlatform] GetValue path_device_null]} \
                  result]} {
            # We have tasklist; check output format
            set task_list [split $result "\n"]
            set user_index [lsearch -regexp $task_list {^User Name:}]
            if {$user_index>=0 &&
                [regexp -- {^User Name: *([^ ].*[^ ]) *$} \
                    [lindex $task_list $user_index] dummy user]} {
               set user [file tail $user]
               if {[string compare $user [file tail [Nb_GetUserName]]]==0} {
                  set have_tasklist 1
               }
            }
         }
         if {!$have_tasklist} {
            return   ;# No way to get user from foreign PID
         }
      }

      # Build non-default Net_CheckSocketPairAccess
      set body {
         set localsock "${local_addr}:${local_port}"
         set foreignsock "${foreign_addr}:${foreign_port}"
         set netstat_output [exec netstat -p tcp -o -n \
                     2> [[Oc_Config RunPlatform] GetValue path_device_null]]
         set netstat_output [split $netstat_output "\n"]
         set sockline [lsearch -regexp $netstat_output \
                          "^ *TCP *$foreignsock *$localsock"]
         if {$sockline<0 || [llength [lindex $netstat_output $sockline]]!=5} {
            # Can't find matching socket line in netstat output
            return 1  ;# Unknown match status
         }
         set foreign_pid [lindex [lindex $netstat_output $sockline] 4]
      }
      if {$have_getpidusername} {
         append body {
            set foreign_user [file tail [Nb_GetPidUserName $foreign_pid]]
            set local_user [file tail [Nb_GetUserName]]
            # Note: Nb_GetPidUserName will likely return an empty string
            # in the case the foreign user is different than the current
            # user (which is assumed to be the same as the local user).
            # This is due to default access restrictions on Windows, and
            # so can be assumed to indicate a client/server user
            # mismatch.
            if {[string compare $local_user $foreign_user]==0} {
               return 2  ;# Match
            }
            return 0  ;# No match
         }
      } else {
         # Use tasklist to extract foreign user from foreign pid
         append body {
            set local_user [file tail [Nb_GetUserName]]
            if {[catch {exec tasklist /v /fi "PID eq $foreign_pid" /fo list \
                    2> [[Oc_Config RunPlatform] GetValue path_device_null]} \
                    result]} {
               # Error; presumably no such PID.  Did foreign process exit?
               return 1  ;# Unknown match status
            }
            set task_list [split $result "\n"]
            set user_index [lsearch -regexp $task_list {^User Name:}]
            if {$user_index>=0 &&
                [regexp -- {^User Name: *([^ ].*[^ ]) *$} \
                    [lindex $task_list $user_index] dummy user]} {
               set foreign_user [file tail $user]
               if {[string compare $local_user $foreign_user]==0} {
                  return 2  ;# Match
               }
               return 0 ;# No match
            }
            return 1 ;# Unknown match status
         }
      }

      # Define replacement Net_CheckSocketPairAccess proc
      if {[info exists body]} {
         proc Net_CheckSocketPairAccess { local_addr local_port \
                                          foreign_addr foreign_port } $body
      }

   } ;# End of Windows platform branch


   # Code to convert a machine address + port from dot to hex form.
   # This is designed for use with the Linux /proc/net/tcp file.
   if {![info exists tcl_platform(byteOrder)] || \
          [string match littleEndian $tcl_platform(byteOrder)]} {
      # Write addr in little endian order
      proc Net_MakeHexSocketAddress { addr port } {
         set hexaddr {}
         foreach byte [split $addr "."] {
            set hexaddr "[format "%02X" $byte]$hexaddr"
         }
         set hexport [format "%04X" $port]
         return "$hexaddr:$hexport"
      }
   } else {
      # Write addr in big endian order
      proc Net_MakeHexSocketAddress { addr port } {
         foreach byte [split $addr "."] {
            append hexaddr [format "%02X" $byte]
         }
         set hexport [format "%04X" $port]
         return "$hexaddr:$hexport"
      }
   }

   if {[string match unix $tcl_platform(platform)]} {
      # First choice: use "ss" (sockstat) if available (Linux).  It is
      # faster than netstat, and in principle more reliable than reading
      # /proc/net/tcp if the tcp_diag module is loaded.
      # ss -tne src raddr:rport dst laddr:lport | lsearch uid:####
      set have_ss 0

      # Next choice: use /proc/net/tcp if available (Linux).
      # Note: For IPv6 connections one should look in /proc/net/tcp6.
      # However, at present (May 2008) Tcl doesn't support IPv6, meaning
      # that the addresses returned by 'fconfigure $sock -sockname'
      # etc. are IPv4.  So we don't provide any support for IPv6 at this
      # time.
      # Note: Depending on the kernel, there may be some unreliability
      # in reading /proc/net/tcp, in that if the reading is broken up
      # into multiple system calls then the file may change between the
      # calls.  In particular, the back end of one line may not
      # correlate with the front.

      set have_proc_tcp 0
      set proc_tcp_file [file join /proc net tcp]
      if {[file exists $proc_tcp_file]} {
         catch {
            set chan [open $proc_tcp_file r]
            gets $chan header_line
            close $chan
            set lsock_index [lsearch -exact $header_line local_address]
            set rsock_index [lsearch -exact $header_line rem_address]
            set uid_index [lsearch -exact $header_line uid]
            set inode_index [lsearch -exact $header_line inode]
            if { $lsock_index == 1 && $rsock_index == 2 \
                    && $uid_index == 9 && $inode_index == 11 } {
               # This is annoying, but two of the fields in the header
               # are separated by a colon in the data table.  These come
               # before uid_index and node_index, so uid_index and
               # node_index need to be decremented by two for use on the
               # data table.  On the other hand, we can't split the data
               # lines at colons either, because the local_address and
               # rem_address fields have embedded colons.
               set uid_index [expr {$uid_index - 2}]
               set inode_index [expr {$inode_index - 2}]
               set have_proc_tcp 1
            }
         }
      }

      # Otherwise, try netstat, lsof, ...?

      if {$have_proc_tcp} {
         # Build Net_CheckSocketPairAccess using /proc/net/tcp
         set body [subst {
            set lsock \[Net_MakeHexSocketAddress \$local_addr \$local_port\]
            set rsock \[Net_MakeHexSocketAddress \$foreign_addr \$foreign_port\]
            set match_found 0
            for {set try 0} {\$try<100} {incr try} {
               set chan \[open $proc_tcp_file r\]
               fconfigure \$chan -blocking 1 -buffering none -translation binary
               set data \[read \$chan\]
               close \$chan
               set data_lines \[split \$data "\\n"\]
               set data_lines \[lreplace \$data_lines 0 0\] ;# Drop header
               foreach line \$data_lines {
                  if {\[llength \$line\]<$inode_index} {
                     # Bad line;  We assume that inode_index is largest index.
                     continue
                  }
                  set local \[lindex \$line $lsock_index\]
                  set remote \[lindex \$line $rsock_index\]
                  if {\[string compare \$rsock \$local\]==0\\
                         && \[string compare \$lsock \$remote\]==0} {
                     set uid \[lindex \$line $uid_index\]
                     set inode \[lindex \$line $inode_index\]
                     if {!\[regexp {^\[0-9\]+\$} \$uid\] || \\
                            !\[regexp {^\[0-9\]+\$} \$inode\] } {
                        continue  ;# Bad line
                     }
                     set match_found 1
                     break
                  }
               }
               if {\$match_found && \$inode > 0} {
                  break  ;# Match found
               }
               # Otherwise, data is not ready.  Pause and try again
               set match_found 0
               after 100
            }
            if {!\$match_found} { return 1}
            if {\$uid == [Nb_GetUserId]} { return 2 }
            ;# Note hard-coded Nb_GetUserId result.
            return 0
         }]
      }

      if {[info exists body]} {
         proc Net_CheckSocketPairAccess { local_addr local_port \
                                          foreign_addr foreign_port } $body
      }

   } ;# End of unix branch
}

# Note: This file only gets loaded if Net_CheckSocketAccess,
# Net_CheckSocketPairAccess, or Net_CheckSocketAccessInit is called.
# (See tclIndex).  In particular, if the Net checkUserIdentities options
# are set to 0, then this file should not be loaded, and the above
# (potentially slow, possibly fragile) system introspection code
# won't be run.

Net_CheckSocketAccessInit
