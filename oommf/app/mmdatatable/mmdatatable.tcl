# FILE: mmDataTable.tcl
#
# Display Table of data values
#

# This code can be source'd from inside another script by setting
# no_net and no_event_loop as desired before sourcing this file.
# If no_net is set true, use the proc "AddTriples { triples }" to add
# data.

if {![info exists no_net]} {
    set no_net 0      ;# Default is use net
}

if {![info exists no_event_loop]} {
    set no_event_loop 0   ;# Default is to enter the event loop
}

# Support libraries
package require Oc
package require Tk
package require Ow
wm withdraw .

Oc_Main SetAppName mmDataTable
Oc_Main SetVersion 2.0b0
regexp \\\044Date:(.*)\\\044 {$Date: 2015/10/09 05:50:34 $} _ date
Oc_Main SetDate [string trim $date]
# regexp \\\044Author:(.*)\\\044 {$Author: donahue $} _ author
# Oc_Main SetAuthor [Oc_Person Lookup [string trim $author]]
Oc_Main SetAuthor [Oc_Person Lookup donahue]
Oc_Main SetHelpURL [Oc_Url FromFilename [file join [file dirname \
        [file dirname [file dirname [Oc_DirectPathname [info \
        script]]]]] doc userguide userguide \
	Data_Table_Display_mmDataTa.html]]
Oc_Main SetDataRole consumer

Oc_CommandLine ActivateOptionSet Net

Oc_CommandLine Parse $argv

source [file join [file dirname [info script]] dialog.tcl]

# Death
proc Die { win } {
    if {![string match "." $win]} { return }  ;# Child destroy event
    exit
}
bind . <Destroy> {+Die %W}
wm protocol . WM_DELETE_WINDOW { exit }
Oc_EventHandler New _ Oc_Main Exit {proc Die win {}}

proc InitArrayDefaults {} {
    global fmt justify shortlabel select displaylist jobname
    set fmt(default) "%g"    ;# Default format
    set justify(default) "r" ;# Default justification
    set shortlabel(default) 1 ;# Default label format (1==short)
    label .dummylabel
    set select(offback) [.dummylabel cget -background]
    set select(offfore) [.dummylabel cget -foreground]
    set select(offerror) red
    destroy .dummylabel
    text .dummytext
    set select(onback)  [.dummytext cget -selectbackground]
    set select(onfore)  [.dummytext cget -selectforeground]
    if {[string match {} $select(onfore)]} {
       set select(onfore) $select(offfore)
    }
    destroy .dummytext
    set displaylist {}
    set jobname {}
}

proc MakeMenuLabel { label } {
    regsub -all "\[\r\n\t\]" $label " " txt
    return $txt
}

proc RemoveLabel { label } {
    # Remove label from display
    global onoff
    if {$onoff($label)} {
	set onoff($label) 0
	DisplayListAppend $label
    }

    # Remove label from dmenu
    global dmenu
    set menulabel [MakeMenuLabel $label]
    set lastindex [$dmenu index end]
    for {set i 0} {$i<=$lastindex} {incr i} {
	if {![catch {$dmenu entrycget $i -label} mlbl] && \
		[string compare $mlbl $menulabel]==0} {
	    $dmenu delete $i
            # There appears to be a bug in Tcl/Tk 8.4.12.0 (others?)
            # on Windows/XP whereby deleting a menu entry causes all
            # remaining entries below the deleted entry to not
            # highlight when the mouse is placed over them.  One
            # workaround is to toggle the -tearoff flag.
            $dmenu configure -tearoff [expr {![$dmenu cget -tearoff]}]
            $dmenu configure -tearoff [expr {![$dmenu cget -tearoff]}]
	    break
	}
    }

    # Remove label from global arrays
    foreach arr \
	    {unit fmt justify value fmtvalue shortlabel onoff select} {
	global $arr
	catch {unset ${arr}($label)}
	## Note: The justify, shortlabel and fmtvalue arrays are only
	##  filled as needed.
    }
}

