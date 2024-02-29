# FILE: net.tcl
#
#	The Net extension.
#
#	This extension provides a set of Oc_Classes which support networking
# operations.
#
# Last modified on: $Date: 2015/03/25 16:45:08 $
# Last modified by: $Author: dgp $
#
# When this version of the Net extension is selected by the 'package require'
# command, this file is sourced.

# Other package requirements
package require Tcl 8.5-
package require Oc 2
if {[catch {package require Nb 2}]} {
   # The Nb library is needed for socket connection account access
   # checks.  Ignore load failure if checks are disabled in
   # config/options.tcl, otherwise fail.
   set server_checks [set link_checks 0]
   Oc_Option Get Net_Server checkUserIdentities server_checks
   Oc_Option Get Net_Link checkUserIdentities link_checks
   if {$server_checks>0 || $link_checks>0} {
      error "Net package can't make access checks (Nb package not available)"
   }
}

Oc_CheckTclIndex Net

# CVS 
package provide Net 2.1a0

# Set up for autoloading of Net extension commands
set _net(library) [file dirname [info script]]
if { [lsearch -exact $auto_path $_net(library)] == -1 } {
    lappend auto_path $_net(library)
}

# Load in any local modifications
set local [file join [file dirname [info script]] local net.tcl]
if {[file isfile $local] && [file readable $local]} {
    uplevel #0 [list source $local]
}

Oc_EventHandler New _ Oc_Main LibShutdown Net_StartCleanShutdown
