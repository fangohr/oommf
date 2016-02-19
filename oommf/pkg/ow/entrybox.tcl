# FILE: entrybox.tcl
#
# Entry box dialog sub-widget
#
# Last modified on: $Date: 2010/08/23 23:36:08 $
# Last modified by: $Author: donahue $
#
# debugging enable/disable/level

#   Use the method call "ReadEntryString" to get the value
# currently displayed in the entry box.  Use "Cget -value" to
# get the last value "committed" with a <Key-Return> event.
#   The <Key-Return> event copies the value from the entry widget
# into -value, and then calls the -callback proc, with ACTIONID of
# UPDATE, and arg list of -value $value.
#   Set -autoundo if you want automatic undo action (equivalent
# to the <Key-Escape> event) on <FocusOut> events.  This copies the
# last "committed" value (as stored in -value) into the entry widget
# display.
#   Use the method Set to set both -value and the displayed
# text.  Use SetEdit to set only the displayed text.
Oc_Class Ow_EntryBox {
    const public variable winpath
    const public variable outer_frame_options = "-bd 2 -relief raised"
    public variable winentry = {}
    public variable label = {} {
	if {[winfo exists $winpath.label]} {
            $winpath.label configure -text $label
        }
    }
    const public variable labelwidth = 0 ;# Set this to -1 if the
    ## EntryBox will be used w/o a label; this removes some label
    ## padding.
    public variable callback = {}
    const public variable valuewidth = 16
    public variable value = {}  ;# Last committed value

    # Name of a global variable to
    # tie to value.  Any time this global is written to, that value
    # gets sent to Set.  When a value is committed, either
    # $callback is called (if $callback != {}), or otherwise the
    # global variable $variable is written (iff $callback == {}).
    private variable oldvariable = {}
    public variable variable = {} {
	if {[info exists oldvariable] && ![string match {} $oldvariable]} {
	    upvar #0 $oldvariable tievariable
            trace vdelete tievariable w "$this TraceValue $oldvariable"
	}
        if {![string match {} $variable]} {
            upvar #0 $variable tievariable
            if {[info exists tievariable]} {
		if {![string match {} $winentry]} {
		    $this TraceValue $variable ;# Update value + display
		} else {
		    set value $tievariable ;# Display not initialized
		}
            } else {
		set tievariable $value  ;# Safety initialization
	    }
            trace variable tievariable w "$this TraceValue $variable"
        } else {
            set writethrough 0
        }
	set oldvariable $variable
    }
    ## Note: If one wishes to remove the 'const' attribute, then the
    ## verification script will need to to unset and reset traces on
    ## $variable.

    const public variable writethrough = 1 ;# If set to 1, then all
    ## edits to the entry widget are shadowed in $variable.  If
    ## writethrough is 0, then $variable is only written to on
    ## commits (unless $callback!={} as discussed above).  The
    ## default value of $writethrough is 1, to better mimic the
    ## behavior of standard Tk entry widgets, but is reset to 0
    ## in the Constructor if $variable == {}.

    const public variable valuetype = text
    const public variable valuecheck = Oc_Nop
    ## See oommf/ext/ow/procs.tcl for complete list of predefined
    ## valid types.  Currently these are text, int, posint, fixed,
    ## posfixed and float.  Use "custom" and the valuecheck variable
    ## to register a custom routine.  See procs.tcl check routine
    ## structure.

    const public variable valuejustify = left

    # Hard limits.  If not {}, then no value outside the
    # specified range will be accepted.
    public variable hardmax = {}
    public variable hardmin = {}

    private variable testvalue = {} ;# Used to enforce write restrictions
    private variable oldvalue = {}  ;# (see valuetype and valuecheck).

    # Default colors are setup in constructor to take the
    # associated values from the standard Tk button widget.
    const private variable default_foreground
    const private variable default_disabledforeground

    const public variable foreground = {} {
	if {[info exists default_foreground]} {
	    # If default_foreground isn't setup, then we
	    # must be inside Constructor.
	    if {[string match {} $foreground]} {
		set foreground $default_foreground   ;# Use default
	    }
	    # Update foreground colors in "normal state" widgets
	    if {[string match normal $state]} {
		$this Configure -state normal
	    }
	}
    }

    const public variable disabledforeground = {} {
	if {[info exists default_disabledforeground]} {
	    # If default_disabledforeground isn't setup, then we
	    # must be inside Constructor.
	    if {[string match {} $disabledforeground]} {
		# Use default
		set disabledforeground $default_disabledforeground
	    }
	    # Update colors in "disabled state" widgets
	    if {[string match disabled $state]} {
		$this Configure -state disabled
	    }
	}
    }

    const public variable coloredits = 0
    const public variable activeeditcolor = red
    const public variable commiteditcolor
    const public variable commiteditsfcolor
    # Set coloredits 1 to color uncommitted edits

    public variable display_preference = {} ;# Index of entry string to
    ## make visible on external updates.  Typically set to "end" for
    ## potentially long strings like pathnames.

    public variable state = normal {
	# Adjust state of inner label widget
	if {[winfo exists $winpath.label]} {
	    # Starting with Tcl/Tk 8.3.2, the label
	    # widget supports the "-state" configuration
	    # option.  But for backwards compatibility
	    # we'll just manually adjust the text color.
            if {[string match disabled $state]} {
                $winpath.label configure -foreground $disabledforeground
            } else {
                $winpath.label configure -foreground $foreground
            }
	}
        # Adjust state of inner entry widget
        if {![string match {} $winentry]} {
            $winentry configure -state $state
            if {[string match disabled $state]} {
                $winentry configure -foreground $disabledforeground
            } else {
                $winentry configure -foreground $foreground
            }
        }
    }

    const public variable autoundo = 0 ;# If set, then widget performs
    ## an undo action on FocusOut events

    const public variable commitontab = 1 ;# If set, then
    ## tabbing out of the widget commits the value

    const public variable uselabelfont = 1 ;# If set, then the font used
    ## in the label widget is also used in the entry widget.  Otherwise,
    ## the default font is used in the entry widget.

    Constructor {basewinpath args} {
	# If $basewinpath doesn't exist, then we use that name
	# for the outer frame.  Otherwise, we create a hopefully
	# unique name based on $this, as a child of $basewinpath.
	# Don't add '.' suffix if there already is one, which
	# happens when basewinpath is exactly '.'
	if {![winfo exists $basewinpath]} {
	    set winpath $basewinpath
	} else {
	    if {![string match {.} \
		    [string index $basewinpath \
		    [expr [string length $basewinpath]-1]]]} {
		append basewinpath {.}
	    }
	    set winpath ${basewinpath}[$this WinBasename]
	}

	# Process user preferences
	eval $this Configure $args

        # Check for tied variable, and if it exists use it to
        # override any -value setting
if {0} {
        if {![string match {} $variable]} {
            upvar #0 $variable tievariable
            if {[info exists tievariable]} {
                set value $tievariable
            } else {
		set tievariable $value  ;# Safety initialization
	    }
        } else {
            set writethrough 0
        }
}

        # Validate input
	if {![string match custom $valuetype]} {
	    set valuecheck [Ow_GetEntryCheckRoutine $valuetype]
        }

	# Temporarily disable callbacks until construction is complete
	set callback_keep $callback ;	set callback {}

	# Pack entire subwidget into a subframe.  This makes
	# housekeeping easier, since the only bindings on
	# $winpath will be ones added from inside the class,
	# and also all subwindows can be destroyed by simply
	# destroying $winpath.
	eval [linsert $outer_frame_options 0 frame $winpath]

	# Setup default colors, using a temporary button widget
	set btn [button $winpath.throwawaybutton]
	set default_foreground [$btn cget -foreground]
	set default_disabledforeground [$btn cget -disabledforeground]
	destroy $btn

	# If foreground and disabledforeground weren't setup by
	# user, then use default colors.
	if {[string match {} $foreground]} {
	    set foreground $default_foreground
	}
	if {[string match {} $disabledforeground]} {
	    set disabledforeground $default_disabledforeground
	}

	# Create label subwidget
	label $winpath.label -text $label
	if {![string match {} $labelwidth]} {
	    if {$labelwidth>=0} {
		$winpath.label configure -width $labelwidth
	    } else {
		# No label; remove extra space
		$winpath.label configure \
			-width 0 -padx 0 -pady 0 -borderwidth 0
	    }
	}
	if {[string match disabled $state]} {
	    $winpath.label configure -foreground $disabledforeground
	} else {
	    $winpath.label configure -foreground $foreground
	}
	pack $winpath.label -side left -anchor e

	# Create entry subwidget
	set winentry [entry $winpath.value -relief sunken -bd 2 \
		-width $valuewidth -bg [$winpath.label cget -bg] \
                -state $state -justify $valuejustify]
	## The default color scheme under Tcl 8.0 + Windows makes
	## labels a light gray, and entry backgrounds bright white.
	## *I* think it looks pretty hideous...  -mjd 97/10/3
        if {![string match disabled $state]} {
            $winentry configure -foreground $foreground
        } else {
            $winentry configure -foreground $disabledforeground
        }
	set commiteditcolor [$winentry cget -foreground]
	set commiteditsfcolor [$winentry cget -selectforeground]
        if {[string match {} $commiteditsfcolor]} {
           set commiteditsfcolor $commiteditcolor
        }

	# Set up edit tracing
	$winentry configure -textvariable [$this GlobalName testvalue]
	trace variable testvalue w "$this TraceEdits"

	pack $winentry -fill x -expand 1 -side left
	$this Set $value

if {0} {
        # Set trace to tied variable, if any (upvar from $variable
        # to tievariable done above).
        if {![string match {} variable]} {
            if {![info exists tievariable] || $tievariable!=$value} {
                set tievariable $value
            }
            trace variable tievariable w "$this TraceValue $variable"
        }
}

	# Setup fonts
	if {$uselabelfont} {
	    $winentry configure -font [$winpath.label cget -font]
	}

	# Widget bindings
	bind $winentry <Key-Return> "$this CommitValue"
	if {$commitontab} {
	    bind $winentry <Key-Tab> "$this CommitValue"
	    Oc_SetShiftTabBinding $winentry "$this CommitValue"
	}
	bind $winentry <Key-Escape> "$this Undo"
	if {$autoundo} {
	    bind $winentry <FocusOut> [bind $winentry <Key-Escape>]
	}
	bind $winpath <Destroy> "$this WinDestroy %W"

	# Re-enable callbacks
	set callback $callback_keep
    }
    callback method TraceEdits { args } {
	# Ignore imports
	if {[eval [linsert $valuecheck end $testvalue]]>1} {
	    # Bad value; change back to old value
	    set testvalue $oldvalue
	} else {
	    # Accept new value
	    set oldvalue $testvalue
	    if {$coloredits} {
		$winentry configure -foreground $activeeditcolor \
			-selectforeground $activeeditcolor
	    }
            if {$writethrough} {
                # Note: If $writethrough is true, then $variable
                #  should not equal {}.
                upvar #0 $variable shadow
                set shadow $testvalue
            }
	}
    }
    callback method TraceValue { globalname args } {
        ## The trace is set to pass in the global variable name so that
        ## even if the trace is triggered by a call to Tcl_SetVar
        ## in C code called from a proc from which the traced variable
        ## is not visible (due to no 'global' statement on that variable),
        ## this trace can still function.        
        upvar #0 $globalname shadow
	set keep_state [$winentry cget -state]
	$winentry configure -state normal
        $this Set $shadow
        $this CommitValue 0
	$winentry configure -state $keep_state
    }
    method ReadEntryString {} { winentry } {
	return [$winentry get]
    }
    method CommitValue { {do_callbacks 1} } {
	set text [$winentry get]
	if {[eval [linsert $valuecheck end $text]]!=0} {
            # Invalid value
            $this Set $value
            return
        }
	# Check hard limits
	set update_widget 0
	if {![string match {} $hardmin]} {
	    catch { if { $text < $hardmin } \
		    {set text $hardmin ; set update_widget 1} }
	}
	if {![string match {} $hardmax]} {
	    catch { if { $text > $hardmax } \
		    {set text $hardmax ; set update_widget 1} }
	}
        set value $text
	if {$update_widget} {$this Set $value}
	if {$coloredits} {
	    $winentry configure -foreground $commiteditcolor \
		    -selectforeground $commiteditsfcolor
	}
        if {$do_callbacks} {
            if {![string match {} $callback]} {
                eval [linsert $callback end $this UPDATE -value $value]
		# Note: The preceding callback may delete $this,
		# in which case "variable" will cease to exist.
		if {[info exists variable] && \
			![string match {} $variable]} {
		    upvar #0 $variable shadow
		    $this Set $shadow
		}
            } elseif { ![string match {} $variable] } {
                upvar #0 $variable shadow
                # Note: 'global $variable' won't work if $variable
                #   is an element in an array.
                set shadow $value
            }
        }
    }
    method Set { newvalue } {
	if {[string compare $newvalue [$winentry get]]!=0} {
	    $winentry delete 0 end
	    $winentry insert 0 $newvalue
	}
	if {![string match {} $display_preference]} {
	    $winentry xview $display_preference
	}
	set value $newvalue
	if {$coloredits} {
	    $winentry configure -foreground $commiteditcolor \
		    -selectforeground $commiteditsfcolor
	}
    }
    method SetEdit { newvalue } {
	if {$writethrough} {
	    # Call commit version of this routine
	    $this Set $newvalue
	} else {
	    if {[string compare $newvalue [$winentry get]]!=0} {
		$winentry delete 0 end
		$winentry insert 0 $newvalue
	    }
	    if {![string match {} $display_preference]} {
		$winentry xview $display_preference
	    }
	    set testvalue $newvalue
	}
    }
    method Undo {} { value } {
	$this Set $value
    }
    method WinDestroy { w } { winpath } {
	if {[string compare $winpath $w]!=0} {
	    return    ;# Child destroy event
	}
	$this Delete
    }

    # Methods providing Tk-style naming convention interfaces
    # to Ow_EntryBox methods.
    method cget { option } {} { $this Cget $option }
    method configure { args } {} { eval $this Configure $args }

    Destructor {
        # Delete variable traces.
        if {[catch {
            if {![string match {} variable]} {
                upvar #0 $variable tievariable
                trace vdelete tievariable w "$this TraceValue $variable"
            }
        } errmsg]} {
            Oc_Log Log $errmsg error
        }
        if {[catch {
	    trace vdelete testvalue w "$this TraceEdits"
        } errmsg]} {
            Oc_Log Log $errmsg error
        }

        # Destroy windows
        if {[catch {
            if {[winfo exists $winpath]} {
                # Remove <Destroy> binding
                bind $winpath <Destroy> {}
                # Destroy instance frame, all children, and bindings
                destroy $winpath
            }
        } errmsg]} {
            Oc_Log Log $errmsg error
        }
    }
}
