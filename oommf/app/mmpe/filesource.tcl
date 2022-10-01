# FILE: filesource.tcl

# Library support
package require Oc 2	;# [Oc_TempName]
package require Ow 2
package require Net 2
wm withdraw .

Oc_Main SetAppName FileSource
Oc_Main SetVersion 2.0b0
regexp \\\044Date:(.*)\\\044 {$Date: 2015/03/25 16:43:44 $} _ date
Oc_Main SetDate [string trim $date]
# regexp \\\044Author:(.*)\\\044 {$Author: dgp $} _ author
# Oc_Main SetAuthor [Oc_Person Lookup [string trim $author]]
Oc_Main SetAuthor [Oc_Person Lookup donahue]
Oc_Main SetHelpURL [Oc_Url FromFilename [file join [file dirname \
        [file dirname [file dirname [Oc_DirectPathname [info \
        script]]]]] doc userguide userguide \
	Micromagnetic_Problem_File_.html]]

Oc_CommandLine ActivateOptionSet Net

Oc_CommandLine Option [Oc_CommandLine Switch] {
	{{file optional} {} "MIF file to provide"}
    } {
	global newname; set newname $file
} "End of options; next argument is file"
Oc_CommandLine Parse $argv

set  SmartDialogs 1

#**************************************************************************#  
#top level window
set exportLabel [label .l -relief ridge -bd 4 -padx 4 -pady 4]
pack .l -side top -expand 1

proc SetFilename { newpath newname } {
    global importedFilename exportLabel
    if {[string match {} $newname]} {
	set importedFilename {}
	$exportLabel configure -text "No file selected"
    } else {
	set importedFilename [file join $newpath $newname]
	$exportLabel configure -text "Export: $importedFilename"
    }
}

set newpath [file dirname [Oc_DirectPathname $newname]]
set newname [file tail $newname]
SetFilename $newpath $newname

proc Die { win } {
    if {![string match . $win]} { return }  ;# Child destroy event
    exit
}
bind . <Destroy> "+Die %W"
wm protocol . WM_DELETE_WINDOW { exit }
Oc_EventHandler New _ Oc_Main Exit {proc Die win {}}

wm deiconify .
wm iconname . [wm title .]

#**************************************************************************#
# Menubar
set menubar .menubar
foreach {menuname helpmenu} [Ow_MakeMenubar . $menubar File Help] {}
$menuname add command -label "Open..." -command\
           [list LoadButton $menuname "Open..."] -underline 0
$menuname add separator
$menuname add command -label Exit -command { exit } -underline 1
Ow_StdHelpMenu $helpmenu

# Set minimum primary window width
set menuwidth [Ow_GuessMenuWidth $menubar]
set bracewidth [Ow_SetWindowTitle . [Oc_Main GetInstanceName]]
if {$bracewidth<$menuwidth} {
   set bracewidth $menuwidth
}
set brace [canvas .brace -width $bracewidth -height 0 -borderwidth 0 \
        -highlightthickness 0]
pack $brace -side top
Oc_EventHandler New _ Net_Account NewTitle [subst -nocommands {
  $brace configure -width \
    [expr {%winwidth<$menuwidth ? $menuwidth : %winwidth}]
}]

#**************************************************************************#
# File select
proc LoadButton { btn item } {
    if { [string match disabled [$btn entrycget $item -state]] } {
        return
    }
    global oommf SmartDialogs
    Ow_FileDlg New dialog -callback LoadCallback \
	    -dialog_title "Open File -- [Oc_Main GetTitle]" \
	    -selection_title "Open MIF File..." \
	    -select_action_id "OPEN" \
	    -filter "*.mif" \
	    -menu_data [list $btn $item] \
	    -smart_menu $SmartDialogs
    # Set icon
    Ow_SetIcon [$dialog Cget -winpath]
}
proc LoadCallback { widget actionid args } {
    global resetfilepath
    switch -exact -- $actionid {
	DELETE {
	    eval [join $args]
	    return
	}
	OPEN   {
	    foreach { newpath newfile } [$widget GetFileComponents] {}
	    SetFilename $newpath $newfile
	}
	default {
	    return "ERROR (proc LoadCallBack):\
		    Invalid actionid: $actionid"
	}
    }
}

#**************************************************************************#  
# networking
Net_Protocol New protocol -name "OOMMF ProbEd protocol 0.1"
$protocol AddMessage start GetProbDescFile {} {
    global importedFilename
    set exportedFilename [Oc_TempName]
    file copy -force $importedFilename $exportedFilename
    return [list start [list 0 $exportedFilename]]
}
Net_Server New server -protocol $protocol -alias FileSource
$server Start 0

Ow_SetIcon .

# This is required to keep the app from exiting before deregistration completes
vwait forever
