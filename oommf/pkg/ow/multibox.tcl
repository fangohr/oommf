# FILE: multibox.tcl
#
# Multi-entry box dialog sub-widget
#
# Last modified on: $Date: 2007/03/21 01:17:09 $
# Last modified by: $Author: donahue $
#
# debugging enable/disable/level
#
#   Initialize with -labellist set to a list of label+default+options
# triples.  One entry box is create for each label in -labellist.  The
# default and option elements may be omitted, in which case {} is
# substituted.  The "options" element is also a list, and is passed to
# the entry box constructor, expanded with an 'eval'.
#   A <Key-Return> event in any entry box generates a callback with
# ACTIONID of UPDATE and a 2 element list, {label} {text string}.
# Use the method LookUp to get the text string currently associated
# with a given label.  The method Names returns a list of all the
# labels, and Get returns a list of label+text string pairs.
# Note that neither the Names nor the Get return lists are in
# any particular order.
#  The method Set sets the entrybox text contents.  This is used
# analogously to the "array set" command.  Pass in label+value pairs.
# This adjusts both the displayed and the "committed" entrybox values.
#   Other initialization options include -title (leave unset if you
# don't want a title bar), and -labelwidth and -valuewidth to set the
# common label and entry value widget widths.  The default values for
# these is 7 and 16, respectively.
#   The method EntryConfigure allows access to the individual entrybox
# configure methods.
Oc_Class Ow_MultiBox {
    private variable titlebox
    private array variable boxarray
    const public variable winpath
    const public variable outer_frame_options = "-bd 4 -relief ridge"
    const public variable labellist
    const public variable labelwidth = 7
    const public variable valuewidth = 16
    const public variable title
    const public variable titlewidth = 0
    public variable callback = Oc_Nop
    Constructor {basewinpath args} {
	# Don't add '.' suffix if there already is one.
	# (This happens chiefly when basewinpath is exactly '.')
	if {![string match {.} \
		[string index $basewinpath \
		[expr [string length $basewinpath]-1]]]} {
	    append basewinpath {.}
	}
	eval $this Configure -winpath ${basewinpath}w$this $args
        if {![info exists labellist]} {
            error "option '-labellist' must be specified"
        }

	# Temporarily disable callbacks until construction is complete
	set callback_keep $callback ;	set callback Oc_Nop

	# Pack entire subwidget into a subframe.  This makes
	# housekeeping easier, since the only bindings on
	# $winpath will be ones added from inside the class,
	# and also all subwindows can be destroyed by simply
	# destroying $winpath.
	eval frame $winpath $outer_frame_options

	# Title (if any)
	if {[info exists title]} {
	    set titlebox [label $winpath.title \
		    -text $title -bd 2 -relief raised]
	    if {[string compare $titlewidth ""]!=0} {
		$titlebox configure -width $titlewidth
	    }
	    pack $titlebox -side top -anchor n -fill x
	} else {
	    set title {}
	}

	# Entry box widgets and array
	foreach triple $labellist {
	    set entryname [lindex $triple 0]
	    set entryvalue [lindex $triple 1]
	    set entryoptions [lindex $triple 2]
            eval {Ow_EntryBox New widget $winpath \
		    -label $entryname -labelwidth $labelwidth \
		    -value $entryvalue -valuewidth $valuewidth \
		    -callback "$this EntryBoxCallback"} $entryoptions
	    set boxarray($entryname) $widget
	    pack [$widget Cget -winpath] -side top -fill x -expand 0
	}

	bind $winpath <Destroy> "$this WinDestroy %W"

	# Re-enable callbacks
	set callback $callback_keep
    }
    method EntryBoxCallback { widget actionid args } { callback } {
	eval $callback $this UPDATE [$widget Cget -label] \
		[lindex $args 1]
    }
    method EntryConfigure { label args } {
	set widget $boxarray($label)
	return [eval {$widget} Configure $args]
    }
    method LookUp { label } { boxarray } {
	return [$boxarray($label) ReadEntryString]
    }
    method Names {} { boxarray } {
	return [array names $boxarray]
    }
    method Get {} { boxarray } {
	# Returns all label/value pairs from entry boxes.
	# NOTE: As in an 'array get', the output ordering
	#  is probably not the same as the input ordering.
	set mylist {}
	foreach label [array names $boxarray] {
	    lappend mylist $label \
		    [$boxarray($label) ReadEntryString]
	}
	return $mylist
    }
    method Set { args } { boxarray } {
	foreach { label text } $args {
	    set widget $boxarray($label)
	    $widget Set $text
	}
    }
    method WinDestroy { w } { winpath } {
	if {[info exists winpath] && [string compare $winpath $w]!=0} {
	    return    ;# Child destroy event
	}
	$this Delete
    }
    Destructor {
	catch { unset $boxarray }
	if {[info exists winpath] && [winfo exists $winpath]} {
            # Remove <Destroy> binding
	    bind $winpath <Destroy> {}
	    # Destroy instance frame, all children, and bindings
	    destroy $winpath
	}
    }
}
