# FILE: fileselect.tcl
#
# File selection dialog subwidget
#
# Last modified on: $Date: 2008/04/01 22:04:52 $
# Last modified by: $Author: donahue $
#
# General documentation for dialog subwidget classes:
#
# A dialog subwidget is created with a call of the form
#
#   Ow_FileSelect New mydialog .basewinpath
#
# Here "$mydialog" will be set to the name of a new instance of the
# requested dialog subwidget class (in this example Ow_FileSelect).
# The ".basewinpath" import specifies the Tk window path point
# to attach the subwidget (required).  All the Tk widget elements
# of the subwidget are packed into a frame attached to ".basewinpath".
# The name of this outer frame is available via
#
#  $mydialog Cget -winpath
#
# You will need to use this syntax to pack the subwidget.  (The
# subwidgets do *not* get packed during the construction process.)
#
# Similarly, each subwidget has a "const" public variable
# "outer_frame_options" that may be set during construction to change
# the border options of the outer frame.  The "-winpath" variable may
# be used to access and change the outer frame options after
# construction.
#
# Each subwidget also maintains a "callback" public variable, which
# may be set to a proc for the subwidget to call when some internal
# state change has occurred that is accessible to exterior routines.
# (A callback function is not required.)  The callback, if specified,
# is launched as
#
#  eval $callback $this ACTIONID [optional arguments...]
#
# where "$this" is the instance name (in the above example, the
# value "$mydialog"), and "ACTIONID" identifies the type of internal
# state change.  The "optional arguments", if any, are subwidget and 
# ACTIONID dependent.  See the docs for each subwidget for details.
#
# The values for ACTIONID are subwidget dependent, but should be
# upper case letters, and will generally include "UPDATE", inviting
# exterior routines to query the subwidget public variables via
# "$this Cget".  If there are any optional arguments, they will be a
# list of the form "-varname1 -varname2 ...", denoting the names of
# state variables that may have changed.  If this list is empty then
# the callback routine should assume that any or all of the subwidget
# public variables may have changed.  (Either way, the callback routine
# will need to make a "Cget" call to get the changed values.)
#
# Note 1: Many of the dialog subwidgets maintain a public variable
# named "value" containing the chief export of the subwidget.
#
# Note 2: The callback function can use the command "$subwidget class"
#   (where "subwidget" is the first argument to the callback
#   function, set to "$this" by the subwidget) to obtain the class name
#   of the calling subwidget.
#
# Note 3: The callback will *not* be called during instance construction.
#   One may, of course, artificially call the callback from the exterior
#   code after construction.
#
# Note 4: These widgets bind the <Destroy> event on $winpath to
#   "$this WinDestroy %W", which calls "$this Delete".  This way
#   the widgets automatically delete themselves if the window is
#   closed.  Also, inside the destructor, a call is made to destroy
#   the outer window.  In order to not call the destructor twice,
#   the <Destroy> binding is *deleted* inside the destructor.  This
#   means that one should not bind to the <Destroy> event outside
#   the instance methods.  The destructor should make a callback
#   with actionid DELETE, that can  be used instead.  This is called
#   at the top of the destructor, so $this still exists at the time
#   of the callback.  The variable delete_callback_arg is passed
#   to the callback routine in the optional args field.  This may
#   be set by the client application in anyway it wishes.  The default
#   value is {}.

