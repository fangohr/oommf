# FILE: dirselect.tcl
#
# Directory selection dialog sub-widget
#
# Last modified on: $Date: 2008-04-01 22:04:52 $
# Last modified by: $Author: donahue $
#
# Callbacks: UPDATE -value $value
#
# debugging enable/disable/level

Oc_Class Ow_DirSelect {
    const public variable winpath
    const public variable outer_frame_options = "-bd 4 -relief ridge"
    const public variable selectmode = browse ;# Listbox select mode
    const public variable troughcolor ;# Scrollbar trough color
    const public variable horizontal_scroll_mode = 0 ;# Set to 1 to get
    ## horizontal scroll bar turned on.
    public variable callback = Oc_Nop
    public variable value = {}   ;# Current directory path
    const public variable valuewidth = 35
    private variable pathbox
    public variable dirlist
    Constructor {basewinpath args} {
	# Don't add '.' suffix if there already is one.
	# (This happens chiefly when basewinpath is exactly '.')
	if {![string match {.} \
		[string index $basewinpath \
		[expr [string length $basewinpath]-1]]]} {
	    append basewinpath {.}
	}
	eval $this Configure -winpath ${basewinpath}w$this $args
	# Temporarily disable callbacks until construction is complete
	set callback_keep $callback ;	set callback Oc_Nop

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
	# destroying $winpath.  (The instance destructor should
	# not "destroy" $frame, because $frame is owned external
	# to this class.
	eval frame $winpath $outer_frame_options
	Ow_EntryBox New pathbox $winpath \
		-label "Path:" -display_preference "end" \
		-valuewidth $valuewidth \
		-callback "$this PathBoxCallback"
	pack [$pathbox Cget -winpath] -side top -fill x -expand 0
	frame $winpath.listframe -relief raised -bd 2
	set dirlist [listbox $winpath.list -bd 0 \
		-selectmode $selectmode -height 6 \
		-yscrollcommand "$winpath.listscry set"]
	global tcl_platform
	if {[string match windows $tcl_platform(platform)]} {
	    $dirlist configure -background SystemButtonFace
	}
        ## Sometime around Tcl/Tk 8.0.4, the default -background
	## color for listboxes on Windows changed from SystemButtonFace
	## to SystemWindow; the former was usually gray, the latter
        ## white.  *I* think it looks pretty hideous...  -mjd 2002/10/22
	bind $dirlist <Key-Return>  "$this DirSelect %W"
	bind $dirlist <Double-Button-1> [bind $dirlist <Key-Return>]
	bind $dirlist <Button> "+focus %W"
	scrollbar $winpath.listscry -bd 2 -command "$dirlist yview"
	if {![string match {} $troughcolor]} {
	    $winpath.listscry configure -troughcolor $troughcolor
	}
	pack $winpath.listscry -side right -fill y -in $winpath.listframe
	if {$horizontal_scroll_mode} {
	    scrollbar $winpath.listscrx -bd 2 -command "$dirlist xview" \
		    -orient horizontal
	    if {![string match {} $troughcolor]} {
		$winpath.listscrx configure -troughcolor $troughcolor
	    }
	    pack $winpath.listscrx -side bottom -fill x -in $winpath.listframe
	    $winpath.list configure -xscrollcommand "$winpath.listscrx set"
	}

	pack $dirlist -side top -fill both -expand 1 -in $winpath.listframe
	pack $winpath.listframe -fill both -expand 1
	bind $winpath <Destroy> "$this WinDestroy %W"
	$this SetPath [pwd]

	# Re-enable callbacks
	set callback $callback_keep
    }
    method DirSelect { win } { value } {
	# Read new directory from listbox
	set index [$win curselection]
	if { [string match {} $index] } then return
	set newpath [file join $value [$win get $index]]
	$this SetPath $newpath
    }
    method PathBoxCallback { subwidget actionid args } {} {
	$this SetPath [lindex $args 1]
    }
    method SetPath { newpath } { value callback pathbox } {
	if {[string compare $newpath $value]==0} { return }
	if {[file isdirectory "$newpath"]} {
	    catch {cd $newpath}
	    set temppath [pwd]
	    if {[string compare $value $temppath]!=0} {
		set value $temppath
		eval $callback $this UPDATE -value {$value}
		$this FillList
	    }
	}
	$pathbox Set $value
    }
    method WinDestroy { w } { winpath } {
	if {[string compare $winpath $w]!=0} { return } ;# Child destroy event
	$this Delete
    }
    method Rescan {} {} {
	$this FillList
    }
    method FillList {} { value pathbox dirlist } {
	cd $value
	set value [pwd]
	$pathbox Set $value
	$dirlist delete 0 end
	$dirlist insert end ".."
	set filelist [lsort [glob -nocomplain *]]
	foreach i $filelist {
	    if {[file isdirectory $i]} { $dirlist insert end $i }
	}
    }
    Destructor {
	# Delete pathbox
	if {[info exists pathbox] \
		&& ![string match {} [info commands $pathbox]]} {
	    $pathbox Delete
	}
	# Delete everything else
	if {[info exists winpath] && [winfo exists $winpath]} {
            # Remove <Destroy> binding
	    bind $winpath <Destroy> {}
	    # Destroy instance frame, all children, and bindings
	    destroy $winpath
	}
    }
}
