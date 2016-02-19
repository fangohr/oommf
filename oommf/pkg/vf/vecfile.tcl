# FILE: vecfile.tcl
#
# Tcl/Tk routines for vector file input
#
# Last modified on: $Date: 2007/03/21 01:17:11 $
# Last modified by: $Author: donahue $
#

proc Vf_SvfIsType { filename chanid } {
    # Returns 1 if the input file is an OVF v0.0 (aka SVF) file.

    if { [gets $chanid line] <= 0 } { return 0; }
    set leadline [string tolower [string trimright $line]]

    # See if first line has "# SVF-2" -like string
    if { [regexp -- "^#\[ \t\]*svf-\[0-9\]+" $leadline] } { return 1 }

    # Pre 15-July-1999 versions of mmSolve wrote "v0.0a0" into what
    # are really ovf v1.0 files.
    if {[regexp -- "^#\[ \t\]*oommf:.*v0.0a0" $leadline]} { return 0 }

    # Check for proper OVF v0.0 header: "# OOMMF v0.0"
    if {[regexp -- "^#\[ \t\]*oommf.*v\[\.0\]+$" $leadline]} { return 1 }

    # If leadline still starts with "# OOMMF", then it must be a later
    # version of OVF.
    if {[regexp -- "^#\[ \t\]*oommf" $leadline]} { return 0; }

    # Otherwise, if it has an svf or o?f extension, and the first
    # non-comment line has the proper 5- or 6-column numeric format,
    # assume it is an OVF v1.0 file/
    if { ![string match {*.svf*} [string tolower $filename]] \
	&& ![string match {*.o?f*} [string tolower $filename]] } {
	return 0;
    }
    while { [regexp -- "^\[ \t\]*#|^\[ \t\]*$" $line] } {
	# $line is a blank or comment line, so get another line
	if { [gets $chanid line] <= 0 } { return 0; }
    }
    # $line is now first "real" line
    set nf {([-+]?[.0-9]+[eEdD]?[-+]?[0-9]*)}
    ## Numeric field regexp pattern with grouping
    set ws "\[ \t\]"         ;# Whitespace regexp pattern
    set cmt {(#.*)?}         ;# Comment regexp pattern
    set datapat "^$ws*$nf$ws+$nf$ws+$nf$ws+$nf$ws+$nf\($ws+$nf\)?$ws*$cmt$"
    if { [regexp -- $datapat $line] } {
	return 1      ;# Line has proper data format
    }
    return 0
}

# Utility proc for Vf_SvfRead to process svf header lines
# Returns >0 if line contains auxilary info,
#          0 if line is uninterpreted valid (an ordinary comment),
#    or   -1 if line has bad format (not a comment line)
;proc _vfProcessCommentLine { sfi_index line } {
    set ws "\[ \t\]"           ;# Whitespace regexp pattern
    set cmt {(#.*)?}           ;# Comment regexp pattern
    set blankpat "^$ws*$cmt$"  ;# Blank line
    set returncode "0"
    # If not in valid comment format, mark as bad.
    if { ![regexp -- $blankpat $line] } {
	set returncode "-1"
    } else {
	# Valid comment line.  Check for auxiliary information.
	# Boundary info
	if { [regexp -nocase -- "^##$ws*boundary.*xy\[:\]?(.*)" \
		$line junk points] } {
	    # $points should be list of boundary points
	    set points [string trim $points]
	    Vf_SvfFileInputTA setbdry $sfi_index $points
	    incr returncode
	}
	# Grid step info
	if { [regexp -nocase -- "##$ws*grid$ws*step\[:\]?(.*)" \
		$line junk steps] } {
	    # $steps should be list of x, y, z steps
	    set steps [string trim $steps]
	    Vf_SvfFileInputTA stephints $sfi_index $steps
	    incr returncode
	}
	# Title info
	if { [regexp -nocase -- "##$ws*title$ws*\[:\]?(.*)" \
		$line junk title] } {
	    set title [string trim $title]
	    Vf_SvfFileInputTA settitle $sfi_index $title
	    incr returncode
	}
    }
    return $returncode
}

# Procedure to process an SVF file.  Returns the number of points, the
# number of lines, and the number of lines that couldn't be parsed,
# i.e., the third field should be 0 for a properly formatted file.
proc Vf_SvfRead { chanid sfi_index } {
    set commentcount 0 ; set badlinecount 0
    while { [gets $chanid line] >= 0 } {
	set linecode [Vf_SvfFileInputTA processline $sfi_index $line]
	if { [string match "1" $linecode] } { continue }
	## The above case is the most common, data line case.  The
	## parsing is done in C for speed.

	# Below follow the comment line or "bad" data line cases.
	if { [string match "0" $linecode] } {
	    _vfProcessCommentLine $sfi_index $line  ;#Comment line
	    incr commentcount
	} elseif { [string match "-1" $linecode] } {
	    incr badlinecount                        ;# Bad data line
	} else {
	    Oc_Log Log \
	    "Unknown return code from \"Vf_SvfFileInputTA processline\"" \
            panic
            exit 1
	}
    }
    set pointcount [Vf_SvfFileInputTA getpointcount $sfi_index]
    set linecount  [expr {$pointcount + $commentcount + $badlinecount}]
    Oc_Log Log  "Vf_SvfRead Debug Output:\
	    $pointcount $linecount $badlinecount" debug
    return [list $pointcount $linecount $badlinecount]
}