proc Reset {} {
    # First destroy display panels and "Data" menu entries
    global panel
    foreach win [winfo children $panel] {
	grid forget $win
	destroy $win
    }

    # Clear "Data" menu
    global dmenu
    $dmenu delete 0 end

    # Empty global arrays
    foreach arr [list unit fmt fmterror justify shortlabel value fmtvalue \
	    onoff select displaylist jobname] {
	global $arr
	if {[info exists $arr]} {
	    unset $arr
	}
    }

    # Re-initialize default array values
    InitArrayDefaults

    # I (mjd, 12-2000) think this is a bug in pack: when the
    # last panel is removed, the display doesn't get resized.
    # The following hack is a workaround.
    if {[llength $displaylist]==0} {
	$panel configure -height 1 -width 1
	$panel configure -height 0 -width 0
    }

    # Set geometry to autosize
    wm geometry . {}
}

set menubar .mb
foreach {fmenu dmenu omenu hmenu} \
        [Ow_MakeMenubar . $menubar File Data Options Help] {}
$fmenu add command -label "Reset" -command { Reset } -underline 0
$fmenu add separator
$fmenu add command -label "Exit" -command { exit } -underline 1
$dmenu configure -tearoff 1 -title "Data: [Oc_Main GetInstanceName]"
$omenu add command -label "Format..." -underline 0 \
	-command { ChangeFormatInteractive }
$omenu add command -label "Select all" -underline 0 \
        -command { ToggleSelectAll 1 }
$omenu add command -label "Select none" -underline 7 \
        -command { ToggleSelectAll 0 }
$omenu add command -label "Copy" -underline 0 \
        -command { CopySelected } -accelerator "Ctrl+C"
bind . <Control-c> CopySelected
Ow_StdHelpMenu $hmenu

set menuwidth [Ow_GuessMenuWidth $menubar]
set bracewidth [Ow_SetWindowTitle . [Oc_Main GetInstanceName]]
if {$bracewidth<$menuwidth} {
   set bracewidth $menuwidth
}
set brace [canvas .brace -width $bracewidth -height 0 -borderwidth 0 \
        -highlightthickness 0]
pack $brace -side top
# Resize root window when OID is assigned:
Oc_EventHandler New _ Net_Account NewTitle [subst -nocommands {
  $brace configure -width \
    [expr {%winwidth<$menuwidth ? $menuwidth : %winwidth}]
}]

set panel [frame .panel -relief groove]
if {[Ow_IsAqua]} {
   $panel configure -bd 4 -relief ridge
}
pack $panel -side top -fill x -expand 1 -anchor nw
grid columnconfigure $panel 0 -weight 4
grid columnconfigure $panel 1 -weight 0
grid columnconfigure $panel 2 -weight 1
if {[Ow_IsAqua]} {
   # Add some padding to allow space for Aqua window resize
   # control in lower righthand corner
   $panel configure -bd 4 -relief ridge
   grid columnconfigure $panel 3 -minsize 10
}

wm iconname . [wm title .]
Ow_SetIcon .
wm deiconify .

proc GetSelectList {} {
    global select displaylist
    set selectlist {}
    foreach label $displaylist {
        if { $select($label) } {
            lappend selectlist $label
        }
    }
    return $selectlist
}

proc MakeShortName { name } {
    regsub {^[^:]*::?} $name {} txt
    return $txt
}

proc ReturnShortNameMatch { nickname } {
    # Returns a list of the full names of all entries
    # with short name that matches nickname
    global value
    set labellist [array names value]
    set matchlist {}
    foreach lbl $labellist {
	if {[string compare $nickname [MakeShortName $lbl]]==0} {
	    lappend matchlist $lbl
	}
    }
    return $matchlist
}

proc ToggleSelectAll { {state {}} } {
    # If 'state' is {}, toggles selection of all displayed entries,
    # Otherwise sets selection of all displayed entries as specified.
    # In all cases, also toggles selection of on all non-displayed
    # entries.
    global value onoff select
    foreach label [array names value] {
        if {$onoff($label)} {
            ToggleSelect $label $state
        } else {
            # Force off if not displayed (safety)
            set select($label) 0
        }
    }
}

