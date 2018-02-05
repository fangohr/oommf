# FILE: oc.tcl
#
#	OOMMF core
#
# Last modified on: $Date: 2015/03/25 16:45:12 $
# Last modified by: $Author: dgp $
#
# When this version of the Oc extension is selected by the 'package require'
# command, this file is sourced.

# Verify minimum Tcl/Tk support
package require Tcl 8.5-

# Insure floating point ops are at full precision
if {[catch {set tcl_precision 0}]} {
   set tcl_precision 17
}

proc Oc_CheckTclIndex {pkg} {
    if {[llength [info commands Oc_DirectPathname]]} {
        # Cosmetic fix, if it's available
        set index [Oc_DirectPathname \
            [file join [file dirname [info script]] tclIndex]]
    } else {
        set index [file join [file dirname [info script]] tclIndex]
    }
    if {![file isfile $index] || ![file readable $index]} {
        set msg "\nNo index found for $pkg\n\tMissing or unreadable\
                file: $index\n\n"
        if {[file exists [file join [file dirname [info script]] \
                makerules.tcl]]} {
            append msg "Run pimake in [file dirname $index]\nto construct\
                    the missing index file.\n"
        } else {
            append msg "This probably means $pkg was installed incorrectly.\n"
        }
        return -code error $msg
    }
}

if {[catch {Oc_CheckTclIndex Oc}]} {
    catch {auto_mkindex}
    if {[llength [info commands auto_mkindex_old]]} {
	interp alias {} auto_mkindex {} auto_mkindex_old
    }
    source [file join [file dirname [info script]] procs.tcl]
    Oc_MakeTclIndex [file dirname [info script]]
    Oc_CheckTclIndex Oc
}

# CVS 
package provide Oc 2.0a0

# Set up for autoloading of Oc extension commands
set oc(library) [file dirname [info script]]
if { [lsearch -exact $auto_path $oc(library)] == -1 } {
    lappend auto_path $oc(library)
}

# Gain access to other extensions in the OOMMF project
if { [lsearch -exact $auto_path [file dirname $oc(library)]] == -1 } {
    lappend auto_path [file dirname $oc(library)]
}

# Patch any bugs and missing features in old versions of Tcl
regsub {[.]} [package provide Tcl] {} _
set _ [file join [file dirname [info script]] bug$_.tcl]
if {[file readable $_]} {
    source $_
} else {
    source [file join [file dirname [info script]] bug.tcl]
}

# Bring in OOMMF customizations
source [file join [file dirname [info script]] custom.tcl]

# Initialize the Oc_Main class
Oc_Main Initialize

# If the C part of Oc is active, it provided [Oc_InitScript]
if {[llength [info commands Oc_InitScript]]} {
    # Add the command line options already parsed by the C part of Oc
    Oc_CommandLine Option tk {{flag {expr {![catch {expr {$flag && $flag}}]}} 
	"= 0 or 1"}} {#} "Enable or disable Tk (Some apps require Tk)"

    # If we also have Tk, add the options which Tk accepts
    if {[string length [package provide Tk]]} {
       Oc_CommandLine ActivateOptionSet tk
    }
}

# Add standard command line options
Oc_CommandLine ActivateOptionSet standard

# Load in any local modifications
set _ [file join [file dirname [info script]] local oc.tcl]
if {[file isfile $_] && [file readable $_]} {
    uplevel #0 [list source $_]
}
unset _

