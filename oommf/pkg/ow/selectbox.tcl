# FILE: selectbox.tcl
#
# Selection box dialog sub-widget
#
# Last modified on: $Date: 2007/03/21 01:17:10 $
# Last modified by: $Author: donahue $
#
#   This subwidget provides a wrapper around a 2-entry multiwidget,
# (Path: and File:), allowing path+filename to be treated as a single
# entity.  The -callback function is invoked whenever a callback is
# obtained from the multiwidget (check the multiwidget docs, but
# currently this occurs only on a <Key-Return> event in one of the entry
# boxes).  On this event, the current _displayed_ values are read from
# the Path: and File: entry boxes, and stored in the variables -pathvalue
# and -filevalue.  Then -callback is called with ACTIONID set to SELECT,
# with args "-filevalue -pathvalue".  The client routine
# may at any time access these values to get the last committed value, or
# may call ReadDisplayedFile and ReadDisplayedPath to get the values as
# currently displayed.
#   The widget also provides the routines ReadDisplayedFullname and
# ReadCommittedFullname that return a combined pathvalue+filevalue result.
# WARNING: There is an inherent shortcoming in working with the
# combined filename, because that there is no way to detect when
# the filevalue portion is empty.  For example, if Path: is /foo/bar, and
# File: is empty, then [file dirname /foo/bar] returns foo and
# [file tail /foo/bar] returns bar.  To protect a little against this,
# The combined name return routines will append a "." in place of {}
# for the filevalue portion.
#   The routine SetFullname is provided to fill the Path: and File:
# entries from without.  This sets the committed value as well as the
# displayed text.  This has the same shortcomings as the the Read Fullname
# routines discussed above.  As a workaround, if [file tail $filename]
# returns ".", then the filevalue is set to {}.  It is preferable to
# set the path and file values separately, using the SetPath and
# SetFilename method calls.
#   The default title bar value is "Selection".  This can be set via
# the -title option at construct time.  Set to {} to turn the title
# off.  Other options include -labelwidth and -valuewidth to set the
# sizes of the labels and text entry areas, respectively.
#
# debugging enable/disable/level

Oc_Class Ow_SelectBox {
    const public variable winpath
    const public variable outer_frame_options = "-bd 0 -relief flat"
    const private variable multibox
    public variable pathvalue = {}
    public variable filevalue = {}
    public variable fileshadowvalue = {}
    const public variable title = "Selection"
    const public variable labelwidth = 5
    const public variable valuewidth = 16
    public variable callback = Oc_Nop
    const private variable pathlabel = "Path:"
    const private variable filelabel = "File:"
    Constructor {basewinpath args} {
	# Don't add '.' suffix if there already is one.
	# (This happens chiefly when basewinpath is exactly '.')
	if {![string match {.} \
		[string index $basewinpath \
		[expr [string length $basewinpath]-1]]]} {
	    append basewinpath {.}
	}
	eval $this Configure -winpath ${basewinpath}w$this $args
	eval frame $winpath $outer_frame_options
	## Note: We need a separate outer frame because we can't
	##  bind to <Destroy> on the multibox widget (as usual).

	# Temporarily disable callbacks until construction is complete
	set callback_keep $callback ;	set callback Oc_Nop

	# Create multibox
	set opts [list -labellist [list \
		[list $pathlabel {} [list -commitontab 0]] \
		[list $filelabel {} [list -commitontab 0]]]]
	lappend opts -callback "$this MultiboxCallback"
	if {![string match {} $labelwidth]} {
	    lappend opts -labelwidth $labelwidth
	}
	if {![string match {} $valuewidth]} {
	    lappend opts -valuewidth $valuewidth
	}
	if {![string match {} $title]} {
	    lappend opts -title $title
	}
	eval Ow_MultiBox New multibox {$winpath} $opts
	$multibox EntryConfigure $pathlabel -display_preference "end"
	$multibox EntryConfigure $filelabel \
		-variable [$this GlobalName fileshadowvalue]
	pack [$multibox Cget -winpath] -fill x -expand 0
	
	bind $winpath <Destroy> "$this WinDestroy %W"

	# Re-enable callbacks
	set callback $callback_keep
    }
    public method SetFileDisplayTrace { cmd } {
	trace variable fileshadowvalue w $cmd
	return $fileshadowvalue
    }
    private method CombineName { path file } {
	if {[regexp -- "^\[ \t\n\]*\$" $file]} {
	    # Check for whitespace-only strings
	    set file "."
	}
	set fullname [file join $path $file]
	if {[string compare $path $fullname]==0} {
	    # Safety
	    set fullname [file join $path "."]
	}
	return $fullname
    }
    method ReadDisplayedPath {} { multibox pathlabel } {
	$multibox LookUp $pathlabel
    }
    method ReadDisplayedFile {} { multibox filelabel } {
	$multibox LookUp $filelabel
    }
    method ReadDisplayedFullname {} { multibox pathlabel filelabel } {
	$this CombineName [$multibox LookUp $pathlabel] \
		[$multibox LookUp $filelabel]
    }
    method ReadCommittedFullname {} { pathvalue filevalue } {
	$this CombineName $pathvalue $filevalue
    }
    method CommitDisplay {} { multibox pathlabel filelabel \
	    pathvalue filevalue } {
	set pathvalue [$multibox LookUp $pathlabel]
	set filevalue [$multibox LookUp $filelabel]
    }
    method MultiboxCallback { subwidget actionid args } { callback } {
	$this CommitDisplay
	eval $callback $this SELECT -pathvalue -filevalue
    }
    method SetFullname { myfullname } { pathvalue filevalue \
	    pathlabel filelabel multibox } {
	set pathvalue [file dirname $myfullname]
	set filevalue [file tail $myfullname]
	if {[string compare "." $filevalue]==0} {
	    set filevalue {}
	}
	$multibox Set $pathlabel $pathvalue $filelabel $filevalue
    }
    method SetFilename { myfilevalue } {filevalue multibox filelabel} {
	set filevalue $myfilevalue
	$multibox Set $filelabel $filevalue
    }
    method SetPath { mypathvalue } {pathvalue multibox pathlabel} {
	set pathvalue $mypathvalue
	$multibox Set $pathlabel $pathvalue
    }
    method WinDestroy { w } { winpath } {
	if {[info exists winpath] && [string compare $winpath $w]!=0} {
	    return    ;# Child destroy event
	}
	$this Delete
    }
    Destructor {
	if {[info exists winpath] && [winfo exists $winpath]} {
            # Remove <Destroy> binding
	    bind $winpath <Destroy> {}
	    # Destroy instance frame, all children, and bindings
	    destroy $winpath
	}
    }
}