proc ToggleSelect { label { state {} } } {
    # Sets select status for entry 'label'.  If 'state' is {}, then
    # toggles current state, in which case this routine *assumes*
    # that select($label) is previously initialized.
    global select panel onoff fmterror
    if {[string match {} $state]} { set state [expr {!$select($label)}] }
    set foo [CompressLabel $label]
    if {$state} {
        # Select
        if {[catch {
            $panel.label$foo configure \
		    -background $select(onback) -foreground $select(onfore)
            $panel.sep$foo configure \
		    -background $select(onback) -foreground $select(onfore)
            $panel.value$foo configure \
		    -background $select(onback) -foreground $select(onfore)}]
        } {
            # Error occured; Entry is probably hidden.  Override
            # request and deselect.
            set state 0
        }
    }
    if {!$state} {
        # Deselect
        catch {
            $panel.label$foo configure \
		    -background $select(offback) -foreground $select(offfore)
            $panel.sep$foo configure \
		    -background $select(offback) -foreground $select(offfore)
	    if {$fmterror($label)} {
		set color $select(offerror)
	    } else {
		set color $select(offfore)
	    }
            $panel.value$foo configure \
		    -background $select(offback) -foreground $color
        }
    }
    set select($label) $state
}

proc FormatValue { fmt val errname } {
    upvar 1 $errname check
    set check 0
    if {![catch {format $fmt $val} result]} { return $result }
    # Good chance $fmt is of "%d"-type, but
    # $val looks like "3.0".  Fix if possible.
    if {![catch {expr {int($val)}} tmp] && \
	    $tmp == $val && \
	    ![catch {format $fmt $tmp} result]} {
	set val $result
    } else {
	set check 1
    }
    return $val ;# Fallback solution is to return unformatted string
}

proc SetDisplayName { label } {
    global shortlabel unit panel
    set foo [CompressLabel $label]
    if {$shortlabel($label)} {
	set txt [MakeShortName $label]
    } else {
	set txt $label
    }
    if {[string length $unit($label)]} {
	append txt " ($unit($label))"
    }
    $panel.label$foo configure -text $txt -anchor e
}

