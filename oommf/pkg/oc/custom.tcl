# FILE: custom.tcl
#
#	OOMMF core provided customizations to Tcl/Tk
#
# Last modified on: $Date: 2016/03/08 21:34:01 $
# Last modified by: $Author: donahue $
#
# This file provides some customization/extensions to Tcl/Tk that
# are common across the OOMMF project.
if {[llength [info commands bind]] && [llength [info commands wm]] \
       && [llength [info commands toplevel]]} {
    ################################
    # Bind auto-default resize behavior to all toplevel windows.
    proc Oc_AutoSizeCheck { win w h } {
       set wmin 5
       set hmin 5
       if { $w <= $wmin || $h <= $hmin } {
          wm geometry $win {}   ;# Resize to default
          ## Note: On windows, and perhaps other systems depending on
          ## the window manager, the window width is limited by
          ## decorations in the title bar.  Earlier versions of this
          ## routine required both $w and $h to be smaller than the
          ## specified limit to invoke resizing, but there does not
          ## appear to be any practical way to determine the minimum
          ## window width the system will allow, so instead we trigger
          ## if either condition is met.  (All tested systems allow
          ## height to be sized to 0 (or smaller).)
       }
    }
    bind AutoSize <Configure> {Oc_AutoSizeCheck %W %w %h}

    # Set up root window to use AutoSize binding.
    bindtags . [concat AutoSize [bindtags .]]


    # Bind Ctrl-Home keypress to resize window to natural size. We do
    # this on the root window, but Enter and Scale widgets have
    # <Key-Home> bindings that will trigger on <Control-Key-Home>
    # unless we provide an explicit <Control-Key-Home> binding.
    foreach tag {. Entry Scale} {
       bind $tag <Control-Key-Home> { wm geometry . {} }
    }

    # We also want <Key-Home> to resize window to natural size
    # generally, except in Entry and Scale widgets where that would
    # trigger confusion with the widget default behavior. One
    # complication is that in Tk 8.5 and earlier there are bindings on
    # <Key-Home> directly, but in Tk 8.6 those are replaced with
    # bindings on the virtual event <<LineStart>> instead. Either way,
    # append a break to short-circuit the root window binding. Entry
    # widgets also have a default binding on <Shift-Key-Home> or
    # <<SelectLineStart>>, so handle those too.
    bind . <Key-Home> { wm geometry . {} }
    catch {bind . <Key-KP_Home>   { wm geometry . {} }}
    foreach tag {Entry Scale} {
       if {![string match {} [bind $tag <<LineStart>>]]} {
          bind $tag <<LineStart>> {+break}
          if {![string match {} [bind $tag <<SelectLineStart>>]]} {
             bind $tag <<SelectLineStart>> {+break}
          }
       } else {
          bind $tag <Key-Home> {+break}
          if {![string match {} [bind $tag <Shift-Key-Home>]]} {
             bind $tag <Shift-Key-Home> {+break}
          }
       }
    }

    # Redefine 'toplevel' so all toplevel windows automatically use the
    # AutoSize binding, and get assigned to the "."  group.
    rename toplevel Tcl_toplevel
    proc toplevel { pathName args } {
       set win [eval Tcl_toplevel $pathName $args]
       bindtags $win [concat AutoSize [bindtags $win]]
       wm group $win . ;# By default, bind to root window group

       bind $win <Key-Home>         [subst { wm geometry $win {} }]
       bind $win <Control-Key-Home> [subst { wm geometry $win {} }]

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

    # Bind Home keypress to resize outer window to natural size (i.e.,
    # the size requested by the subwidgets). These bindings may be
    # overridden by applications such as mmDisp having more specific
    # behavior.
    bind . <Key-Home>        { wm geometry . {} }
    bind . <Shift-Key-Home>  { wm geometry . {} }
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
	}
	return -code $code $result
    }
}

################################
# Default history length is 20 commands, which is rather niggardly.
# Increase to some arbitrary amount.
history keep 999
