# FILE: checkpoint.tcl
#
# Tcl/Tk scripts for checkpoint control dialog box for Oxsii and Boxsi
# and support procs.  These scripts run in the user interface interp
# (i.e., in mmLaunch), *not* the oxsii/boxsi solver interp.

Oc_Class Oxs_CheckpointDialog {
    const public variable dialog_title = "Checkpoint Control"
    const public variable winpath

    # Action button paths
    const private variable close
    const private variable apply
    const private variable ok

    # Import trace and initialization array.  Must be global.
    const public variable import_arrname = {}

    # Routine in mainline to call on an Apply or OK event.
    public variable apply_callback = Oc_Nop 

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

    # Internal state array
    private array variable chkptdata

    Constructor {args} {
        # Create a (presumably) unique name for the toplevel
        # window based on the instance name.
        set winpath ".[$this WinBasename]"

	# Process user preferences
	eval $this Configure $args

        # Create dialog window
        toplevel $winpath
        if {[Ow_IsAqua]} {
           # Add some padding to allow space for Aqua window resize
           # control in lower righthand corner
           $winpath configure -bd 4 -relief flat -padx 5 -pady 5
        }
        wm group $winpath .
        wm title $winpath $dialog_title
        Ow_PositionChild $winpath ;# Position at +.25+/25 over '.'
        focus $winpath

        # Setup destroy binding.
	bind $winpath <Destroy> "$this WinDestroy %W"

	# Bind Ctrl-. shortcut to raise and transfer focus to '.'
	bind $winpath <Control-Key-period> {Ow_RaiseWindow .}

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

        # Setup internal state array default values
        array set chkptdata {
           filename ""
           timestamp "never"
           interval "15.0"
           disposal "standard"
        }

        set gchkptdata [$this GlobalName chkptdata]
        ## Global name of chkptdata array.  This is needed to tie
        ## elements of the array to Tk buttons.

        # Allow override from user initialization array
        if {![string match {} $import_arrname]} {
            upvar #0 $import_arrname userinit
            if {[info exists userinit]} {
                foreach elt [array names userinit] {
                    if {[info exists chkptdata($elt)]} {
                        set chkptdata($elt) $userinit($elt)
                    }
                }
            }
            trace variable userinit w "$this ExternalUpdate $import_arrname"
        }

        # Checkpoint filename
        label $winpath.file -text "File:\n$chkptdata(filename)"
        trace variable chkptdata(filename) w \
         "$winpath.file configure -text \"File:\n\$chkptdata(filename)\n\" ;# "


        # Checkpoint timestamp
        label $winpath.timestamp \
           -text "Last checkpoint:\n$chkptdata(timestamp)"
        trace variable chkptdata(timestamp) w \
           "$winpath.timestamp configure \
           -text \"Last checkpoint:\n\$chkptdata(timestamp)\" ;# "

        # Interval
        set if [frame $winpath.if]
        Ow_EntryBox New interval $if \
           -outer_frame_options "-bd 0 -relief flat" \
           -label {Interval: } \
           -valuetype float -valuejustify right -valuewidth 5 \
           -variable ${gchkptdata}(interval)
        label $if.minutes -text " minutes"
	pack [$interval Cget -winpath]  $if.minutes -side left


        # Disposal (aka cleanup)
        set cuf [frame $winpath.cu]
        label $cuf.culab -text "Disposal:"
        radiobutton $cuf.curb0 -text "standard" \
           -value "standard" -variable ${gchkptdata}(disposal)
        radiobutton $cuf.curb1 -text "done_only" \
           -value "done_only" -variable ${gchkptdata}(disposal)
        radiobutton $cuf.curb2 -text "never" \
           -value "never" -variable ${gchkptdata}(disposal)
        grid configure $cuf.culab -row 0 -sticky e
        grid configure x $cuf.curb0 $cuf.curb1 $cuf.curb2 -row 0 -sticky w

        # Control buttons
        set ctrlframe [frame $winpath.ctrlframe -relief flat]
        set close [button $ctrlframe.close -text "Close" \
                -command "$this Action close"]
        set apply [button $ctrlframe.apply -text "Apply" \
                -command "$this Action apply" -state disabled]
        set ok [button $ctrlframe.ok -text "OK" \
                -command "$this Action ok" -state disabled]
        pack $close $apply $ok -side left -expand 0 -padx 20

        $this NoEditInProgress

	pack $winpath.file -fill x -expand 1 -padx 10 -pady 5
	pack $winpath.timestamp -fill x -expand 1 -padx 10 -pady 5
        pack $if -padx 10 -pady 5
	pack $cuf -padx 10 -pady 5
	pack $ctrlframe -fill none -expand 0 -padx 10 -pady 10

        wm protocol $winpath WM_DELETE_WINDOW "$close invoke"
	Ow_SetIcon $winpath
    }

    method EditInProgress { args } {
       # User has edited values.  Enable Apply/OK buttons so user can
       # save the changes.
       $apply configure -state normal
       $ok configure -state normal
       trace vdelete chkptdata(interval) w  "$this EditInProgress"
       trace vdelete chkptdata(disposal) w  "$this EditInProgress"
    }

    method NoEditInProgress {} {
       $apply configure -state disabled
       $ok configure -state disabled
       trace variable chkptdata(interval) w  "$this EditInProgress"
       trace variable chkptdata(disposal) w  "$this EditInProgress"
       # Note: Due to the way variable writes are handled in
       # Ow_EntryBox, a trace on the chkptdata array as a whole is not
       # triggered by user changes in the Ow_EntryBox.  But a trace on
       # the individual element (as above) works.  Also, we don't want
       # the edit status to be changed by changes to
       # chkptdata(timestamp).
    }

    method CheckEditStatus {} {
       if {![string match {} $import_arrname]} {
          upvar #0 $import_arrname userinit
          if {([info exists userinit(interval)] &&
               [string compare $chkptdata(interval) $userinit(interval)]!=0)
              ||
              ([info exists userinit(disposal)] &&
               [string compare $chkptdata(disposal) $userinit(disposal)]!=0)} {
             # Changes made
             $this EditInProgress
          } else {
             # No edit in progress
             $this NoEditInProgress
          }
       }
    }

    # Update from external source
    callback method ExternalUpdate { globalarrname name elt ops } {
        ## The trace is setup to pass in the global array name so that
        ## even if the trace is triggered by a call to Tcl_SetVar
        ## in C code called from a proc from which the traced array
        ## is not visible (due to no 'global' statement on that array),
        ## this trace can still function.        
        if {[string match {} $elt]} {return}
        upvar #0 $globalarrname source
        if {[info exists chkptdata($elt)] && \
                [string compare $chkptdata($elt) $source($elt)]!=0} {
            set chkptdata($elt) $source($elt)
        }
        $this CheckEditStatus
    }

    # Dialog action routine
    callback method Action { cmd } {
        switch $cmd {
           close {
              $this Delete
              return
           }
           apply {
              eval $apply_callback $this chkptdata
              $this NoEditInProgress
              return
           }
           ok {
              eval $apply_callback $this chkptdata
              $this Delete
              return
           }
           default {
              error "Illegal action code: $cmd"
           }
        }
    }

    # Destructor methods
    method WinDestroy { w } { winpath } {
	if {[string compare $winpath $w]!=0} { return } ;# Child destroy event
	$this Delete
    }
    Destructor {
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

        # Remove external update trace, if any
        if {[catch {
            if {![string match {} $import_arrname]} {
                upvar #0 $import_arrname userinit
                trace vdelete userinit w "$this ExternalUpdate $import_arrname"
            }
        } errmsg]} {
            Oc_Log Log $errmsg error
        }

	# Return focus to main window.  (This will probably
	# fail if $winpath doesn't currently have the focus.)
	Ow_MoveFocus .

        # Destroy windows
	if {[catch { 
            if {[info exists winpath] && [winfo exists $winpath]} {
                # Remove <Destroy> binding
                bind $winpath <Destroy> {}
                # Destroy toplevel, all children, and all bindings
                destroy $winpath
            }
        } errmsg]} {
            Oc_Log Log $errmsg error
        }
    }
}

