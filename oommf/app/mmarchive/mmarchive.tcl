# FILE: mmarchive.tcl
#
# Accept data from solvers and save to file.
# Accepts vectorfield, scalarfield, and datatable formats
# Provide no GUI, so this app survives death of X

# Support libraries
package require Oc 1.2.0.4	;# [Oc_TempName]
package require Net 1.2.0.4
if {[Oc_Main HasTk]} {
    wm withdraw .
}

Oc_Main SetAppName mmArchive
Oc_Main SetVersion 1.2.0.5
regexp \\\044Date:(.*)\\\044 {$Date: 2012-09-25 17:11:47 $} _ date
Oc_Main SetDate [string trim $date]
Oc_Main SetAuthor [Oc_Person Lookup dgp]

Oc_CommandLine ActivateOptionSet Net
Oc_CommandLine Parse $argv

Oc_EventHandler Bindtags all all

##------ GUI ---------#
set gui {
    package require Oc 1.1
    package require Tk
    package require Ow
    wm withdraw .
}
append gui "[list Oc_Main SetAppName [Oc_Main GetAppName]]\n"
append gui "[list Oc_Main SetVersion [Oc_Main GetVersion]]\n"
append gui "[list Oc_Main SetPid [pid]]\n"
append gui {

regexp \\\044Date:(.*)\\\044 {$Date: 2012-09-25 17:11:47 $} _ date
Oc_Main SetDate [string trim $date]

# This won't cross different OOMMF installations nicely
Oc_Main SetAuthor [Oc_Person Lookup dgp]

proc SetOidCallback {code result args} {
   if {$code == 0} {Oc_Main SetOid $result}
   rename SetOidCallback {}
}
remote -callback SetOidCallback serveroid

catch {
    set mmArchive_doc_section [list [file join \
	    [Oc_Main GetOOMMFRootDir] doc userguide userguide \
	    Data_Archive_mmArchive.html]]
    Oc_Main SetHelpURL [Oc_Url FromFilename $mmArchive_doc_section]
}

# May want to add error sharing code here.
set menubar .mb
foreach {fmenu hmenu} [Ow_MakeMenubar . $menubar File Help] {}
$fmenu add command -label "Show Console" -command { console show } -underline 0
$fmenu add command -label "Close Interface" -command { closeGui } -underline 0
$fmenu add command -label "Exit [Oc_Main GetAppName]" -command exit -underline 1
Ow_StdHelpMenu $hmenu
set menuwidth [Ow_GuessMenuWidth $menubar]
set brace [canvas .brace -width $menuwidth -height 0 -borderwidth 0 \
        -highlightthickness 0]
pack $brace -side top

share status
pack [label .statusLabel -text Status: -anchor e] -side left 
pack [label .statusValue -textvariable status -anchor w] -side right -fill x

if {[Ow_IsAqua]} {
   # Add some padding to allow space for Aqua window resize
   # control in lower righthand corner
   . configure -bd 4 -relief ridge -padx 10
}

}

##------ Control server ---------#
#
[Net_Server New _ -alias [Oc_Main GetAppName] \
	-protocol [Net_GeneralInterfaceProtocol $gui {}]] Start 0
#
##------ Control server ---------#

##------ DataTable server ---------#
#
Net_Protocol New protocol(dt) -name "OOMMF DataTable protocol 0.1"
$protocol(dt) AddMessage start DataTable { triples } {
    WriteTriples $connection $triples
    return [list start [list 0 0]]
}
[Net_Server New server(dt) -protocol $protocol(dt) \
	-alias [Oc_Main GetAppName]] Start 0
#
##------ DataTable server ---------#

##------ Vectorfield server ---------#
#
Net_Protocol New protocol(vf) -name "OOMMF vectorField protocol 0.1"
$protocol(vf) AddMessage start datafile { fnlist } {
    global status
    set tmpfile [lindex $fnlist 0]
    set permfile [lindex $fnlist 1]
    # Report writing of file
    # Ought to add lots of error checking here...
    set status "Writing $permfile ..."
    file rename -force $tmpfile $permfile
    return [list start [list 0 0]]
}
[Net_Server New server(vf) -protocol $protocol(vf) \
	-alias [Oc_Main GetAppName]] Start 0
#
##------ Vectorfield server ---------#

##------ Scalarfield server ---------#
#
Net_Protocol New protocol(vf) -name "OOMMF scalarField protocol 0.1"
$protocol(vf) AddMessage start datafile { fnlist } {
    global status
    set tmpfile [lindex $fnlist 0]
    set permfile [lindex $fnlist 1]
    # Report writing of file
    # Ought to add lots of error checking here...
    set status "Writing $permfile ..."
    file rename -force $tmpfile $permfile
    return [list start [list 0 0]]
}
[Net_Server New server(vf) -protocol $protocol(vf) \
	-alias [Oc_Main GetAppName]] Start 0
#
##------ Scalarfield server ---------#

