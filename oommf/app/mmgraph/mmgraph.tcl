# FILE: mmGraph.tcl
#
# The control widget and network wrapping for an instance of the
# Ow_GraphWin class.
#

# This code can be source'd from inside another script by setting
# no_net and no_event_loop as desired before sourcing this file.
# If no_net is set true, use the proc "AddTriples { triples }" to add
# data.
if {![info exists no_net]} {
    set no_net 0          ;# Default is to setup server
}
if {![info exists no_event_loop]} {
    set no_event_loop 0   ;# Default is to enter the event loop
}

if {[info exists tcl_precision] &&
        0<$tcl_precision && $tcl_precision<17} {
   set tcl_precision 17
}

# Support libraries
package require Oc 2
package require Tk
package require Ow 2	;# Ow_PrintDlg: -smart_menu
package require Nb 2	;# [Nb_InputFilter]
wm withdraw .

Oc_Main SetAppName mmGraph
Oc_Main SetVersion 2.0b0
regexp \\\044Date:(.*)\\\044 {$Date: 2015/10/09 05:50:34 $} _ date
Oc_Main SetDate [string trim $date]
# regexp \\\044Author:(.*)\\\044 {$Author: donahue $} _ author
# Oc_Main SetAuthor [Oc_Person Lookup [string trim $author]]
Oc_Main SetAuthor [Oc_Person Lookup donahue]
Oc_Main SetHelpURL [Oc_Url FromFilename [file join [file dirname \
        [file dirname [file dirname [Oc_DirectPathname [info \
        script]]]]] doc userguide userguide \
	Data_Graph_Display_mmGraph.html]]
Oc_Main SetDataRole consumer

if {!$no_net} {
   Oc_CommandLine Option net {
      {flag {expr {![catch {expr {$flag && $flag}}]}} {= 0 or 1}}
   } {
      global no_net;  set no_net [expr {!$flag}]
   } {Disable/enable data from other apps; default=enabled}

   Oc_CommandLine ActivateOptionSet Net
}

Oc_CommandLine Option [Oc_CommandLine Switch] {
    {{loadfile list} {} {File(s) to preload.}}
    } {
    global loadList; set loadList $loadfile
} {End of options; loadfiles follow, if any.}
set loadList [list]

Oc_CommandLine Parse $argv

# Death
proc Die { win } {
    if {![string match "." $win]} { return }  ;# Child destroy event
    exit
}
bind . <Destroy> {+Die %W}
wm protocol . WM_DELETE_WINDOW { exit }
Oc_EventHandler New _ Oc_Main Exit {proc Die win {}}

set SmartDialogs 1 ;# If 1, then when a dialog box is created, rather
## that disabling the associated menubutton, instead the behavior
## of the button is changed to raise that dialog box to the top of
## the window stacking order.

proc SetDefaultConfiguration {} {
    global _mmgraph _mmgpsc graphprn
    global showkey smoothcurves graph panstat

    # Autoreset value is persistent across mmGraph resets.
    # Same behavior for other Options menu items (see below).
    if {[info exists _mmgraph(autoreset)]} {
	set initvalue_autoreset $_mmgraph(autoreset)
    } else {
	set initvalue_autoreset 1
    }

    # Release and reset state arrays
    catch {unset _mmgraph}
    catch {unset _mmgpsc}
    catch {unset graphprn}
    catch {unset panstat}

    # _mmgraph() general state information array
    array set _mmgraph [subst {
	saveall 1
	autosave 0
	autosave_filename {}
	autosave_handle {}
	autosave_column_index_list {}
	autosave_strfmt {}
	autosave_numfmt {}
	autoreset $initvalue_autoreset
	table_end 0
	jobname {}
    }]
    trace variable _mmgraph(autosave) w MenuOptionsSaveAutoSaveState
    # Autosave Note: All the autosave elts are setup inside the SaveFile proc,
    #   including the autosave element itself.

    #    _mmgpsc() display configuration array:
    set default_color_selection curve  ;# Either "curve" or "segment"
    Oc_Option Get Ow_GraphWin default_color_selection default_color_selection
    set default_canvas_color [Ow_GraphWin GetDefaultCanvasColor]
    set default_curve_width 0
    Oc_Option Get Ow_GraphWin default_curve_width default_curve_width
    set default_symbol_freq 0
    Oc_Option Get Ow_GraphWin default_symbol_freq default_symbol_freq
    set default_symbol_size 10
    Oc_Option Get Ow_GraphWin default_symbol_size default_symbol_size
    array set _mmgpsc [subst {
       title  {}
       autolabel 1
       xlabel {}
       ylabel {}
       y2label {}
       xdefaultlabel {}
       ydefaultlabel {}
       y2defaultlabel {}
       autolimits 1
       auto_offset_y  0
       auto_offset_y2 0
       xlogscale 0
       ylogscale 0
       y2logscale 0
       xmin   0
       xmax   1
       ymin   0
       ymax   1
       y2min   0
       y2max   1
       lmargin_min  0
       rmargin_min  0
       tmargin_min  0
       bmargin_min  0
       color_selection $default_color_selection
       canvas_color $default_canvas_color
       curve_width $default_curve_width
       symbol_freq $default_symbol_freq
       symbol_size $default_symbol_size
       ptbufsize 0
       oldptbufsize 0
    }]
    ## Note: Default ptbufsize 0 => no limit

    array set graphprn {
	orient   "landscape"
	paper    "letter"
	hpos     "center"
	vpos     "center"
	units    "in"
	tmargin  "1.0"
	lmargin  "1.0"
	pwidth   "6.0"
	pheight  "6.0"
	croptoview "0"
	aspect_crop {}
	aspect_nocrop "1"
    }

    # Pan schedule info
    set panstat {}

    # User option menu settings.  Only initialize these once.
    # After that, keep user setting.
    if {![info exists showkey]} {
	set showkey 1
    }
    if {![info exists smoothcurves]} {
	set smoothcurves 0
    }

    if {[info exists graph]} {
	$graph Configure -ptlimit $_mmgpsc(ptbufsize)      \
                -color_selection $_mmgpsc(color_selection) \
                -canvas_color $_mmgpsc(canvas_color)       \
		-curve_width $_mmgpsc(curve_width)         \
		-symbol_freq $_mmgpsc(symbol_freq)         \
		-symbol_size $_mmgpsc(symbol_size)
	$graph SetKeyState $showkey
	$graph SetSmoothState $smoothcurves
        $graph SetLogAxes \
           $_mmgpsc(xlogscale) $_mmgpsc(ylogscale) $_mmgpsc(y2logscale)

       # Update graph limits
       foreach { _mmgpsc(xmin) _mmgpsc(xmax) \
                 _mmgpsc(ymin) _mmgpsc(ymax) \
                 _mmgpsc(y2min) _mmgpsc(y2max) } \
           [$graph GetGraphLimits] { break }

    }
}

SetDefaultConfiguration

# Routine to compress whitespace in column labels
proc CompressLabel { label } {
    regsub -all "\[ \r\t\n\]+" $label { } compressed_label
    return $compressed_label
}

# Save data routines
proc SaveRecord { record } {
    # Used to implement autosave functionality.  For better performance,
    # the SaveFile proc has a builtin version of this.
    # NOTE: This routine _assumes_ that all the _mmgraph autosave elts
    #       are valid.
    global _mmgraph

    set fileid $_mmgraph(autosave_handle)

    if {[catch {
       if {[llength $record]==0} {
          # Curve break
          SaveFileStop $fileid
          set _mmgraph(autosave_handle) {}
       } else {
          if {[string match {} $fileid]} {
             # (re)open output file
             set fileid [SaveFileStart \
                            $_mmgraph(autosave_filename) \
                            $_mmgraph(autosave_strfmt)   \
                            $_mmgraph(autosave_collab)   \
                            $_mmgraph(autosave_unitlab)  \
                            $_mmgraph(autosave_unitoff)   ]
             if {[string match {} $fileid]} {
		return ;# File open error
             }
             set _mmgraph(autosave_handle) $fileid
          }
          set column_index $_mmgraph(autosave_column_index_list)
          set strfmt $_mmgraph(autosave_strfmt)
          set numfmt $_mmgraph(autosave_numfmt)
          # Indent past heading   "# Columns:"
          puts -nonewline $fileid "          "
          foreach col $column_index {
             set value [lindex $record $col]
             if {[string match {} $value]} {
		puts -nonewline $fileid [format $strfmt "      {}"]
             } else {
		puts -nonewline $fileid [format $numfmt $value]
             }
          }
          puts $fileid {}   ;# newline
          flush $fileid
       }
    } msg]} {
       catch {CloseAutoSave}
       error "AUTOSAVE DISABLED: Error writing file\
              \"$_mmgraph(autosave_filename)\": $msg"
    }
}

proc CloseAutoSave {} {
    # Close current autosave file, if any.  Always use this routine to
    # close an open autosave file.  This routine should be robust,
    # meaning it can be called without checking the state of
    # _mmgraph(autosave).
    global _mmgraph
    set fileid $_mmgraph(autosave_handle)
    if {[catch {
       SaveFileStop $fileid
    } msg]} {
       set _mmgraph(autosave_handle) {}
       set _mmgraph(autosave) 0
       error "Error closing file \"$_mmgraph(autosave_filename)\": $msg"
    }
    set _mmgraph(autosave_handle) {}
    set _mmgraph(autosave) 0
}
Oc_EventHandler New _ Oc_Main Exit CloseAutoSave -oneshot 1

