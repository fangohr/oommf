# FILE: filedlg.tcl
#
# Generic file dialog box
#
# Last modified on: $Date: 2010/06/25 00:04:46 $
# Last modified by: $Author: donahue $

# NOTE: It appears that the SELECT callback must return an empty string
# to indicate success.

Oc_Class Ow_FileDlg {
    const public variable winpath
    const public variable outer_frame_options = "-bd 2 -relief raised"
    const public variable movable_panes = 1 ;# Whether or not to use
    # a movable pane between the file selection area and the rest
    # of the dialog.  Setting this to 1 enables this feature, provided
    # the panedwindow command exists.  If 0, then the relative widths
    # of the two areas are held at a constant ratio, which cannot be
    # adjusted by the end user.
    const public variable filter = "*"
    const public variable compress_suffix = {}
    const public variable dialog_title = "File Select"
    const public variable selection_title = "Selection"
    const public variable select_action_id = "SELECT"
    const public variable optionbox_setup = {} ;# Optional option widget
    ## setup callback.  If set != {}, Ow_FileDlg will call this routine
    ## during initialization with the name of this Ow_FileDlg widget, and
    ## the name of a frame widget.  The option setup routine should then
    ## construct itself inside that frame widget. Currently, no
    ## additional callbacks to the option widget are made. It is the
    ## responsibility of the Ow_FileDlg caller to query the option
    ## widget as necessary on general callbacks from Ow_FileDlg.

    const public variable descbox_setup = {} ;# Optional "description"
    ## widget setup callback.  If set != {}, Ow_FileDlg will call this
    ## routine during initialization with the name of this Ow_FileDlg
    ## widget, and the name of a frame widget.  The desc setup routine
    ## should then construct itself inside that frame widget. Currently,
    ## no additional callbacks to the desc widget are made. It is the
    ## responsibility of the Ow_FileDlg caller to query the desc widget
    ## as necessary on general callbacks from Ow_FileDlg.  For example
    ## use, see the mmDisp "Save as..." dialog box.

    public variable callback = Oc_Nop
    public variable delete_callback_arg = {}
    private variable filewidget = {}
    private variable selectionwidget = {}
    private variable dirwidget = {}

    # Browse control
    const public variable allow_browse = 0
    public variable browse_state = 0 { $this BrowseStateTrace }

    # Control box layout.  Choices are wide, tall, and auto.
    # This setting interacts with the auto setting of the
    # optbox_position control, but has visible effect on the
    # control box only if Browse is enabled.
    const public variable ctrlbox_layout = auto {
	switch -exact -- $ctrlbox_layout {
	    wide -
	    tall -
	    auto {}
	    default { 
		return -code error \
			"Bad ctrlbox_layout value: $ctrlbox_layout"
	    }
	}
    }

    # Option box position, relative to control box.  Choices
    # are top, left, right, bottom, or auto.
    const public variable optbox_position = auto {
        switch -exact -- $optbox_position {
            top -
            left -
            right -
            bottom -
            auto {}
            default {
                return -code error \
                        "Bad optbox_position value: $optbox_position"
            }
        }
    }


    # If menu_data is a two item list, consisting of menuname and
    # itemlabel, then this dialog box will automatically take care
    # of disabling itemlabel at construction, and re-enable itemlabel
    # on destruction.  If smart_menu is true, then rather than
    # disabling itemlabel, it gets recolored and redefined to raise
    # the existing dialog box.  On destruction, itemlabel is reset
    # to its previous definition.
    const public variable smart_menu = 1
    const public variable smart_menu_color = red
    const public variable menu_data = {}
    const private variable menu_cleanup = Oc_Nop

    # The following variables control filename checks performed on
    # SELECT action before calling $callback.
    public variable dir_must_exist = 1
    public variable file_must_exist = 0
    public variable valid_file_types = {file fifo socket}
    ## Set valid_file_types to {} to disable file type checking.
    ## Refer to the Tcl 'file type' command for a list of possible
    ## file types.  NOTE: If 'file type' returns "link", and "link"
    ## is not on this list, then Oc_ResolveLink is called to
    ## the underlying file type, which is then recompared to this
    ## list.  If you don't *ever* want links, then put "link" on
    ## this list and check the file type explicitly in the callback.

    Constructor {args} {
        eval $this Configure -winpath .w$this $args
        # Create toplevel frame to hold dialog
        toplevel $winpath
        if {[Ow_IsAqua]} {
           # Add some padding to allow space for Aqua window resize
           # control in lower righthand corner
           $winpath configure -bd 4 -relief flat -padx 5 -pady 5
        }
        wm group $winpath .
        set dialogwidth [Ow_SetWindowTitle $winpath $dialog_title]
        set brace [canvas ${winpath}.brace -width $dialogwidth \
                    -height 0 -borderwidth 0 -highlightthickness 0]
        pack $brace -side top
        if {![string match {} [info commands Ow_PositionChild]]} {
            Ow_PositionChild $winpath  ;# Position at +.25+.25 over '.'
        }
        wm protocol $winpath WM_DELETE_WINDOW \
                "$this SwitchBoard $this CLOSE"

        # Temporarily disable callbacks until construction is complete
        set callback_keep $callback ;   set callback Oc_Nop

        # If requested, reconfigure menu
        if {[llength $menu_data]==2} {
            set name [lindex $menu_data 0]
            set item [lindex $menu_data 1]
            if {$smart_menu} {
                set origcmd [$name entrycget $item -command]
                set menuorigfgcolor [$name entrycget $item -foreground]
                set menuorigafgcolor [$name entrycget $item -activeforeground]
                set menu_cleanup \
                        [list $name entryconfigure $item \
                          -command $origcmd \
                          -foreground $menuorigfgcolor\
                          -activeforeground $menuorigafgcolor]
                $name entryconfigure $item \
                        -command "Ow_RaiseWindow $winpath" \
                        -foreground $smart_menu_color \
                        -activeforeground $smart_menu_color
            } else {
                $name entryconfigure $item -state disabled
                set menu_cleanup \
                        [list $name entryconfigure $item -state normal]
            }
        }

        # Determine control and option box layouts
        switch -exact -- $ctrlbox_layout {
            wide { set tallctrl 0 }
            tall { set tallctrl 1 }
            default {
                # ctrlbox_layout == auto
                set tallctrl 0
                if {![string match {} $optionbox_setup]} {
		    switch -exact -- $optbox_position {
			left -
			right -
			auto { set tallctrl 1 }
		    }
                }
            }
        }
        set optpos $optbox_position
        if {[string match auto $optpos]} {
            if {$tallctrl} {
                set optpos left
            } else {
                set optpos bottom
            }
        }

        # Create subwidgets
        if {$movable_panes && [llength [info commands panedwindow]]} {
           # Movable panes
           set panedframe [panedwindow $winpath.pane \
                              -showhandle 0 -orient horizontal]
           pack $panedframe -side left -fill both -expand 1
        }
        set rightpane [frame $winpath.rightpane]

        Ow_FileSelect New filewidget $winpath \
                -callback "$this SwitchBoard" \
                -filter $filter \
                -compress_suffix $compress_suffix
        set neframe [frame $winpath.neframe] ;# Packing frame

	# The option box is created either before or after the
	# control box, depending on relative position.  This is
	# so keyboard focus traversal has a sensible order.
	set optframe {}
	if {([string match left $optpos] || \
		[string match top $optpos]) && \
		![string match {} $optionbox_setup]} {
            set optframe [frame $winpath.optframe -bd 4 -relief ridge]
            $optionbox_setup $this $optframe
        }

        set ctrlframe [frame $winpath.ctrlframe]
        if {$allow_browse} {
            checkbutton $ctrlframe.browse -text " Browse " \
                    -relief raised -anchor w \
                    -padx 8 -pady 4 \
                    -variable [$this GlobalName browse_state] \
                    -command "$this BrowseStateTrace"
        }
        button $ctrlframe.rescan -text " Rescan " \
                -padx 8 -pady 4 \
                -command [list $this SwitchBoard $this RESCAN]
        button $ctrlframe.close -text " Close " \
                -padx 8 -pady 4 \
                -command [list $this SwitchBoard $this CLOSE]
        button $ctrlframe.ok -text " OK " \
                -padx 8 -pady 4 \
                -command [list $this SwitchBoard $this SELECT]

	if {![string match {} $optionbox_setup] && \
		[string match {} $optframe]} {
            set optframe [frame $winpath.optframe -bd 4 -relief ridge]
            $optionbox_setup $this $optframe
        }

	# The description box is created immediately before the
	# selection widget.  This provides sensible keyboard focus
	# traversal order.
	set descframe {}
	if {![string match {} $descbox_setup]} {
            set descframe [frame $winpath.descframe -bd 4 -relief ridge]
            $descbox_setup $this $descframe
        }

        Ow_SelectBox New selectionwidget $winpath \
                -title $selection_title \
                -callback "$this SwitchBoard"
        Ow_DirSelect New dirwidget $winpath \
                -callback "$this SwitchBoard"

        $this BrowseStateTrace ;# Initialize ok button text

        # Initialize selection path
        $selectionwidget SetPath [$dirwidget Cget -value]

        # Setup and initialize tying "ok" button state to
        # non-empty File: selection.
        $selectionwidget SetFileDisplayTrace "$this FileDisplayTrace"
        $this FileDisplayTrace ;# Initialize ok button state

        # Pack subwidgets
        if {!$movable_panes || ![llength [info commands panedwindow]]} {
           # Fixed panes
           pack [$filewidget Cget -winpath] -side left \
              -fill both -expand 1 -padx 2 -pady 2
           pack $rightpane -side left -fill both -expand 1
        } else {
           $panedframe add [$filewidget Cget -winpath] $rightpane \
              -sticky news -padx 2 -pady 2
        }
        if {$allow_browse} {
            $ctrlframe.ok configure -width 8
            $ctrlframe.close configure -width 8
            if {!$tallctrl} {
                # Layout control box horizontally
                pack $ctrlframe.browse \
                        $ctrlframe.rescan \
                        $ctrlframe.close \
                        $ctrlframe.ok \
                        -side left -fill both -expand 1
            } else {
                # Layout control box in a 2x2 array
                frame $ctrlframe.f1
                lower $ctrlframe.f1
                frame $ctrlframe.f2
                lower $ctrlframe.f2
                pack $ctrlframe.f1 $ctrlframe.f2 \
                        -side left -fill both -expand 1
                $ctrlframe.browse configure -padx 1
                $ctrlframe.rescan configure -padx 1
                $ctrlframe.ok configure -padx 1
                $ctrlframe.close configure -padx 1
                pack $ctrlframe.browse $ctrlframe.rescan \
                        -in $ctrlframe.f1 \
                        -side top -fill both -expand 1
                pack $ctrlframe.ok $ctrlframe.close \
                        -in $ctrlframe.f2 \
                        -side top -fill both -expand 1
                $ctrlframe configure -bd 4 -relief ridge

            }
        } else {
            pack $ctrlframe.rescan \
                    $ctrlframe.close \
                    $ctrlframe.ok \
                    -side left -fill both -expand 1
        }

	if {[string match {} $optframe]} {
	    pack $ctrlframe -side right -in $neframe \
		    -expand 0 -anchor ne -padx 2 -pady 2
	} else {
	    switch -exact -- $optpos {
		left {
		    pack $ctrlframe -side right -in $neframe \
			    -expand 0 -anchor ne -padx 2 -pady 2
                    pack $optframe -side left -in $neframe \
                            -expand 0 -anchor nw -padx 2 -pady 2
		}
		right {
		    pack $ctrlframe -side left -in $neframe \
			    -expand 0 -anchor nw -padx 2 -pady 2
                    pack $optframe -side right -in $neframe \
                            -expand 0 -anchor ne -padx 2 -pady 2
		}
		bottom {
		    pack $ctrlframe -side top -in $neframe \
			    -expand 0 -anchor ne -padx 2 -pady 2
                    pack $optframe -side top -in $neframe \
                            -anchor nw -pady 2 \
			    -expand 1 -fill x
		}
		top {
                    pack $optframe -side top -in $neframe \
                            -anchor nw -pady 2 \
			    -expand 1 -fill x
		    pack $ctrlframe -side top -in $neframe \
			    -expand 0 -anchor ne -padx 2 -pady 2
		}
		default {
		    return -code error "Programming error in Ow_FileDlg;\
			    bad optpos: $optpos"
		}
	    }
        }

	pack $neframe -in $rightpane \
           -side top -fill x -expand 0 -anchor ne -padx 2 -pady 2


	if {![string match {} $descframe]} {
           pack $descframe -in $rightpane \
              -side top -fill x -expand 0 -anchor n -padx 2 -pady 4
        }

	pack [$selectionwidget Cget -winpath] -in $rightpane \
           -side top -fill x -expand 0 -anchor n -padx 2 -pady 4

	pack [$dirwidget Cget -winpath] -in $rightpane \
           -side top -fill both -expand 1 -padx 2 -pady 2

	# Widget bindings
	bind $winpath <Destroy> "+$this WinDestroy %W"

	# Bind Ctrl-. shortcut to raise and transfer focus to '.'
 	bind $winpath <Control-Key-period> {Ow_RaiseWindow .}

	# Re-enable callbacks
	set callback $callback_keep
	update idletasks
    }
    callback method FileDisplayTrace { args } {
	set checkname [$selectionwidget ReadDisplayedFile]
	if {[string match {} [string trim $checkname]]} {
	    $winpath.ctrlframe.ok configure -state disabled
	} else {
	    $winpath.ctrlframe.ok configure -state normal
	}
    }
    method BrowseStateTrace {} { browse_state winpath } {
	if {![info exists winpath]              \
		|| ![winfo exists $winpath.ctrlframe.ok] } {
	    return
	}
	if {$browse_state} {
	    $winpath.ctrlframe.ok configure -text Apply
	} else {
	    $winpath.ctrlframe.ok configure -text OK
	}
    }
    method GetFileComponents {} { selectionwidget } {
	# Returns list consisting of { path filename }
	return [list [$selectionwidget ReadDisplayedPath] \
		[$selectionwidget ReadDisplayedFile]]
    }
    method GetFilename {} { selectionwidget } {
	# Returns full path+filename of selected value
	return [$selectionwidget ReadDisplayedFullname]
    }
    method SwitchBoard { subwidget actionid args } {
	# General callback routing routine
	switch -exact -- $actionid {
	    UPDATE {
		# Update actions
		if {[string compare $subwidget $filewidget]==0} {
			$selectionwidget SetFilename [lindex $args 1]
		} elseif {[string compare $subwidget $dirwidget]==0} {
			$filewidget SetPath [lindex $args 1]
			$selectionwidget SetPath [lindex $args 1]
		} else {
		    error "Unknown UPDATE callback source: [\
			    $subwidget class]"
		}
	    }
	    SELECT {
		# Check that selected filename is valid
		set fname [$this GetFilename]
		if {$dir_must_exist \
			&& ![file isdirectory [file dirname $fname]]} {
		    error "Filename error: [file dirname $fname]\
			    is not a valid directory"
		}
                if {[file exists $fname]} {
                    # File exists.  Check that it is of valid type
                    if {![string match {} $valid_file_types]} {
                        set ftype [file type $fname]
                        if {[lsearch -exact $valid_file_types $ftype]<0} {
                            set badtype 1
                            if {[string match "link" $ftype]} {
                                # Resolve link and compare that type
                                if {![catch {Oc_ResolveLink $fname} rname]} {
                                    set ttype [file type $rname]
                                    if {[lsearch -exact \
                                            $valid_file_types $ttype]>=0} {
                                        set ftype $ttype
                                        set badtype 0
                                    }
                                }
                            }
                            if {$badtype} {
                                error "File $fname has invalid type:\
                                        $ftype\nShould be one of:\
                                        $valid_file_types"
                            }
                        }
                    }
		} elseif {$file_must_exist} {
		    error "File $fname does not exist"
		}

		set errmsg [eval $callback $this $select_action_id]
		if { [string match {} $errmsg] } {
		    if {!$browse_state} {
			$this Delete
		    }
		    return
		}
                $filewidget Rescan
                $dirwidget Rescan
                error $errmsg
	    }
	    RESCAN {
		$filewidget Rescan
		$dirwidget Rescan
	    }
	    CLOSE {
		$this Delete ; return
	    }
	    DELETE {}
	    default { error "Unknown actionid: $actionid" }
	}
    }
    method WinDestroy { w } { winpath } {
	if {[info exists winpath] && [string compare $winpath $w]!=0} {
	    return    ;# Child destroy event
	}
	$this Delete
    }
    private variable delete_in_progress = 0
    Destructor {
	# Protect against re-entrancy
	if { [info exists delete_in_progress] && \
		$delete_in_progress } { return }
	set delete_in_progress 1

        # Re-enable launching menu
	if {[llength $menu_data]==2} {
	    set menuwin [lindex $menu_data 0]
	    if {[winfo exists $menuwin]} {
		# If main window is going down, menu might already
		# be destroyed.
		if {[catch { eval $menu_cleanup } errmsg]} {
		    Oc_Log Log $errmsg error
		}
	    }
        }

	# Notify client of impending destruction
	catch {eval $callback $this DELETE {$delete_callback_arg}}

	# Delete subwidgets
	catch {$filewidget      Delete}
	catch {$selectionwidget Delete}
	catch {$dirwidget       Delete}

	# Return focus to main window.  (This will probably
	# fail if $winpath doesn't currently have the focus.)
	Ow_MoveFocus .

	# Delete window
	if {[info exists winpath] && [winfo exists $winpath]} {
	    # Destroy instance frame, all children, and bindings
	    destroy $winpath
	}
    }
}
