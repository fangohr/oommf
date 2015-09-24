# FILE: log.tcl
#
# Last modified on: $Date: 2007-03-21 01:17:09 $
# Last modified by: $Author: donahue $

Oc_Class Ow_Log {
    public variable title {
        wm title [winfo toplevel $basewinpath] $title
    }
    public variable memory = 1000
    public variable dateformat = "\[%Y %b %d, %H:%M:%S %Z\] "
    public variable basewinpath = .
    const private variable winpath
    const private variable tw
    const private variable xscroll
    const private variable yscroll
    private variable lineslogged
    private variable linesinwidget

    Constructor {args} {
        eval $this Configure $args
        # Don't add '.' suffix if there already is one.
        # (This happens chiefly when basewinpath is exactly '.')
        if {![string match {.} \
                [string index $basewinpath \
                [expr [string length $basewinpath]-1]]]} {
            append basewinpath {.}
        }

        set winpath ${basewinpath}w$this
        frame $winpath -bd 4 -relief ridge
        set tw [text $winpath.text -background white -height 5]
        set xscroll [scrollbar $winpath.xscroll -orient horizontal \
            -command [list $tw xview]]
        set yscroll [scrollbar $winpath.yscroll -orient vertical \
            -command [list $tw yview]]
        $tw configure -yscrollcommand [list $yscroll set] \
            -xscrollcommand [list $xscroll set] -state disabled -wrap none
        eval $this Configure $args
        if {![info exists title]} {
            $this Configure -title "Widget Log"
        }
        set lineslogged 0
        set linesinwidget 0
        pack $yscroll -side right -fill y
        pack $xscroll -side bottom -fill x
        pack $tw -side left -fill both -expand 1
        bind $winpath <Destroy> [list $this WinDestroy %W]
        pack $winpath -side bottom -fill both -expand 1
        $this Append "Starting $title ..."
    }

    method Append { args } { tw dateformat lineslogged linesinwidget memory } {
        # Cleanup routines might try to log messages after the window
        # is destroyed.  Prevent that error.
	if {![winfo exists $tw]} {
            return
        }
        $tw configure -state normal
        if {[llength $args] == 1} {
            set msg [lindex $args 0]
            set nonewline 0
        } elseif {([llength $args] == 2) \
                && [string match -nonewline [lindex $args 0]]} {
            set msg [lindex $args 1]
            set nonewline 1
        } else {
            error "usage: <$class instance> Append ?-nonewline? <msg>"
        }
        set msg "[clock format [clock seconds] -format $dateformat]$msg"
        set linestolog [llength [split $msg "\n"]]
        if {$nonewline} {
            $tw insert end "$msg"
        } else {
            $tw insert end "$msg\n"
        }
        $tw see end
	### update idletasks   ;# Only uncomment this if *really* needed,
	### because sometimes we want to call this method from inside an
	### event handler.
        incr lineslogged $linestolog
        incr linesinwidget $linestolog
        set excess [expr $linesinwidget - $memory]
        if {$excess > 0} {
            $this Purge $excess
        }
        $tw configure -state disabled
    }

    private method Purge { numlines } { tw linesinwidget } {
        $tw delete 1.0 "1.0 + $numlines lines"
        incr linesinwidget -$numlines
    }

    method WinDestroy { w } { winpath } {
        if {[string compare $winpath $w]} {
            return	;# Child destroy event
        }
        $this Delete
    }

    # Append a line to this log
    Destructor {
        Oc_EventHandler Generate $this Delete
	if {[info exists winpath] && [winfo exists $winpath]} {
            bind $winpath <Destroy> {}
            destroy $winpath
        }
    }

}