proc SaveFileStart { filename strfmt column_labels \
	unit_labels unit_offsets } {
    # Opens filename and writes header info.
    # Returns handle to open file.

    global _mmgpsc

    set oldfile [file exists $filename]
    if {[catch {open $filename "a+"} fileid]} {
	global oommf
	Ow_NonFatalError "[Oc_Main GetInstanceName]:\
		Unable to open $filename for appending"
	return
    }
    if {[catch {
       if {!$oldfile} {
          puts $fileid "# ODT 1.0"
       } else {
          puts $fileid {}   ;# Blank line
       }
       puts $fileid "# Table Start"
       if {![string match {} $_mmgpsc(title)]} {
          puts $fileid "# Title: $_mmgpsc(title)"
       } else {
          puts $fileid "## [Oc_Main GetAppName] output"
       }

       # Write column header
       puts -nonewline $fileid   "# Columns: "
       foreach lab $column_labels {
          puts -nonewline $fileid [format $strfmt [list $lab]]
       }
       puts -nonewline $fileid "\n# Units:   "
       foreach lab $unit_labels off $unit_offsets {
          puts -nonewline $fileid [format $strfmt	\
                                      [format "%*s%s" $off {} [list $lab]]]
       }
       puts $fileid {} ;# newline
    } msg]} {
       error "Error writing file \"$filename\": $msg"
    }
    return $fileid
}

proc SaveFileStop { handle } {
    # Write trailer and closes file.
    if {![string match {} $handle]} {
       puts $handle "# Table End"
       flush $handle   ;# Insure error on disk full
       close $handle
    }
}

proc SaveFile { filename setup_autosave } {
    global oommf _mmgpsc _mmgraph

    # Close any current autosave file
    CloseAutoSave

    # New file output
    set numfmt " %- 19.16g"  ;# For table body
    set strfmt " %-19s"     ;# For column headers

    global menu_select data_value data_index data_unit

    # Determing output table column labels
    if {$_mmgraph(saveall)} {
	# Save all data
	set column_index {}
	set column_list $data_index(all)
	foreach ordinate $column_list {
	    lappend column_index $data_index($ordinate)
	}
    } else {
	# Save data selected by displayed graphs
	set column_list {}
	set column_index {}
	if {[string match {} $menu_select(x)]} {
	    return ;# No abscissa selected
	}
	lappend column_list $menu_select(x)
	lappend column_index $data_index($menu_select(x))
	foreach ordinate $data_index(all) {
	    if {[info exists menu_select(y,$ordinate)] && \
		    $menu_select(y,$ordinate) } {
		# Label $ordinate is turned on in y-axis menu
		lappend column_list $ordinate
		lappend column_index $data_index($ordinate)
	    }
	}
	foreach ordinate $data_index(all) {
	    if {[info exists menu_select(y2,$ordinate)] && \
		    $menu_select(y2,$ordinate) } {
		# Label $ordinate is turned on in y2-axis menu
		lappend column_list $ordinate
		lappend column_index $data_index($ordinate)
	    }
	}
    }

    # Build column headers
    set column_labels {}
    set unit_labels {}
    set unit_offsets {}
    foreach col $column_list {
	# Compress and strip newlines
	set clab [CompressLabel $col]
	lappend column_labels $clab
	set ulab [CompressLabel $data_unit($col)]
	lappend unit_labels $ulab
	set lendiff [expr {[string length [list $clab]] \
		- [string length [list $ulab]]}]
	if {$lendiff>1} {
	    lappend unit_offsets [expr {$lendiff/2}]
	} else {
	    lappend unit_offsets 0
	}
    }

    # Write data
    set fileid {}
    for {set i $data_value(count,start)} \
	    {$i<$data_value(count,end)} {incr i} {
	set record $data_value($i)
	if {[llength $record]==0} {
	    # Curve break
	    SaveFileStop $fileid
	    set fileid {}
	} else {
	    if {[string match {} $fileid]} {
		# Open file and write header info
		set fileid [SaveFileStart $filename $strfmt \
			$column_labels $unit_labels $unit_offsets]
		if {[string match {} $fileid]} {
		    return   ;# File open error
		}
	    }
	    # Indent past heading   "# Columns:"
            if {[catch {
               puts -nonewline $fileid "          "
               foreach col $column_index {
                  set value [lindex $record $col]
                  if {[string match {} $value]} {
                     puts -nonewline $fileid [format $strfmt "      {}"]
                  } else {
                     puts -nonewline $fileid [format $numfmt $value]
                  }
               }
               puts $fileid {}   ;# newline
            } msg]} {
               error "Error writing file \"$filename\": $msg"
            }
	}
    }

    if { $setup_autosave } {
	if {![string match {} $fileid]} { flush $fileid }
	set _mmgraph(autosave_filename) $filename
	set _mmgraph(autosave_handle) $fileid
	set _mmgraph(autosave_column_index_list) $column_index
	set _mmgraph(autosave_strfmt) $strfmt
	set _mmgraph(autosave_numfmt) $numfmt
	set _mmgraph(autosave_collab) $column_labels
	set _mmgraph(autosave_unitlab) $unit_labels
	set _mmgraph(autosave_unitoff) $unit_offsets
	set _mmgraph(autosave) 1
    } else {
        if {[catch {
           SaveFileStop $fileid
        } msg]} {
           error "Error writing file \"$filename\": $msg"
        }
	set fileid {}
	set _mmgraph(autosave) 0
	set _mmgraph(autosave_handle) {}  ;# Safety
    }
}

proc SaveFileOptionBoxSetup { widget frame } {
    global _mmgraph
    set _mmgraph(SFOB_autosave) $_mmgraph(autosave)
    checkbutton $frame.autosave -text "Auto Save" \
	    -variable _mmgraph(SFOB_autosave) \
	    -relief raised -bd 2
    pack $frame.autosave -side top -fill x -expand 1
    set _mmgraph(SFOB_saveall) $_mmgraph(saveall)
    frame $frame.saveselect -relief raised -bd 2
    label $frame.saveselect.topic -text "Save:" -relief flat
    pack $frame.saveselect.topic -side left -anchor nw
    frame $frame.saveselect.rb -bd 2 -relief sunken
    radiobutton $frame.saveselect.rb.saveall -text "All Data" \
	    -variable _mmgraph(SFOB_saveall) -value 1
    radiobutton $frame.saveselect.rb.savesome -text "Selected\nData" \
	    -variable _mmgraph(SFOB_saveall) -value 0
    pack $frame.saveselect.rb.saveall $frame.saveselect.rb.savesome \
	    -side top -anchor w
    pack $frame.saveselect.rb -side left
    pack $frame.saveselect -side top
}

proc SaveFileCallback { widget } {
    global _mmgraph
    set _mmgraph(saveall) $_mmgraph(SFOB_saveall)
    SaveFile [$widget GetFilename] $_mmgraph(SFOB_autosave)
    return {}
}