proc DisplayListAppend { label } {
    # Routine for adding and removing entries from display
    global panel unit onoff fmt value
    global fmtvalue fmterror
    global justify shortlabel select displaylist
    set foo [CompressLabel $label]
    if {$onoff($label)} {
        # Display entry
	if {![winfo exists $panel.label$foo]} {
	    if {![info exists shortlabel($label)]} {
		set shortlabel($label) $shortlabel(default)
		if {$shortlabel($label)} {
		    # Use short form by default only if
		    # there are no other entries with the
		    # same shortname
		    if {[llength [ReturnShortNameMatch \
			    [MakeShortName $label]]]>1} {
			set shortlabel($label) 0
		    }
		}
	    }
	    label $panel.label$foo
	    SetDisplayName $label
            bind $panel.label$foo <Button-1> [list ToggleSelect $label]
            bind $panel.label$foo <Button-3> [list QuickFormat $label]
            if {[Ow_IsAqua]} {
               # Macs have only one button, so use "Command" modifier
               # key with button 1 as a stand-in for Button-3.  Tk
               # registers the Command key as Mod1, but on Windows Tk
               # associates Mod1 with the NumLock key.  Some laptop
               # keyboards lack a NumLock key, in which case the
               # system may report NumLock as always on.  This wrecks
               # havoc with other Button-1 bindings, so only add these
               # binding on the Mac.
               bind $panel.label$foo <Mod1-Button-1> [list QuickFormat $label]
            }
            # If using OS X + X11, then the "Command" key maps to "Meta"
            # (instead of Mod1).  Since I'm not currently able to find
            # any other systems that bind anything to Meta, it is
            # probably(?) okay to make this binding always.
            bind $panel.label$foo <Meta-Button-1> [list QuickFormat $label]

            lappend displaylist $label
	}
	if {![winfo exists $panel.sep$foo]} {
	    label $panel.sep$foo -text ":"
            bind $panel.sep$foo <Button-1> [list ToggleSelect $label]
            bind $panel.sep$foo <Button-3> [list QuickFormat $label]
            if {[Ow_IsAqua]} {
               bind $panel.sep$foo <Mod1-Button-1> [list QuickFormat $label]
            }
            bind $panel.sep$foo <Meta-Button-1> [list QuickFormat $label]
	}
	if {![winfo exists $panel.value$foo]} {
	    set fmtvalue($label) \
		    [FormatValue $fmt($label) $value($label) fmterror($label)]
            label $panel.value$foo -font TkFixedFont \
               -textvariable fmtvalue($label)
	    if {![info exists justify($label)]} {
		set justify($label) $justify(default)
	    }
	    if {[string match "r" $justify($label)]} {
		$panel.value$foo configure -anchor e
	    } elseif {[string match "c" $justify($label)]} {
		$panel.value$foo configure -anchor center
	    } else {
		$panel.value$foo configure -anchor w
	    }
            bind $panel.value$foo <Button-1> [list ToggleSelect $label]
            bind $panel.value$foo <Button-3> [list QuickFormat $label]
            if {[Ow_IsAqua]} {
               bind $panel.value$foo <Mod1-Button-1> [list QuickFormat $label]
            }
            bind $panel.value$foo <Meta-Button-1> [list QuickFormat $label]
	}
        set select($label) 0
	ToggleSelect $label 0 ;# Setup fore/background colors
	grid $panel.label$foo $panel.sep$foo $panel.value$foo
	grid configure $panel.label$foo -sticky ew
	grid configure $panel.value$foo -sticky ew
    } else {
        # Hide entry
	if {[winfo exists $panel.label$foo] && \
		[winfo exists $panel.value$foo]} {
	    grid forget $panel.label$foo $panel.value$foo $panel.sep$foo
	    destroy $panel.label$foo $panel.value$foo $panel.sep$foo
	}
        while {[set index [lsearch -exact $displaylist $label]]>=0} {
            set displaylist [lreplace $displaylist $index $index]
        }

	# I (mjd, 12-2000) think this is a bug in pack: when the
	# last panel is removed, the display doesn't get resized.
	# The following hack is a workaround.
	if {[llength $displaylist]==0} {
	    $panel configure -height 1 -width 1
	    $panel configure -height 0 -width 0
	}
    }
    # Hack to work around broken size propagation.  This mostly
    # works, but doesn't catch the case where the window needs
    # to resize because a value has grown too big to be fully
    # displayed at the current window geometry.  That can be
    # caught with a binding to the <Configure> event, e.g.
    #    bind $panel <Configure> { Ow_PropagateGeometry . }
    # but this effectively disables all user resizing.  That
    # cure is arguably worse than the disease, so instead just
    # catch selection changes.
    Ow_PropagateGeometry .
}

proc ChangeFormat { newformat {labellist {}} } {
    # Resets default format iff labellist is an empty list
    global fmt value fmtvalue fmterror onoff
    if {[llength $labellist]<1} {
	set lbllist [array names value]
	set fmt(default) $newformat
    } else {
	set lbllist $labellist
    }
    foreach lbl $lbllist {
	set fmt($lbl) $newformat
	if {[info exists onoff($lbl)] && $onoff($lbl) && \
		[info exists value($lbl)] } {
	    set fmtvalue($lbl) \
		    [FormatValue $newformat $value($lbl) errcheck]
	    if {$errcheck != $fmterror($lbl)} {
		set fmterror($lbl) $errcheck
		global select
		ToggleSelect $lbl $select($lbl)
	    }
	}
    }
}

proc ChangeJustify { newside {labellist {}} } {
    # Resets default justify iff labellist is an empty list
    global panel justify value fmtvalue onoff
    switch -exact -- $newside {
        r { set anchor e }
        c { set anchor center }
        l -
        default { set anchor w ; set newside "l" }
    }
    if {[llength $labellist]<1} {
	set lbllist [array names value]
	set justify(default) $newside
    } else {
	set lbllist $labellist
    }
    foreach lbl $lbllist {
	set justify($lbl) $newside
	if {[info exists onoff($lbl)] && $onoff($lbl) && \
		[info exists value($lbl)] } {
	    set foo [CompressLabel $lbl]
            $panel.value$foo configure -anchor $anchor
	}
    }
}