##------ DataTable helper procs ---------#
proc WriteTriples {conn triples} {
    global conn_file_req   ;# Maps connection to requested filename
    global conn_file       ;# Maps connection to actual filename
    global conn_channel    ;# Maps connection to open channel
    global conn_col_list   ;# Maps connection to table column labels
    global conn_head_width ;# Column header widths
    global conn_data_fmt    ;# Format list for data
    global status

    set basewidth 20        ;# Default column width
    set basefmt "%- 20.17g"  ;# Base format for table body

    # Create "tripdata" array that maps labels to values
    foreach trip $triples {
	foreach {label {} value} $trip {
	    set tripdata($label) $value
	}
    }

    # Close current file?
    set newfile [lindex [array names tripdata "@Filename:*"] end]
    if {![string match {} $newfile]} {
	set newfile [string trim [string range $newfile 10 end]]
	if {[string match {} $newfile]} {
	    # Explicit close-file request
	    CloseTriplesFile $conn
	    return
	}
	if {[info exists conn_file_req($conn)] && \
		![string match $conn_file_req($conn) $newfile]} {
	    CloseTriplesFile $conn
	}
    }


    # Have we already opened a file for this connection?
    if {[catch {set chan $conn_channel($conn)}]} {
	# New connection!

	# Build column list
	set conn_file($conn) {}
	set conn_file_req($conn) $newfile
	set conn_channel($conn) {}
	set conn_col_list($conn) {}
        set conn_head_width($conn) {}
        set conn_data_fmt($conn) {}
	set unit_list {}
	foreach trip $triples {
	    foreach {label unit {}} $trip {
		if {![string match "@Filename:*" $label]} {
		    lappend conn_col_list($conn) $label  ;# New data column
		    lappend unit_list $unit
		}
	    }
	}

        # Determine column formats
        foreach label $conn_col_list($conn) unit $unit_list {
            set width [string length [list $label]]
            set unit_width [string length [list $unit]]
            if {$width<$unit_width} { set width $unit_width }
            lappend conn_head_width($conn) $width
            set fmt " $basefmt"
            if {$width>$basewidth} {
                # Extra-wide column.  Pad numeric format with
                # spaces on both sides.
                set left [expr {($width-$basewidth)/2}]
                set right [expr {$width-$basewidth-$left}]
                set fmt [format " %*s%s%*s" $left {} $basefmt $right {}]
            }
            lappend conn_data_fmt($conn) $fmt
        }

	# Open output file for append
	set chan {}
	set conn_file($conn) $conn_file_req($conn)
	if {[string match {} $conn_file($conn)] || \
		[catch {open $conn_file($conn) "a+"} chan]} {
	    # Unable to open output file.  Try using temporary filename
	    set conn_file($conn) [Oc_TempName "mmArchive" ".odt"]
	    if {[catch {open $conn_file($conn) "a+"} chan]} {
		# Abandon ship!
		error "Unable to open DataTable output file: $chan"
	    }
	}
	set status "Opened $conn_file($conn)"
	set conn_channel($conn) $chan

	# Setup exit handler
        global server
	Oc_EventHandler New _ $conn Delete \
		[list CloseTriplesFile $conn] -oneshot 1
	Oc_EventHandler New _ Oc_Main Exit     \
		[list CloseTriplesFile $conn]  \
		-oneshot 1 -groups [list $conn]

	# Write out table header
	if {[tell $chan]<1} {
	    puts $chan "# ODT 1.0" ;# File magic
	} else {
	    puts $chan "#"         ;# Separate blocks with an empty line
	}
	puts $chan "# Table Start"
	puts $chan "# Title: mmArchive Data Table,\
		[clock format [clock seconds]]"
	puts -nonewline $chan "# Columns:"
        # Drop labels and units on left of each column, mutually centered.
	foreach label $conn_col_list($conn) \
                headwidth $conn_head_width($conn) {
            set key [list $label]
            set keylength [string length $key]
            if {$keylength<$headwidth} {
                set left [expr {($headwidth-$keylength)/2}]
                set right [expr {$headwidth-$keylength-$left}]
            } else {
                set left 0
                set right 0
            }
            if {$headwidth<$basewidth} {
                incr right [expr {$basewidth-$headwidth}]
            }
            puts -nonewline $chan \
                    [format " %*s%s%*s" $left {} $key $right {}]
	}
	puts -nonewline $chan "\n# Units:  "
	foreach unit $unit_list \
                headwidth $conn_head_width($conn) {
            set key [list $unit]
            set keylength [string length $key]
            if {$keylength<$headwidth} {
                set left [expr {($headwidth-$keylength)/2}]
                set right [expr {$headwidth-$keylength-$left}]
            } else {
                set left 0
                set right 0
            }
            if {$headwidth<$basewidth} {
                incr right [expr {$basewidth-$headwidth}]
            }
            puts -nonewline $chan \
                    [format " %*s%s%*s" $left {} $key $right {}]
	}
	puts $chan {}  ;# newline
	flush $chan
    }

    # Index into tripdata using the column list.  Note that
    # there is no check for extra (new) data columns; any
    # such will just be ignored.
    puts -nonewline $chan "          "  ;# Indent past "# Columns:" heading
    foreach label $conn_col_list($conn)  \
            width $conn_head_width($conn) \
            fmt $conn_data_fmt($conn) {
	if {[catch {set tripdata($label)} value]} {
	    # Missing value.  This shouldn't happen, but the format could
            # be pre-computed and saved in a list if needed (for speed).
            set left [expr {($width-2)/2}]
            if {$width<$basewidth} { set width $basewidth }
            set right [expr {$width-2-$left}]
	    set value [format " %*s{}%*s" $left {} $right {}]
	} else {
	    set value [format $fmt $value]
	}
	puts -nonewline $chan $value
    }
    puts $chan {}
    flush $chan
}

proc CloseTriplesFile {conn} {
    global conn_file_req conn_file conn_channel conn_col_list
    global conn_head_width conn_data_fmt
    global status
    if {![catch {set conn_channel($conn)} chan]} {
	puts $chan "# Table End"
	close $chan
	set status "Closing $conn_file($conn) ..."
    }
    catch {unset conn_file_req($conn)}
    catch {unset conn_file($conn)}
    catch {unset conn_channel($conn)}
    catch {unset conn_col_list($conn)}
    catch {unset conn_head_width($conn)}
    catch {unset conn_data_fmt($conn)}
}

##^^---- DataTable helper procs -------^^#

vwait forever
