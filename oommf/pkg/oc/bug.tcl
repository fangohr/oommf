# FILE: bug.tcl
#
#	Patches for all Tcl/Tk versions
#
# Last modified on: $Date: 2000/08/25 17:01:25 $
# Last modified by: $Author: donahue $
#
# This file contains Tcl code which patches or enhances the commands of 
# Tcl and Tk so that the Oc extension sees a 'common denominator' interpreter
# regardless of the underlying Tcl/Tk version and platform.
# 
# The patches and enhancements here are not meant to be complete.  New
# code is added here only as we discover the need to do so in writing 
# other OOMMF software.

# For now, we leave the Tcl command [update] as we find it.
# When we attempted to define all [update idletasks] calls on 
# Windows to act as calls to [update], some parts of the Tk
# library (for example [tk_dialog]) which call [update idletasks] 
# behaved badly.  If this turns out to be unsatisfactory in mmDisp 
# on Windows, we'll revisit the issue.
#if {[llength [info commands update]]} {
#    if {[string match windows $tcl_platform(platform)]} {
#        # On Windows the 'update idletasks' command doesn't
#        # reset the cursor (and possibly other problems?).  
#        # Redefine the 'update' command so that even calls to
#        # 'update idletasks' really do a full 'update'
#        rename update Tcl[info tclversion]_update
#        proc update args Tcl[info tclversion]_update
#    }
#}

# Tcl 8.0.3 introduced a new default behavior for [auto_mkindex].  OOMMF
# was designed around the old behavior, which in Tcl 8.0.3+ is available
# as the command [auto_mkindex_old].  Redefine to get the old behavior.
# Starting in Tcl8.1, auto_mkindex has to be auto-loaded.
catch {auto_mkindex}
if {[llength [info commands auto_mkindex_old]]} { 
    interp alias {} auto_mkindex {} auto_mkindex_old
}

# Report that bug patches are done.
Oc_Log Log "Tcl patches complete" status
