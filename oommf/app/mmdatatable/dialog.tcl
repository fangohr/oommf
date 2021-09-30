proc FormatDialog { title allow_select default_short default_format \
	default_justify } {
    # Global dialog box for adjusting data display format.  Imports are
    # title for dialog box, default values, and whether to allow "Adjust:
    # Selected".  The return value is either an empty string (if the user
    # selects 'Cancel'), or else a 4-elt list of desired format (e.g. "%g"),
    # justification (one of "l", "c", or "r"), short choice (0 or 1), and
    # adjust selection (either "all" or "selected").
    package require Ow
    set parent [focus]    ;# Position dialog over toplevel with focus.
    if {[string match {} $parent]} {
	set parent "."
    } else {
	set parent [winfo toplevel $parent]
    }

    set w [toplevel .dtFormatDialog]
    wm group $w .
    set bracewidth [Ow_SetWindowTitle $w $title]
    set brace [canvas $w.brace -width $bracewidth -height 0 -borderwidth 0 \
                  -highlightthickness 0]
    grid $brace - - - -sticky we
    # Resize root window when OID is assigned:
    Oc_EventHandler New _ Net_Account NewTitle \
       [list $brace configure -width %winwidth]

    Ow_PositionChild $w $parent

    global fmtdlg
    if {$allow_select} {
        set fmtdlg(adj) "selected"
    } else {
        set fmtdlg(adj) "all"
    }
    set fmtdlg(jst) $default_justify
    set fmtdlg(fmt) $default_format
    set fmtdlg(sht) $default_short

    set adjlabel [label $w.adjlabel -text "Adjust:"]
    set adjsel [radiobutton $w.adjsel -text "Selected" \
            -variable fmtdlg(adj) -value "selected"]
    if {!$allow_select} { $adjsel configure -state disabled }
    set adjall [radiobutton $w.adjall -text "All" \
            -variable fmtdlg(adj) -value "all"]

    set shortlabel [label $w.shortlabel -text "Names:"]
    set shortNo [radiobutton $w.shortNo -text "Long" \
	    -variable fmtdlg(sht) -value 0]
    set shortYes [radiobutton $w.shortYes -text "Short" \
	    -variable fmtdlg(sht) -value 1]

    set jstlabel [label $w.jstlabel -text "Justify:"]
    set jstleft [radiobutton $w.jstleft -text "Left" \
                    -variable fmtdlg(jst) -value "l"]
    set jstcenter [radiobutton $w.jstcenter -text "Center" \
                      -variable fmtdlg(jst) -value "c"]
    set jstright [radiobutton $w.jstright -text "Right" \
                     -variable fmtdlg(jst) -value "r"]

    set fmtlabel [label $w.fmtlabel -text "Format:"]
    set fmtentry [entry $w.fmtentry -textvariable fmtdlg(fmt)]
    $fmtentry icursor end

    set btnframe [frame $w.btn]
    global dtcmd
    set btnOK [button $w.btnOK -text "OK" \
            -command "set dtcmd 0"]
    set btnCancel [button $w.btnCancel -text "Cancel" \
            -command "set dtcmd 1"]
    wm protocol $w WM_DELETE_WINDOW "$btnCancel invoke"

    pack $btnOK $btnCancel -side left -expand 1 -padx 5 -in $btnframe

    grid $adjlabel $adjsel $adjall x
    grid $shortlabel $shortNo $shortYes x
    grid $jstlabel $jstleft $jstcenter $jstright
    grid $fmtlabel $fmtentry - -
    grid $btnframe - - - -ipady 5
    grid $adjlabel $shortlabel $fmtlabel $jstlabel -sticky e
    grid $adjsel $adjall $shortNo $shortYes  -sticky w
    grid $jstleft $jstcenter $jstright -sticky w
    grid $fmtentry $btnframe -sticky we
    grid columnconfigure $w all -weight 1
    grid rowconfigure    $w all -weight 1
    
    # Extra bindings
    bind $btnOK <Key-Return> "$btnOK invoke"
    bind $btnCancel <Key-Return> "$btnCancel invoke"
    bind $w <Key-Escape> "$btnCancel invoke"
    bind $fmtentry <Key-Return> [bind all <Tab>]
    ## Hmm, I'm not sure the last is a good idea...

    update idletasks
    focus $w
    catch {tkwait visibility $w; grab $w }
    # Note: It appears dangerous to put a "grab" on a window
    #  before it is visible.  (Errant mouse clicks???)
    # Also, the sequence (including order) of
    #  'update idletasks ; focus $w ; catch ...'
    # seems to protect against binding reentrancy from the
    # user clicking too many times, or hitting keys too fast.
 
    Ow_SetIcon $w
    focus $fmtentry
    catch {tkwait variable dtcmd }
    if {$dtcmd==0} {
        # OK invoked
        set result [list  $fmtdlg(adj) $fmtdlg(sht) $fmtdlg(fmt) $fmtdlg(jst)]
    } else {
        # Cancel invoked
        set result {}
    }
    unset fmtdlg
 
    grab release $w
    after 10 "destroy $w" ;# "after" is a workaround for Tk 8.5.x bug
    return $result
}