# Open file routines
proc OpenFile { filename } {
    global oommf _mmgpsc graph

    # Apply filter (based on filename extension), if applicable.
    set tempfile [Nb_InputFilter FilterFile $filename decompress odt]
    if { [string match {} $tempfile] } {
        # No filter match
        set workname $filename
    } else {
        # Filter match
        set workname $tempfile
    }

    if {[catch {open $workname "r"} fileid]} {
	if { ![string match {} $tempfile] } {
	    catch { file delete $tempfile }
	}
	Ow_NonFatalError "[Oc_Main GetInstanceName]:\
		Unable to open $filename for input"
	return
    }
    fconfigure $fileid -buffering full -buffersize 50000

    # Process lines.
    Ow_PushWatchCursor [list . [$graph GetCanvas]]
    set column_list {}
    set unit_list {}
    set new_columns 1
    set headpat "^#\[ \t\]*\(\[^ \t:\]+\)\[ \t:\]+\(.*\)"
    while {[gets $fileid line]>=0} {
	# Check for continuation line(s)
	if {[string compare "\\" [string range $line end end]]==0} {
	    regsub -- {\\$} $line {} line
	    set readmore 1
	    while {$readmore && [gets $fileid buf]>=0} {
		set buf [string trimleft $buf {#}] ;# Trim leading #'s
		set readmore [regsub -- {\\$} $buf {} buf]
		append line $buf
	    }
	}

	# Skip blank lines
	set line [string trim $line]
	if {[string compare {} $line]==0} {
	    continue
	}

	# Data line?
        if {[string compare {#} [string index $line 0]]!=0} {
	    if {$new_columns} {
		SetupDataRecordColumns $column_list $unit_list
		set new_columns 0
	    }
	    AddDataRecord $line 0
	    continue
	}

	# Otherwise, it's a header line.
	# Break into fields:
	if {[regexp -- $headpat	$line dummy type value]} {
	    switch -exact [string tolower $type] {
		columns {
		    # Column header line
		    # Check for proper list structure
		    if {[catch {lindex $value end}]} {
			Ow_NonFatalError "[Oc_Main GetInstanceName]:\
				Skipping illegal\
				\"Columns\" header line in file $filename."
		    } else {
			set column_list $value
			set new_columns 1
		    }
		}
		units {
		    # Units header line
		    # Check for proper list structure
		    if {[catch {lindex $value end}]} {
			Ow_NonFatalError "[Oc_Main GetInstanceName]:\
				Skipping illegal\
				\"Units\" header line in file $filename."
		    } else {
			set unit_list $value
		    }
		}
		title {
		    # Title header line
		    set _mmgpsc(title) $value
		}
		table {
		    set value [string tolower [string trim $value]]
		    if {[string match start $value] \
			    || [string match end $value]} {
			# Table start or end record
			set column_list {} ;# Reset columns
			set unit_list {}
			set new_columns 1
			AddDataRecord {} 0 ;# Add break record
		    }
		}
		default {
		    # Skip other headers
		}
		
	    }
	}
	# Ignore all other header lines

    }

    # Close channel, and if a temp file was used, delete it.
    close $fileid
    if { ![string match {} $tempfile] } {
        file delete $tempfile
    }

    $graph Configure -title \
	    [subst -nocommands -novariables $_mmgpsc(title)]
    $graph DrawAxes
    UpdateGraphDisplay
    Ow_PopWatchCursor
}

proc OpenFileCallback { widget } {
    OpenFile [$widget GetFilename]
    return {}
}

proc DialogCallback { widget actionid args } {
    # Re-enable menubutton when the dialog widget is destroyed
    if {[string match DELETE $actionid]} {
        eval [join $args]
	return
    }

    # Call appropriate action proc
    set errmsg "ERROR (proc DialogCallback): Invalid actionid: $actionid"
    if {[string match SAVE $actionid]} {
	set errmsg [SaveFileCallback $widget]
    } elseif {[string match OPEN $actionid]} {
	set errmsg [OpenFileCallback $widget]
    }

    return $errmsg
}

proc PrintCallback { printwidget arrname } {
    upvar $arrname dialogprn
    global graph graphprn
    if {[info exists dialogprn]} {
	array set graphprn [array get dialogprn]
    }
    Ow_PrintDlg Print $printwidget [$graph GetCanvas] graphprn
}

proc LaunchDialog { menuname itemlabel action } {
    if { [string match "disabled" [$menuname entrycget $itemlabel -state]] } {
	return   ;# Item not enabled
    }
    global oommf SmartDialogs dialog
    set basetitle [Oc_Main GetTitle]
    if {[string match SAVE $action]} {
	Ow_FileDlg New dialogbox -callback DialogCallback \
		-dialog_title "Save File -- $basetitle" \
		-selection_title "Append ODT Output To..." \
		-select_action_id "SAVE" \
		-filter "*.odt" \
		-optionbox_setup SaveFileOptionBoxSetup \
		-menu_data [list $menuname $itemlabel] \
		-smart_menu $SmartDialogs
	set dialog(save) $dialogbox
    } elseif {[string match OPEN $action]} {
	Ow_FileDlg New dialogbox -callback DialogCallback \
		-dialog_title "Open File -- $basetitle" \
		-selection_title "Open ODT File..." \
		-select_action_id "OPEN" \
		-filter "*.odt" \
                -compress_suffix [Nb_InputFilter GetExtensionList decompress] \
		-file_must_exist 1 \
		-allow_browse 1 \
		-menu_data [list $menuname $itemlabel] \
		-smart_menu $SmartDialogs
	set dialog(open) $dialogbox
    } elseif {[string match PRINT $action]} {
        global graphprn
	Ow_PrintDlg New dialogbox \
                -apply_callback PrintCallback \
		-dialog_title "Print -- $basetitle" \
                -menu_data [list $menuname $itemlabel] \
		-smart_menu $SmartDialogs \
                -crop_option 0 \
                -import_arrname graphprn
	set dialog(print) $dialogbox
    } else {
	Ow_NonFatalError "Invalid action code: $action" \
		"Error in LaunchDialog proc"
	return
    }

    # Set icon
    Ow_SetIcon [$dialogbox Cget -winpath]
}

# Menu bar
set menubar .mb
foreach {filemenu xmenu ymenu y2menu optionsmenu helpmenu} \
	[Ow_MakeMenubar . $menubar File X {Y1 1} {Y2 1} Options Help] {}

$filemenu add command -label "Open..." -underline 0 \
	-command { LaunchDialog $filemenu "Open..." "OPEN" }

$filemenu add command -label "Save As..." -underline 5 \
	-command { LaunchDialog $filemenu "Save As..." "SAVE" }
$filemenu add command -label "Print..." -underline 0 \
	-command { LaunchDialog $filemenu "Print..." "PRINT" }
$filemenu add command -label "Reset" -command { Reset } -underline 0
$filemenu add separator
$filemenu add command -label "Exit" -command { exit } -underline 1

$xmenu configure -tearoff 1 -title "X-Axis: [Oc_Main GetInstanceName]"
$ymenu configure -tearoff 1 -title "Y1-Axis: [Oc_Main GetInstanceName]"
$y2menu configure -tearoff 1 -title "Y2-Axis: [Oc_Main GetInstanceName]"
Ow_StdHelpMenu $helpmenu

# Ensure minimal window width
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

global dialog
proc SetPlotConfiguration { pscvar }  {
   global _mmgpsc graph data_base_value
   upvar $pscvar psc
   # Array elements: title,
   #            autolabel,xlabel,ylabel,y2label,
   #            xdefaultlabel,ydefaultlabel,y2defaultlabel,
   #            autolimits, auto_offset_y, auto_offset_y2,
   #            log_scale_x, log_scale_y, log_scale_y2,
   #            xmin,xmax,ymin,ymax,y2min,y2max,
   #            color_selection,canvas_color,curve_width,
   #            symbol_freq,symbol_size,ptbufsize,
   #            lmargin_min,rmargin_min,tmargin_min,bmargin_min
   if {$_mmgpsc(curve_width) != $psc(curve_width)} {
      $graph Configure -curve_width $psc(curve_width)
   }

   # Curve offsets are computed by mmGraph, and the adjusted data is fed
   # to Ow_GraphWin.  So if offset is turned on or off we need to call
   # RefreshGraphData.
   #   On the other hand, logarithmic vs. linear scaling is handled by
   # Ow_GraphWin, so at first glance it appears that switching between
   # linear and log scaling shouldn't require a call to RefreshGraphData.
   # And that is true if curve offsets aren't enabled.  But offsets for
   # linear y-axis scaling are different from offsets for log y-axis
   # scaling; in the former offsets are of the form y - ybase, in the
   # latter offsets are computed as y/ybase.  This means that switching
   # between log and linear scaling requires a call to RefreshGraphData if
   # curve offsets are enabled.
   if {$psc(auto_offset_y) != $_mmgpsc(auto_offset_y) || \
          $psc(auto_offset_y2) != $_mmgpsc(auto_offset_y2)} {
      # Change to offset request
      set offset_change 1
   } else {
      # Offset requests unchanged, but linear/log scaling change may
      # nonetheless affect offsets as explained above.
      if {($psc(ylogscale) != $_mmgpsc(ylogscale) \
              && $psc(auto_offset_y)) || \
          ($psc(y2logscale) != $_mmgpsc(y2logscale) \
              && $psc(auto_offset_y2))} {
         set offset_change 1
      } else {
         set offset_change 0
      }
   }
   array set _mmgpsc [array get psc]
   if {$offset_change} {
      if {[info exists data_base_value]} {
         unset data_base_value
      }
      RefreshGraphData
   }
   $graph Configure -title \
      [subst -nocommands -novariables $_mmgpsc(title)]
   if {$_mmgpsc(autolabel)} {
      set _mmgpsc(xlabel) $_mmgpsc(xdefaultlabel)
      set _mmgpsc(ylabel) $_mmgpsc(ydefaultlabel)
      if {$_mmgpsc(auto_offset_y)} {
         append _mmgpsc(ylabel) " offset"
      }
      set _mmgpsc(y2label) $_mmgpsc(y2defaultlabel)
      if {$_mmgpsc(auto_offset_y2)} {
         append _mmgpsc(y2label) " offset"
      }
      # Note: Default label code needs to coordinate with
      #       corresponding code in proc ChangeCurve.
   }
   $graph Configure \
      -xlabel [subst -nocommands -novariables $_mmgpsc(xlabel)] \
      -ylabel [subst -nocommands -novariables $_mmgpsc(ylabel)] \
      -y2label [subst -nocommands -novariables $_mmgpsc(y2label)] \
      -color_selection $_mmgpsc(color_selection) \
      -canvas_color $_mmgpsc(canvas_color) \
      -symbol_freq $_mmgpsc(symbol_freq) \
      -symbol_size $_mmgpsc(symbol_size)
   $graph SetMargins \
      $_mmgpsc(lmargin_min) $_mmgpsc(rmargin_min) \
      $_mmgpsc(tmargin_min) $_mmgpsc(bmargin_min)
   $graph SetLogAxes \
      $_mmgpsc(xlogscale) $_mmgpsc(ylogscale) $_mmgpsc(y2logscale)
   if {$_mmgpsc(autolimits)} {
      $graph SetGraphLimits
   } else {
      $graph SetGraphLimits \
         $_mmgpsc(xmin) $_mmgpsc(xmax) \
         $_mmgpsc(ymin) $_mmgpsc(ymax) \
         $_mmgpsc(y2min) $_mmgpsc(y2max)
   }
   # Update graph limits; even if limits are explicitly requested
   # (second branch above), they won't be honored if zero or negative
   # values are requested on a log scaled axis.
   foreach { _mmgpsc(xmin) _mmgpsc(xmax) \
             _mmgpsc(ymin) _mmgpsc(ymax) \
             _mmgpsc(y2min) _mmgpsc(y2max) } \
      [$graph GetGraphLimits] { break }

   ResetBufferSize
   $graph RefreshDisplay
}

set thinmenu [menu $optionsmenu.thin -tearoff 0]
$thinmenu add command -label "50%" -underline 0 \
	-command {ThinDataInteractive 2}
$thinmenu add command -label "90%" -underline 0 \
	-command {ThinDataInteractive 10}

$optionsmenu add command -label "Configure..." -underline 0 \
        -command { PlotConfigure New dialog(config) \
        	  -menu "$optionsmenu {Configure...}" \
                  -import_arrname _mmgpsc \
		  -menuraise $SmartDialogs \
                  -apply_callback SetPlotConfiguration
        }

$optionsmenu add command -label "clear Data" -underline 6 \
	-command {ClearDataInteractive}

$optionsmenu add cascade -label "Thin Data" -underline 0 -menu $thinmenu

$optionsmenu add command -label "Stop Autosave" -underline 0 \
	-command {StopAutoSaveInteractive} -state disabled
proc MenuOptionsSaveAutoSaveState { var elt op } {
    global optionsmenu _mmgraph
    if {$_mmgraph(autosave)} {
        catch {$optionsmenu entryconfigure "Stop Autosave" -state normal}
    } else {
        catch {$optionsmenu entryconfigure "Stop Autosave" -state disabled}
    }
}

proc UpdateLimits {} {
    global _mmgpsc graph
    foreach { _mmgpsc(xmin) _mmgpsc(xmax) \
	    _mmgpsc(ymin) _mmgpsc(ymax) \
	    _mmgpsc(y2min) _mmgpsc(y2max) } \
	    [$graph GetGraphLimits] { break }
}

proc Rescale {} {
    global graph
    if {[$graph SetGraphLimits]} {
	$graph RefreshDisplay
    }
    UpdateLimits
}

# User directed curve break command
$optionsmenu add command -label "Break Curves" -underline 0 \
	-command {AddDataRecord {} 0}

$optionsmenu add command -label "Rescale" -underline 0 -command Rescale
bind . <Key-Home> Rescale
bind . <Shift-Key-Home> Rescale
catch {bind . <Key-KP_Home> Rescale}

$optionsmenu add separator

$optionsmenu add checkbutton -label "Key" -underline 0 \
		-variable showkey -offvalue 0 -onvalue 1 \
		-command {$graph SetKeyState $showkey}
$optionsmenu add checkbutton -label "Auto Reset" -underline 0 \
		-variable _mmgraph(autoreset) -offvalue 0 -onvalue 1 \
		-command {$graph SetKeyState $showkey}
$optionsmenu add checkbutton -label "Smooth" -underline 1 \
		-variable smoothcurves -offvalue 0 -onvalue 1 \
		-command {$graph SetSmoothState $smoothcurves}



proc PtBufOverflowCheck {} {
    global _mmgpsc data_value
    if {$_mmgpsc(ptbufsize)<1} { return } ;# No limit
    set bufstart $data_value(count,start)
    set bufend $data_value(count,end)
    set bufoverflow [expr {$bufend-$bufstart-$_mmgpsc(ptbufsize)}]
    if {$bufoverflow<1} { return }  ;# No overflow
    if {$bufoverflow==1} {
	# Normal overflow case (single record)
	unset data_value($bufstart)
	incr data_value(count,start)
    } else {
	# Large overflow
	set newbufstart [expr {$bufstart+$bufoverflow}]
	for {set i $bufstart} {$i<$newbufstart} {incr i} {
	    unset data_value($i)
	}
	set data_value(count,start) $newbufstart
    }
}

proc ResetBufferSize {} {
    global _mmgpsc graph
    set newsize $_mmgpsc(ptbufsize)
    if {[catch {expr {round($newsize)}} newsize] || $newsize<1} {
	set newsize 0
    }
    set _mmgpsc(ptbufsize) $newsize
    if {$newsize == $_mmgpsc(oldptbufsize)} { return }  ;# No change
    set _mmgpsc(oldptbufsize) $newsize
    PtBufOverflowCheck
    $graph Configure -ptlimit $newsize
    UpdateGraphDisplay
}

# Proc to add new labels to the x, y & y2 axis menus.  
# A global array menu_select stores the state of the axis menu
# select boxes:
#    menu_select(x)        = name of selected label, or {} if none
#    menu_select(y,$label) = 1 if box associated with label is selected,
#                            0 otherwise.
#    menu_select(y2,$label) = Same as "y,$label", but for right y-axis
# The RemoveColumn proc can be used to remove a label.
set menu_select(x) {}
proc AppendAxesSelect { new_labels } {
    global xmenu ymenu y2menu menu_select
    # Add new entries
    foreach label $new_labels {
	set menu_select(y,$label) 0
	set menu_select(y2,$label) 0
	set compressed_label [CompressLabel $label]
	$xmenu add radiobutton -label $compressed_label \
		-variable menu_select(x) -value $label  \
		-command CreateGraph
	$ymenu add checkbutton -label $compressed_label \
		-variable menu_select(y,$label) \
		-offvalue 0 -onvalue 1 \
		-command [list ChangeCurve 1 $label 1]
	$y2menu add checkbutton -label $compressed_label \
		-variable menu_select(y2,$label) \
		-offvalue 0 -onvalue 1 \
		-command [list ChangeCurve 2 $label 1]
    }
}

source [file join [file dirname [info script]] dialog.tcl]

# Graph
set graph_frame [frame .gf -borderwidth 0]
Ow_GraphWin New graph $graph_frame -default_width 340
$graph Configure -showkey $showkey -smoothcurves $smoothcurves
pack [$graph Cget -winpath] -side top -fill both -expand 1
pack $graph_frame -side top -fill both -expand 1
bind [$graph GetCanvas] <Configure> {+
    set mywidth [%W cget -width]
    set myheight [%W cget -height]
    if {$myheight>0} {
       set graphprn(aspect_nocrop) \
	       [expr double($mywidth)/double($myheight)]
    }
}

########################################################################
### Keyboard Zoom ######################################################
##
proc Zoom { factor } {
   global graph
   $graph Zoom $factor
   UpdateLimits
}

set szf [expr {sqrt(2)}]  ;# Temporay values that get hard coded into
set zf  2.0               ;# bindings below.
set lzf 4.0
set izf [expr {1./$zf}]
set ilzf [expr {1./$lzf}]
set iszf [expr {1./$szf}]

bind . <Key-Prior>         [list Zoom $zf]
bind . <Shift-Key-Prior>   [list Zoom $lzf]
bind . <Control-Key-Prior> [list Zoom $szf]
bind . <Key-Next>          [list Zoom $izf]
bind . <Shift-Key-Next>    [list Zoom $ilzf]
bind . <Control-Key-Next>  [list Zoom $iszf]
## NOTE: SunOS apparently doesn't have Page_Up & Page_Down.  For the
## record, Prior == Page_Up, and Next == Page_Down.
# Ditto for KeyPad; Note that the <Shift> keypad values are funky,
# and might be wrong for some keyboards.
catch {bind . <Key-KP_Prior>     [list Zoom $zf]}
catch {bind . <Shift-Key-KP_9>   [list Zoom $lzf]}
catch {bind . <Control-Key-KP_9> [list Zoom $szf]}
catch {bind . <Key-KP_Next>      [list Zoom $izf]}
catch {bind . <Shift-Key-KP_3>   [list Zoom $ilzf]}
catch {bind . <Control-Key-KP_3> [list Zoom $iszf]}

########################################################################
### Panning ############################################################
##
set enable_pan_y1 1
set enable_pan_y2 1

proc Pan { xpan y1pan y2pan} {
    global graph panstat
    if {[llength $panstat]>0} {
	# Kill any queued pan requests (safety)
	after cancel [lindex $panstat 0]
    }
    set panstat {}  ;# Clear

    $graph Pan $xpan $y1pan $y2pan
    UpdateLimits
}

proc SchedPan { xpan y1pan y2pan } {
    global panstat
    if {[llength $panstat]>0} {
	foreach {id pancount xoff y1off y2off} $panstat { break }
	after cancel $id
	set xpan [expr {$xpan+$xoff}]
	set y1pan [expr {$y1pan+$y1off}]
	set y2pan [expr {$y2pan+$y2off}]
    } else {
	set pancount 0
    }
    if {$pancount>3} {
	Pan $xpan $y1pan $y2pan   ;# Go straight through
    } else {
	incr pancount
	set id [after 50 [list Pan $xpan $y1pan $y2pan]]
	set panstat [list $id $pancount $xpan $y1pan $y2pan]
    }
}

# Meta key assignments:
#
# AFAICT, Tk neither defines the keyboard to "shift" key assignments,
# nor provides any programatic way to discover the mapping.  So we do
# the best we can...
#
#      |           |         |     Mac OS X      |
#      |  Windows  |  Linux  |  Aqua   |   X11   |
#------+-----------+---------+---------+---------+
#  M1  |   NumLk   |   Alt   | Command |         |
#  M2  |    Alt    |  NumLk  | Option  |         |
#  M3  | ScrollLk  |         |         |         |
#  M4  |    ?      |         |         |         |
#  M5  |    ?      |         |         |         |
# Meta |           |         |         | Command |

set Y1PanKey Lock    ;# Caps Lock
set Y2PanKey M3

foreach prefix {Key Shift-Key Control-Key} fstep {0.5 1.0 0.25} {
    set bstep [expr {-1*$fstep}]
    bind . <$prefix-Left>  "SchedPan $bstep 0 0"
    bind . <$prefix-Right> "SchedPan $fstep 0 0"
    bind . <$prefix-Up>    "SchedPan   0 $fstep $fstep"
    bind . <$prefix-Down>  "SchedPan   0 $bstep $bstep"
    bind . <$Y1PanKey-$prefix-Up>    "SchedPan   0 $fstep 0"
    bind . <$Y1PanKey-$prefix-Down>  "SchedPan   0 $bstep 0"
    bind . <$Y2PanKey-$prefix-Up>    "SchedPan   0 0 $fstep"
    bind . <$Y2PanKey-$prefix-Down>  "SchedPan   0 0 $bstep"
    catch {
	bind . <$prefix-KP_Left>  "SchedPan $bstep 0 0"
	bind . <$prefix-KP_Right> "SchedPan $fstep 0 0"
	bind . <$prefix-KP_Up>    "SchedPan   0 $fstep $fstep"
	bind . <$prefix-KP_Down>  "SchedPan   0 $bstep $bstep"
	bind . <$Y1PanKey-$prefix-KP_Up>    "SchedPan   0 $fstep 0"
	bind . <$Y1PanKey-$prefix-KP_Down>  "SchedPan   0 $bstep 0"
	bind . <$Y2PanKey-$prefix-KP_Up>    "SchedPan   0 0 $fstep"
	bind . <$Y2PanKey-$prefix-KP_Down>  "SchedPan   0 0 $bstep"
    }
}

proc RecoverState { offset } {
   global graph
   $graph RecoverState $offset
   UpdateLimits ;# Pass changes on to Configure dialog box (if open)
}
proc StoreState {} {
   global graph
   $graph StoreState
}
bind . <Key-Escape> "RecoverState -1"
bind . <Shift-Key-Escape> "RecoverState 1"
bind . <Key-Return> "StoreState"

proc ScrollWheelHandler { wFired D horizontal } {
   set D [expr {double($D)*2.*0.0009765625}]  ;# 0.0009765625 = 1./1024.
   if {$horizontal} {
      SchedPan $D 0 0
   } else {
      SchedPan 0 $D $D
   }
}

Ow_BindMouseWheelHandler [$graph GetCanvas] ScrollWheelHandler

# Mouse-based zooming
bind . <Control-ButtonRelease> UpdateLimits

# Proc to reset graph limits, and redraw graph
proc UpdateGraphDisplay {} {
    global graph _mmgpsc

    if {$_mmgpsc(autolimits)} {
	set limits_change [$graph SetGraphLimits]
	foreach { _mmgpsc(xmin) _mmgpsc(xmax) \
		  _mmgpsc(ymin) _mmgpsc(ymax) \
		  _mmgpsc(y2min) _mmgpsc(y2max) } \
		[$graph GetGraphLimits] { break }
    } else {
        set limits_change [$graph SetGraphLimits \
		$_mmgpsc(xmin) $_mmgpsc(xmax) \
		$_mmgpsc(ymin) $_mmgpsc(ymax) \
		$_mmgpsc(y2min) $_mmgpsc(y2max)]
    }
    if {$limits_change} {
	$graph RefreshDisplay
    } else {
	$graph DrawCurves
    }
   # $graph DrawKey
}

# Proc to update graph with latest data input triples
proc UpdateGraphData { index redraw } {
   global menu_select graph _mmgpsc
   global data_value data_index data_base_value

   if {[catch {set data_value($index)} record]} { return } ;# Out-of-bounds

   set abscissa $menu_select(x)
   if {[string match {} $abscissa]} {return} ;# No abscissa selected

   # Check for curve break record
   if {[llength $record]==0} {
      foreach ordinate $data_index(all) {
         if {$menu_select(y,$ordinate)} {
            $graph AddDataPoints $ordinate 1 [list {} {}]
         }
         if {$menu_select(y2,$ordinate)} {
            $graph AddDataPoints $ordinate 2 [list {} {}]
         }
      }
      if {$redraw} {
         $graph DrawCurves
      }
      return
   }

   # Normal record
   set xindex $data_index($abscissa)
   set xvalue [lindex $record $xindex]
   if {[string match {} $xvalue]} {
      return ;# No abscissa value at this index
   }
   set change_limits 0
   foreach { gxmin gxmax gymin gymax gy2min gy2max} \
      [$graph GetGraphLimits] { break }
   foreach ordinate $data_index(all) {
      set y1axis [set y2axis 0]
      if {$menu_select(y,$ordinate)}  { set y1axis 1 }
      if {$menu_select(y2,$ordinate)} { set y2axis 1 }
      if {!$y1axis && !$y2axis} { continue } ;# Skip curve
      set yindex $data_index($ordinate)
      if {![string match {} [set yvalue [lindex $record $yindex]]]} {
         set yoffset_pair [list {} {}]
         if {[info exists data_base_value($ordinate)]} {
            set yoffset_pair $data_base_value($ordinate)
         }
         if {[string match {} [lindex $yoffset_pair 1]]} {
            UpdateBaseValues
            catch {set yoffset_pair $data_base_value($ordinate)}
         }
         foreach {yofflin yofflog} $yoffset_pair { break }
         if {[string match {} $yofflin]} {
            set yofflin 0.0
         }
         if {[string match {} $yofflog]} {
            set yoffllog 1.0
         }
         if {$y1axis} {
            set y1value $yvalue
            if {$_mmgpsc(auto_offset_y)} {
               if {!$_mmgpsc(ylogscale)} {
                  catch {set y1value [expr {$yvalue - $yofflin}]}
               } else {
                  catch {set y1value [expr {$yvalue/$yofflog}]}
               }
            }
            set change_limits \
               [$graph AddDataPoints $ordinate 1 [list $xvalue $y1value]]
         }
         if {$y2axis} {
            set y2value $yvalue
            if {$_mmgpsc(auto_offset_y2)} {
               if {!$_mmgpsc(y2logscale)} {
                  catch {set y2value [expr {$yvalue - $yofflin}]}
               } else {
                  catch {set y2value [expr {$yvalue/$yofflog}]}
               }
            }
            set change_limits \
               [$graph AddDataPoints $ordinate 2 [list $xvalue $y2value]]
         }
      }
   }

   if {$redraw} {
      if {$change_limits && $_mmgpsc(autolimits)} {
         UpdateGraphDisplay
      } else {
         $graph DrawCurves
      }
   }
   return
}

# Proc to add or delete a curve from the graph, as determined by the
# value of menu_select(y,$ordinate) (1=>display, 0=>don't display).
# Import "yaxis" should be either "1" or "2"
proc ChangeCurve { yaxis ordinate redraw } {
   global menu_select graph _mmgpsc
   global data_value data_index data_unit data_base_value

   $graph DeleteCurve $ordinate $yaxis ;# Remove any preexisting curve
   ## by this name
   if {$yaxis == 2} {
      set ys "y2"
      set ylog $_mmgpsc(y2logscale)
   } else {
      set ys "y"
      set ylog $_mmgpsc(ylogscale)
   }

   if {![info exists menu_select($ys,$ordinate)]} {
      return  ;# Safety
   }

   # Turn curve on?
   if {$menu_select($ys,$ordinate)} {
      if {[string match {} $menu_select(x)]} {
         return 0   ;# No abscissa selected
      }
      set abscissa $menu_select(x)
      # Write curve
      set data_list {}
      $graph NewCurve $ordinate $yaxis
      set xindex $data_index($abscissa)
      set yindex $data_index($ordinate)
      set yoffset [expr {$ylog ? 1.0 : 0.0}]
      if {$_mmgpsc(auto_offset_$ys)} {
         set yoffset_pair [list {} {}]
         if {[info exists data_base_value($ordinate)]} {
            set yoffset_pair $data_base_value($ordinate)
         }
         if {[string match {} [lindex $yoffset_pair 1]]} {
            UpdateBaseValues
            catch {set yoffset_pair $data_base_value($ordinate)}
         }
         if {!$ylog} {
            set yoffset [lindex $yoffset_pair 0]
            if {[string match {} $yoffset]} {
               set yoffset 0.0
            }
         } else {
            set yoffset [lindex $yoffset_pair 1]
            if {[string match {} $yoffset]} {
               set yoffset 1.0
            }
         }
      }
      if {!$ylog} {
         # Linear scale on selected y-axis
         for {set i $data_value(count,start)} \
               {$i<$data_value(count,end)} {incr i} {
            set record $data_value($i)
            if {[llength $record]==0} {
               lappend data_list {} {}  ;# Curve break
            } else {
               set xval [lindex $record $xindex]
               set yval [lindex $record $yindex]
               if {![string match {} $xval] && ![string match {} $yval]} {
                  lappend data_list $xval [expr {$yval - $yoffset}]
               }
            }
         }
      } else {
         # Log scale on selected y-axis
         for {set i $data_value(count,start)} \
               {$i<$data_value(count,end)} {incr i} {
            set record $data_value($i)
            if {[llength $record]==0} {
               lappend data_list {} {}  ;# Curve break
            } else {
               set xval [lindex $record $xindex]
               set yval [lindex $record $yindex]
               if {![string match {} $xval] && ![string match {} $yval]} {
                  lappend data_list $xval [expr {$yval/$yoffset}]
               }
            }
         }
      }         
      if {[llength $data_list]>0} {
         $graph AddDataPoints $ordinate $yaxis $data_list
      }
   }

   # Adjust y-axis label
   set curvelist [$graph GetCurveList $yaxis]
   if {[llength $curvelist]==0} {
      set yunit {}
   } else {
      set yunit $data_unit([lindex $curvelist 0])
      if {[string match {} $yunit]} {
         set yunit "Non Dim"
      }
   }
   set _mmgpsc(${ys}defaultlabel) $yunit
   if {$_mmgpsc(autolabel)} {
      set dlab $yunit
      if {$_mmgpsc(auto_offset_${ys})} {
         append dlab " offset"
      }
      if {[string compare $_mmgpsc(${ys}label) $dlab]!=0} {
         set _mmgpsc(${ys}label) $dlab
         $graph Configure -${ys}label \
            [subst -nocommands -novariables $_mmgpsc(${ys}label)]
         $graph DrawAxes
      }
   }
   if {$redraw} {
      UpdateGraphDisplay
   }
   return
}

# Proc to create a complete new graph, erasing any previously existing
# graph.  Return value is the number of curves in new graph.
proc CreateGraph {} {
    global menu_select data_value data_index data_unit graph _mmgpsc showkey
    set key_pos [$graph GetKeyPosition]
    $graph SetKeyState 0 ;# Temporarily hide key, if not already hidden
    $graph Reset    ;# Remove any previously existing graph
    $graph Configure -title \
	    [subst -nocommands -novariables $_mmgpsc(title)]
    if {[string match {} $menu_select(x)]} { return 0 } ;# No abscissa selected
    set abscissa $menu_select(x)
    set xunit " ($data_unit($abscissa))"
    if {[string match " ()" $xunit]} { set xunit {} }
    set _mmgpsc(xdefaultlabel) "$abscissa$xunit"
    if {$_mmgpsc(autolabel)} {
	set _mmgpsc(xlabel) "$abscissa$xunit"
    }
    $graph Configure \
	    -xlabel [subst -nocommands -novariables $_mmgpsc(xlabel)] \
	    -ylabel [subst -nocommands -novariables $_mmgpsc(ylabel)] \
	    -y2label [subst -nocommands -novariables $_mmgpsc(y2label)]
    set curve_count 0
    foreach ordinate $data_index(all) {
	if {![info exists menu_select(y,$ordinate)]} { continue } ;# Safety
	if {! $menu_select(y,$ordinate) } { continue }        ;# Skip curve
	ChangeCurve 1 $ordinate 0
	incr curve_count
    }
    foreach ordinate $data_index(all) {
	if {![info exists menu_select(y2,$ordinate)]} { continue } ;# Safety
	if {! $menu_select(y2,$ordinate) } { continue }        ;# Skip curve
	ChangeCurve 2 $ordinate 0
	incr curve_count
    }
    $graph DrawAxes
    UpdateGraphDisplay

    $graph SetKeyState $showkey $key_pos
    ## Reset original key display state

    return $curve_count
}

# Data storage
#   Data is stored in two global arrays: data_value and data_index.
# The first is an array of lists indexed by import order, e.g.,
#                data_value(4)
# Each element is a list of values.  The data_index array provides a
# mapping from label names to position inside the data_value lists.
# This position is constant for each label across all data_value entries.
# A data_value list entry is {} if no data field corresponding to that
# entry was entered on that import record.  Also, some data_value() lists
# may be longer than others.  If data_index(label) points past the end
# of a data_value() list, then the corresponding value is {} i.e.,
# missing.  The special record {} (empty) indicates a break in the
# curve data.
#   The data_value array also contains two elements,
# data_value(count,start) and data_value(count,end) which denote the first
# and last data_value record indices stored in the array, i.e.,
#      data_value(count,start) <= i < data_value(count,end)
# Similarly, the data_index(count) element contains the length of the
# longest data_value() list.  data_index(all) is an ordered list of
# the data labels.
#   A previous version of this code stored each data item in an array
# element (as opposed to grouping each record together in a list).  For
# 10 element data records, data import appears to be about 20% faster the
# old way, but storage costs are about 4 times higher (about 1K per record
# as opposed to about 250 bytes per record).  Moreover, it is probably
# possible to streamline the new code somewhat.
#
#   The return value for the import StoreDataTable routine is a list of all
# new labels (if any).  Each call to "StoreDataTable" represents 1 record.
# If a labels occurs more than once in an import record, only the last
# instance is stored.
#
# Addendum, 21-May-1998 mjd: As the first step towards a move away from
#  importing triples towards separate column and bare data records, I
#  have added
#     data_use()      : Array mapping storage list index positions to
#                     : positions in the incoming bare data record lists.
#     proc SetupDataRecordColumns { columns_list unit_list }
#                     : Adds column labels to data_index and data_unit
#                     : lists as necessary, and sets up data_use
#                     : for subsequent use by the AddDataRecord proc.
#     proc AddDataRecord { data_list redraw }
#                     : Adds data to the data_value array, via the
#                     : data_use ordering.
#
# Addendum, 9-Mar-2010 mjd: Added data_base_value array.  This array
#  records the first non-empty value for each label in the data_value
#  array lists.  This is used by the auto_offset code.
#
# Addendum, 17-Apr-2019 mjd: Changed data_base_value array from a single
#  value for additive offsets to a two value list where the first entry
#  is an additive offset as before, but the second is a multiplicative
#  value used if the corresponding y-axis is logscaled.
#
set data_use(count) 0
set data_value(count,start) 0
set data_value(count,end) 0
set data_index(count) 0   ;# Current maximum length of data_value lists.
set data_index(all) {}
proc StoreDataTable { conn triples } {
    global data_value data_index data_unit _mmgraph
    set new_labels {}
    foreach trip $triples {
	foreach {label unit value} $trip {}
	if {[string match @* $label]} {
            set checkname "$conn$label"
	    if {[string compare $_mmgraph(jobname) $checkname]!=0} {
		# Job (file) name change marks change
		# in source problem.  Setup for autoreset
		# operation.
		set _mmgraph(table_end) 1
		set _mmgraph(jobname) $checkname
		AddDataRecord {} 0 ;# Add break record
	    }
	    continue
	}
	if {[catch {set data_index($label)} index]} {
	    # New label
	    set index $data_index(count)
	    set data_index($label) $index
	    lappend data_index(all) $label
	    set data_unit($label) $unit
	    incr data_index(count)
	    lappend new_labels $label
	}
	set new_value($index) $value    ;# Store temporarily in an array
    }

    # Find out-of-date columns
    set obsolete_columns {}
    if {$_mmgraph(table_end)} {
	# New table.  Compare to similar code in mmDataTable
	set new_lbls {}
	foreach trip $triples {
	    set lbl [lindex $trip 0]
	    if {![string match @* $lbl]} {
		lappend new_lbls $lbl
		set new_unit($lbl) [lindex $trip 1]
	    }
	}
	if {[llength $new_lbls]>0} {
	    if {$_mmgraph(autoreset)} {
		# Remove any columns not in new_lbls list
		foreach lbl $data_index(all) {
		    if {[lsearch -exact $new_lbls $lbl]<0} {
			# Label not in new label list, so
			# mark for later removal.  We don't
			# remove right away because that would
			# entail reworking indices in the
			# new_value array.
			lappend obsolete_columns $lbl
		    } elseif {[string compare \
			    $new_unit($lbl) $data_unit($lbl)]!=0} {
			# Units change; throw out old data and
			# change units
			ClearDataColumn $lbl
			set data_unit($label) $new_unit($lbl)
		    }
		}
	    }
	    set _mmgraph(table_end) 0
	}
    }

    # Construct data list
    set new_list {} ;# This becomes break record if no new data.
    if {[info exists new_value]} {
	for {set index 0} {$index<$data_index(count)} {incr index} {
	    if {[catch {lappend new_list $new_value($index)}]} {
		lappend new_list {}      ;# Missing element
	    }
	}
    }
    set recno $data_value(count,end) ;  incr data_value(count,end)
    set data_value($recno) $new_list
    if {$_mmgraph(autosave)} { SaveRecord $new_list }
    PtBufOverflowCheck
    if {![string match {} $new_labels]} {
	AppendAxesSelect $new_labels
    }
    UpdateGraphData $recno 1

    if {[llength $obsolete_columns]>0} {
	foreach lbl $obsolete_columns {
	    RemoveColumn $lbl
	}
	UpdateGraphDisplay
    }

    return $new_labels
}

proc SetupDataRecordColumns { columns_list unit_list } {
    # Initialize column input correspondance for bare
    # data record input.
    global data_index data_unit data_use _mmgraph

    # If autoreset enabled, first remove any old data columns
    # missing from new list.
    if {$_mmgraph(autoreset) && [llength $columns_list]>0} {
	foreach lbl $data_index(all) {
	    set index [lsearch -exact $columns_list $lbl]
	    if {$index<0} {
		# $lbl not in new column list
		RemoveColumn $lbl
	    } elseif {[string compare \
		    [lindex $unit_list $index] $data_unit($lbl)]!=0} {
		# Units change; throw out old data and change units
		ClearDataColumn $lbl
		set data_unit($lbl) [lindex $unit_list $index]
                ChangeCurve 1 $lbl 0
                ChangeCurve 2 $lbl 0
	    }
	}
    }

    # Build new data_use list
    catch {unset data_use}
    set new_labels {}
    set offset 0
    foreach label $columns_list unit $unit_list {
	if {[catch {set data_index($label)} index]} {
	    # New column
	    set index $data_index(count)
	    set data_index($label) $index
	    lappend data_index(all) $label
	    set data_unit($label) $unit
	    incr data_index(count)
	    lappend new_labels $label
	}
	set data_use($index) $offset
	incr offset
    }
    set data_use(count) $offset  ;# Data record length
    AppendAxesSelect $new_labels
    return $new_labels
}

proc AddDataRecord { data_list redraw } {
    # Add bare data record, ordered by the data_use() array.
    # Use "AddDataRecord {} 0" to add a break record to
    # the data_value array, with protection against creating
    # runs of break records.

    global data_index data_value data_unit data_use _mmgraph

    # Check that import is a valid list
    if {[catch {llength $data_list}]} {
       return -code error \
          [format "Import data_list is not a list: %s" $data_list]
    }

    # If data_list is longer than data_use column list, add
    # dummy column labels
    if {[llength $data_list]>$data_use(count)} {
	# Find label of the form "Unlabeled #" with largest "#",
	# and store "#" in dummy_count
	set dummy_count 0
	foreach tlabel $data_index(all) {
	    if {[regexp -- {^Unlabeled ([0-9]+)$} $tlabel \
		    tmatch tcount]} {
		if {$tcount>$dummy_count} {set dummy_count $tcount}
	    }
	}
	for {set offset $data_use(count)} \
		{$offset<[llength $data_list]} \
		{incr offset} {
	    incr dummy_count
	    set dummy_label "Unlabeled $dummy_count"
	    set index $data_index(count)
	    set data_use($index) $offset
	    set data_index($dummy_label) $index
	    lappend data_index(all) $dummy_label
	    set data_unit($dummy_label) {}
	    lappend new_labels $dummy_label
	    incr data_index(count)
	}
	set data_use(count) $offset
	AppendAxesSelect $new_labels
    }

    # Construct data list
    set new_list {} ;# Becomes break record if data_list is empty.
    if {[llength $data_list]>0} {
	for {set index 0} {$index<$data_index(count)} {incr index} {
	    if {[catch {set offset $data_use($index)}]} {
		lappend new_list {}      ;# Missing element
	    } else {
		lappend new_list [lindex $data_list $offset]
	    }
	}
    } else {
	# Don't duplicate successive break records, or put one in
	# front of list.
	set recno [expr $data_value(count,end) - 1]
	if {$recno<$data_value(count,start) || \
		(![catch {llength $data_value($recno)} last_length] \
		 && $last_length==0)} {
	    return
	}
    }
    set recno $data_value(count,end) ;  incr data_value(count,end)
    set data_value($recno) $new_list
    if {$_mmgraph(autosave)} { SaveRecord $new_list } ;# Save to disk
    PtBufOverflowCheck
    UpdateGraphData $recno $redraw
}

proc DeleteMenuEntryByName { menu label } {
    # The '$menu delete $label' command has unfortunate behavior
    # if "$label" looks like an integer---it deletes the entry at
    # index $label rather than the entry with -label $label.
    # Sigh...
    #   Note: If the menu is tearoff, then index 0 is the tearoff
    # entry, which does not have a -label configuration option.  If
    # the menu is not tearoff, then index 0 is the first entry.
    # To cover both cases, we start at 0 but put a catch around the
    # -label request.
    #   Note 2: There appears to be a bug in Tcl/Tk 8.4.12.0 (others?)
    # on Windows/XP whereby deleting a menu entry causes all remaining
    # entries below the deleted entry to not highlight when the mouse
    # is placed over them.  One workaround is to toggle the -tearoff
    # flag.
    for {set i 0} {$i<=[$menu index end]} {incr i} {
	if {![catch {$menu entrycget $i -label} xlab]} {
	    if {[string compare $label $xlab]==0} {
		$menu delete $i
                $menu configure -tearoff [expr {![$menu cget -tearoff]}]
                $menu configure -tearoff [expr {![$menu cget -tearoff]}]
		break
	    }
	}
    }
}

proc RemoveColumn { label } {
    global data_index data_use data_value data_unit
    global xmenu ymenu y2menu menu_select
    global graph _mmgraph

    # What is index of $label?
    if {[catch {set data_index($label)} index]} {
	return -code error "Unknown label $label"
    }

    # Remove $label from data_index array
    unset data_index($label)
    set data_index(all) [lreplace $data_index(all) $index $index]

    # Adjust other index labels
    foreach lab [lrange $data_index(all) $index end] {
	set data_index($lab) [incr data_index($lab) -1]
    }
    incr data_index(count) -1

    # Remove data_unit reference
    unset data_unit($label)

    # Remove all data associated with $label
    if {$data_index(count)<1} {
	# No data left!
	unset data_value
	set data_value(count,start) 0
	set data_value(count,end) 0
    } else {
	# Remove data at column $index from each row
	for {set i $data_value(count,start)} \
		{$i<$data_value(count,end)} {incr i} {
	    if {[llength $data_value($i)]>$index} {
		set data_value($i) \
			[lreplace $data_value($i) $index $index]
	    }
	}
    }

    # Adjust data_use record
    if {[info exists data_use($index)]} {
	# Deleted column specified in data_use.  Assume for now that
        # subsequent input will come from a fresh table with new column
	# labels, and so just clear out the data_use array.  If we ever
	# use bare-record (data_use) style input dynamically (i.e., fed
	# in over a socket), then we may have to put in a special
	# handler here.  The current code should at least make it easy
	# to spot the problem.
	unset data_use
	set data_use(count) 0
    } else {
	# Otherwise, column being removed is not specified in data_use
	# record.  Thus, all that is needed is to adjust the indexing.
	set i $index
	set j [expr {$index+1}]
	while {$j<$data_index(count)} {
	    if {[info exists data_use($j)]} {
		set data_use($i) $data_use($j)
		unset data_use($j)
	    }
	    set i $j
	    incr j
	}
    }

    # Remove from graph display
    if {[string compare $label $menu_select(x)]==0} {
	# Column in use as x-axis selection
	$graph DeleteAllCurves
	set menu_select(x) {}
    } else {
	if {$menu_select(y,$label)} {
	    set menu_select(y,$label) 0
	    ChangeCurve 1 $label 0
	}
	if {$menu_select(y2,$label)} {
	    set menu_select(y2,$label) 0
	    ChangeCurve 2 $label 0
	}
    }
    unset menu_select(y,$label)
    unset menu_select(y2,$label)
    DeleteMenuEntryByName $xmenu $label
    DeleteMenuEntryByName $ymenu $label
    DeleteMenuEntryByName $y2menu $label


    # Handle autosave, if enabled
    if {$_mmgraph(autosave)} {
	# Is label in save list?
	set i [lsearch -exact $_mmgraph(autosave_collab) $label]
	if {$i<0} {
	    # label not in outbound list.  We just need to adjust
	    # the column index list
	    set _mmgraph(autosave_column_index_list) {}
	    foreach lab $_mmgraph(autosave_collab) {
		lappend _mmgraph(autosave_column_index_list) \
			$data_index($lab)
	    }
	} else {
	    # Otherwise, close autosave file and alert user
	    CloseAutoSave
	    Oc_Log Log "Autosave stopped due to removal of\
		    column $label, which had been selected\
		    for autosaving." error
	}
    }
}

proc RemoveAllColumns {} {
    # NOTE: This routine isn't set up to gracefully end
    # data autosave.  Autosaving should be terminated
    # in an appropriate manner _before_ calling this
    # routine.

    # End autosave (safety)
    global _mmgraph
    if {$_mmgraph(autosave)} {
	# Otherwise, close autosave file and alert user
	CloseAutoSave
	Oc_Log Log "Autosave stopped due to destruction of\
		all column information." error
    }

    global data_index data_use data_value data_unit
    global xmenu ymenu y2menu menu_select
    global graph _mmgraph

    unset data_index
    set data_index(count) 0
    set data_index(all) {}

    catch {unset data_unit}

    unset data_value
    set data_value(count,start) 0
    set data_value(count,end) 0

    unset data_use
    set data_use(count) 0

    $graph Reset

    $xmenu delete 1 end
    $ymenu delete 1 end
    $y2menu delete 1 end
    unset menu_select
    set menu_select(x) {}
}

proc Reset {} {
    # Destroy all open dialog boxes
    global dialog
    foreach elt [array names dialog] {
	if {[string compare $dialog($elt) [info commands $dialog($elt)]]==0} {
	    $dialog($elt) Delete
	}
	unset dialog($elt)
    }
    # Close any menu tearoffs
    foreach w [winfo children .] {
	if {![catch {$w cget -type} type]} {
	    if {[string compare tearoff $type]==0} {
		destroy $w
	    }
	}
    }
    CloseAutoSave
    RemoveAllColumns
    SetDefaultConfiguration
}

proc ClearData {} {
    global data_value data_base_value graph
    unset data_value
    if {[info exists data_base_value]} {
       unset data_base_value
    }
    set data_value(count,start) 0
    set data_value(count,end) 0
    $graph ResetSegmentColors
    $graph DeleteAllData
}

proc ClearDataColumn { label } {
    # NB: This code does not reset data_value(count,start) or
    #  data_value(count,end), even if the result of this proc
    #  is to leave all data_value records filled with nothing
    #  but empty lists.
    global data_index data_value data_base_value graph
    set index $data_index($label)
    for {set i $data_value(count,start)} \
	    {$i<$data_value(count,end)} {incr i} {
	if {[llength $data_value($i)]>$index} {
	    set data_value($i) \
		    [lreplace $data_value($i) $index $index {}]
	}
    }
    unset data_base_value($label)
}

proc ClearDataInteractive {} {
    set response [Ow_Dialog 1 "[Oc_Main GetInstanceName]: Clear Data" \
	    warning "Are you *sure* want to erase all data stored\
	    in this graph widget?" \
	    {} 1 "Yes" "No"]
    if { $response == 0 } { ClearData }
}

proc RefreshGraphData {} {
   # The graph object holds a copy of a subset of the data
   # in the global array data_value.  This proc refreshes
   # that copy.
   global graph menu_select _mmgpsc
   global data_index data_value data_base_value

   $graph DeleteAllData
   UpdateBaseValues

   set ord_list {}
   foreach ord $data_index(all) {
      if {$menu_select(y,$ord) || $menu_select(y2,$ord)} {
         lappend ord_list $ord $data_index($ord)
      }
   }
   if {[llength $ord_list]==0} {
      return ;# No curves
   }

   set abscissa $menu_select(x)
   if {[catch {set xindex $data_index($abscissa)}]} {
      return ;# Abscissa not selected
   }

   set istart $data_value(count,start)
   set iend   $data_value(count,end)
   if {$istart >= $iend} {
      # No data
      $graph DrawCurves
      return
   }
   for {set i $istart} {$i<$iend} {incr i} {
      set record $data_value($i)
      # If this is a curve break record, then record will be empty.
      # In this case both xval and yal below will be set to the
      # empty string, which is correct data for the curve_data lists.
      set xval [lindex $record $xindex]
      foreach {ord yindex} $ord_list {
         set yval [lindex $record $yindex]
         lappend curve_data($ord) $xval $yval
      }
   }

   foreach ord $data_index(all) {

      set offset_pair [list {} {}]
      catch {set offset_pair $data_base_value($ord)}
      foreach {linoff logoff} $offset_pair { break }
      if {[string match {} $linoff]} { set linoff 0.0 }
      if {[string match {} $logoff]} { set logoff 1.0 }
      
      if {$menu_select(y,$ord)} {
         if {!$_mmgpsc(auto_offset_y)} {
            $graph AddDataPoints $ord 1 $curve_data($ord)
         } else {
            set offset_data {}
            if {!$_mmgpsc(ylogscale)} {
               # Linear scale
               foreach {xval yval} $curve_data($ord) {
                  if {[string match {} $yval]} {
                     lappend offset_data $xval {}
                  } else {
                     lappend offset_data $xval [expr {$yval - $linoff}]
                  }
               }
            } else {
               # Log scale
               foreach {xval yval} $curve_data($ord) {
                  if {[string match {} $yval]} {
                     lappend offset_data $xval {}
                  } else {
                     lappend offset_data $xval [expr {$yval/$logoff}]
                  }
               }
            }
            $graph AddDataPoints $ord 1 $offset_data
         }
      }

      if {$menu_select(y2,$ord)} {
         if {!$_mmgpsc(auto_offset_y2)} {
            $graph AddDataPoints $ord 2 $curve_data($ord)
         } else {
            set offset_data {}
            if {!$_mmgpsc(y2logscale)} {
               # Linear scale
               foreach {xval yval} $curve_data($ord) {
                  if {[string match {} $yval]} {
                     lappend offset_data $xval {}
                  } else {
                     lappend offset_data $xval [expr {$yval - $linoff}]
                  }
               }
            } else {
               # Log scale
               foreach {xval yval} $curve_data($ord) {
                  if {[string match {} $yval]} {
                     lappend offset_data $xval {}
                  } else {
                     lappend offset_data $xval [expr {$yval/$logoff}]
                  }
               }
            }
            $graph AddDataPoints $ord 2 $offset_data
         }
      }
   }

   $graph DrawCurves
   return
}

proc ThinDataRaw { skip } {
   # Index through data_value array, keeping only every "$skip" element.
   # A complication here is that we want to retain the first and last
   # element of each curve segment.  (Breaks in curves are marked by
   # an empty record in the data_value array.)
   global data_value
   set j 0  ;# Index into rebuilt data_value array.
   set k 0  ;# Interval record skip count.
   set skipped_record {}
   for {set i $data_value(count,start)} {$i<$data_value(count,end)} {incr i} {
      set record $data_value($i)
      unset data_value($i)
      if {[string match {} $record]} {
         # Blank record, which is used as a marker for
         # curve breaks.  Retain this marker.
         if {![string match {} $skipped_record]} {
            set data_value($j) $skipped_record
            incr j
            set skipped_record {}
         }
         set data_value($j) {}
         incr j
         set k 0
      } else {
         if {$k==0} {
            set data_value($j) $record
            incr j
            set skipped_record {}
         } else {
            set skipped_record $record
         }
         if {[incr k]>=$skip} { set k 0 }
      }
   }
   if {![string match {} $skipped_record]} {
      set data_value($j) $skipped_record
      incr j
   }
   set data_value(count,start) 0
   set data_value(count,end) $j
   RefreshGraphData
}

proc ThinDataIteration { skip } {
   # Similar to proc ThinDataRaw, but tries to maintain equal spacing
   # based on Iteration counts.  For dynamic simulations We might want
   # to consider an alternative thinning option indexed off of time.
   global data_value data_index
   set iteration_index -1
   set index_check [lsearch -all -regexp $data_index(all) {:Iteration$}]
   if {[llength $index_check]==1} {
      set iteration_index [lindex $index_check 0]
      set istart $data_value(count,start)
      set ita [lindex $data_value($istart) $iteration_index]
      set istop  $data_value(count,end)
      set itb [lindex $data_value([expr {$istop-1}]) $iteration_index]
      if {![regexp {^[0-9]+$} $ita] || ![regexp {^[0-9]+$} $itb] \
             || $ita >= $itb} {
         set iteration_index -1  ;# Bad data
      }
   }
   if {$iteration_index < 0} {
      # No unique, valid iteration index found.  Fall back to
      # raw record filtering:
      return [ThinDataRaw $skip]
   }
   set record_count [expr {$istop-$istart}]
   set it_skip [expr {$skip*($itb-$ita)/double($record_count)}]
   ## Note that it_skip might not be an integer
   if {$it_skip<=1.0} {
      # Bail
      return [ThinDataRaw $skip]
   }

   # After thinning, the difference in iteration count between two
   # successive records should be approximately it_skip.  But also
   # preserve the first and last record in each curve.
   set j 0  ;# Index into rebuilt data_value array.
   set k -1.0 ;# Next iteration to not skip
   set skipped_record {}
   for {set i $istart} {$i<$istop} {incr i} {
      set record $data_value($i)
      unset data_value($i)
      if {[string match {} $record]} {
         # Blank record, which is used as a marker for
         # curve breaks.  Retain this marker.
         if {![string match {} $skipped_record]} {
            set data_value($j) $skipped_record
            incr j
            set skipped_record {}
         }
         set data_value($j) {}
         incr j
         set k -1.0 ;# Very next iteration is not skipped
      } else {
         set test_it [lindex $record $iteration_index]
         if {[catch {expr {$test_it + $it_skip}} test_k]} {
            # Apparently iteration count in this record is not an
            # integer; punt
            set data_value($j) $record
            incr j
            set skipped_record {}
         } elseif {$test_it>=$k} {
            set data_value($j) $record
            incr j
            set skipped_record {}
            if {$k>=0} {
               set k [expr {$k+$it_skip}]
            } else {
               set k $test_k
            }
         } else {
            set skipped_record $record
         }
      }
   }
   if {![string match {} $skipped_record]} {
      set data_value($j) $skipped_record
      incr j
   }
   set data_value(count,start) 0
   set data_value(count,end) $j
   RefreshGraphData
}

proc ThinDataInteractive { skip } {
   if {$skip<2} {
      return ;# Nothing to do
   } elseif {$skip==2} {
      set ordinal "second"
   } elseif {$skip==3} {
      set ordinal "third"
   } elseif {$skip==4} {
      set ordinal "fourth"
   } elseif {$skip==5} {
      set ordinal "fifth"
   } elseif {$skip==6} {
      set ordinal "sixth"
   } elseif {$skip==7} {
      set ordinal "seventh"
   } elseif {$skip==8} {
      set ordinal "eighth"
   } elseif {$skip==9} {
      set ordinal "ninth"
   } elseif {$skip==10} {
      set ordinal "tenth"
   } else {
      set ordinal "${skip}th"
   }
   set response [Ow_Dialog 1 "[Oc_Main GetInstanceName]: Thin Data" warning \
                    "Are you *sure* want to thin all data stored\
	    in this graph widget, keeping only every $ordinal point?" \
                    {} 1 "Yes" "No"]
   if { $response == 0 } { ThinDataIteration $skip }
}

proc UpdateBaseValues {} {
   # Base values used by auto_offset code
   global data_value data_index data_base_value
   set istart $data_value(count,start)
   set iend   $data_value(count,end)
   foreach label $data_index(all) {
      set lin_offset {}
      set log_offset {}
      if {[info exists data_base_value($label)]} {
         foreach {lin_offset log_offset} $data_base_value($label) { break }
      }
      if {![string match {} $log_offset]} {
         continue   ;# Keep current settings
      }
      set index $data_index($label)
      set i $istart
      if {[string match {} $lin_offset]} {
         while {$i<$iend} {
            # Find and store first non-empty value and non-zero value
            set val [lindex $data_value($i) $index]
            incr i
            if {![string match {} $val]} {
               set lin_offset $val
               break
            }
         }
      }
      if {![string match {} $lin_offset] && $lin_offset != 0.0} {
         set log_offset [expr {abs($lin_offset)}]
      } else {
         while {$i<$iend} {
            set val [lindex $data_value($i) $index]
            incr i
            if {![string match {} $val] && $val != 0.0} {
               set log_offset [expr {abs($val)}]
               break
            }
         }
      }
      set data_base_value($label) [list $lin_offset $log_offset]
   }
}


proc StopAutoSaveInteractive {} {
    set response [Ow_Dialog 1 "[Oc_Main GetInstanceName]: Stop Auto Save" \
	    warning "Are you *sure* want to stop automatically saving\
	    data received by this graph widget?" \
	    {} 1 "Yes" "No"]
    if { $response == 0 } { CloseAutoSave }
}

proc CloseSource { conn } {
   # This proc is called when connection is deleted.
   global _mmgraph
   if {[info exists _mmgraph(jobname)] \
           && [string match "${conn}*" $_mmgraph(jobname)]} {
      set _mmgraph(table_end) 1
      set _mmgraph(jobname) {}
   }
}

if {!$no_net} {
    # Protocol
    package require Net 2
    Net_Protocol New protocol -name "OOMMF DataTable protocol 0.1"
    $protocol Init {
	Oc_EventHandler New _ $connection Delete \
		[list CloseSource $connection] -oneshot 1
    }
    $protocol AddMessage start DataTable { triples } {
	StoreDataTable $connection $triples
	return [list start [list 0 1]]
    }

    Net_Server New server -protocol $protocol -alias [Oc_Main GetAppName]
    $server Start 0
} else {
   proc AddTriples { conn triples } {
      StoreDataTable $conn $triples
   }
}

Ow_SetIcon .
wm deiconify .

# Load files from command line, if any
# Note: Is it possible for the loading of a very large file
#  to interfere with, and in particular, cause a timeout during,
#  initialization of the networking interface?
foreach fn $loadList { OpenFile $fn }

# Enter event loop
if {!$no_event_loop} {
    vwait forever
}