proc ChangeLabelLength { shortchoice {labellist {}} } {
    global panel shortlabel value onoff unit
    if {[llength $labellist]<1} {
	set labellist [array names value]
	set shortlabel(default) $shortchoice
    }
    foreach lbl $labellist {
	set shortlabel($lbl) $shortchoice
	if {[info exists onoff($lbl)] && $onoff($lbl) } {
	    set foo [CompressLabel $lbl]
	    if {[winfo exists $panel.label$foo]} {
		if {$shortchoice} {
		    set txt [MakeShortName $lbl]
		} else {
		    set txt $lbl
		}
		if {[string length $unit($lbl)]} {
		    append txt " ($unit($lbl))"
		}
		$panel.label$foo configure -text $txt
	    }
	}
    }
}

proc ChangeFormatInteractive { {label {}} } {
    # If import label is specified, use the current settings of that
    # label to initialize the format and justification specs.
    global fmt justify shortlabel
    if {[string match {} $label] || ![info exists fmt($label)] \
	|| ![info exists justify($label)]} {
	set myfmt $fmt(default)
	set myjst $justify(default)
        set myshort $shortlabel(default)
    } else {
	set myfmt $fmt($label)
	set myjst $justify($label)
        set myshort $shortlabel($label)
    }
    set selectlist [GetSelectList]
    if {[string match {} $selectlist]} {
        set allowselect 0
    } else {
        set allowselect 1
    }
    set result [FormatDialog "Format -- [Oc_Main GetTitle]" \
	    $allowselect $myshort $myfmt $myjst]
    if {![string match {} $result]} {
        foreach {mysel myshort myfmt myjst}  $result {}
        if {[string match "all" $mysel]} {
	    set selectlist {}
	}
        ChangeFormat $myfmt $selectlist
        ChangeJustify $myjst $selectlist
	ChangeLabelLength $myshort $selectlist
	ToggleSelectAll 0
    }
}

proc QuickFormat { label } {
    ToggleSelect $label 1
    ChangeFormatInteractive $label
}

proc dtSelectionHandler { str offset maxbytes } {
    return [string range $str $offset [expr {$offset+$maxbytes}]]
}

proc CopySelected {} {
    # Copies "selected" entries into both the PRIMARY and
    # CLIPBOARD selections.
    global panel fmtvalue justify

    # Collect display labels and values, and determine lengths
    # (to support column alignment).
    set lbllen 0
    set vallen 0
    set dsplist {}
    foreach label [GetSelectList] {
	set foo [CompressLabel $label]
	set lbl "[$panel.label$foo cget -text] [$panel.sep$foo cget -text] "
	set len [string length $lbl]
	if {$len>$lbllen} { set lbllen $len }
	lappend dsplist $lbl

	set val $fmtvalue($label)
	set len [string length $val]
	if {$len>$vallen} { set vallen $len }
	lappend dsplist $val

	lappend dsplist $justify($label)
    }

    # Construct display string, with embedded newlines
    set str {}
    foreach { lbl val jst } $dsplist {
	append str [format "%*s" $lbllen $lbl]
	switch -exact -- $jst {
	    r {
		append str [format "%*s\n" $vallen $val]
	    }
	    c {
		set len [expr {($vallen + [string length $val])/2}]
		append str [format "%*s\n" $len $val]
	    }
	    l -
	    default: {
		append str [format "%s\n" $val]
	    }
	}
    }

    # Remove trailing newline
    set str [string trimright $str "\n"]

    # Copy to clipboard
    clipboard clear
    clipboard append -- $str

    # Make available as primary selection
    selection handle . [list dtSelectionHandler $str]
    selection own .
}

