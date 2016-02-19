# FILE: listbox.tcl
#
# Scrolled listbox subwidget
#
# Last modified on: $Date: 2010/08/23 23:36:08 $
# Last modified by: $Author: donahue $
#

Oc_Class Ow_ListBox {
    const public variable winpath
    const public variable troughcolor ;# Scrollbar trough color
    const public variable outer_frame_options = "-bd 2 -relief sunken"
    public variable listbox_options =  "-bd 2 -relief ridge" {
	eval $this ListConfigure $listbox_options
    }
    const private variable list  ;# Tk listbox widget
    const private variable scrx  ;# x-scrollbar
    const private variable scry  ;# y-scrollbar
    private variable list_normal_colors
    private variable list_disabled_colors
    ## {fg bg selectforeground selectbackground}

    public variable selectmode = browse ;# Listbox select mode
    public variable variable = {} {
	# Written to whenever selection changes.  Must be global
        catch {
	    ## This is ugly intrusion into the internals of Oc_Class!
            upvar oldval old_variable ;# Previous value stored
            ## in local variable oldval in Configure method before
            ## calling the verification script.
            if {![string match {} $old_variable]} {
                # Remove old trace, if any
                upvar #0 $old_variable shadow
                trace vdelete shadow w "$this SelectItems"
            }
        }
        if {![catch {upvar #0 $variable shadow}]} {
            # Set new trace
            trace variable shadow w "$this SelectItems"
        }
        # Make selection and $variable agree
        if {[info exists shadow]} {
            $this SelectItems shadow
        } else {
            set shadow {}
            if {[info exists list]} {
                foreach index [$list curselection] {
                    lappend shadow [$list get $index]
                }
            }
        }
    }
    public variable height = 5 { $this ListConfigure -height $height }
    ;## Listbox height in lines
    public variable width  = 0 { $this ListConfigure -width $width }
    public variable yscroll_state = 1 {
	if {!$yscroll_state} { grid remove $scry } else { grid $scry }	
    }
    const public variable xscroll_state = 0 {
	if {!$xscroll_state} { grid remove $scrx } else { grid $scrx }	
    }
    public variable state = normal {
	if {![regexp {normal|disabled} $state]} {
	    error {Bad value; must normal or disabled}
	}
	$this SetListState $state
    }
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

	if {![info exists troughcolor]} {
	    global omfTroughColor
	    if [info exists omfTroughColor] {
		set troughcolor $omfTroughColor
	    } else {
		set troughcolor {}
	    }
	}

	# Pack entire subwidget into a subframe.  This makes
	# housekeeping easier, since the only bindings on
	# $winpath will be ones added from inside the class,
	# and also all subwindows can be destroyed by simply
	# destroying $winpath.
	eval frame $winpath $outer_frame_options

	set list [listbox $winpath.list \
		-selectmode $selectmode \
		-height $height -width $width \
                -takefocus "$this TakeFocus"]
	eval $list configure $listbox_options -exportselection 0
	## Note: Typically want selection highlighting to indicate
	## selection; if exportselection is on, then we loose
	## highlighting when another selection is made anywhere else
	## on the screen.

        # Sometime around Tcl/Tk 8.0.4, the default -background
	# color for listboxes on Windows changed from SystemButtonFace
	# to SystemWindow; the former was usually gray, the latter
        # white.  *I* think it looks pretty hideous...  -mjd 2002/10/22
	global tcl_platform
	if {[string match windows $tcl_platform(platform)]} {
	    $list configure -background SystemButtonFace
	}

        set sfg [$list cget -selectforeground]
        if {[string match {} $sfg]} {
           set sfg [$list cget -foreground]
        }
        set sbg [$list cget -selectbackground]

	set list_normal_colors [list \
		-fg [$list cget -fg] \
		-bg [$list cget -bg] \
		-selectforeground $sfg \
		-selectbackground $sbg ]
	# Pull "disabled" color from a button widget.
	set tb [button $winpath.tempbutton]
	set dfc [$tb cget -disabledforeground]
	destroy $tb
	set list_disabled_colors [list \
		-fg $dfc \
		-bg [$list cget -bg] \
		-selectforeground $dfc \
		-selectbackground [$list cget -bg]]

	set scry [scrollbar $winpath.scry -bd 2 -command "$list yview" \
                -takefocus "$this TakeFocus"]
	set scrx [scrollbar $winpath.scrx -bd 2 -command "$list xview" \
		-orient horizontal -takefocus "$this TakeFocus"]
	if {![string match {} $troughcolor]} {
	    $scry configure -troughcolor $troughcolor
	    $scrx configure -troughcolor $troughcolor
	}

        # Explicitly bind Tab events so they will operate even
        # when bindings to <Key> are disabled (i.e., when -state
        # is set to disabled).
        foreach win [list $list $scrx $scry] {
            bind $win <Key-Tab> { tk_focusNext %W }
            Oc_SetShiftTabBinding $win { tk_focusPrev %W }
        }

        # Pack
	grid $list $scry
	grid $list -sticky news
	grid configure $scry -sticky ns
	grid $scrx -sticky ew
	grid columnconfigure $winpath 0 -weight 1
	grid rowconfigure $winpath 0 -weight 1

	# Selectively display scrollbars
	if {!$xscroll_state} { grid remove $scrx}
	if {!$yscroll_state} { grid remove $scry}

	# Setup bindings for the listbox
	$this Configure -state $state

        # Setup destroy binding.
	bind $winpath <Destroy> "$this WinDestroy %W"
    }
    private method ListConfigure { args } { list } {
	# Protect against calls inside the constructor (due to
	# Constructor $args) made before $list is initialized.
	if {[info exists list]} {
	    eval $list configure $args
	}
    }
    private method SetListState { newstate } {
	if {[string match disabled $newstate]} {
	    # Disable
	    $list configure -xscrollcommand {} -yscrollcommand {}
	    eval $list configure $list_disabled_colors
            foreach win [list $list $scrx $scry] {
                bind $win <Button> { break }
                bind $win <ButtonRelease> { break }
                bind $win <KeyRelease> { break }
                bind $win <Key> { break }
                bind $win <Motion> { break }
                bind $win <Leave> { break }
            }
	} else {
	    # Enable
            foreach win [list $list $scrx $scry] {
                bind $win <Button> {}
                bind $win <ButtonRelease> {}
                bind $win <KeyRelease> {}
                bind $win <Key> {}
                bind $win <Motion> {}
                bind $win <Leave> {}
            }
	    $list configure -xscrollcommand "$scrx set" \
		-yscrollcommand "$scry set"
	    eval $list configure $list_normal_colors
	    bind $list <Button> "focus %W"
	    bind $list <ButtonRelease> "$this Select %W"
	    bind $list <KeyRelease> "$this Select %W"
            if {[string match {} [$list curselection]]} {
                # Nothing selected, so select from -variable
                $this SelectItems $variable
            } else {
                # Value selected, so set -variable from selection
                $this Select
            }
	}
    }
    callback method SelectItems { varname args } { list selectmode} {
        # The input should be either a simple variable,
        # or an array name followed by the name of an
        # element in that array (as first element of $args).
        # This routine then turns the selection on for the
        # listbox entry, if any, that exactly matches a value
        # in the list held by the import variable.
        # NOTE: This routine will fail if varname is an array
        #  and no element is given, i.e., $args == {}
        if {![string match {} [lindex $args 0]]} {
            # Trace on array element reference
            set varname "${varname}([lindex $args 0])"
        }
        if {[catch {upvar $varname shadow}] || \
                ![info exists list] || ![info exists shadow]} {
            return
        }
        set boxlist [$list get 0 end]
        $list selection clear 0 end
	foreach item $shadow {
            set index [lsearch -exact $boxlist $item]
            if {$index>=0} {
        	$list selection set $index
            }
	}
	if {[string match browse $selectmode] && [llength $shadow] == 1 
		&& $index>=0} {
            $list see $index
            $list activate $index
	}
    }
    callback method Select { args } { list variable } {
	# Note: $args not used
	if {![catch {upvar #0 $variable shadow}]} {
	    set selection {}
	    foreach index [$list curselection] {
		lappend selection [$list get $index]
	    }
	    if {![info exists shadow] || \
		    [string compare $selection $shadow]!=0} {
		set shadow $selection
	    }
	}
    }
    method TakeFocus { widget } {
        # Used by keyboard traversal routines
        if {[string match disabled $state]} {
            return 0  ;# Don't take focus from tab
        }
        return {} ;# Allow focus depending on widget state and bindings
    }
    method Insert { index args } { list variable } {
        # Add to list
	eval {$list insert $index} $args
        # See if any if the items should be selected, based
        # on the value of the tied variable
        catch {
            upvar #0 $variable shadow
            $this SelectItems shadow
        }
        # Adjust keyboard insertion point to last selected
        # item, or 0 if none are selected.  Also scroll as
        # as necessary to make the insertion point visible.
        set pt [lindex [$list curselection] end]
        if {[string match {} $pt]} { set pt 0 }
        $list see $pt
        $list activate $pt
    }
    method Remove { args } { list } {
	# Interface to the listbox 'delete' command
	eval {$list delete} $args
    }

    # Methods providing Tk-style naming convention interfaces
    # to Ow_ListBox methods.  'delete' is absent because it
    # is too close in name to the Oc_Class intrinsic 'Delete'.
    method cget { option } {} { $this Cget $option }
    method configure { args } {} { eval $this Configure $args }
    method insert { args } {} { eval $this Insert $args }

    # Destructor methods
    method WinDestroy { w } { winpath } {
	if {[string compare $winpath $w]!=0} { return } ;# Child destroy event
	$this Delete
    }
    Destructor {
        catch {
            if {![catch {upvar #0 $variable shadow}]} {
                # Remove trace
                trace vdelete shadow w "$this SelectItems"
            }
        }

	if {[info exists winpath] && [winfo exists $winpath]} {
            # Remove <Destroy> binding
	    bind $winpath <Destroy> {}
	    # Destroy instance frame, all children, and all bindings
	    destroy $winpath
	}
    }
}