########################################################################
# Support procs and traces

proc ReadCheckpointControl { args } {
   global checkpoint_control checkpoint_control_list
   if {[info exists checkpoint_control_list]} {
      # Copy only those elements that change.  This protects against,
      # for example, changes in last checkpoint time from resetting
      # in progress edits to checkpoint interval.
      foreach {elt val} $checkpoint_control_list {
         if {![info exists checkpoint_control($elt)] \
                || [string compare $checkpoint_control($elt) $val]!=0} {
            set checkpoint_control($elt) $val
         }
      }
   }
}
ReadCheckpointControl
trace variable checkpoint_control_list w ReadCheckpointControl
# If multiple checkpoint control dialogs for the same solver are open in
# multiple mmLaunch interfaces, then the trace insures they stay in
# sync.

proc CheckpointControlButton { btn item } {
   if { [string match disabled [$btn entrycget $item -state]] } {
      return
   }
   global SmartDialogs
   global checkpoint_control
   if {![info exists SmartDialogs]} {
      set SmartDialogs 1  ;# Default setting
   }
   set basetitle [Oc_Main GetTitle]
   Oxs_CheckpointDialog New dialogbox \
      -apply_callback Oxs_CheckpointControlCallback \
      -dialog_title "Checkpoint Control -- $basetitle" \
      -menu_data [list $btn $item] \
      -smart_menu $SmartDialogs \
      -import_arrname checkpoint_control
   Ow_SetIcon [$dialogbox Cget -winpath]
}

proc Oxs_CheckpointControlCallback { chkptwidget arrname } {
   upvar $arrname chkptdata
   global checkpoint_control checkpoint_control_list
   if {[info exists chkptdata]} {
      trace vdelete checkpoint_control_list w ReadCheckpointControl
      ## The ReadCheckpointControl trace is for handling changes
      ## coming from (or through, via another mmLaunch interface)
      ## the solver interp.  In this code sequence we know that
      ## the changes (if any) to checkpoint_control_list are already
      ## in the checkpoint_control array, so we can disable the
      ## trace.
      array set checkpoint_control [array get chkptdata]
      set checkpoint_control_list [array get checkpoint_control]
      trace variable checkpoint_control_list w ReadCheckpointControl
   }
}

########################################################################