######################################################################
# PROC: AddTriples
#
# Data import routine for mmDataTable widget
# Parameters:
#  Each input consists of a triple of values: label, unit, value
# Data storage:
#  The first element of each import triple is used as an index into
# the following collection of global arrays:
#
#     unit($label)     :  Value units
#     fmt($label)      :  Format code to use for display
#     justify($label)  :  Value justification: one of l, c, or r
#     value($label)    :  Value
#     fmtvalue($label) :  Formatted value
#     fmterror($label) :  If true, then last format attempt produced an error
#     shortlabel($label) : Display full/short version of label
#     onoff($label)    :  Whether or not value is currently displayed
#     select($label)   :  Whether or not value is currently selected
#     displaylist      :  List of displayed entries, in display order
#     jobname          :  Requested output filename; this is used to
#                         detect new problem load event, and trigger
#                         the removal of unused columns
#
#  The fmt, justify and shortlabel arrays also include a "default" element
# which is applied to new labels.  The select array includes elts
# "onfore/back" and "offfore/back" which are the selected & deselected
# fore/background colors.
#  Use 'array names value' to get a list of all labels.  The justify,
# shortlabel and fmtvalue arrays are filled on demand, i.e., they may
# be shorter than the other arrays.
#
InitArrayDefaults

proc CompressLabel { lbl } {
    regsub -all "\[ \r\t\n\]" $lbl {} foo
    return $foo
}
proc AddTriples { triples } {
    global unit fmt value fmtvalue fmterror dmenu onoff select jobname
    set newjobname $jobname
    foreach trip $triples {
        set lbl [lindex $trip 0]
	if {[string match @* $lbl]} {
	    set newjobname $lbl
	    continue
	}
	set value($lbl) [lindex $trip 2]
        if {![info exists unit($lbl)]} {
            set unit($lbl) [lindex $trip 1]
            set onoff($lbl) 0
	    set fmt($lbl) $fmt(default)
	    set fmterror($lbl) 0
            set select($lbl) 0
            $dmenu add checkbutton -label [MakeMenuLabel $lbl] \
                    -variable onoff($lbl) \
                    -command [list DisplayListAppend $lbl]
        }
	if {$onoff($lbl)} {
	    set fmtvalue($lbl) \
		    [FormatValue $fmt($lbl) $value($lbl) errcheck]
	    if {$errcheck != $fmterror($lbl)} {
		set fmterror($lbl) $errcheck
		global select
		ToggleSelect $lbl $select($lbl)
	    }
	}
    }
    if {[string compare $jobname $newjobname]!=0} {
	# Job name (actually output filename) change
	set new_lbls {}
	foreach trip $triples {
	    set lbl [lindex $trip 0]
	    if {![string match @* $lbl]} {
		lappend new_lbls $lbl
		set new_unit($lbl) [lindex $trip 1]
	    }
	}
	if {[llength $new_lbls]>0} {
	    # Remove any columns not in new_lbls list
	    foreach lbl [array names value] {
		if {[lsearch -exact $new_lbls $lbl]<0} {
		    # Label not in new label list
		    RemoveLabel $lbl
		} elseif {[string compare $new_unit($lbl) $unit($lbl)]!=0} {
		    # Units change; keep label, but update units.
		    set unit($lbl) $new_unit($lbl)
		    SetDisplayName $lbl
		}
	    }
	}
	set jobname $newjobname
    }
}

if {!$no_net} {
    package require Net 2
    Net_Protocol New protocol -name "OOMMF DataTable protocol 0.1"
    $protocol AddMessage start DataTable { triples } {
	AddTriples $triples
	return [list start [list 0 0]]
    }
    Net_Server New server -protocol $protocol -alias [Oc_Main GetAppName]
    $server Start 0
} else {
   package forget Net
}

if {!$no_event_loop} {
    catch {
	if {[string match aqua [tk windowingsystem]]} {
	    # Bring window to front of window manager display stack.
	    # Otherwise, the new mmDataTable may be hidden by
	    # mmLaunch.
	    wm attributes . -topmost 1
	    after idle {raise . ;  wm attributes . -topmost 0}
	}
    }
    vwait forever
}

