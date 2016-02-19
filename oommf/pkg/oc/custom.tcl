# FILE: custom.tcl
#
#	OOMMF core provided customizations to Tcl/Tk
#
# Last modified on: $Date: 2014/02/21 21:36:25 $
# Last modified by: $Author: dgp $
#
# This file provides some customization/extensions to Tcl/Tk that
# are common across the OOMMF project.
if {[llength [info commands bind]] && [llength [info commands wm]] \
    && [llength [info commands toplevel]]} {
################################
# Bind auto-default resize behavior to all toplevel windows.
    proc Oc_AutoSizeCheck { win w h } {
	foreach {wmin hmin} [wm minsize $win] {}
	if { $w <= $wmin && $h <= $hmin } {
	    wm geometry $win {}   ;# Turn on auto-sizing behavior
	}
    }
    bind AutoSize <Configure> {Oc_AutoSizeCheck %W %w %h}

    # Set up root window to use AutoSize binding.
    bindtags . [concat AutoSize [bindtags .]]
    wm minsize . 5 5  ;# Adjust minimum size so the binding
    ## is easier for the user to trigger.

    # Redefine 'toplevel' so all toplevel windows automatically use the
    # AutoSize binding, and get assigned to the "."  group.
    rename toplevel Tcl_toplevel
    proc toplevel { pathName args } {
	set win [eval Tcl_toplevel $pathName $args]
	bindtags $win [concat AutoSize [bindtags $win]]
	wm minsize $win 5 5  ;# Adjust minimum size so the
	## binding is easier for the user to trigger.
	wm group $win . ;# By default, bind to root window group
	return $win
    }

    # Utility procs to add/remove AutoSize binding from a single
    # window.  AutoSize can be completely disabled with the command
    #
    #   bind AutoSize <Configure> {}
    #
    proc Oc_DisableAutoSize { win } {
	set wintags [bindtags $win]
	set newtags {}
	foreach tag $wintags {
	    if {![string match AutoSize $tag]} {
		lappend newtags $tag
	    }
	}
	bindtags $win $newtags
    }
    proc Oc_EnableAutoSize { win } {
	Oc_DisableAutoSize $win
	bindtags $win [concat AutoSize [bindtags $win]]
    }
}



if {[llength [info commands bind]]} {
################################
# Additional key bindings
#

# Bind <Key-Return> to Invoke on buttons.  By default, at least
# under X, only <Key-space> is bound to Invoke.
bind Button <Key-Return> [bind Button <Key-space>]

# Try to handle <Shift-Key-Tab> variants.  The [bind all <Shift-Tab>]
# parameter expands to the proc that is the current binding to
# <Shift-Tab> (probably 'tkTabToWindow [tk_focusPrev %W]').  Some
# X servers return a distinct keysym for the Shift-Tab event, e.g.,
# ISO_Left_Tab.  One can use the X utility program xev to see what
# keysyms are being generated.
#  One problem that occurs, is if one is running a binary on one
# machine, say, a Sun, that does not know the ISO_Left_Tab keysym
# (cf. the system <X11/keysymdef.h> file), and displaying on a
# different system, say a recent Linux machine, that does.  Then the
# X server on the Linux machine generates the keysym number corresponding
# to ISO_LEFT_TAB (0xfe20), but the program built on the Sun doesn't
# know the symbolic interpretation for that keysym.  Therefore we
# provide a numeric binding if the symbolic binding is not known.  (I'm
# assuming the symbolic bindings, if defined, are the same across all
# platforms.)
#  In addition to xev, the following wish script is useful in diagnosing
# and solving such problems:
#
# bind . <KeyPress> {puts stdout {%%K=%K, %%A=%A, %%k=%k, %%s=%s, %%N=%N.}}
#
# This prints to stdout the  keysym (%K), ASCII character (%A), keycode (%k),
# state (%s), which for key events is modifier information, like shift key
# status), and keysym as a decimal number (%N).  The last should be defined
# even when the first isn't.          -mjd, Sep-1997
#
proc Oc_SetShiftTabBinding { win script } {
    # NOTE: The fallback to numeric bindings doesn't properly
    #  handle the '+script" syntax.
    regexp -- {^\+?(.*)} $script match cleanscript
    if {[catch {bind $win <Shift-Key-ISO_Left_Tab> $script}]} {
	# ISO_Left_Tab probably not defined.  Try numeric binding
	catch {
	    bind $win <Shift-Any-Key> \
		    "+if {\"%N\"==65056} \{$cleanscript\}"
	    # ISO_Left_Tab = 0xFE20 = 65056
	}
    }
    if {[catch {bind $win <Shift-Key-3270_BackTab> $script}]} {
	# 3270_BackTab probably not defined.  Try numeric binding
	catch {
	    bind all <Shift-Any-Key> \
		    "+if {\"%N\"==64773} \{$cleanscript\}"
	    # 3270_BackTab = 0xFD05 = 64773
	}
    }
    bind $win <Shift-Tab> $script
}

set _shift_tab_script [bind all <Shift-Tab>]
if {[string match {} $_shift_tab_script]} {
    # Recent versions of Tk set <<PrevWindow>> instead
    # of <Shift-Tab>
    if {![catch {bind all <<PrevWindow>>} _shift_tab_script]} {
	Oc_SetShiftTabBinding all $_shift_tab_script
    }
} else {
    Oc_SetShiftTabBinding all $_shift_tab_script
}
unset _shift_tab_script

################################
}

if {[llength [info commands interp]]} {
    # Make loaded packages available via [package require]
    # in slave interpreters.
    rename interp Tcl_interp
    proc interp {args} {
	set code [catch {uplevel 1 [linsert $args 0 Tcl_interp]} result]
	if {!$code && [string match c* [lindex $args 0]]} {
	    # [interp create]
	    # Work around broken [expr rand()] in $result
	    catch {expr {rand()}}
	    if {![Tcl_interp issafe $result]} {
		# [interp create] of trusted interp
		foreach pair [info loaded {}] {
		    foreach {_ pkg} $pair {break}
		    set ver [package provide $pkg]
		    if {[catch {package vcompare $ver $ver}]} {
			# Attempt to work around [info loaded] misfeature that
			# "package" names returned are not [package] names.
			# Grrrrr......
			set ver [package provide [string tolower $pkg]]
			if {[catch {package vcompare $ver $ver}]} {
			    continue
			}
		    }
		    $result eval [list package ifneeded $pkg $ver \
			    [list load {} $pkg]]
		}
		global oc argv0
		$result eval [list lappend auto_path $oc(library)]
		$result eval [list set argv0 $argv0]
	    }
            # Tcl expr fixes and extras
            catch {Oc_AddTclExprExtensions $result}
	}
	return -code $code $result
    }
}