Oc_Class Ow_FileSelect {
    const public variable winpath
    const public variable outer_frame_options = "-bd 4 -relief ridge"
    const public variable selectmode = browse ;# Listbox select mode
    const public variable troughcolor ;# Scrollbar trough color
    public variable callback = Oc_Nop
    public variable delete_callback_arg = {}
    public variable filter = "*"
    public variable dircur
    public variable compress_suffix = {}
    public variable value = {}   ;# Selected file (full pathname), if any
    public variable filelist
    private variable filterbox
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

	if {![info exists dircur]} {
	    set dircur [pwd]
	}

	# Pack entire subwidget into a subframe.  This makes
	# housekeeping easier, since the only bindings on
	# $winpath will be ones added from inside the class,
	# and also all subwindows can be destroyed by simply
	# destroying $winpath.  (The instance destructor should
	# not "destroy" $frame, because $frame is owned external
	# to this class.
	eval frame $winpath $outer_frame_options
	frame $winpath.filelistframe -relief raised -bd 2
	set filelist [listbox $winpath.filelist -bd 0 \
		-selectmode $selectmode -height 15 \
		-xscrollcommand "$winpath.filescrx set" \
		-yscrollcommand "$winpath.filescry set"]
	global tcl_platform
	if {[string match windows $tcl_platform(platform)]} {
	    $filelist configure -background SystemButtonFace
	}
        ## Sometime around Tcl/Tk 8.0.4, the default -background
	## color for listboxes on Windows changed from SystemButtonFace
	## to SystemWindow; the former was usually gray, the latter
        ## white.  *I* think it looks pretty hideous...  -mjd 2002/10/22
	catch {bind $filelist <Key-space> "$this FileSelect %W"}
	bind $filelist <Key-Return> "$this FileSelect %W"
	bind $filelist <ButtonRelease> "$this FileSelect %W"
	bind $filelist <Button> "focus %W"

	# Add "SELECT" bindings
	bind $filelist <Key-Return> "+$this CallbackSelect"
	bind $filelist <Double-Button-1> "$this CallbackSelect"

	scrollbar $winpath.filescry -bd 2 -command "$filelist yview"
	scrollbar $winpath.filescrx -bd 2 -command "$filelist xview" \
		-orient horizontal
	if {![string match {} $troughcolor]} {
	    $winpath.filescry configure -troughcolor $troughcolor
	    $winpath.filescrx configure -troughcolor $troughcolor
	}

	# Filter box
	Ow_EntryBox New filterbox $winpath \
		-label "Filter:" -value $filter \
		-callback "$this FilterBoxCallback"

        # Pack
	pack [$filterbox Cget -winpath] -side bottom -fill x -expand 0
	pack $winpath.filescry -side right -fill y -in $winpath.filelistframe
	pack $winpath.filescrx -side bottom -fill x -in $winpath.filelistframe
	pack $filelist -side top -fill both -expand 1 \
		-in $winpath.filelistframe
	pack $winpath.filelistframe -fill both -expand 1

        # Setup destroy binding and fill file list.
	bind $winpath <Destroy> "$this WinDestroy %W"
	$this FillList

	# Re-enable callbacks
	set callback $callback_keep
    }
    method CallbackSelect { } { callback value } {
       # Disable callback if no file is selected
       if {![string match {} $value]} {
          # Otherwise, make callback with selected value.
          eval $callback $this SELECT -value {$value}
       }
    }
    method FileSelect { win } { dircur value callback } {
	# Read filename from listbox
	set index [$win curselection]
	if { "$index" == {} } {
	    set value {}
	} else {
	    set value [$win get $index]
	}
	eval $callback $this UPDATE -value {$value}
    }
    method FilterBoxCallback { subwidget actionid args } {} {
	# Callback from the filter entry box.
	$this SetFilter [$subwidget Cget -value]
    }
    method SetFilter { newvalue } { filter } {
	set filter [string trim $newvalue]
	$this FillList
    }
    method SetPath { newdir } { dircur } {
	if {[string compare $newdir $dircur]==0} return
	if {![file isdirectory "$newdir"]} return
	catch {cd $newdir}
	set dircur [pwd]
	$this FillList
    }
    method WinDestroy { w } { winpath } {
	if {[string compare $winpath $w]!=0} { return } ;# Child destroy event
	$this Delete
    }
    method Rescan {} {} {
	$this FillList
    }
    method FillList {} { dircur filter filelist compress_suffix } {
	cd $dircur
	set localfilter [lrange $filter 0 end]
	if {[llength $localfilter]==0} {
	    # Treat empty list as "*"
	    set localfilter "*"
	} else {
	    set localfilterz {}
	    foreach pat $localfilter {
		foreach suffix $compress_suffix {
		    lappend localfilterz "$pat$suffix"
		}
	    }
	    set localfilter [concat $localfilter $localfilterz]
	}
	$filelist delete 0 end;
	$filelist insert end \
	   {*}[lsort [glob -nocomplain -type f -- {*}$localfilter]]
    }
    Destructor {
	# Notify client of impending destruction
	eval $callback $this DELETE {$delete_callback_arg}

	# Delete filterbox
	if {[info exists filterbox] \
		&& ![string match {} [info commands $filterbox]]} {
	    $filterbox Delete
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
